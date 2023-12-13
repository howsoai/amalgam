#pragma once

//project headers:
#include "Concurrency.h"
#include "EvaluableNode.h"

typedef int64_t ExecutionCycleCount;
typedef int32_t ExecutionCycleCountCompactDelta;

//describes an EvaluableNode reference and whether it is uniquely referenced
class EvaluableNodeReference
{
public:
	constexpr EvaluableNodeReference()
		: referenceAndImmediate(nullptr), unique(true), immediate(false)
	{	}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique)
		: referenceAndImmediate(_reference), unique(_unique), immediate(false)
	{	}

	__forceinline EvaluableNodeReference(const EvaluableNodeReference &inr)
		: referenceAndImmediate(inr.referenceAndImmediate), unique(inr.unique), immediate(inr.immediate)
	{	}

	//when attached a child node, make sure that this node reflects the same properties
	void UpdatePropertiesBasedOnAttachedNode(EvaluableNodeReference &attached)
	{
		if(attached.referenceAndImmediate.reference == nullptr)
			return;

		if(!attached.unique)
		{
			unique = false;
			//if new attachments aren't unique, then can't guarantee there isn't a cycle present
			referenceAndImmediate.reference->SetNeedCycleCheck(true);
		}
		else if(attached.referenceAndImmediate.reference->GetNeedCycleCheck())
		{
			referenceAndImmediate.reference->SetNeedCycleCheck(true);
		}

		if(!attached.referenceAndImmediate.reference->GetIsIdempotent())
			referenceAndImmediate.reference->SetIsIdempotent(false);
	}

	//calls GetNeedCycleCheck if the reference is not nullptr, returns false if it is nullptr
	constexpr bool GetNeedCycleCheck()
	{
		if(referenceAndImmediate.reference == nullptr)
			return false;
	
		return referenceAndImmediate.reference->GetNeedCycleCheck();
	}

	//calls SetNeedCycleCheck if the reference is not nullptr
	constexpr void SetNeedCycleCheck(bool need_cycle_check)
	{
		if(referenceAndImmediate.reference == nullptr)
			return;
	
		referenceAndImmediate.reference->SetNeedCycleCheck(need_cycle_check);
	}

	constexpr static EvaluableNodeReference Null()
	{
		return EvaluableNodeReference(nullptr, true);
	}

	inline void SetReference(EvaluableNode *_reference)
	{
		referenceAndImmediate.reference = _reference;
	}

	inline void SetReference(EvaluableNode *_reference, bool _unique)
	{
		referenceAndImmediate.reference = _reference;
		unique = _unique;
	}

	inline EvaluableNode *&GetReference()
	{
		return referenceAndImmediate.reference;
	}

	//allow to use as an EvaluableNode *
	constexpr operator EvaluableNode *&()
	{	return referenceAndImmediate.reference;	}

	//allow to use as an EvaluableNode *
	constexpr EvaluableNode *operator->()
	{	return referenceAndImmediate.reference;	}

	__forceinline EvaluableNodeReference &operator =(const EvaluableNodeReference &enr)
	{
		//perform a memcpy because it's a union, to be safe; the compiler should optimize this out
		std::memcpy(&referenceAndImmediate, &enr.referenceAndImmediate, sizeof(referenceAndImmediate));
		unique = enr.unique;
		immediate = enr.immediate;

		return *this;
	}

protected:

	//efficient way to handle whether an InterpretNode method
	// is returning an immediate value based on the immediate attribute
	union ReferenceAndImmediate
	{
		constexpr ReferenceAndImmediate()
			: reference(nullptr)
		{	}

		constexpr ReferenceAndImmediate(EvaluableNode *_reference)
			: reference(_reference)
		{	}

		__forceinline ReferenceAndImmediate(const ReferenceAndImmediate &rai)
		{
			//perform a memcpy because it's a union, to be safe; the compiler should optimize this out
			std::memcpy(this, &rai, sizeof(this));
		}

		EvaluableNode *reference;
		EvaluableNodeImmediateValue immediate;
	} referenceAndImmediate;
	
public:

	//this is the only reference to the result
	bool unique;

	//if immediate, then the storage is immediate
	bool immediate;

	EvaluableNodeImmediateValueType immediateType;
};


//Uses an EvaluableNode as a stack which may already have elements in it
// upon destruction it restores the stack back to the state it was when constructed
class EvaluableNodeStackStateSaver
{
public:
	__forceinline EvaluableNodeStackStateSaver(std::vector<EvaluableNode *> *_stack)
	{
		stack = _stack;
		originalStackSize = stack->size();
	}

	//constructor that adds one first element
	__forceinline EvaluableNodeStackStateSaver(std::vector<EvaluableNode *> *_stack, EvaluableNode *initial_element)
	{
		stack = _stack;
		originalStackSize = stack->size();

		stack->push_back(initial_element);
	}

	__forceinline ~EvaluableNodeStackStateSaver()
	{
		stack->resize(originalStackSize);
	}

	__forceinline void PushEvaluableNode(EvaluableNode *n)
	{
		stack->push_back(n);
	}

	__forceinline void PopEvaluableNode()
	{
		stack->pop_back();
	}

	std::vector<EvaluableNode *> *stack;
	size_t originalStackSize;
};

	
class EvaluableNodeManager
{
public:
	EvaluableNodeManager() :
		executionCyclesSinceLastGarbageCollection(0), firstUnusedNodeIndex(0)
	{	}

	~EvaluableNodeManager();

	//////////////////////////////////
	//convenience functions to alloc nodes with specific types of data
	inline EvaluableNode *AllocNode(EvaluableNodeType type, const std::string &string_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(type, string_value);
		return n;
	}

	inline EvaluableNode *AllocNode(EvaluableNodeType type, StringInternPool::StringID string_id)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(type, string_id);
		return n;
	}

	//like AllocNode, but hands off the string reference to string_id
	inline EvaluableNode *AllocNodeWithReferenceHandoff(EvaluableNodeType type, StringInternPool::StringID string_id)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeTypeWithReferenceHandoff(type, string_id);
		return n;
	}

	inline EvaluableNode *AllocNode(EvaluableNodeType type, StringInternRef &sir)
	{	return AllocNode(type, static_cast<StringInternPool::StringID>(sir));	}
	inline EvaluableNode *AllocNode(EvaluableNodeType type, StringInternWeakRef &siwr)
	{	return AllocNode(type, static_cast<StringInternPool::StringID>(siwr));	}
	
	inline EvaluableNode *AllocNode(double float_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(float_value);
		return n;
	}

	inline EvaluableNode *AllocNode(int64_t int_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(static_cast<double>(int_value));
		return n;
	}

	inline EvaluableNode *AllocNode(EvaluableNodeType type)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(type);
		return n;
	}

	inline EvaluableNode *AllocListNode(std::vector<EvaluableNode *> *child_nodes)
	{
		EvaluableNode *n = AllocNode(ENT_LIST);
		n->SetOrderedChildNodes(*child_nodes);
		return n;
	}

	//allocates and returns a node of type ENT_LIST
	// and allocates num_child_nodes child nodes initialized to child_node_type (with an appropriate default value)
	EvaluableNode *AllocListNodeWithOrderedChildNodes(EvaluableNodeType child_node_type, size_t num_child_nodes);

	//ways that labels can be modified when a new node is allocated
	enum EvaluableNodeMetadataModifier
	{
		ENMM_NO_CHANGE,					//leave labels as they are
		ENMM_LABEL_ESCAPE_INCREMENT,	//insert a # in front of each label
		ENMM_LABEL_ESCAPE_DECREMENT,	//remove a # from the front of each label
		ENMM_REMOVE_ALL					//remove all metadata
	};
	EvaluableNode *AllocNode(EvaluableNode *original, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE);

	//ensures that the top node is modifiable -- will allocate the node if necessary,
	// and if the result and any child nodes are all unique, then it will return an EvaluableNodeReference that is unique
	inline void EnsureNodeIsModifiable(EvaluableNodeReference &original,
		EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE)
	{
		if(original.unique)
			return;

		EvaluableNode *copy = AllocNode(original.GetReference(), metadata_modifier);
		//the copy will only be unique if all child nodes are unique or there are no child nodes
		original = EvaluableNodeReference(copy, original.unique || (copy->GetNumChildNodes() == 0));
	}

	//attempts to reuse candidate if it is unique and change it into the specified type
	//if candidate is not unique, then it allocates and returns a new node
	inline EvaluableNodeReference ReuseOrAllocNode(EvaluableNodeReference candidate, EvaluableNodeType type)
	{
		if(candidate.unique && candidate != nullptr)
		{
			//if not cyclic, can attempt to free all child nodes
			//if cyclic, don't try, in case a child node points back to candidate
			if(!candidate->GetNeedCycleCheck())
			{
				if(candidate->IsAssociativeArray())
				{
					for(auto &[_, e] : candidate->GetMappedChildNodesReference())
					{
						if(e != nullptr)
							FreeNodeTreeRecurse(e);
					}
				}
				else if(!candidate->IsImmediate())
				{
					for(auto &e : candidate->GetOrderedChildNodesReference())
					{
						if(e != nullptr)
							FreeNodeTreeRecurse(e);
					}
				}
			}

			candidate->ClearAndSetType(type);
			return candidate;
		}
		else
		{
			return EvaluableNodeReference(AllocNode(type), true);
		}
	}

	//like ReuseOrAllocNode, but picks whichever node is reusable and frees the other if possible
	//will try candidate_1 first
	inline EvaluableNodeReference ReuseOrAllocOneOfNodes(
		EvaluableNodeReference candidate_1, EvaluableNodeReference candidate_2, EvaluableNodeType type)
	{
		if(candidate_1.unique && candidate_1 != nullptr)
		{
			FreeNodeTreeIfPossible(candidate_2);
			return ReuseOrAllocNode(candidate_1, type);
		}

		//candidate_1 wasn't unique, so try for candidate 2
		return ReuseOrAllocNode(candidate_2, type);
	}

	//Copies the data structure and everything underneath it, modifying labels as specified
	// if cycle_free is true on input, then it can perform a faster copy
	inline EvaluableNodeReference DeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE)
	{	
		if(tree == nullptr)
			return EvaluableNodeReference::Null();

		if(!tree->GetNeedCycleCheck())
			return EvaluableNodeReference(NonCycleDeepAllocCopy(tree, metadata_modifier), true);

		EvaluableNode::ReferenceAssocType references;
		return DeepAllocCopy(tree, references, metadata_modifier);
	}

	//used to hold all of the references for DeepAllocCopy calls
	struct DeepAllocCopyParams
	{
		constexpr DeepAllocCopyParams(EvaluableNode::ReferenceAssocType *_references, EvaluableNodeMetadataModifier metadata_modifier)
			: references(_references), labelModifier(metadata_modifier)
		{	}

		EvaluableNode::ReferenceAssocType *references;
		EvaluableNodeMetadataModifier labelModifier;
	};

	//Copies the data structure and everything underneath it, modifying labels as specified
	//  modifies labels as specified
	//  will determine whether the tree is cycle free and return the appropriate value in the EvaluableNodeReference
	//references is a map of those nodes that have already been copied, with the key being the original and the value being the copy -- it first looks in references before making a copy
	inline EvaluableNodeReference DeepAllocCopy(EvaluableNode *tree, EvaluableNode::ReferenceAssocType &references, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE)
	{
		if(tree == nullptr)
			return EvaluableNodeReference::Null();

		//start with cycleFree true, will be set to false if it isn't
		DeepAllocCopyParams dacp(&references, metadata_modifier);
		auto [copy, need_cycle_check] = DeepAllocCopy(tree, dacp);
		return EvaluableNodeReference(copy, true);
	}

	//modifies the labels for the tree as described by metadata_modifier
	inline static void ModifyLabelsForNodeTree(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE)
	{
		if(tree == nullptr || metadata_modifier == ENMM_NO_CHANGE)
			return;

		if(!tree->GetNeedCycleCheck())
		{
			NonCycleModifyLabelsForNodeTree(tree, metadata_modifier);
			return;
		}

		EvaluableNode::ReferenceSetType checked;
		ModifyLabelsForNodeTree(tree, checked, metadata_modifier);
	}

	//computes whether the code is cycle free and idempotent and updates all nodes appropriately
	static inline void UpdateFlagsForNodeTree(EvaluableNode *tree)
	{
		if(tree == nullptr)
			return;

		EvaluableNode::ReferenceSetType checked;
		UpdateFlagsForNodeTreeRecurse(tree, checked);
	}

	//computes whether the code is cycle free and idempotent and updates all nodes appropriately
	// uses checked to store nodes
	static inline void UpdateFlagsForNodeTree(EvaluableNode *tree, EvaluableNode::ReferenceSetType &checked)
	{
		if(tree == nullptr)
			return;

		checked.clear();
		UpdateFlagsForNodeTreeRecurse(tree, checked);
	}

	//heuristic used to determine whether unused memory should be collected (e.g., by FreeAllNodesExcept*)
	//force this inline because it occurs in inner loops
	__forceinline bool RecommendGarbageCollection()
	{
		//makes sure to perform garbage collection between every opcode to find memory reference errors
	#ifdef PEDANTIC_GARBAGE_COLLECTION
		return true;
	#endif

		if(executionCyclesSinceLastGarbageCollection > minCycleCountBetweenGarbageCollects)
		{
			auto cur_size = GetNumberOfUsedNodes();

			size_t next_expansion_size = static_cast<size_t>(cur_size * allocExpansionFactor);
			if(next_expansion_size < nodes.size())
			{
				executionCyclesSinceLastGarbageCollection = 0;
				return false;
			}

			return true;
		}

		return false;
	}

	//moves garbage collection to be more likely to be triggered next time CollectGarbage is called
	__forceinline void AdvanceGarbageCollectionTrigger()
	{
		//count setting data on an entity toward trigger gc
		executionCyclesSinceLastGarbageCollection += minCycleCountBetweenGarbageCollects / 4;
	}

	//runs heuristics and collects garbage
#ifdef MULTITHREAD_SUPPORT
	//if multithreaded, then memory_modification_lock is the lock used for memoryModificationMutex if not nullptr
	void CollectGarbage(Concurrency::ReadLock *memory_modification_lock);
#else
	void CollectGarbage();
#endif

	//frees an EvaluableNode (must be owned by this EvaluableNodeManager)
	inline void FreeNode(EvaluableNode *n)
	{
		if(n == nullptr)
			return;

		n->Invalidate();

		ReclaimFreedNodesAtEnd();
	}

	//attempts to free the node reference
	__forceinline void FreeNodeIfPossible(EvaluableNodeReference &enr)
	{
		if(enr.unique)
			FreeNode(enr);
	}
	
	//frees all nodes
	void FreeAllNodes();

	//frees the entire tree in the respective ways for the corresponding permanance types allowed
	inline void FreeNodeTree(EvaluableNode *en)
	{
		if(en == nullptr)
			return;
		
		if(IsEvaluableNodeTypeImmediate(en->GetType()))
		{
			en->Invalidate();
		}
		else if(!en->GetNeedCycleCheck())
		{
			FreeNodeTreeRecurse(en);
		}
		else //more costly cyclic free
		{
		#ifdef MULTITHREAD_SUPPORT
			//need to acquire a read lock, because if any node is reclaimed or compacted while this free is taking place,
			// and another thread allocates it, then this cyclic free could accidentally free a node that was freed and
			// reclaimed by another thread
			Concurrency::ReadLock lock(managerAttributesMutex);
		#endif
			FreeNodeTreeWithCyclesRecurse(en);
		}

		ReclaimFreedNodesAtEnd();
	}

	//attempts to free the node reference
	__forceinline void FreeNodeTreeIfPossible(EvaluableNodeReference &enr)
	{
		if(enr.unique)
			FreeNodeTree(enr);
	}

	//just frees the child nodes of tree, but not tree itself; assumes no cycles
	inline void FreeNodeChildNodes(EvaluableNode *tree)
	{
		if(tree->IsAssociativeArray())
		{
			for(auto &[_, e] : tree->GetMappedChildNodesReference())
			{
				if(e != nullptr)
					FreeNodeTreeRecurse(e);
			}
		}
		else if(tree->IsOrderedArray())
		{
			for(auto &e : tree->GetOrderedChildNodesReference())
			{
				if(e != nullptr)
					FreeNodeTreeRecurse(e);
			}
		}		

		ReclaimFreedNodesAtEnd();
	}

	//if no nodes are referenced, then will free all
	inline void ClearAllNodesIfNoneReferenced()
	{
		if(nodesCurrentlyReferenced.size() == 0)
			FreeAllNodes();
	}

	//adds the node to nodesCurrentlyReferenced
	void KeepNodeReference(EvaluableNode *en);

	//like KeepNodeReference but iterates over a collection
	template<typename EvaluableNodeCollection>
	inline void KeepNodeReferences(EvaluableNodeCollection &node_collection)
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock lock(managerAttributesMutex);
	#endif

		for(auto en : node_collection)
		{
			if(en == nullptr)
				continue;

			//attempt to put in value 1 for the reference
			auto [inserted_result, inserted] = nodesCurrentlyReferenced.insert(std::make_pair(en, 1));

			//if couldn't insert because already referenced, then increment
			if(!inserted)
				inserted_result->second++;
		}
	}

	//removes the node from nodesCurrentlyReferenced
	void FreeNodeReference(EvaluableNode *en);

	//like FreeNodeReference but iterates over a collection
	template<typename EvaluableNodeCollection>
	inline void FreeNodeReferences(EvaluableNodeCollection &node_collection)
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock lock(managerAttributesMutex);
	#endif

		for(auto en : node_collection)
		{
			if(en == nullptr)
				continue;

			//get reference count
			auto node = nodesCurrentlyReferenced.find(en);

			//don't do anything if not counted
			if(node == nodesCurrentlyReferenced.end())
				continue;

			//if it has sufficient refcount, then just decrement
			if(node->second > 1)
				node->second--;
			else //otherwise remove reference
				nodesCurrentlyReferenced.erase(node);
		}
	}

	//compacts allocated nodes so that the node pool can be used more efficiently
	// and can improve reuse without calling the more expensive FreeAllNodesExceptReferencedNodes
	void CompactAllocatedNodes();

	//allows freed nodes at the end of nodes to be reallocated
	inline void ReclaimFreedNodesAtEnd()
	{
	#ifdef MULTITHREAD_SUPPORT
		//this is much more expensive with multithreading, so only do when useful
		if((executionCyclesSinceLastGarbageCollection & 511) != 0)
			return;

		//be opportunistic and only attempt to reclaim if it can grab a write lock
		Concurrency::WriteLock write_lock(managerAttributesMutex, std::defer_lock);
		if(!write_lock.try_lock())
			return;
	#endif

		//if any group of nodes on the top are ready to be cleaned up cheaply, do so
		while(firstUnusedNodeIndex > 0 && nodes[firstUnusedNodeIndex - 1] != nullptr && nodes[firstUnusedNodeIndex - 1]->GetType() == ENT_DEALLOCATED)
			firstUnusedNodeIndex--;
	}

	//returns the number of nodes currently being used that have not been freed yet
	__forceinline size_t GetNumberOfUsedNodes()
	{	return firstUnusedNodeIndex;		}
	
	__forceinline size_t GetNumberOfUnusedNodes()
	{	return nodes.size() - firstUnusedNodeIndex;		}

	__forceinline size_t GetNumberOfNodesReferenced()
	{
		return nodesCurrentlyReferenced.size();
	}

	//returns the root node, implicitly defined as the first node in memory
	// note that this means there should be at least one node allocated and SetRootNode called before this function is called
	inline EvaluableNode *GetRootNode()
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(managerAttributesMutex);
	#endif

		if(firstUnusedNodeIndex == 0)
			return nullptr;

		return nodes[0];
	}

	//sets the root node, implicitly defined as the first node in memory, to new_root
	// note that new_root MUST have been allocated by this EvaluableNodeManager
	inline void SetRootNode(EvaluableNode *new_root)
	{
	#ifdef MULTITHREAD_SUPPORT
		//use WriteLock to be safe
		Concurrency::WriteLock lock(managerAttributesMutex);
	#endif

		//iteratively search forward; this will be fast for newly created entities but potentially slow for those that are not
		// however, this should be rarely called on those entities since it's basically clearing them out, so it should not generally be a performance issue
		auto location = std::find(begin(nodes), begin(nodes) + firstUnusedNodeIndex, new_root);

		//swap the pointers
		if(location != end(nodes))
			std::swap(*begin(nodes), *location);
	}

	//returns a copy of the nodes referenced; should be used only for debugging
	inline EvaluableNode::ReferenceCountType &GetNodesReferenced()
	{
		return nodesCurrentlyReferenced;
	}

	//returns true if any node is referenced other than root, which is an indication if there are
	// any interpreters operating on the nodes managed by this instance
	inline bool IsAnyNodeReferencedOtherThanRoot()
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(managerAttributesMutex);
	#endif

		size_t num_nodes_currently_referenced = nodesCurrentlyReferenced.size();
		if(num_nodes_currently_referenced > 1)
			return true;

		if(num_nodes_currently_referenced == 0)
			return false;

		//exactly one node referenced; if the root is null, then it has to be something else
		if(nodes[0] == nullptr)
			return true;

		//in theory this should always find the root node being referenced and thus return false
		// but if there is any sort of unusual situation where the root node isn't referenced, it'll catch it
		// and report that there is something else being referenced
		return (nodesCurrentlyReferenced.find(nodes[0]) == end(nodesCurrentlyReferenced));		
	}

	//Returns all nodes still in use.  For debugging purposes
	std::vector<EvaluableNode *> GetUsedNodes()
	{	return std::vector<EvaluableNode *>(begin(nodes), begin(nodes) + firstUnusedNodeIndex);	}
	
	//returns an estimate of the amount of memory allocated by the nodes managed
	// only an estimate because the platform's underlying memory management system may need to allocate additional
	// memory that cannot be easily accounted for
	size_t GetEstimatedTotalReservedSizeInBytes();
	size_t GetEstimatedTotalUsedSizeInBytes();

	//makes sure that the evaluable node and everything referenced by it has not been deallocated
	//asserts an error if it finds any
	//intended for debugging only
	static void ValidateEvaluableNodeTreeMemoryIntegrity(EvaluableNode *en);

	//total number of execution cycles since one of the FreeAllNodes* functions was called
#ifdef MULTITHREAD_SUPPORT
	std::atomic<ExecutionCycleCount> executionCyclesSinceLastGarbageCollection;
#else
	ExecutionCycleCount executionCyclesSinceLastGarbageCollection;
#endif

protected:
	//allocates an EvaluableNode of the respective memory type in the appropriate way
	// returns an uninitialized EvaluableNode -- care must be taken to set fields properly
	EvaluableNode *AllocUninitializedNode();

	//frees everything execpt those nodes referenced by nodesCurrentlyReferenced
	void FreeAllNodesExceptReferencedNodes();

	//support for FreeNodeTree, but requires that tree not be nullptr
	void FreeNodeTreeRecurse(EvaluableNode *tree);

	//support for FreeNodeTreeWithCycles, but requires that tree not be nullptr
	void FreeNodeTreeWithCyclesRecurse(EvaluableNode *tree);

	//modifies the labels of n with regard to metadata_modifier
	// assumes n is not nullptr
	static void ModifyLabels(EvaluableNode *n, EvaluableNodeMetadataModifier metadata_modifier);

	//more efficient version of DeepAllocCopy
	//returns a pair of the copy and true if the copy needs cycle check
	//assumes tree is not nullptr
	std::pair<EvaluableNode *, bool> DeepAllocCopy(EvaluableNode *tree, DeepAllocCopyParams &dacp);

	//performs a deep copy on tree when tree is guaranteed to have no reference cycles
	// assumes tree is NOT nullptr
#ifndef _OPENMP
	EvaluableNode *NonCycleDeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier);
#else
	//keep track of whether there's a top level parallelization
	EvaluableNode *NonCycleDeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier, bool parallelize = true);
#endif

	//recursive helper function for ModifyLabelsForNodeTree
	//assumes tree is not nullptr
	static void ModifyLabelsForNodeTree(EvaluableNode *tree, EvaluableNode::ReferenceSetType &checked, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE);

	//recursive helper function for ModifyLabelsForNodeTree
	//assumes tree is not nullptr
	static void NonCycleModifyLabelsForNodeTree(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE);

	//sets all referenced nodes' garbage collection iteration to gc_collect_iteration
	inline void SetAllReferencedNodesGCCollectIteration(uint8_t gc_collect_iteration)
	{
		//check for null or insertion before calling recursion to minimize number of branches (slight performance improvement)
		for(auto &[t, _] : nodesCurrentlyReferenced)
		{
			if(t == nullptr || t->GetGarbageCollectionIteration() == gc_collect_iteration)
				continue;

			SetAllReferencedNodesGCCollectIterationRecurse(t, gc_collect_iteration);
		}
	}

	//computes whether the code is cycle free and idempotent and updates all nodes appropriately
	// returns flags for whether cycle free and idempotent
	// requires tree not be nullptr
	static std::pair<bool, bool> UpdateFlagsForNodeTreeRecurse(EvaluableNode *tree, EvaluableNode::ReferenceSetType &checked);

	//inserts all nodes referenced by tree into the set references
	//note that tree cannot be nullptr and it should already be inserted into the references prior to calling
	static void SetAllReferencedNodesGCCollectIterationRecurse(EvaluableNode *tree, uint8_t gc_collect_iteration);

	static void ValidateEvaluableNodeTreeMemoryIntegrityRecurse(EvaluableNode *en, EvaluableNode::ReferenceSetType &checked);

#ifdef MULTITHREAD_SUPPORT
public:

	//updates garbage collection process based on current number of threads and number of tasks
	static inline void UpdateMinCycleCountBetweenGarbageCollectsBasedOnThreads(size_t num_tasks)
	{
		//can't go above the max number of threads
		num_tasks = std::min(num_tasks, Concurrency::threadPool.GetCurrentMaxNumThreads());
		//don't want to go below the number of threads being used by other things
		num_tasks = std::max(num_tasks, Concurrency::threadPool.GetNumActiveThreads());

		minCycleCountBetweenGarbageCollects = minCycleCountBetweenGarbageCollectsPerThread
			* static_cast<ExecutionCycleCountCompactDelta>(num_tasks);
	}

	//mutex to manage attributes of manager, including operations such as
	// memory allocation, reference management, etc.
	Concurrency::ReadWriteMutex managerAttributesMutex;

	//global mutex to manage whether memory nodes are being modified
	//concurrent modifications can occur as long as there is only one unique thread
	// that has allocated the memory
	//garbage collection or destruction of the manager require a unique lock
	// so that the memory can be traversed
	//note that this is a global lock because nodes may be mixed among more than one
	// EvaluableNodeManager and so garbage collection should not happening while memory is being modified
	static Concurrency::ReadWriteMutex memoryModificationMutex;

protected:
#endif

	//keeps track of all of the nodes currently referenced by any resource or interpreter
	EvaluableNode::ReferenceCountType nodesCurrentlyReferenced;

	//nodes that have been allocated and may be in use
	// all nodes in use are below firstUnusedNodeIndex, such that all above that index are free for use
	// nodes cannot be nullptr for lower indices than firstUnusedNodeIndex
	std::vector<EvaluableNode *> nodes;

#ifdef MULTITHREAD_SUPPORT
	std::atomic<size_t> firstUnusedNodeIndex;
#else
	size_t firstUnusedNodeIndex;
#endif

	//extra space to allocate when allocating
	static const double allocExpansionFactor;

#ifdef MULTITHREAD_SUPPORT
	//minimum number of cycles between collects per thread
	static const ExecutionCycleCountCompactDelta minCycleCountBetweenGarbageCollectsPerThread;
#else
	//make the next value constant if no threads
	const
#endif
	//current number of cycles between collects based on number of threads
	static ExecutionCycleCountCompactDelta minCycleCountBetweenGarbageCollects;
};
