//project headers:
#include "EvaluableNodeManagement.h"
#include "Interpreter.h"
#include "PerformanceProfiler.h"

//system headers:
#include <ranges>
#include <string>
#include <vector>
#include <utility>

//TODO 26165: assert lab size is a multiple of _num_nodes_per_slab
//TODO 26165: reduce alloc code by lambda

constexpr size_t _min_gc_nodes_threshold_default = 64;
constexpr size_t _max_gc_nodes_threshold_default = 1000000000;
constexpr float _extra_memory_capacity_factor_default = 3.0f;
constexpr float _min_memory_retention_factor_default = 0.95f;
constexpr float _alloc_expansion_factor_default = 1.5f;

//number of nodes allocated per slab block to reduce memory fragmentation
constexpr int _num_nodes_per_slab = 4;

static constexpr size_t RoundNodeIndexToNextSlab(size_t index)
{
	size_t slab_index = (index + _num_nodes_per_slab - 1) / _num_nodes_per_slab;
	return slab_index * _num_nodes_per_slab;
}

size_t EvaluableNodeManager::minGarbageCollectionNodesThreshold = _min_gc_nodes_threshold_default;
size_t EvaluableNodeManager::maxGarbageCollectionNodesThreshold = _max_gc_nodes_threshold_default;
float EvaluableNodeManager::extraMemoryCapacityFactor = _extra_memory_capacity_factor_default;
float EvaluableNodeManager::minMemoryRetentionFactor = _min_memory_retention_factor_default;
float EvaluableNodeManager::allocExpansionFactor = _alloc_expansion_factor_default;

#ifdef MULTITHREAD_SUPPORT
//tunable parameter for how many nodes to have a garbage collection sweep perform at a time
constexpr size_t _invalidate_nodes_task_size = 4000;
#endif

EvaluableNodeManager::~EvaluableNodeManager()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(managerAttributesMutex);

	//clear from any threads
	LocalAllocationBuffer::IterateFunctionOverRegisteredLabs(
		[this](LocalAllocationBuffer *lab)
	{
		lab->Clear(this);
	});
#else
	localAllocationBuffer.Clear(this);
#endif

	//sort by the slab starts and only delete those; will ensure all memory is freed
	auto slab_end_it = std::partition(begin(nodes), end(nodes),
		[](EvaluableNode *n)
		{ return n != nullptr && n->GetIsSlabStart(); });

	for(auto it = begin(nodes); it != slab_end_it; ++it)
	{
		EvaluableNode *slab_start = *it;
		std::destroy_n(slab_start, _num_nodes_per_slab);
		operator delete(slab_start);
	}
}

void EvaluableNodeManager::UpdateGarbageCollectionTrigger(size_t previous_num_nodes)
{
	//assume at least a factor larger than the base memory usage for the entity
	//add 1 for good measure and to make sure the smallest size isn't zero
	size_t max_from_current = static_cast<size_t>(extraMemoryCapacityFactor * GetNumberOfUsedNodes()) + 1;

	size_t cur_num_nodes = GetNumberOfUsedNodes();
	//use snapshot, in the off chance that a concurrent collection occurred
	size_t previous_trigger = numNodesToRunGarbageCollection;
	size_t next_trigger = max_from_current;
	if(previous_trigger > cur_num_nodes)
	{
		//scale down the number of nodes previously allocated, because there is always a chance that
		//a large allocation goes beyond that size and so the memory keeps growing
		//by using a fraction less than 1, it reduces the chances of a slow memory increase
		size_t diff_from_current = previous_trigger - cur_num_nodes;
		size_t max_from_previous = cur_num_nodes
			+ static_cast<size_t>(minMemoryRetentionFactor * diff_from_current);

		next_trigger = std::max<size_t>(max_from_previous, max_from_current);
	}

	//make sure doesn't go below the threshold
	next_trigger = std::max(minGarbageCollectionNodesThreshold, next_trigger);
	numNodesToRunGarbageCollection = std::min(next_trigger, cur_num_nodes + maxGarbageCollectionNodesThreshold);
}

void EvaluableNodeManager::CollectGarbage()
{
	if(PerformanceProfiler::IsProfilingEnabled())
	{
		static const std::string collect_garbage_string = ".collect_garbage";
		PerformanceProfiler::StartOperation(collect_garbage_string, GetNumberOfUsedNodes());
	}

	//clear regardless of what's in the buffer
	localAllocationBuffer.Clear();
	//clear all threads' local allocation buffers that are using this enm
#ifdef MULTITHREAD_SUPPORT
	LocalAllocationBuffer::IterateFunctionOverRegisteredLabs(
		[this](LocalAllocationBuffer *lab)
		{
			lab->Clear(this);
		});
#endif

	MarkAllReferencedNodesInUse(firstUnusedNodeIndex);

	FreeAllNodesExceptReferencedNodes(firstUnusedNodeIndex);

	if(PerformanceProfiler::IsProfilingEnabled())
		PerformanceProfiler::EndOperation(GetNumberOfUsedNodes());
}

#ifdef MULTITHREAD_SUPPORT
void EvaluableNodeManager::CollectGarbageWithConcurrentAccess(Concurrency::ReadLock &memory_modification_lock)
{
	if(PerformanceProfiler::IsProfilingEnabled())
	{
		static const std::string collect_garbage_string = ".collect_garbage";
		PerformanceProfiler::StartOperation(collect_garbage_string, GetNumberOfUsedNodes());
	}

	//clear regardless of what's in the buffer
	// the clear by the thread that gets selected for GC below will catch and clear any threads that have gone inactive
	localAllocationBuffer.Clear();

	//the first thread to set the flag becomes the garbage collector
	bool gc_on_this_thread = false;
	if(RecommendGarbageCollection())
		gc_on_this_thread =
			!activeInterpreters->garbageCollectionThreadSelectionFlag.test_and_set(std::memory_order_acquire);

	//release after gc recommendation has been checked
	memory_modification_lock.unlock();

	if(gc_on_this_thread)
	{
		//make other threads wait, but do so before locking other threads out in case some thread activating
		{
			Concurrency::SingleLock lock(activeInterpreters->garbageCollectionNotificationMutex);
			activeInterpreters->garbageCollectionInProgress.store(true, std::memory_order_release);
		}

		Concurrency::WriteLock write_lock(activeInterpreters->memoryModificationMutex);

		//clear all threads' local allocation buffers that are using this enm
		LocalAllocationBuffer::IterateFunctionOverRegisteredLabs(
			[this](LocalAllocationBuffer *lab)
		{
			lab->Clear(this);
		});

		size_t cur_first_unused_node_index = firstUnusedNodeIndex;
		//clear firstUnusedNodeIndex to signal to other threads that they won't need to do garbage collection
		firstUnusedNodeIndex = 0;

		//if any group of nodes on the top are ready to be cleaned up cheaply, do so first
		while(cur_first_unused_node_index > 0 && nodes[cur_first_unused_node_index - 1] != nullptr
				&& nodes[cur_first_unused_node_index - 1]->IsNodeDeallocated())
			cur_first_unused_node_index--;

		MarkAllReferencedNodesInUse(cur_first_unused_node_index);
		FreeAllNodesExceptReferencedNodes(cur_first_unused_node_index);

		//wake up remaining threads 
		{
			//lock the notification mutex to prevent other threads from waking up and seeing
			//an outdated state of garbageCollectionThreadSelectionFlag
			Concurrency::SingleLock lock(activeInterpreters->garbageCollectionNotificationMutex);
			activeInterpreters->garbageCollectionInProgress.store(false, std::memory_order_release);
			activeInterpreters->garbageCollectionThreadSelectionFlag.clear(std::memory_order_release);
		}
		activeInterpreters->garbageCollectionConditionVar.notify_all();

		write_lock.unlock();
	}
	else //wait for GC to finish
	{
		Concurrency::SingleLock lock(activeInterpreters->garbageCollectionNotificationMutex);
		activeInterpreters->garbageCollectionConditionVar.wait(lock, [this]()
		{
			return !activeInterpreters->garbageCollectionInProgress.load(std::memory_order_acquire);
		});
	}

	memory_modification_lock.lock();

	if(PerformanceProfiler::IsProfilingEnabled())
		PerformanceProfiler::EndOperation(GetNumberOfUsedNodes());
}
#endif

void EvaluableNodeManager::FreeAllNodes()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(managerAttributesMutex);
#endif

	size_t original_num_nodes = firstUnusedNodeIndex;
	//use original_num_nodes in loop in case of multithreading for performance on some platforms
	for(size_t i = 0; i < original_num_nodes; i++)
	{
		if(nodes[i] != nullptr && !nodes[i]->IsNodeDeallocated())
			nodes[i]->Invalidate();
	}

	firstUnusedNodeIndex = 0;

	UpdateGarbageCollectionTrigger(original_num_nodes);

	localAllocationBuffer.Clear(this);
}

EvaluableNode *EvaluableNodeManager::AllocUninitializedNode()
{
#ifdef DEBUG_REPORT_LAB_USAGE
	{
	#if defined(MULTITHREAD_SUPPORT)
		Concurrency::SingleLock lock(labCountMutex);
	#endif

		labSize += localAllocationBuffer.buffer.size();
		labSizeCount++;

		//use an exponentially rolling average, use a small fraction of the new value (1/256th)
		rollingAveLABSize = 0.99609375 * rollingAveLABSize + 0.00390625 * localAllocationBuffer.buffer.size();
		if(labSizeCount % 4000 == 0)
			std::cout << "ave local allocation buffer size: " << rollingAveLABSize << std::endl;
	}
#endif

	EvaluableNode *lab_node = AllocNodeFromLocalAllocationBufferIfAvailable();
	if(lab_node != nullptr)
		return lab_node;

	//round up to the next nearest slab
	constexpr size_t num_nodes_to_allocate = RoundNodeIndexToNextSlab(labBlockAllocationSize);

	//attempt to allocate enough nodes to refill local allocation buffer
#ifdef MULTITHREAD_SUPPORT
	//slow path allocation; attempt to allocate using an atomic without write locking
	Concurrency::ReadLock read_lock(managerAttributesMutex);

	size_t first_index_to_allocate = firstUnusedNodeIndex.fetch_add(num_nodes_to_allocate);
#else
	size_t first_index_to_allocate = firstUnusedNodeIndex;
	firstUnusedNodeIndex += num_nodes_to_allocate;
#endif

	size_t last_index_to_allocate = first_index_to_allocate + num_nodes_to_allocate;

	if(last_index_to_allocate < nodes.size())
	{
		for(size_t i = first_index_to_allocate; i < last_index_to_allocate; i++)
		{
			if(nodes[i] == nullptr)
			{
				//this can only occur on nodes that are round numbers of _num_nodes_per_slab
			#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
				AmlgAssert(i % _num_nodes_per_slab == 0);
			#endif

				EvaluableNode *slab_memory = static_cast<EvaluableNode *>(
						operator new(sizeof(EvaluableNode) * _num_nodes_per_slab)
					);

				for(size_t j = 0; j < _num_nodes_per_slab; j++)
				{
				#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
					AmlgAssert(nodes[i + j] == nullptr);
				#endif

					nodes[i + j] = new (&slab_memory[j]) EvaluableNode(ENT_DEALLOCATED);
				}
				nodes[i]->SetIsSlabStart(true);
			}

			AddNodeToLocalAllocationBuffer(nodes[i]);
		}

	#ifdef MULTITHREAD_SUPPORT
		read_lock.unlock();
	#endif
		return AllocNodeFromLocalAllocationBufferIfAvailable();
	}

#ifdef MULTITHREAD_SUPPORT
	read_lock.unlock();

	//don't have enough nodes, so need to attempt a write lock to allocate more
	Concurrency::WriteLock write_lock(managerAttributesMutex);
#endif

	size_t num_nodes = nodes.size();
	if(last_index_to_allocate >= num_nodes)
	{
		//ran out, so need another node; push a bunch on the heap so don't need to reallocate as often and slow down garbage collection
		//add extra node at the end in case of rounding down
		//preallocate additional resources, making sure to at least add one block
		size_t new_num_nodes = static_cast<size_t>(allocExpansionFactor * (num_nodes + labBlockAllocationSize)) + 1;
		
		//round new_num_nodes up to nearest _num_nodes_per_slab
		new_num_nodes = RoundNodeIndexToNextSlab(new_num_nodes);

		//fill new EvaluableNode slots with nullptr
		nodes.resize(new_num_nodes, nullptr);
	}

	for(size_t i = first_index_to_allocate; i < last_index_to_allocate; i++)
	{
		if(nodes[i] == nullptr)
		{
			//this can only occur on nodes that are round numbers of _num_nodes_per_slab
		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			AmlgAssert(i % _num_nodes_per_slab == 0);
		#endif

			EvaluableNode *slab_memory =
				static_cast<EvaluableNode *>(operator new(sizeof(EvaluableNode) * _num_nodes_per_slab));

			for(size_t j = 0; j < _num_nodes_per_slab; j++)
			{
			#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
				AmlgAssert(nodes[i + j] == nullptr);
			#endif

				nodes[i + j] = new (&slab_memory[j]) EvaluableNode(ENT_DEALLOCATED);
			}
			nodes[i]->SetIsSlabStart(true);
		}

		AddNodeToLocalAllocationBuffer(nodes[i]);
	}

#ifdef MULTITHREAD_SUPPORT
	write_lock.unlock();
#endif
	return AllocNodeFromLocalAllocationBufferIfAvailable();
}

void EvaluableNodeManager::FreeAllNodesExceptReferencedNodes(size_t cur_first_unused_node_index)
{
	//move all nodes in use to the front and unused ones to the back
	size_t last_active_index = cur_first_unused_node_index;
	//index that is being considered
	size_t cur_candidate_index = 0;
	//next index that can be written to in a swap
	size_t next_write_index = 0;

	//traverse nodes until find the first unused
	for(; cur_candidate_index < last_active_index; cur_candidate_index++)
	{
		auto &current_node = nodes[cur_candidate_index];
		if(!current_node->GetKnownToBeInUse())
			break;

		current_node->SetKnownToBeInUse(false);
	}

	//move to the next node, leaving the previous one behind
	next_write_index = cur_candidate_index;
	cur_candidate_index++;

	//move unused back into the next_write_index
	for(; cur_candidate_index < last_active_index; cur_candidate_index++)
	{
		auto &current_node = nodes[cur_candidate_index];

		if(current_node->GetKnownToBeInUse())
		{
			current_node->SetKnownToBeInUse(false);
			std::swap(current_node, nodes[next_write_index]);
			next_write_index++;
		}
	}

#ifdef MULTITHREAD_SUPPORT
	size_t num_nodes_to_invalidate = last_active_index - next_write_index;
	if(Concurrency::GetMaxNumThreads() > 1 && num_nodes_to_invalidate > 2 * _invalidate_nodes_task_size)
	{
		Concurrency::TaskSet task_set;

		//free each full block of _invalidate_nodes_task_size
		size_t start_index = next_write_index;
		for(; start_index + _invalidate_nodes_task_size < last_active_index; start_index += _invalidate_nodes_task_size)
			task_set.emplace(
				[this, start_index]
				{
					size_t end_index = start_index + _invalidate_nodes_task_size;
					for(size_t i = start_index; i < end_index; i++)
					{
						if(!nodes[i]->IsNodeDeallocated())
							nodes[i]->Invalidate();
					}
				});

		//invalidate any remaining that are fewer than _invalidate_nodes_task_size
		if(start_index < last_active_index)
			task_set.emplace(
				[this, start_index, last_active_index]
				{
					for(size_t i = start_index; i < last_active_index; i++)
					{
						if(!nodes[i]->IsNodeDeallocated())
							nodes[i]->Invalidate();
					}
				});

		Concurrency::RunSystemTasks(task_set);
	}
	else
#endif
	{
		for(size_t i = next_write_index; i < last_active_index; i++)
		{
			if(!nodes[i]->IsNodeDeallocated())
				nodes[i]->Invalidate();
		}
	}

	firstUnusedNodeIndex = RoundNodeIndexToNextSlab(next_write_index);
	UpdateGarbageCollectionTrigger(cur_first_unused_node_index);

	//if have more yet another entire extraMemoryCapacityFactor available, free it
	if(nodes.size() > extraMemoryCapacityFactor * numNodesToRunGarbageCollection)
		ShrinkMemoryToCurrentUtilizationWithLock();
}

//TODO 26165: make this more efficient and replace method below
/*
void EvaluableNodeManager::ShrinkMemoryToCurrentUtilizationWithLock()
{
	if(nodes.empty())
		return;

	size_t min_capacity = static_cast<size_t>(firstUnusedNodeIndex * extraMemoryCapacityFactor) + 1;
	size_t target_size = std::min(nodes.size(), min_capacity);
	target_size = RoundNodeIndexToNextSlab(target_size);

	//if the current size is already at or below our floor, nothing to shrink
	if(nodes.size() <= target_size)
		return;

	std::sort(begin(nodes) + firstUnusedNodeIndex, end(nodes));

	//walk the locations and find completed slabs to delete
	for(size_t i = nodes.size() - 1; i >= firstUnusedNodeIndex; i--)
	{
		EvaluableNode *node = nodes[i];
		if(node == nullptr || !node->GetIsSlabStart())
			continue;

		bool slab_complete = true;
		for(size_t offset = 1; offset < _num_nodes_per_slab; ++offset)
		{
			EvaluableNode *expected =
				reinterpret_cast<EvaluableNode *>(reinterpret_cast<char *>(node) + (offset * sizeof(EvaluableNode)));

			auto it = std::find(begin(nodes) + firstUnusedNodeIndex, end(nodes), expected);
			if(it == end(nodes))
			{
				slab_complete = false;
				break;
			}
		}

		if(slab_complete)
		{
			EvaluableNode *laundered_start = std::launder(node);
			std::destroy_n(laundered_start, _num_nodes_per_slab);
			operator delete(laundered_start);

			for(size_t j = firstUnusedNodeIndex; j < nodes.size(); ++j)
			{
				if(nodes[j] != nullptr)
				{
					//check if this pointer falls within the range of the deleted slab
					char *ptr_addr = reinterpret_cast<char *>(nodes[j]);
					char *slab_base = reinterpret_cast<char *>(node);
					if(ptr_addr >= slab_base && ptr_addr < (slab_base + (_num_nodes_per_slab * sizeof(EvaluableNode))))
						nodes[j] = nullptr;
				}
			}
		}
	}

	//move remaining non-null pointers to the front of the unused section
	size_t write_index = firstUnusedNodeIndex;
	for(size_t read_index = firstUnusedNodeIndex; read_index < nodes.size(); read_index++)
	{
		if(nodes[read_index] != nullptr)
			nodes[write_index++] = nodes[read_index];
	}

	size_t final_size = std::max(write_index, target_size);
	if(final_size < nodes.size())
	{
		nodes.resize(final_size);
		nodes.shrink_to_fit();
	}
}
*/

void EvaluableNodeManager::ShrinkMemoryToCurrentUtilizationWithLock()
{
	size_t new_size = std::min(nodes.size(),
		static_cast<size_t>(firstUnusedNodeIndex * extraMemoryCapacityFactor) + 1);

	size_t new_size = RoundNodeIndexToNextSlab(new_size);

	//look for any empty space that can be trimmed
	for(; new_size < nodes.size(); new_size += _num_nodes_per_slab)
	{
		if(nodes[new_size] == nullptr)
			break;
	}

	if(new_size == nodes.size())
		return;
	nodes.resize(new_size);

	//TODO 26165: sort remaining nodes by address, look for groups of _num_nodes_per_slab and if all are accounted for, delete

	//look for any empty space that can be trimmed
	for(; new_size < nodes.size(); new_size += _num_nodes_per_slab)
	{
		if(nodes[new_size] == nullptr)
			break;
	}

	if(new_size == nodes.size())
		return;

	nodes.resize(new_size);
	nodes.shrink_to_fit();
}

EvaluableNode *EvaluableNodeManager::GetGarbageCollectionParamsAsEvaluableNode()
{
	EvaluableNode *params_en = AllocNode(ENT_ASSOC);
	params_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_min_gc_nodes_threshold),
		AllocNode(static_cast<double>(minGarbageCollectionNodesThreshold)));
	params_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_max_gc_nodes_threshold),
		AllocNode(static_cast<double>(maxGarbageCollectionNodesThreshold)));
	params_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_extra_memory_capacity_factor),
		AllocNode(extraMemoryCapacityFactor));
	params_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_min_memory_retention_factor),
		AllocNode(minMemoryRetentionFactor));
	params_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alloc_expansion_factor),
		AllocNode(allocExpansionFactor));

	return params_en;
}

void EvaluableNodeManager::SetGarbageCollectionParamsByEvaluableNode(EvaluableNode *en)
{
	if(EvaluableNode::IsAssociativeArray(en))
	{
		for(auto [parameter, value] : en->GetMappedChildNodesViewOnAssoc())
		{
			if(parameter == GetStringIdFromBuiltInStringId(ENBISI_min_gc_nodes_threshold))
			{
				double value_num = EvaluableNode::ToNumber(value);
				if(value_num >= 0 && value_num < static_cast<double>(std::numeric_limits<size_t>::max()))
					minGarbageCollectionNodesThreshold = static_cast<size_t>(value_num);
				else
					minGarbageCollectionNodesThreshold = _min_gc_nodes_threshold_default;
			}
			else if(parameter == GetStringIdFromBuiltInStringId(ENBISI_max_gc_nodes_threshold))
			{
				double value_num = EvaluableNode::ToNumber(value);
				if(value_num >= 0 && value_num < static_cast<double>(std::numeric_limits<size_t>::max()))
					maxGarbageCollectionNodesThreshold = static_cast<size_t>(value_num);
				else
					maxGarbageCollectionNodesThreshold = _max_gc_nodes_threshold_default;
			}
			else if(parameter == GetStringIdFromBuiltInStringId(ENBISI_extra_memory_capacity_factor))
			{
				double value_num = EvaluableNode::ToNumber(value);
				if(value_num >= 1.0 && value_num < 1e9)
					extraMemoryCapacityFactor = static_cast<float>(value_num);
				else
					extraMemoryCapacityFactor = _extra_memory_capacity_factor_default;
			}
			else if(parameter == GetStringIdFromBuiltInStringId(ENBISI_min_memory_retention_factor))
			{
				double value_num = EvaluableNode::ToNumber(value);
				if(value_num >= 0 && value_num <= 1.0)
					minMemoryRetentionFactor = static_cast<float>(value_num);
				else
					minMemoryRetentionFactor = _min_memory_retention_factor_default;
			}
			else if(parameter == GetStringIdFromBuiltInStringId(ENBISI_alloc_expansion_factor))
			{
				double value_num = EvaluableNode::ToNumber(value);
				if(value_num > 1.0 && value_num <= 1e9)
					allocExpansionFactor = static_cast<float>(value_num);
				else
					allocExpansionFactor = _alloc_expansion_factor_default;
			}
		}
	}
}

size_t EvaluableNodeManager::GetEstimatedTotalReservedSizeInBytes()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadLock lock(managerAttributesMutex);
#endif

	size_t total_size = 0;
	for(auto &a : nodes)
		total_size += EvaluableNode::GetEstimatedNodeSizeInBytes(a);

	return total_size;
}

size_t EvaluableNodeManager::GetEstimatedTotalUsedSizeInBytes()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadLock lock(managerAttributesMutex);
#endif

	size_t total_size = 0;
	for(size_t i = 0; i < firstUnusedNodeIndex; i++)
		total_size += EvaluableNode::GetEstimatedNodeSizeInBytes(nodes[i]);

	return total_size;
}

void EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(EvaluableNode *en,
	EvaluableNodeManager *ensure_nodes_in_enm, bool check_cycle_flag_consistency)
{
	if(en == nullptr)
		return;

	EvaluableNode::ReferenceSetType checked;

	if(ensure_nodes_in_enm)
	{
		FastHashSet<EvaluableNode *> existing_nodes;
		existing_nodes.clear();

		for(size_t i = 0; i < ensure_nodes_in_enm->firstUnusedNodeIndex; i++)
		{
			if(ensure_nodes_in_enm->nodes[i] != nullptr)
				existing_nodes.insert(ensure_nodes_in_enm->nodes[i]);
		}

		ValidateEvaluableNodeTreeMemoryIntegrityRecurse(en, checked, &existing_nodes, check_cycle_flag_consistency);
	}
	else
	{
		ValidateEvaluableNodeTreeMemoryIntegrityRecurse(en, checked, nullptr, check_cycle_flag_consistency);
	}
}

void EvaluableNodeManager::VerifyEvaluableNodeIntegrityForAllReferencedNodes()
{
	ValidateEvaluableNodeTreeMemoryIntegrity(rootNode, this);

	if(activeInterpreters != nullptr)
	{
		for(Interpreter *interpreter : activeInterpreters->activeInterpreters)
		{
			for(EvaluableNode *en : interpreter->scopeStack)
				ValidateEvaluableNodeTreeMemoryIntegrity(en, this);

			for(EvaluableNode *en : interpreter->opcodeStackNodes)
				ValidateEvaluableNodeTreeMemoryIntegrity(en, this);

			for(auto &cs_entry : interpreter->constructionStack)
			{
				ValidateEvaluableNodeTreeMemoryIntegrity(cs_entry.targetOrigin, this);
				ValidateEvaluableNodeTreeMemoryIntegrity(*cs_entry.targetRefPtr, this);
				ValidateEvaluableNodeTreeMemoryIntegrity(cs_entry.currentValue, this);
				ValidateEvaluableNodeTreeMemoryIntegrity(cs_entry.previousResult, this);
			}
		}
	}
}

EvaluableNodeReference EvaluableNodeManager::DeepAllocCopy(EvaluableNode *en, bool copy_metadata)
{
	if(en == nullptr)
		return EvaluableNodeReference::Null();

	//quick copy iteratively if don't need cycle check
	if(!en->GetNeedCycleCheck())
	{
		auto &node_stack = EvaluableNode::reusableBuffer;
		node_stack.clear();

		EvaluableNode *root_copy = AllocNode(en, copy_metadata);
		if(root_copy->IsTerminal())
			return EvaluableNodeReference(root_copy, true);

		//walk the tree depth‑first using the buffer as a stack
		node_stack.push_back(root_copy);
		while(!node_stack.empty())
		{
			EvaluableNode *cur = node_stack.back();
			node_stack.pop_back();

			if(cur->IsAssociativeArray())
			{
				for(auto &child : cur->GetMappedChildNodesViewOnAssoc() | std::views::values)
				{
					if(child == nullptr)
						continue;

					child = AllocNode(child, copy_metadata);
					if(!child->IsTerminal())
						node_stack.push_back(child);
				}
			}
			else //ordered, since immediates are not pushed in stack
			{
				for(auto &child : cur->GetOrderedChildNodesReference())
				{
					if(child == nullptr)
						continue;

					child = AllocNode(child, copy_metadata);
					if(!child->IsTerminal())
						node_stack.push_back(child);
				}
			}
		}

		return EvaluableNodeReference(root_copy, true);
	}

	DeepAllocCopyParams dacp(copy_metadata);
	auto [copy, need_cycle_check] = DeepAllocCopyRecurse(en, dacp);
	return EvaluableNodeReference(copy, true);
}

std::pair<EvaluableNode *, bool> EvaluableNodeManager::DeepAllocCopyRecurse(EvaluableNode *tree,
	DeepAllocCopyParams &dacp)
{
	//attempt to insert a new reference for this node, start with null
	auto [inserted_copy, inserted] = dacp.references.emplace(tree, nullptr);

	//can't insert, so already have a copy
	// need to indicate that it has a cycle
	if(!inserted)
		return {inserted_copy->second, true};

	EvaluableNode *copy = AllocNode(tree, dacp.copyMetadata);

	//shouldn't happen, but just to be safe
	if(copy == nullptr) [[unlikely]]
		return {nullptr, false};

	//start without needing a cycle check in case it can be cleared
	copy->SetNeedCycleCheck(false);

	//write the value to the iterator from the earlier insert
	inserted_copy->second = copy;

	//copy and update any child nodes
	if(copy->IsAssociativeArray())
	{
		auto copy_mcn = copy->GetMappedChildNodesViewOnAssoc();
		for(auto &s : copy_mcn | std::views::values)
		{
			//get current item in list
			EvaluableNode *n = s;
			if(n == nullptr)
				continue;

			//make copy; if need cycle check, then mark it on the parent copy
			auto [child_copy, need_cycle_check] = DeepAllocCopyRecurse(n, dacp);
			if(need_cycle_check)
				copy->SetNeedCycleCheck(true);

			//replace item in assoc with copy
			s = child_copy;
		}
	}
	else
	{
		auto &copy_ocn = copy->GetOrderedChildNodes();
		for(size_t i = 0; i < copy_ocn.size(); i++)
		{
			//get current item in list
			EvaluableNode *n = copy_ocn[i];
			if(n == nullptr)
				continue;

			//make copy; if need cycle check, then mark it on the parent copy
			auto [child_copy, need_cycle_check] = DeepAllocCopyRecurse(n, dacp);
			if(need_cycle_check)
				copy->SetNeedCycleCheck(true);

			//replace current item in list with copy
			copy_ocn[i] = child_copy;
		}
	}

	return {copy, copy->GetNeedCycleCheck()};
}

//sets or clears all referenced nodes' in use flags
//if set_in_use is true, then it will set the value, if false, it will clear the value
//note that tree cannot be nullptr and it should already be inserted into the references prior to calling
static void MarkAllReferencedNodesInUseForNode(EvaluableNode *tree)
{
	tree->SetKnownToBeInUse(true);
	auto &node_stack = EvaluableNode::reusableBuffer;
	//a previous traversal may have unwound after an allocation failure.
	node_stack.clear();
	node_stack.push_back(tree);

	while(!node_stack.empty())
	{
		auto *node = node_stack.back();
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		AmlgAssert(node->IsNodeValid());
	#endif
		node_stack.pop_back();

		auto type = node->GetType();
		if(DoesEvaluableNodeTypeUseAssocData(type))
		{
			for(auto &cn : node->GetMappedChildNodesViewOnAssoc() | std::views::values)
			{
				if(cn != nullptr && !cn->GetKnownToBeInUse())
				{
					cn->SetKnownToBeInUse(true);
					node_stack.push_back(cn);
				}
			}
		}
		else if(!IsEvaluableNodeTypeTerminalNode(type))
		{
			for(auto &cn : node->GetOrderedChildNodesReference())
			{
				if(cn != nullptr && !cn->GetKnownToBeInUse())
				{
					cn->SetKnownToBeInUse(true);
					node_stack.push_back(cn);
				}
			}
		}
	}
}

#ifdef MULTITHREAD_SUPPORT
//like its non-concurrent variant above but for concurrency
static void MarkAllReferencedNodesInUseConcurrentForNode(EvaluableNode *tree)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(tree->IsNodeValid());
#endif

	if(!tree->TrySetKnownToBeInUseAtomic())
		return;
	auto &node_stack = EvaluableNode::reusableBuffer;

	node_stack.clear();
	node_stack.push_back(tree);

	while(!node_stack.empty())
	{
		auto *node = node_stack.back();
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		AmlgAssert(node->IsNodeValid());
	#endif
		node_stack.pop_back();

		auto type = node->GetType();
		if(DoesEvaluableNodeTypeUseAssocData(type))
		{
			for(auto &cn : node->GetMappedChildNodesViewOnAssoc() | std::views::values)
			{
				if(cn != nullptr && cn->TrySetKnownToBeInUseAtomic())
					node_stack.push_back(cn);
			}
		}
		else if(!IsEvaluableNodeTypeTerminalNode(type))
		{
			for(auto &cn : node->GetOrderedChildNodesReference())
			{
				if(cn != nullptr && cn->TrySetKnownToBeInUseAtomic())
					node_stack.push_back(cn);
			}
		}
	}
}
#endif

void EvaluableNodeManager::MarkAllReferencedNodesInUse(size_t estimated_nodes_in_use)
{
	size_t num_active_interpreters = 0;
	if(activeInterpreters.get() != nullptr)
		num_active_interpreters = activeInterpreters->activeInterpreters.size();

#ifdef MULTITHREAD_SUPPORT
	//because code cannot be executed when in garbage collection due to other locks,
	//the nodes referenced cannot be modified while in this method, so nr.mutex does not need to be locked

	//heuristic to ensure there's enough to do to warrant the overhead of using multiple threads
	if(Concurrency::GetMaxNumThreads() > 1 && num_active_interpreters >= 1 && estimated_nodes_in_use >= 10000)
	{
		Concurrency::TaskSet task_set;
		for(Interpreter *interpreter : activeInterpreters->activeInterpreters)
		{
			task_set.emplace(
				[interpreter]
				{
					for(EvaluableNode *en : interpreter->scopeStack)
					{
						if(en == nullptr || en->GetKnownToBeInUse())
							continue;

						MarkAllReferencedNodesInUseConcurrentForNode(en);
					}

					for(auto &cs_entry : interpreter->constructionStack)
					{
						if(cs_entry.targetOrigin != nullptr && !cs_entry.targetOrigin->GetKnownToBeInUse())
							MarkAllReferencedNodesInUseConcurrentForNode(cs_entry.targetOrigin);

						if(*cs_entry.targetRefPtr != nullptr && !(*cs_entry.targetRefPtr)->GetKnownToBeInUse())
							MarkAllReferencedNodesInUseConcurrentForNode(*cs_entry.targetRefPtr);

						if(cs_entry.currentValue != nullptr && !cs_entry.currentValue->GetKnownToBeInUse())
							MarkAllReferencedNodesInUseConcurrentForNode(cs_entry.currentValue);

						if(cs_entry.previousResult != nullptr && !cs_entry.previousResult->GetKnownToBeInUse())
							MarkAllReferencedNodesInUseConcurrentForNode(cs_entry.previousResult);
					}

					//this is more likely to overlap with the entity's nodes, not unique to an interpreter, so do it last
					for(EvaluableNode *en : interpreter->opcodeStackNodes)
					{
						if(en == nullptr || en->GetKnownToBeInUse())
							continue;

						MarkAllReferencedNodesInUseConcurrentForNode(en);
					}

				}
			);
		}

		//add the root node last since references above are more likely to mark pieces of it concurrently
		task_set.emplace(
			[this]
			{
				MarkAllReferencedNodesInUseConcurrentForNode(rootNode);
			}
		);

		Concurrency::RunSystemTasks(task_set);
		return;
	}
#endif

	MarkAllReferencedNodesInUseForNode(rootNode);
	if(num_active_interpreters > 0)
	{
		for(Interpreter *interpreter : activeInterpreters->activeInterpreters)
		{
			for(EvaluableNode *en : interpreter->scopeStack)
			{
				if(en == nullptr || en->GetKnownToBeInUse())
					continue;

				MarkAllReferencedNodesInUseForNode(en);
			}

			for(EvaluableNode *en : interpreter->opcodeStackNodes)
			{
				if(en == nullptr || en->GetKnownToBeInUse())
					continue;

				MarkAllReferencedNodesInUseForNode(en);
			}

			for(auto &cs_entry : interpreter->constructionStack)
			{
				if(cs_entry.targetOrigin != nullptr && !cs_entry.targetOrigin->GetKnownToBeInUse())
					MarkAllReferencedNodesInUseForNode(cs_entry.targetOrigin);

				if(*cs_entry.targetRefPtr != nullptr && !(*cs_entry.targetRefPtr)->GetKnownToBeInUse())
					MarkAllReferencedNodesInUseForNode(*cs_entry.targetRefPtr);

				if(cs_entry.currentValue != nullptr && !cs_entry.currentValue->GetKnownToBeInUse())
					MarkAllReferencedNodesInUseForNode(cs_entry.currentValue);

				if(cs_entry.previousResult != nullptr && !cs_entry.previousResult->GetKnownToBeInUse())
					MarkAllReferencedNodesInUseForNode(cs_entry.previousResult);
			}
		}
	}
}

std::pair<bool, bool> EvaluableNodeManager::UpdateFlagsForNodeTreeRecurse(EvaluableNode *tree,
	EvaluableNode *parent, EvaluableNode::ReferenceAssocType &checked_to_parent)
{
	//attempt to insert; if new, mark as not needing a cycle check yet
	// though that may be changed when child nodes are evaluated below
	auto [existing_record, inserted] = checked_to_parent.emplace(tree, parent);
	if(inserted)
	{
		tree->SetNeedCycleCheck(false);
	}
	else //this node has already been checked
	{
		//climb back up to top setting cycle checks needed,
		//starting with tree's parent node
		EvaluableNode *cur_node = existing_record->second;
		while(cur_node != nullptr)
		{
			//if it's already set to cycle check, don't need to keep going
			if(cur_node->GetNeedCycleCheck())
				break;

			cur_node->SetNeedCycleCheck(true);

			auto parent_record = checked_to_parent.find(cur_node);
			if(parent_record == end(checked_to_parent)) [[unlikely]]
			{
				AmlgAssert(false);
			}

			cur_node = parent_record->second;
		}
		return {true, tree->GetIsIdempotent()};
	}

	bool is_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(tree->GetType());
	tree->SetIsIdempotent(is_idempotent);

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(tree->IsNodeValid());
#endif

	if(tree->IsAssociativeArray())
	{
		bool need_cycle_check = false;

		for(auto &[cn_id, cn] : tree->GetMappedChildNodesViewOnAssoc())
		{
			if(cn == nullptr)
				continue;

			auto [cn_need_cycle_check, cn_is_idempotent] = UpdateFlagsForNodeTreeRecurse(cn, tree, checked_to_parent);

			//update flags for tree
			if(cn_need_cycle_check)
				need_cycle_check = true;

			if(!cn_is_idempotent)
				is_idempotent = false;
		}

		if(need_cycle_check)
			tree->SetNeedCycleCheck(need_cycle_check);
		if(!is_idempotent)
			tree->SetIsIdempotent(is_idempotent);
		return {need_cycle_check, is_idempotent};
	}
	else if(!tree->IsTerminal())
	{
		bool need_cycle_check = false;

		for(auto cn : tree->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			auto [cn_need_cycle_check, cn_is_idempotent] = UpdateFlagsForNodeTreeRecurse(cn, tree, checked_to_parent);

			//update flags for tree
			if(cn_need_cycle_check)
				need_cycle_check = true;

			if(!cn_is_idempotent)
				is_idempotent = false;
		}

		if(need_cycle_check)
			tree->SetNeedCycleCheck(need_cycle_check);
		if(!is_idempotent)
			tree->SetIsIdempotent(is_idempotent);
		return {need_cycle_check, is_idempotent};
	}
	else //terminal value
	{
		tree->SetIsIdempotent(is_idempotent);
		return {false, is_idempotent};
	}
}

std::pair<bool, bool> EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrityRecurse(
	EvaluableNode *en, EvaluableNode::ReferenceSetType &checked,
	FastHashSet<EvaluableNode *> *existing_nodes, bool check_cycle_flag_consistency)
{
	auto [_, inserted] = checked.insert(en);
	//can't assume that, just because something was inserted before,
	// doesn't mean it isn't cycle free from where it is, so return true to exclude false negatives
	if(!inserted)
		return {true, en->GetIsIdempotent()};

	if(!en->IsNodeValid() || en->GetKnownToBeInUse()) [[unlikely]]
		AmlgAssert(false);

	if(existing_nodes != nullptr)
	{
		if(existing_nodes->find(en) == end(*existing_nodes)) [[unlikely]]
			AmlgAssert(false);
	}

	bool child_nodes_cycle_free = true;
	bool child_nodes_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(en->GetType());
	if(en->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : en->GetMappedChildNodesViewOnAssoc())
		{
			if(cn == nullptr)
				continue;

			auto [node_cycle_free, node_idempotent]
				= ValidateEvaluableNodeTreeMemoryIntegrityRecurse(cn, checked,
					existing_nodes, check_cycle_flag_consistency);
			if(!node_cycle_free)
				child_nodes_cycle_free = false;
			if(!child_nodes_idempotent)
				child_nodes_idempotent = false;
		}
	}
	else if(!en->IsTerminal())
	{
		for(auto cn : en->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			auto [node_cycle_free, node_idempotent]
				= ValidateEvaluableNodeTreeMemoryIntegrityRecurse(cn, checked,
					existing_nodes, check_cycle_flag_consistency);
			if(!node_cycle_free)
				child_nodes_cycle_free = false;
			if(!child_nodes_idempotent)
				child_nodes_idempotent = false;
		}
	}

	if(!child_nodes_idempotent && en->GetIsIdempotent()) [[unlikely]]
		AmlgAssert(false);

	if(check_cycle_flag_consistency && !child_nodes_cycle_free && !en->GetNeedCycleCheck()) [[unlikely]]
		AmlgAssert(false);

	return {!en->GetNeedCycleCheck(), en->GetIsIdempotent()};
}
