//project headers:
#include "EvaluableNodeManagement.h"
#include "PerformanceProfiler.h"

//system headers:
#include <string>
#include <vector>
#include <utility>

#ifdef MULTITHREAD_SUPPORT
Concurrency::ReadWriteMutex EvaluableNodeManager::memoryModificationMutex;
#endif

const double EvaluableNodeManager::allocExpansionFactor = 1.5;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
	EvaluableNodeManager *EvaluableNodeManager::lastEvaluableNodeManager = nullptr;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
	std::vector<EvaluableNode *> EvaluableNodeManager::threadLocalAllocationBuffer;

EvaluableNodeManager::~EvaluableNodeManager()
{
	if(lastEvaluableNodeManager == this)
		ClearThreadLocalAllocationBuffer();

#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(managerAttributesMutex);
#endif

	for(auto &n : nodes)
		delete n;
}

EvaluableNode *EvaluableNodeManager::AllocNode(EvaluableNode *original, EvaluableNodeMetadataModifier metadata_modifier)
{
	EvaluableNode *n = AllocUninitializedNode();
	n->InitializeType(original, metadata_modifier == ENMM_NO_CHANGE, metadata_modifier != ENMM_REMOVE_ALL);

	if(metadata_modifier == ENMM_LABEL_ESCAPE_INCREMENT)
	{
		size_t num_labels = original->GetNumLabels();
		n->ReserveLabels(num_labels);

		//add # in front
		for(size_t i = 0; i < num_labels; i++)
		{
			std::string label = "#" + original->GetLabel(i);
			n->AppendLabel(label);
		}
	}
	else if(metadata_modifier == ENMM_LABEL_ESCAPE_DECREMENT)
	{
		size_t num_labels = original->GetNumLabels();
		n->ReserveLabels(num_labels);

		//remove # in front
		for(size_t i = 0; i < num_labels; i++)
		{
			std::string label = original->GetLabel(i);
			if(label.size() > 0 && label[0] == '#')
				label = label.substr(1);

			n->AppendLabel(label);
		}
	}

	return n;
}

void EvaluableNodeManager::UpdateGarbageCollectionTrigger(size_t previous_num_nodes)
{
	//assume at least a factor larger than the base memory usage for the entity
	//add 1 for good measure and to make sure the smallest size isn't zero
	size_t max_from_current = 3 * GetNumberOfUsedNodes() + 1;

	size_t cur_num_nodes = GetNumberOfUsedNodes();
	if(numNodesToRunGarbageCollection > cur_num_nodes)
	{
		//scale down the number of nodes previously allocated, because there is always a chance that
		//a large allocation goes beyond that size and so the memory keeps growing
		//by using a fraction less than 1, it reduces the chances of a slow memory increase
		size_t diff_from_current = (numNodesToRunGarbageCollection - cur_num_nodes);
		size_t max_from_previous = cur_num_nodes + static_cast<size_t>(.9 * diff_from_current);

		numNodesToRunGarbageCollection = std::max<size_t>(max_from_previous, max_from_current);
	}
	else
	{
		numNodesToRunGarbageCollection = max_from_current;
	}
}

void EvaluableNodeManager::CollectGarbage()
{
	if(PerformanceProfiler::IsProfilingEnabled())
	{
		static const std::string collect_garbage_string = ".collect_garbage";
		PerformanceProfiler::StartOperation(collect_garbage_string, GetNumberOfUsedNodes());
	}

	ClearThreadLocalAllocationBuffer();

	MarkAllReferencedNodesInUse(firstUnusedNodeIndex);

	FreeAllNodesExceptReferencedNodes(firstUnusedNodeIndex);

	if(PerformanceProfiler::IsProfilingEnabled())
		PerformanceProfiler::EndOperation(GetNumberOfUsedNodes());
}

#ifdef MULTITHREAD_SUPPORT
void EvaluableNodeManager::CollectGarbageWithConcurrentAccess(Concurrency::ReadLock *memory_modification_lock)
{
	if(PerformanceProfiler::IsProfilingEnabled())
	{
		static const std::string collect_garbage_string = ".collect_garbage";
		PerformanceProfiler::StartOperation(collect_garbage_string, GetNumberOfUsedNodes());
	}

	ClearThreadLocalAllocationBuffer();
	
	//free lock so can attempt to enter write lock to collect garbage
	if(memory_modification_lock != nullptr)
		memory_modification_lock->unlock();

	//keep trying to acquire write lock to see if this thread wins the race to collect garbage
	Concurrency::WriteLock write_lock(memoryModificationMutex, std::defer_lock);

	//wait for either the lock or no longer need garbage collecting
	while(!write_lock.try_lock() && RecommendGarbageCollection())
	{	}
		
	//if owns lock, double-check still needs collection,
	// and not that another thread collected it since acquiring the lock
	if(write_lock.owns_lock())
	{
		if(RecommendGarbageCollection())
		{
			size_t cur_first_unused_node_index = firstUnusedNodeIndex;
			//clear firstUnusedNodeIndex to signal to other threads that they won't need to do garbage collection
			firstUnusedNodeIndex = 0;

			//if any group of nodes on the top are ready to be cleaned up cheaply, do so first
			while(cur_first_unused_node_index > 0 && nodes[cur_first_unused_node_index - 1] != nullptr
					&& nodes[cur_first_unused_node_index - 1]->IsNodeDeallocated())
				cur_first_unused_node_index--;

			MarkAllReferencedNodesInUse(cur_first_unused_node_index);

			FreeAllNodesExceptReferencedNodes(cur_first_unused_node_index);
		}

		//free the unique lock and reacquire the shared lock
		write_lock.unlock();
	}

	if(memory_modification_lock != nullptr)
		memory_modification_lock->lock();

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

	ClearThreadLocalAllocationBuffer();
}

EvaluableNode *EvaluableNodeManager::AllocUninitializedNode()
{
	if(!IsAssignedToTLAB())
	{
		//attempt to allocate enough nodes to refill thread local allocation buffer
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock read_lock(managerAttributesMutex);

		size_t index_to_allocate = firstUnusedNodeIndex.fetch_add(1);
	#else
		size_t index_to_allocate = firstUnusedNodeIndex;
		firstUnusedNodeIndex += 1;
	#endif

		if(index_to_allocate < nodes.size())
		{
			if(nodes[index_to_allocate] == nullptr)
				nodes[index_to_allocate] = new EvaluableNode(ENT_DEALLOCATED);

			return nodes[index_to_allocate];
		}
	
	#ifdef MULTITHREAD_SUPPORT
		read_lock.unlock();

		//don't have enough nodes, so need to attempt a write lock to allocate more
		Concurrency::WriteLock write_lock(managerAttributesMutex);
	#endif

		size_t num_nodes = nodes.size();
		if(index_to_allocate >= num_nodes)
		{
			//ran out, so need another node; push a bunch on the heap so don't need to reallocate as often and slow down garbage collection
			 //preallocate additional resources, making sure to at least add one block
			size_t new_num_nodes = static_cast<size_t>(allocExpansionFactor * num_nodes) + tlabBlockAllocationSize;

			//fill new EvaluableNode slots with nullptr
			nodes.resize(new_num_nodes, nullptr);
		}

		if(nodes[index_to_allocate] == nullptr)
			nodes[index_to_allocate] = new EvaluableNode(ENT_DEALLOCATED);
		return nodes[index_to_allocate];
	}

	EvaluableNode *tlab_node = GetNextNodeFromTLAB();

	//TODO 22458: remove this
	//{
	//	Concurrency::SingleLock lock(tlabCountMutex);
	//
	//	tlabSize += threadLocalAllocationBuffer.size();
	//	tlabSizeCount++;
	//
	//	rollingAveTlabSize = .98 * rollingAveTlabSize + .02 * threadLocalAllocationBuffer.size();
	//
	//	if(tlabSizeCount % 1000 == 0)
	//	{
	//		double ave_size = static_cast<double>(tlabSize) / tlabSizeCount;
	//		std::cout << "ave tlab size: " << ave_size << std::endl;
	//	}
	//}

	//Fast Path; get node from thread local buffer
	if(tlab_node != nullptr)
		return tlab_node;

	//attempt to allocate enough nodes to refill thread local allocation buffer
#ifdef MULTITHREAD_SUPPORT
	//slow path allocation; attempt to allocate using an atomic without write locking
	Concurrency::ReadLock read_lock(managerAttributesMutex);

	size_t first_index_to_allocate = firstUnusedNodeIndex.fetch_add(tlabBlockAllocationSize);
#else
	size_t first_index_to_allocate = firstUnusedNodeIndex;
	firstUnusedNodeIndex += tlabBlockAllocationSize;
#endif

	size_t last_index_to_allocate = first_index_to_allocate + tlabBlockAllocationSize;

	if(last_index_to_allocate < nodes.size())
	{
		for(size_t i = first_index_to_allocate; i < last_index_to_allocate; i++)
		{
			if(nodes[i] == nullptr)
				nodes[i] = new EvaluableNode(ENT_DEALLOCATED);

			AddNodeToTLAB(nodes[i]);
		}

	#ifdef MULTITHREAD_SUPPORT
		read_lock.unlock();
	#endif
		return GetNextNodeFromTLAB();
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
		 //preallocate additional resources, making sure to at least add one block
		size_t new_num_nodes = static_cast<size_t>(allocExpansionFactor * num_nodes) + tlabBlockAllocationSize;

		//fill new EvaluableNode slots with nullptr
		nodes.resize(new_num_nodes, nullptr);
	}

	//transfer nodes already allocated by this call into thread local allocation buffer
	for(size_t i = first_index_to_allocate; i < last_index_to_allocate; i++)
	{
		if(nodes[i] == nullptr)
			nodes[i] = new EvaluableNode(ENT_DEALLOCATED);

		AddNodeToTLAB(nodes[i]);
	}

#ifdef MULTITHREAD_SUPPORT
	write_lock.unlock();
#endif
	return GetNextNodeFromTLAB();
}

void EvaluableNodeManager::FreeAllNodesExceptReferencedNodes(size_t cur_first_unused_node_index)
{
	//create a temporary variable for multithreading as to not use the atomic variable to slow things down
	size_t first_unused_node_index_temp = 0;

#ifdef MULTITHREAD_SUPPORT
	if(Concurrency::GetMaxNumThreads() > 1 && cur_first_unused_node_index > 6000)
	{
		//used to climb up the indices, swapping out unused nodes above this as moves downward
		std::atomic<size_t> lowest_known_unused_index = cur_first_unused_node_index;
		//used by the independent freeing thread to climb down from lowest_known_unused_index
		size_t highest_possibly_unfreed_node = cur_first_unused_node_index;
		std::atomic<bool> all_nodes_finished = false;

		//free nodes in a separate thread
		auto completed_node_cleanup = Concurrency::urgentThreadPool.EnqueueTaskWithResult(
			[this, &lowest_known_unused_index, &highest_possibly_unfreed_node, &all_nodes_finished]
			{
				while(true)
				{
					while(highest_possibly_unfreed_node > lowest_known_unused_index)
					{
						auto &cur_node_ptr = nodes[--highest_possibly_unfreed_node];
						if(!cur_node_ptr->IsNodeDeallocated())
							cur_node_ptr->Invalidate();
					}

					if(all_nodes_finished)
					{
						//need to double-check to make sure there's nothing left
						//just in case the atomic variables were updated in a different order
						//otherwise go around the loop again
						if(highest_possibly_unfreed_node <= lowest_known_unused_index)
							return;
					}
				}
			}
		);

		//organize nodes above lowest_known_unused_index that are unused
		//don't need to check nodes if they are nullptr because if it has been used, it won't be nullptr
		while(first_unused_node_index_temp < lowest_known_unused_index)
		{
			//nodes can't be nullptr below firstUnusedNodeIndex
			auto &cur_node_ptr = nodes[first_unused_node_index_temp];

			//if the node has been found on this iteration, then clear it as counted so it's clean for next garbage collection
			if(cur_node_ptr->GetKnownToBeInUse())
			{
				cur_node_ptr->SetKnownToBeInUse(false);
				first_unused_node_index_temp++;
			}
			else //collect the node
			{
				//see if out of things to free; if so exit early
				if(lowest_known_unused_index == 0)
					break;

				//put the node up at the top where unused memory resides
				// and reduce lowest_known_unused_index after the swap occurs so the other thread doesn't get misaligned
				std::swap(cur_node_ptr, nodes[lowest_known_unused_index - 1]);
				--lowest_known_unused_index;
			}
		}

		all_nodes_finished = true;

		completed_node_cleanup.wait();

		//assign back to the atomic variable
		firstUnusedNodeIndex = first_unused_node_index_temp;

		UpdateGarbageCollectionTrigger(cur_first_unused_node_index);
		return;
	}
#endif

	size_t lowest_known_unused_index = cur_first_unused_node_index;
	//organize nodes above lowest_known_unused_index that are unused
	//don't need to check nodes if they are nullptr because if it has been used, it won't be nullptr
	while(first_unused_node_index_temp < lowest_known_unused_index)
	{
		//nodes can't be nullptr below firstUnusedNodeIndex
		auto &cur_node_ptr = nodes[first_unused_node_index_temp];

		//if the node has been found on this iteration, then clear it as counted so it's clean for next garbage collection
		if(cur_node_ptr->GetKnownToBeInUse())
		{
			cur_node_ptr->SetKnownToBeInUse(false);
			first_unused_node_index_temp++;
		}
		else //collect the node
		{
			//free any extra memory used, since this node is no longer needed
			if(!cur_node_ptr->IsNodeDeallocated())
				cur_node_ptr->Invalidate();

			//see if out of things to free; if so exit early
			if(lowest_known_unused_index == 0)
				break;

			//put the node up at the top where unused memory resides and reduce lowest_known_unused_index
			std::swap(cur_node_ptr, nodes[--lowest_known_unused_index]);
		}
	}

	//assign back to the atomic variable
	firstUnusedNodeIndex = first_unused_node_index_temp;

	UpdateGarbageCollectionTrigger(cur_first_unused_node_index);
}

void EvaluableNodeManager::FreeNodeTreeRecurse(EvaluableNode *tree)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(tree->IsNodeValid());
	assert(!tree->GetNeedCycleCheck());
#endif

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
		{
			if(e != nullptr)
				FreeNodeTreeRecurse(e);
		}
	}
	else
	{
		for(auto &e : tree->GetOrderedChildNodes())
		{
			if(e != nullptr)
				FreeNodeTreeRecurse(e);
		}
	}

	tree->Invalidate();
	AddNodeToTLAB(tree);
}

void EvaluableNodeManager::FreeNodeTreeWithCyclesRecurse(EvaluableNode *tree)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(tree->IsNodeValid());
#endif

	if(tree->IsAssociativeArray())
	{
		//pull the mapped child nodes out of the tree before invalidating it
		//need to invalidate before call child nodes to prevent infinite recursion loop
		EvaluableNode::AssocType mcn = std::move(tree->GetMappedChildNodesReference());
		tree->Invalidate();
		AddNodeToTLAB(tree);

		for(auto &[_, e] : mcn)
		{
			if(e != nullptr && !e->IsNodeDeallocated())
				FreeNodeTreeWithCyclesRecurse(e);
		}

		//free the references
		string_intern_pool.DestroyStringReferences(mcn, [](auto n) { return n.first; });
	}
	else if(tree->IsImmediate())
	{
		tree->Invalidate();
		AddNodeToTLAB(tree);
	}
	else //ordered
	{
		//pull the ordered child nodes out of the tree before invalidating it
		//need to invalidate before call child nodes to prevent infinite recursion loop
		std::vector<EvaluableNode *> ocn = std::move(tree->GetOrderedChildNodesReference());
		tree->Invalidate();
		AddNodeToTLAB(tree);

		for(auto &e : ocn)
		{
			if(e != nullptr && !e->IsNodeDeallocated())
				FreeNodeTreeWithCyclesRecurse(e);
		}
	}
}

void EvaluableNodeManager::ModifyLabels(EvaluableNode *n, EvaluableNodeMetadataModifier metadata_modifier)
{
	size_t num_labels = n->GetNumLabels();
	if(num_labels == 0)
		return;

	if(metadata_modifier == ENMM_NO_CHANGE)
		return;

	if(metadata_modifier == ENMM_REMOVE_ALL)
	{
		n->ClearLabels();
		n->ClearComments();
		return;
	}

	if(num_labels == 1)
	{
		std::string label_string = n->GetLabel(0);
		n->ClearLabels();

		if(metadata_modifier == ENMM_LABEL_ESCAPE_INCREMENT)
		{
			label_string.insert(begin(label_string), '#');
			n->AppendLabel(label_string);
		}
		else if(metadata_modifier == ENMM_LABEL_ESCAPE_DECREMENT)
		{
			//remove # in front
			if(label_string.size() > 0 && label_string[0] == '#')
				label_string.erase(begin(label_string));

			n->AppendLabel(label_string);
		}

		return;
	}

	//remove all labels and turn into strings
	auto string_labels = n->GetLabelsStrings();
	n->ClearLabels();

	if(metadata_modifier == ENMM_LABEL_ESCAPE_INCREMENT)
	{
		//add # in front
		for(auto &label : string_labels)
			n->AppendLabel("#" + label);
	}
	else if(metadata_modifier == ENMM_LABEL_ESCAPE_DECREMENT)
	{
		//remove # in front
		for(auto &label : string_labels)
		{
			if(label.size() > 0 && label[0] == '#')
				label = label.substr(1);

			n->AppendLabel(label);
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

std::pair<EvaluableNode *, bool> EvaluableNodeManager::DeepAllocCopy(EvaluableNode *tree, DeepAllocCopyParams &dacp)
{
	//attempt to insert a new reference for this node, start with null
	auto [inserted_copy, inserted] = dacp.references->emplace(tree, nullptr);

	//can't insert, so already have a copy
	// need to indicate that it has a cycle
	if(!inserted)
		return std::make_pair(inserted_copy->second, true);

	EvaluableNode *copy = AllocNode(tree, dacp.labelModifier);

	//shouldn't happen, but just to be safe
	if(copy == nullptr)
		return std::make_pair(nullptr, false);

	//start without needing a cycle check in case it can be cleared
	copy->SetNeedCycleCheck(false);

	//write the value to the iterator from the earlier insert
	inserted_copy->second = copy;

	//copy and update any child nodes
	if(copy->IsAssociativeArray())
	{
		auto &copy_mcn = copy->GetMappedChildNodesReference();
		for(auto &[_, s] : copy_mcn)
		{
			//get current item in list
			EvaluableNode *n = s;
			if(n == nullptr)
				continue;

			//make copy; if need cycle check, then mark it on the parent copy
			auto [child_copy, need_cycle_check] = DeepAllocCopy(n, dacp);
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
			auto [child_copy, need_cycle_check] = DeepAllocCopy(n, dacp);
			if(need_cycle_check)
				copy->SetNeedCycleCheck(true);

			//replace current item in list with copy
			copy_ocn[i] = child_copy;
		}
	}

	return std::make_pair(copy, copy->GetNeedCycleCheck());
}

EvaluableNode *EvaluableNodeManager::NonCycleDeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier)
{
	EvaluableNode *copy = nullptr;
	#pragma omp critical
	{
		copy = AllocNode(tree, metadata_modifier);
	}

	if(copy->IsAssociativeArray())
	{
		//for any mapped children, copy and update
		for(auto &[_, s] : copy->GetMappedChildNodesReference())
		{
			//get current item in list
			EvaluableNode *n = s;
			if(n == nullptr)
				continue;

			//replace item in list with copy
			s = NonCycleDeepAllocCopy(n, metadata_modifier);
		}
	}
	else if(!copy->IsImmediate())
	{
		//for any ordered children, copy and update
		auto &copy_ocn = copy->GetOrderedChildNodesReference();
		for(size_t i = 0; i < copy_ocn.size(); i++)
		{
			//get current item in list
			EvaluableNode *n = copy_ocn[i];
			if(n == nullptr)
				continue;

			//replace current item in list with copy
			copy_ocn[i] = NonCycleDeepAllocCopy(n, metadata_modifier);
		}
	}

	return copy;
}

void EvaluableNodeManager::ModifyLabelsForNodeTree(EvaluableNode *tree, EvaluableNode::ReferenceSetType &checked, EvaluableNodeMetadataModifier metadata_modifier)
{
	//attempt to insert; if new, mark as not needing a cycle check yet
	// though that may be changed when child nodes are evaluated below
	auto [_, inserted] = checked.insert(tree);
	if(inserted)
		tree->SetNeedCycleCheck(false);
	else //already exists, nothing to do
		return;

	ModifyLabels(tree, metadata_modifier);

	if(tree->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : tree->GetMappedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			ModifyLabelsForNodeTree(cn, checked, metadata_modifier);
		}
	}
	else if(!tree->IsImmediate())
	{
		for(auto cn : tree->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			ModifyLabelsForNodeTree(cn, checked, metadata_modifier);
		}		
	}
}

void EvaluableNodeManager::NonCycleModifyLabelsForNodeTree(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier)
{
	ModifyLabels(tree, metadata_modifier);

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, cn] : tree->GetMappedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			NonCycleModifyLabelsForNodeTree(cn, metadata_modifier);
		}
	}
	else if(!tree->IsImmediate())
	{
		for(auto cn : tree->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			NonCycleModifyLabelsForNodeTree(cn, metadata_modifier);
		}
	}
}

void EvaluableNodeManager::MarkAllReferencedNodesInUse(size_t estimated_nodes_in_use)
{
	NodesReferenced &nr = GetNodesReferenced();
	EvaluableNode *root_node = nodes[0];

#ifdef MULTITHREAD_SUPPORT
	//because code cannot be executed when in garbage collection due to other locks,
	//the nodes referenced cannot be modified while in this method, so nr.mutex does not need to be locked

	size_t reference_count = nr.nodesReferenced.size();
	//heuristic to ensure there's enough to do to warrant the overhead of using multiple threads
	if(Concurrency::GetMaxNumThreads() > 1 && reference_count > 0 && (estimated_nodes_in_use / (reference_count + 1)) >= 1000)
	{
		//allocate all the tasks assuming they will happen, but mark when they can be skipped
		auto task_set = Concurrency::urgentThreadPool.CreateCountableTaskSet(1 + nr.nodesReferenced.size());

		//start processing root node first, as there's a good chance it will be the largest
		if(root_node != nullptr && !root_node->GetKnownToBeInUseAtomic())
		{
			//don't enqueue in batch, as threads racing ahead of others will reduce memory
			//contention
			Concurrency::urgentThreadPool.EnqueueTask(
				[root_node, &task_set]
				{
					MarkAllReferencedNodesInUseConcurrent(root_node);
					task_set.MarkTaskCompleted();
				}
			);
		}
		else //autocompleted
		{
			task_set.MarkTaskCompletedBeforeWaitForTasks();
		}

		for(auto &[enr, _] : nr.nodesReferenced)
		{
			//some compilers are pedantic about the types passed into the lambda, so make a copy
			EvaluableNode *en = enr;
			//only enqueue a task if the top node isn't known to be in use
			if(en != nullptr && !en->GetKnownToBeInUseAtomic())
			{
				//don't enqueue in batch, as threads racing ahead of others will reduce memory
				//contention
				Concurrency::urgentThreadPool.EnqueueTask(
					[en, &task_set]
					{
						MarkAllReferencedNodesInUseConcurrent(en);
						task_set.MarkTaskCompleted();
					}
				);
			}
			else //autocompleted
			{
				task_set.MarkTaskCompletedBeforeWaitForTasks();
			}
		}

		task_set.WaitForTasks();
		return;
	}
#endif

	//check for null or insertion before calling recursion to minimize number of branches (slight performance improvement)
	if(root_node != nullptr && !root_node->GetKnownToBeInUse())
		MarkAllReferencedNodesInUse(root_node);

	for(auto &[t, _] : nr.nodesReferenced)
	{
		if(t == nullptr || t->GetKnownToBeInUse())
			continue;

		MarkAllReferencedNodesInUse(t);
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
			if(parent_record == end(checked_to_parent))
			{
				assert(false);
			}

			cur_node = parent_record->second;
		}
		return std::make_pair(true, tree->GetIsIdempotent());
	}

	bool is_idempotent = (IsEvaluableNodeTypePotentiallyIdempotent(tree->GetType()) && (tree->GetNumLabels() == 0));
	tree->SetIsIdempotent(is_idempotent);

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(tree->IsNodeValid());
#endif
	
	if(tree->IsAssociativeArray())
	{
		bool need_cycle_check = false;

		for(auto &[cn_id, cn] : tree->GetMappedChildNodesReference())
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
		return std::make_pair(need_cycle_check, is_idempotent);
	}
	else if(!tree->IsImmediate())
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
		return std::make_pair(need_cycle_check, is_idempotent);
	}
	else //immediate value
	{
		tree->SetIsIdempotent(is_idempotent);
		return std::make_pair(false, is_idempotent);
	}
}

void EvaluableNodeManager::MarkAllReferencedNodesInUse(EvaluableNode *tree)
{
	if(tree == nullptr)
		return;

	tree->SetKnownToBeInUse(true);
	auto &node_stack = threadLocalAllocationBuffer;
	node_stack.push_back(tree);

	while(!node_stack.empty())
	{
		auto *node = node_stack.back();
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(node->IsNodeValid());
	#endif
		node_stack.pop_back();

		auto type = node->GetType();
		if(DoesEvaluableNodeTypeUseOrderedData(type))
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
		else if(DoesEvaluableNodeTypeUseAssocData(type))
		{
			for(auto &[_, cn] : node->GetMappedChildNodesReference())
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
void EvaluableNodeManager::MarkAllReferencedNodesInUseConcurrent(EvaluableNode *tree)
{
	if(tree == nullptr)
		return;

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(tree->IsNodeValid());
#endif

	tree->SetKnownToBeInUseAtomic(true);
	//because marking occurs during garbage collection and threadLocalAllocationBuffer
	// is cleared, can reuse that buffer as the local stack to eliminate the overhead of recursion
	auto &node_stack = threadLocalAllocationBuffer;
	node_stack.push_back(tree);

	while(!node_stack.empty())
	{
		auto *node = node_stack.back();
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(node->IsNodeValid());
	#endif
		node_stack.pop_back();

		auto type = node->GetType();
		if(DoesEvaluableNodeTypeUseOrderedData(type))
		{
			for(auto &cn : node->GetOrderedChildNodesReference())
			{
				if(cn != nullptr && !cn->GetKnownToBeInUseAtomic())
				{
					cn->SetKnownToBeInUseAtomic(true);
					node_stack.push_back(cn);
				}
			}
		}
		else if(DoesEvaluableNodeTypeUseAssocData(type))
		{
			for(auto &[_, cn] : node->GetMappedChildNodesReference())
			{
				if(cn != nullptr && !cn->GetKnownToBeInUseAtomic())
				{
					cn->SetKnownToBeInUseAtomic(true);
					node_stack.push_back(cn);
				}
			}
		}
	}
}
#endif

std::pair<bool, bool> EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrityRecurse(
	EvaluableNode *en, EvaluableNode::ReferenceSetType &checked,
	FastHashSet<EvaluableNode *> *existing_nodes, bool check_cycle_flag_consistency)
{
	auto [_, inserted] = checked.insert(en);
	//can't assume that, just because something was inserted before,
	// doesn't mean it isn't cycle free from where it is, so return true to exclude false negatives
	if(!inserted)
		return std::make_pair(true, en->GetIsIdempotent());

	if(!en->IsNodeValid() || en->GetKnownToBeInUse())
		assert(false);

	if(existing_nodes != nullptr)
	{
		if(existing_nodes->find(en) == end(*existing_nodes))
			assert(false);
	}

	bool child_nodes_cycle_free = true;
	bool child_nodes_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(en->GetType());
	if(en->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : en->GetMappedChildNodesReference())
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
	else if(!en->IsImmediate())
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

	if(!child_nodes_idempotent && en->GetIsIdempotent())
		assert(false);

	if(check_cycle_flag_consistency && !child_nodes_cycle_free && !en->GetNeedCycleCheck())
		assert(false);

	return std::make_pair(!en->GetNeedCycleCheck(), en->GetIsIdempotent());
}
