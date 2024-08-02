//project headers:
#include "EvaluableNodeManagement.h"
#include "PerformanceProfiler.h"

//system headers:
#include <limits>
#include <string>
#include <vector>
#include <utility>

#ifdef MULTITHREAD_SUPPORT
Concurrency::ReadWriteMutex EvaluableNodeManager::memoryModificationMutex;
#endif

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
std::vector<EvaluableNode *> EvaluableNodeManager::checkedNodesBuffer;

const double EvaluableNodeManager::allocExpansionFactor = 1.5;

EvaluableNodeManager::~EvaluableNodeManager()
{
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

EvaluableNode *EvaluableNodeManager::AllocListNodeWithOrderedChildNodes(EvaluableNodeType child_node_type, size_t num_child_nodes)
{
	size_t num_allocated = 0;
	size_t num_to_alloc = num_child_nodes + 1;
	size_t num_total_nodes_needed = 0;

	EvaluableNode *retval = nullptr;

	//start off allocating the parent node, then switch to child_node_type
	EvaluableNodeType cur_type = ENT_LIST;

	//ordered child nodes destination; preallocate outside of the lock (for performance) and swap in
	std::vector<EvaluableNode *> *ocn_ptr = nullptr;
	std::vector<EvaluableNode *> ocn_buffer;
	ocn_buffer.resize(num_child_nodes);

	//outer loop needed for multithreading, but doesn't hurt anything for single threading
	while(num_allocated < num_to_alloc)
	{
		{
		#ifdef MULTITHREAD_SUPPORT
			//attempt to allocate as many as possible using an atomic without write locking
			Concurrency::ReadLock lock(managerAttributesMutex);
		#endif

			for(; num_allocated < num_to_alloc; num_allocated++)
			{
				//attempt to allocate a node and make sure it's valid
				size_t allocated_index = firstUnusedNodeIndex++;
				if(allocated_index < nodes.size())
				{
					if(nodes[allocated_index] != nullptr)
						nodes[allocated_index]->InitializeType(cur_type);
					else
						nodes[allocated_index] = new EvaluableNode(cur_type);

					//if first node, populate the parent node
					if(num_allocated == 0)
					{
						//prep parent node
						retval = nodes[allocated_index];

						//get the pointer to place child elements,
						// but swap out the preallocated ordered child nodes
						ocn_ptr = &retval->GetOrderedChildNodes();
						std::swap(ocn_buffer, *ocn_ptr);

						//advance type to child node type
						cur_type = child_node_type;
					}
					else //set the appropritae child node
					{
						(*ocn_ptr)[num_allocated - 1] = nodes[allocated_index];
					}
				}
				else
				{
					//the node wasn't valid; put it back and do a write lock to allocate more
					--firstUnusedNodeIndex;
					break;
				}
			}

			//if have allocated enough, just return
			if(num_allocated == num_to_alloc)
				return retval;

			num_total_nodes_needed = firstUnusedNodeIndex + (num_to_alloc - num_allocated);
		}

	#ifdef MULTITHREAD_SUPPORT

		//don't have enough nodes, so need to attempt a write lock to allocate more
		Concurrency::WriteLock write_lock(managerAttributesMutex);

		//try again after write lock to allocate a node in case another thread has performed the allocation
		//already have the write lock, so don't need to worry about another thread stealing firstUnusedNodeIndex
	#endif

		//if don't currently have enough free nodes to meet the needs, then expand the allocation
		if(nodes.size() <= num_total_nodes_needed)
		{
			size_t new_num_nodes = static_cast<size_t>(allocExpansionFactor * num_total_nodes_needed) + 1;

			//fill new EvaluableNode slots with nullptr
			nodes.resize(new_num_nodes, nullptr);
		}
	}

	//shouldn't make it here
	return retval;
}

void EvaluableNodeManager::UpdateGarbageCollectionTrigger(size_t previous_num_nodes)
{
	//scale down the number of nodes previously allocated, because there is always a chance that
	//a large allocation goes beyond that size and so the memory keeps growing
	//by using a fraction less than 1, it reduces the chances of a slow memory increase
	size_t max_from_previous = static_cast<size_t>(0.99609375 * previous_num_nodes);

	//at least use what's already been allocated
	size_t max_from_allocation = static_cast<size_t>(nodes.size() / allocExpansionFactor);

	//assume at least a factor larger than the base memory usage for the entity
	//add 1 for good measure and to make sure the smallest size isn't zero
	size_t max_from_current = static_cast<size_t>(3 * (1 + GetNumberOfUsedNodes()));

	numNodesToRunGarbageCollection = std::max(max_from_allocation, std::max<size_t>(max_from_previous, max_from_current));
}

#ifdef MULTITHREAD_SUPPORT
void EvaluableNodeManager::CollectGarbage(Concurrency::ReadLock *memory_modification_lock)
#else
void EvaluableNodeManager::CollectGarbage()
#endif
{
	if(PerformanceProfiler::IsProfilingEnabled())
	{
		static const std::string collect_garbage_string = ".collect_garbage";
		PerformanceProfiler::StartOperation(collect_garbage_string, GetNumberOfUsedNodes());
	}

#ifdef MULTITHREAD_SUPPORT
		
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
#endif
			size_t cur_first_unused_node_index = firstUnusedNodeIndex;
			//clear firstUnusedNodeIndex to signal to other threads that they won't need to do garbage collection
			firstUnusedNodeIndex = 0;

			//if any group of nodes on the top are ready to be cleaned up cheaply, do so first
			while(cur_first_unused_node_index > 0 && nodes[cur_first_unused_node_index - 1] != nullptr
					&& nodes[cur_first_unused_node_index - 1]->IsNodeDeallocated())
				cur_first_unused_node_index--;

			//set to contain everything that is referenced
			MarkAllReferencedNodesInUse(cur_first_unused_node_index);

			FreeAllNodesExceptReferencedNodes(cur_first_unused_node_index);

#ifdef MULTITHREAD_SUPPORT
		}

		//free the unique lock and reacquire the shared lock
		write_lock.unlock();
	}

	if(memory_modification_lock != nullptr)
		memory_modification_lock->lock();
#endif

	if(PerformanceProfiler::IsProfilingEnabled())
		PerformanceProfiler::EndOperation(GetNumberOfUsedNodes());
}

void EvaluableNodeManager::FreeAllNodes()
{
	size_t original_num_nodes = firstUnusedNodeIndex;
	//get rid of any extra memory
	for(size_t i = 0; i < firstUnusedNodeIndex; i++)
		nodes[i]->Invalidate();

#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(managerAttributesMutex);
#endif

	firstUnusedNodeIndex = 0;
	
	UpdateGarbageCollectionTrigger(original_num_nodes);
}

EvaluableNode *EvaluableNodeManager::AllocUninitializedNode()
{
	size_t allocated_index = 0;
#ifdef MULTITHREAD_SUPPORT
	{
		//attempt to allocate using an atomic without write locking
		Concurrency::ReadLock lock(managerAttributesMutex);

		//attempt to allocate a node and make sure it's valid
		allocated_index = firstUnusedNodeIndex++;
		if(allocated_index < nodes.size())
		{
			//before releasing the lock, make sure the EvaluableNode is initialized, otherwise it could get grabbed by another thread
			if(nodes[allocated_index] != nullptr)
				nodes[allocated_index]->InitializeUnallocated();
			else //allocate if nullptr
				nodes[allocated_index] = new EvaluableNode();

			return nodes[allocated_index];
		}
		//the node wasn't valid; put it back and do a write lock to allocate more
		--firstUnusedNodeIndex;
	}
	//don't have enough nodes, so need to attempt a write lock to allocate more
	Concurrency::WriteLock write_lock(managerAttributesMutex);

	//try again after write lock to allocate a node in case another thread has performed the allocation
	//already have the write lock, so don't need to worry about another thread stealing firstUnusedNodeIndex
	//use the cached value for firstUnusedNodeIndex, allocated_index, to check if another thread has performed the allocation
	//as other threads may have reduced firstUnusedNodeIndex, incurring more unnecessary write locks when a memory expansion is needed
#else
	allocated_index = firstUnusedNodeIndex;
#endif

	size_t num_nodes = nodes.size();
	if(allocated_index < num_nodes && firstUnusedNodeIndex < num_nodes)
	{
		if(nodes[firstUnusedNodeIndex] != nullptr)
		{
		#ifdef MULTITHREAD_SUPPORT
			//before releasing the lock, make sure the EvaluableNode is initialized, otherwise it could get grabbed by another thread
			nodes[firstUnusedNodeIndex]->InitializeUnallocated();
		#endif
		}
		else //allocate if nullptr
		{
			nodes[firstUnusedNodeIndex] = new EvaluableNode();
		}

		return nodes[firstUnusedNodeIndex++];
	}

	//ran out, so need another node; push a bunch on the heap so don't need to reallocate as often and slow down garbage collection
	size_t new_num_nodes = static_cast<size_t>(allocExpansionFactor * num_nodes) + 1; //preallocate additional resources, plus current node
	
	//fill new EvaluableNode slots with nullptr
	nodes.resize(new_num_nodes, nullptr);

	if(nodes[firstUnusedNodeIndex] != nullptr)
	{
	#ifdef MULTITHREAD_SUPPORT
		//before releasing the lock, make sure the EvaluableNode is initialized, otherwise it could get grabbed by another thread
		nodes[firstUnusedNodeIndex]->InitializeUnallocated();
	#endif	
	}
	else //allocate if nullptr
	{
		nodes[firstUnusedNodeIndex] = new EvaluableNode();
	}

	return nodes[firstUnusedNodeIndex++];
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
						if(cur_node_ptr != nullptr && !cur_node_ptr->IsNodeDeallocated())
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
		while(first_unused_node_index_temp < lowest_known_unused_index)
		{
			//nodes can't be nullptr below firstUnusedNodeIndex
			auto &cur_node_ptr = nodes[first_unused_node_index_temp];

			//if the node has been found on this iteration, then clear it as counted so it's clean for next garbage collection
			if(cur_node_ptr != nullptr && cur_node_ptr->GetKnownToBeInUse())
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
	while(first_unused_node_index_temp < lowest_known_unused_index)
	{
		//nodes can't be nullptr below firstUnusedNodeIndex
		auto &cur_node_ptr = nodes[first_unused_node_index_temp];

		//if the node has been found on this iteration, then clear it as counted so it's clean for next garbage collection
		if(cur_node_ptr != nullptr && cur_node_ptr->GetKnownToBeInUse())
		{
			cur_node_ptr->SetKnownToBeInUse(false);
			first_unused_node_index_temp++;
		}
		else //collect the node
		{
			//free any extra memory used, since this node is no longer needed
			if(cur_node_ptr != nullptr && !cur_node_ptr->IsNodeDeallocated())
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

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(!tree->GetNeedCycleCheck());
#endif

	tree->Invalidate();
}

void EvaluableNodeManager::FreeNodeTreeWithCyclesRecurse(EvaluableNode *tree)
{
	if(tree->IsAssociativeArray())
	{
		//pull the mapped child nodes out of the tree before invalidating it
		//need to invalidate before call child nodes to prevent infinite recrusion loop
		EvaluableNode::AssocType mcn;
		auto &tree_mcn = tree->GetMappedChildNodesReference();
		std::swap(mcn, tree_mcn);
		tree->Invalidate();

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
	}
	else //ordered
	{
		//pull the ordered child nodes out of the tree before invalidating it
		//need to invalidate before call child nodes to prevent infinite recrusion loop
		std::vector<EvaluableNode *> ocn;
		auto &tree_ocn = tree->GetOrderedChildNodesReference();
		std::swap(ocn, tree_ocn);
		tree->Invalidate();

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

void EvaluableNodeManager::CompactAllocatedNodes()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock write_lock(managerAttributesMutex);
#endif

	size_t lowest_known_unused_index = firstUnusedNodeIndex;	//store any unused nodes here

	//start with a clean slate, and swap everything in use into the in-use region
	firstUnusedNodeIndex = 0;

	//just in case empty
	if(nodes.size() == 0)
		return;

	while(firstUnusedNodeIndex < lowest_known_unused_index)
	{
		if(nodes[firstUnusedNodeIndex] != nullptr && !nodes[firstUnusedNodeIndex]->IsNodeDeallocated())
			firstUnusedNodeIndex++;
		else
		{
			//see if out of things to free; if so exit early
			if(lowest_known_unused_index == 0)
				break;

			//put the node up at the edge of unused memory, grab the next lowest node and pull it down to increase density
			std::swap(nodes[firstUnusedNodeIndex], nodes[--lowest_known_unused_index]);
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
	EvaluableNodeManager *ensure_nodes_in_enm)
{
	if(en == nullptr)
		return;

	static EvaluableNode::ReferenceSetType checked;
	checked.clear();

	if(ensure_nodes_in_enm)
	{
		static FastHashSet<EvaluableNode *> existing_nodes;
		existing_nodes.clear();

		for(size_t i = 0; i < ensure_nodes_in_enm->firstUnusedNodeIndex; i++)
		{
			if(ensure_nodes_in_enm->nodes[i] != nullptr)
				existing_nodes.insert(ensure_nodes_in_enm->nodes[i]);
		}

		return ValidateEvaluableNodeTreeMemoryIntegrityRecurse(en, checked, &existing_nodes);
	}
	else
	{
		return ValidateEvaluableNodeTreeMemoryIntegrityRecurse(en, checked, nullptr);
	}
}

std::pair<EvaluableNode *, bool> EvaluableNodeManager::DeepAllocCopy(EvaluableNode *tree, DeepAllocCopyParams &dacp)
{
	//attempt to insert a new reference for this node, start with null
	auto [inserted_copy, inserted] = dacp.references->insert(std::make_pair(tree, nullptr));

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

#ifdef _OPENMP
EvaluableNode *EvaluableNodeManager::NonCycleDeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier, bool parallelize)
#else
EvaluableNode *EvaluableNodeManager::NonCycleDeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier)
#endif
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
		#ifdef _OPENMP
			s = NonCycleDeepAllocCopy(n, metadata_modifier, parallelize);
		#else
			s = NonCycleDeepAllocCopy(n, metadata_modifier);
		#endif
		}
	}
	else if(!copy->IsImmediate())
	{
		//for any ordered children, copy and update
		auto &copy_ocn = copy->GetOrderedChildNodesReference();

		#pragma omp parallel for schedule(static) if(parallelize && copy->GetOrderedChildNodes().size() > 16)
		for(int64_t i = 0; i < static_cast<int64_t>(copy_ocn.size()); i++)
		{
			//get current item in list
			EvaluableNode *n = copy_ocn[i];
			if(n == nullptr)
				continue;

			//replace current item in list with copy
		#ifdef _OPENMP
			copy_ocn[i] = NonCycleDeepAllocCopy(n, metadata_modifier, parallelize);
		#else
			copy_ocn[i] = NonCycleDeepAllocCopy(n, metadata_modifier);
		#endif
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
		ThreadPool::CountableTaskSet task_set;

		//start processing root node first, as there's a good chance it will be the largest
		if(root_node != nullptr && !root_node->GetKnownToBeInUseAtomic())
		{
			//don't enqueue in batch, as threads racing ahead of others will reduce memory
			//contention
			Concurrency::urgentThreadPool.EnqueueTask(
				[root_node, &task_set]
				{
					MarkAllReferencedNodesInUseRecurseConcurrent(root_node);
					task_set.MarkTaskCompleted();
				}
			);
			task_set.AddTask();
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
						MarkAllReferencedNodesInUseRecurseConcurrent(en);
						task_set.MarkTaskCompleted();
					}
				);
				task_set.AddTask();
			}
		}

		Concurrency::urgentThreadPool.ChangeCurrentThreadStateFromActiveToWaiting();

		task_set.WaitForTasks();

		Concurrency::urgentThreadPool.ChangeCurrentThreadStateFromWaitingToActive();

		return;
	}
#endif

	//check for null or insertion before calling recursion to minimize number of branches (slight performance improvement)
	if(root_node != nullptr && !root_node->GetKnownToBeInUse())
		MarkAllReferencedNodesInUseRecurse(root_node);

	for(auto &[t, _] : nr.nodesReferenced)
	{
		if(t == nullptr || t->GetKnownToBeInUse())
			continue;

		MarkAllReferencedNodesInUseRecurse(t);
	}
}

std::pair<bool, bool> EvaluableNodeManager::UpdateFlagsForNodeTreeRecurse(EvaluableNode *n, std::vector<EvaluableNode *> &stack)
{
	//do a linear find because the logarithmic size of depth should be small enough to make this faster
	// than a ReferenceSetType
	if(std::find(begin(stack), end(stack), n) != end(stack))
		return std::make_pair(false, n->GetIsIdempotent());

	stack.push_back(n);

	bool is_idempotent = (IsEvaluableNodeTypePotentiallyIdempotent(n->GetType()) && (n->GetNumLabels() == 0));

	if(n->IsAssociativeArray())
	{
		bool need_cycle_check = false;

		for(auto &[cn_id, cn] : n->GetMappedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			auto [cn_need_cycle_check, cn_is_idempotent] = UpdateFlagsForNodeTreeRecurse(cn, stack);

			//update flags for tree
			if(cn_need_cycle_check)
				need_cycle_check = true;

			if(!cn_is_idempotent)
				is_idempotent = false;
		}

		n->SetNeedCycleCheck(need_cycle_check);
		n->SetIsIdempotent(is_idempotent);
		return std::make_pair(need_cycle_check, is_idempotent);
	}
	else if(!n->IsImmediate())
	{
		bool need_cycle_check = false;

		for(auto cn : n->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			auto [cn_need_cycle_check, cn_is_idempotent] = UpdateFlagsForNodeTreeRecurse(cn, stack);

			//update flags for tree
			if(cn_need_cycle_check)
				need_cycle_check = true;

			if(!cn_is_idempotent)
				is_idempotent = false;
		}

		n->SetNeedCycleCheck(need_cycle_check);
		n->SetIsIdempotent(is_idempotent);
		return std::make_pair(need_cycle_check, is_idempotent);
	}
	else //immediate value
	{
		n->SetIsIdempotent(is_idempotent);
		n->SetNeedCycleCheck(false);
		return std::make_pair(false, is_idempotent);
	}
}

void EvaluableNodeManager::MarkAllReferencedNodesInUseRecurse(EvaluableNode *tree)
{
	//if entering this function, then the node hasn't been marked yet
	tree->SetKnownToBeInUse(true);

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
		{
			if(e != nullptr && !e->GetKnownToBeInUse())
				MarkAllReferencedNodesInUseRecurse(e);
		}
	}
	else if(!tree->IsImmediate())
	{
		for(auto &e : tree->GetOrderedChildNodesReference())
		{
			if(e != nullptr && !e->GetKnownToBeInUse())
				MarkAllReferencedNodesInUseRecurse(e);
		}
	}	
}

#ifdef MULTITHREAD_SUPPORT
void EvaluableNodeManager::MarkAllReferencedNodesInUseRecurseConcurrent(EvaluableNode* tree)
{
	//if entering this function, then the node hasn't been marked yet
	tree->SetKnownToBeInUseAtomic(true);

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
		{
			if(e != nullptr && !e->GetKnownToBeInUseAtomic())
				MarkAllReferencedNodesInUseRecurseConcurrent(e);
		}
	}
	else if(!tree->IsImmediate())
	{
		for(auto &e : tree->GetOrderedChildNodesReference())
		{
			if(e != nullptr && !e->GetKnownToBeInUseAtomic())
				MarkAllReferencedNodesInUseRecurseConcurrent(e);
		}
	}
}
#endif

void EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrityRecurse(EvaluableNode *en,
	EvaluableNode::ReferenceSetType &checked, FastHashSet<EvaluableNode *> *existing_nodes)
{
	auto [_, inserted] = checked.insert(en);
	if(!inserted)
		return;

	if(en->IsNodeDeallocated() || en->GetKnownToBeInUse())
		assert(false);

	if(existing_nodes != nullptr)
	{
		if(existing_nodes->find(en) == end(*existing_nodes))
			assert(false);
	}

	if(en->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : en->GetMappedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			ValidateEvaluableNodeTreeMemoryIntegrityRecurse(cn, checked, existing_nodes);
		}
	}
	else if(!en->IsImmediate())
	{
		for(auto cn : en->GetOrderedChildNodesReference())
		{
			if(cn == nullptr)
				continue;

			ValidateEvaluableNodeTreeMemoryIntegrityRecurse(cn, checked, existing_nodes);
		}
	}	
}
