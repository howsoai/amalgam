#pragma once

//project headers:
#include "Concurrency.h"
#include "EvaluableNode.h"

//system headers:
#include <memory>

//if the macro PEDANTIC_GARBAGE_COLLECTION is defined, then garbage collection will be performed
//after every opcode, to help find and debug memory issues

typedef int64_t ExecutionCycleCount;
typedef int32_t ExecutionCycleCountCompactDelta;

//describes an EvaluableNode value and whether it is uniquely referenced
//this is mostly used for actual EvaluableNode *'s, and so most of the methods are built as such
//however, if it may contain an immediate value, then that must be checked via IsImmediateValue()
class EvaluableNodeReference
{
public:
	constexpr EvaluableNodeReference()
		: value(), unique(true)
	{	}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique)
		: value(_reference), unique(_unique)
	{	}

	constexpr EvaluableNodeReference(const EvaluableNodeReference &inr)
		: value(inr.value), unique(inr.unique)
	{	}

	__forceinline EvaluableNodeReference(bool value)
		: value(value), unique(true)
	{	}

	__forceinline EvaluableNodeReference(double value)
		: value(value), unique(true)
	{	}

	__forceinline EvaluableNodeReference(StringInternPool::StringID string_id)
		: value(string_intern_pool.CreateStringReference(string_id)), unique(true)
	{	}

	__forceinline EvaluableNodeReference(const std::string &str)
		: value(string_intern_pool.CreateStringReference(str)), unique(true)
	{	}

	//frees resources associated with immediate values
	//note that this could be placed in a destructor, but this is such a rare use,
	//i.e., only when an immediate value is requested, and the references are usually handled specially,
	//that it's just as complex to put it in the destructor but will incur more overhead
	__forceinline void FreeImmediateResources()
	{
		if(value.nodeType == ENIVT_STRING_ID)
			string_intern_pool.DestroyStringReference(value.nodeValue.stringID);
	}

	//when attached a child node, make sure that this node reflects the same properties
	void UpdatePropertiesBasedOnAttachedNode(EvaluableNodeReference &attached)
	{
		if(attached.value.nodeValue.code == nullptr)
			return;

		if(!attached.unique)
		{
			unique = false;
			//if new attachments aren't unique, then can't guarantee there isn't a cycle present
			value.nodeValue.code->SetNeedCycleCheck(true);
		}
		else if(attached.value.nodeValue.code->GetNeedCycleCheck())
		{
			value.nodeValue.code->SetNeedCycleCheck(true);
		}

		if(!attached.value.nodeValue.code->GetIsIdempotent())
			value.nodeValue.code->SetIsIdempotent(false);
	}

	//when attached a child node to a random location under this node, checks to see
	//if all flags for this node should be rechecked
	//this will update uniqueness based on the new attachment
	bool NeedAllFlagsRecheckedAfterNodeAttachedAndUpdateUniqueness(EvaluableNodeReference &attached)
	{
		if(attached.value.nodeValue.code == nullptr)
			return false;

		if(!attached.unique)
		{
			unique = false;
			return true;
		}

		if(value.nodeValue.code->GetNeedCycleCheck() != attached.value.nodeValue.code->GetNeedCycleCheck())
			return true;

		if(value.nodeValue.code->GetIsIdempotent() != attached.value.nodeValue.code->GetIsIdempotent())
			return true;

		return false;
	}

	//calls GetNeedCycleCheck if the reference is not nullptr, returns false if it is nullptr
	__forceinline bool GetNeedCycleCheck()
	{
		if(value.nodeType != ENIVT_CODE)
			return false;

		if(value.nodeValue.code == nullptr)
			return false;

		return value.nodeValue.code->GetNeedCycleCheck();
	}

	//calls SetNeedCycleCheck if the reference is not nullptr
	__forceinline void SetNeedCycleCheck(bool need_cycle_check)
	{
		if(value.nodeValue.code == nullptr)
			return;

		value.nodeValue.code->SetNeedCycleCheck(need_cycle_check);
	}

	//returns true if the reference is idempotent
	__forceinline bool GetIsIdempotent()
	{
		if(value.nodeType != ENIVT_CODE)
			return true;

		if(value.nodeValue.code == nullptr)
			return true;

		return value.nodeValue.code->GetIsIdempotent();
	}

	//sets idempotency if the reference is code and not nullptr
	__forceinline void SetIsIdempotent(bool is_idempotent)
	{
		if(value.nodeType != ENIVT_CODE)
			return;

		if(value.nodeValue.code == nullptr)
			return;

		return value.nodeValue.code->SetIsIdempotent(is_idempotent);
	}

	__forceinline static EvaluableNodeReference Null()
	{
		return EvaluableNodeReference(nullptr, true);
	}

	__forceinline void SetReference(EvaluableNode *_reference)
	{
		value = _reference;
	}

	__forceinline void SetReference(EvaluableNode *_reference, bool _unique)
	{
		value = _reference;
		unique = _unique;
	}

	//returns true if it is an immediate value stored in this EvaluableNodeReference
	__forceinline bool IsImmediateValue()
	{
		return (value.nodeType != ENIVT_CODE);
	}

	//returns true if the type of whatever is stored is an immediate type
	__forceinline bool IsImmediateValueType()
	{
		if(value.nodeType != ENIVT_CODE)
			return true;

		return (value.nodeValue.code == nullptr || value.nodeValue.code->IsImmediate());
	}

	__forceinline bool IsNonNullNodeReference()
	{
		return (value.nodeType == ENIVT_CODE && value.nodeValue.code != nullptr);
	}

	__forceinline EvaluableNodeImmediateValueWithType &GetValue()
	{
		return value;
	}

	__forceinline EvaluableNode *&GetReference()
	{
		return value.nodeValue.code;
	}

	//allow to use as an EvaluableNode *
	__forceinline operator EvaluableNode *&()
	{	return value.nodeValue.code;	}

	//allow to use as an EvaluableNode *
	__forceinline EvaluableNode *operator->()
	{	return value.nodeValue.code;	}

	__forceinline EvaluableNodeReference &operator =(const EvaluableNodeReference &enr)
	{
		//perform a memcpy because it's a union, to be safe; the compiler should optimize this out
		value = enr.value;
		unique = enr.unique;

		return *this;
	}

protected:

	EvaluableNodeImmediateValueWithType value;

public:

	//this is the only reference to the result
	bool unique;
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

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(initial_element == nullptr || !initial_element->IsNodeDeallocated());
	#endif

		stack->push_back(initial_element);
	}

	__forceinline ~EvaluableNodeStackStateSaver()
	{
		stack->resize(originalStackSize);
	}

	__forceinline void PushEvaluableNode(EvaluableNode *n)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(n == nullptr || !n->IsNodeDeallocated());
	#endif
		stack->push_back(n);
	}

	__forceinline void PopEvaluableNode()
	{
		stack->pop_back();
	}

	//returns the offset to the location of the current top of the stack
	__forceinline size_t GetLocationOfCurrentStackTop()
	{
		return stack->size() - 1;
	}

	//replaces the position of the stack with new_value
	__forceinline void SetStackLocation(size_t location, EvaluableNode *new_value)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(new_value == nullptr || !new_value->IsNodeDeallocated());
	#endif
		(*stack)[location] = new_value;
	}

	std::vector<EvaluableNode *> *stack;
	size_t originalStackSize;
};

class EvaluableNodeManager
{
public:
	//data structure to store which nodes are referenced with a lock
	struct NodesReferenced
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleMutex mutex;
	#endif

		//adds the node to nodes referenced
		inline void KeepNodeReference(EvaluableNode *en)
		{
			if(en == nullptr)
				return;

			//attempt to put in value 1 for the reference
			auto [inserted_entry, inserted] = nodesReferenced.insert(std::make_pair(en, 1));

			//if couldn't insert because already referenced, then increment
			if(!inserted)
				inserted_entry->second++;
		}

		//removes the node from nodes referenced
		inline void FreeNodeReference(EvaluableNode *en)
		{
			if(en == nullptr)
				return;

			//get reference count
			auto node = nodesReferenced.find(en);

			//don't do anything if not counted
			if(node == nodesReferenced.end())
				return;

			//if it has sufficient refcount, then just decrement
			if(node->second > 1)
				node->second--;
			else //otherwise remove reference
				nodesReferenced.erase(node);
		}

		EvaluableNode::ReferenceCountType nodesReferenced;
	};

	EvaluableNodeManager() :
		numNodesToRunGarbageCollection(200), firstUnusedNodeIndex(0)
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

	inline EvaluableNode *AllocNode(const std::string &string_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(ENT_STRING, string_value);
		return n;
	}

	inline EvaluableNode *AllocNode(EvaluableNodeType type, StringInternPool::StringID string_id)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(type, string_id);
		return n;
	}

	inline EvaluableNode *AllocNode(StringInternPool::StringID string_id)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(ENT_STRING, string_id);
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

	inline EvaluableNode *AllocNode(bool bool_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(bool_value ? ENT_TRUE : ENT_FALSE);
		return n;
	}

	inline EvaluableNode *AllocNode(EvaluableNodeType type)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(type);
		return n;
	}

	inline EvaluableNode *AllocNode(std::vector<EvaluableNode *> &child_nodes,
		bool need_cycle_check = true, bool is_idempotent = false)
	{
		EvaluableNode *n = AllocNode(ENT_LIST);
		n->SetOrderedChildNodes(child_nodes, need_cycle_check, is_idempotent);
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
		//if not cyclic, can attempt to free all child nodes
		//if cyclic, don't try, in case a child node points back to candidate
		if(candidate.unique && candidate != nullptr && !candidate->GetNeedCycleCheck())
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

			candidate->ClearAndSetType(type);
			return candidate;
		}
		else
		{
			return EvaluableNodeReference(AllocNode(type), true);
		}
	}

	//like ReuseOrAllocNode but allocates an ENT_NUMBER
	inline EvaluableNodeReference ReuseOrAllocNode(EvaluableNodeReference candidate, double value)
	{
		EvaluableNodeReference node = ReuseOrAllocNode(candidate, ENT_NUMBER);
		node->SetNumberValue(value);
		return node;
	}

	//like ReuseOrAllocNode but allocates an ENT_STRING
	inline EvaluableNodeReference ReuseOrAllocNode(EvaluableNodeReference candidate, StringInternPool::StringID value)
	{
		//perform a handoff in case candidate is the only value
		string_intern_pool.CreateStringReference(value);
		EvaluableNodeReference node = ReuseOrAllocNode(candidate, ENT_STRING);
		node->SetStringIDWithReferenceHandoff(value);
		return node;
	}

	//like ReuseOrAllocNode but allocates an ENT_STRING
	inline EvaluableNodeReference ReuseOrAllocNode(EvaluableNodeReference candidate, const std::string &value)
	{
		EvaluableNodeReference node = ReuseOrAllocNode(candidate, ENT_STRING);
		node->SetStringValue(value);
		return node;
	}

	//like ReuseOrAllocNode but allocates either ENT_TRUE or ENT_FALSE
	inline EvaluableNodeReference ReuseOrAllocNode(EvaluableNodeReference candidate, bool value)
	{
		return ReuseOrAllocNode(candidate, value ? ENT_TRUE: ENT_FALSE);
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

	//like ReuseOrAllocOneOfNodes but allocates an ENT_NUMBER
	inline EvaluableNodeReference ReuseOrAllocOneOfNodes(
		EvaluableNodeReference candidate_1, EvaluableNodeReference candidate_2, double value)
	{
		EvaluableNodeReference node = ReuseOrAllocOneOfNodes(candidate_1, candidate_2, ENT_NUMBER);
		node->SetNumberValue(value);
		return node;
	}

	//like ReuseOrAllocOneOfNodes but allocates an ENT_STRING
	inline EvaluableNodeReference ReuseOrAllocOneOfNodes(
		EvaluableNodeReference candidate_1, EvaluableNodeReference candidate_2, StringInternPool::StringID value)
	{
		//perform a handoff in case one of the candidates is the only value
		string_intern_pool.CreateStringReference(value);
		EvaluableNodeReference node = ReuseOrAllocOneOfNodes(candidate_1, candidate_2, ENT_STRING);
		node->SetStringIDWithReferenceHandoff(value);
		return node;
	}

	//like ReuseOrAllocOneOfNodes but allocates an ENT_STRING
	inline EvaluableNodeReference ReuseOrAllocOneOfNodes(
		EvaluableNodeReference candidate_1, EvaluableNodeReference candidate_2, const std::string &value)
	{
		EvaluableNodeReference node = ReuseOrAllocOneOfNodes(candidate_1, candidate_2, ENT_STRING);
		node->SetStringValue(value);
		return node;
	}

	//like ReuseOrAllocOneOfNodes but allocates either ENT_TRUE or ENT_FALSE
	inline EvaluableNodeReference ReuseOrAllocOneOfNodes(
		EvaluableNodeReference candidate_1, EvaluableNodeReference candidate_2, bool value)
	{
		return ReuseOrAllocOneOfNodes(candidate_1, candidate_2, value ? ENT_TRUE : ENT_FALSE);
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

		nodeToParentNodeCache.clear();
		UpdateFlagsForNodeTreeRecurse(tree, nullptr, nodeToParentNodeCache);
	}

	//heuristic used to determine whether unused memory should be collected (e.g., by FreeAllNodesExcept*)
	//force this inline because it occurs in inner loops
	__forceinline bool RecommendGarbageCollection()
	{
		//makes sure to perform garbage collection between every opcode to find memory reference errors
	#ifdef PEDANTIC_GARBAGE_COLLECTION
		return true;
	#endif

		auto cur_size = GetNumberOfUsedNodes();
		return (cur_size >= numNodesToRunGarbageCollection);
	}

	//updates the memory threshold when garbage collection will be next called
	void UpdateGarbageCollectionTrigger(size_t previous_num_nodes = 0);

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
		if(enr.IsImmediateValue())
			enr.FreeImmediateResources();

		if(enr.unique && enr != nullptr && !enr->GetNeedCycleCheck())
		{
			enr->Invalidate();
			ReclaimFreedNodesAtEnd();
		}
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
		if(enr.IsImmediateValue())
			enr.FreeImmediateResources();
		else if(enr.unique)
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

	//retuns the nodes currently referenced, allocating if they don't exist
	NodesReferenced &GetNodesReferenced()
	{
		if(nodesCurrentlyReferenced.get() == nullptr)
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::WriteLock write_lock(managerAttributesMutex);

			//double check that it's still nullptr in case another thread created it
			if(nodesCurrentlyReferenced.get() == nullptr)
		#endif
				nodesCurrentlyReferenced = std::make_unique<NodesReferenced>();
		}

		return *nodesCurrentlyReferenced.get();
	}

	//adds the node to nodes referenced
	//if called within multithreading, GetNodeReferenceUpdateLock() needs to be called
	//to obtain a lock around all calls to this methed
	template<typename ...EvaluableNodeReferenceType>
	inline void KeepNodeReferences(EvaluableNodeReferenceType... nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : { nodes... })
			nr.KeepNodeReference(en);
	}

	//removes the node from nodes referenced
	//if called within multithreading, GetNodeReferenceUpdateLock() needs to be called
	//to obtain a lock around all calls to this methed
	template<typename ...EvaluableNodeReferenceType>
	void FreeNodeReferences(EvaluableNodeReferenceType... nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : { nodes... })
			nr.FreeNodeReference(en);
	}

	//removes the node from nodes referenced
	//if called within multithreading, GetNodeReferenceUpdateLock() needs to be called
	//to obtain a lock around all calls to this methed
	template<typename NodeType>
	void FreeNodeReferences(std::vector<NodeType> &nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : nodes)
			nr.FreeNodeReference(en);
	}

	//compacts allocated nodes so that the node pool can be used more efficiently
	// and can improve reuse without calling the more expensive FreeAllNodesExceptReferencedNodes
	void CompactAllocatedNodes();

	//allows freed nodes at the end of nodes to be reallocated
	inline void ReclaimFreedNodesAtEnd()
	{
	#ifdef MULTITHREAD_SUPPORT
		//this is much more expensive with multithreading, so do less often
		//use the lower bits of firstUnusedNodeIndex being zero as a fast pseudorandom process
		//given that many opcodes allocate varied number of nodes,
		//it isn't too likely to get stuck around one of these values
		if((firstUnusedNodeIndex & 511) != 0)
			return;

		//be opportunistic and only attempt to reclaim if it can grab a write lock
		Concurrency::WriteLock write_lock(managerAttributesMutex, std::defer_lock);
		if(!write_lock.try_lock())
			return;
	#endif

		//if any group of nodes on the top are ready to be cleaned up cheaply, do so
		while(firstUnusedNodeIndex > 0 && nodes[firstUnusedNodeIndex - 1] != nullptr
				&& nodes[firstUnusedNodeIndex - 1]->IsNodeDeallocated())
			firstUnusedNodeIndex--;
	}

	//returns the number of nodes currently being used that have not been freed yet
	__forceinline size_t GetNumberOfUsedNodes()
	{	return firstUnusedNodeIndex;		}

	__forceinline size_t GetNumberOfUnusedNodes()
	{	return nodes.size() - firstUnusedNodeIndex;		}

	__forceinline size_t GetNumberOfNodesReferenced()
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif

		return nr.nodesReferenced.size();
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
	//ensures that the new root node is kept and the old is released
	inline void SetRootNode(EvaluableNode *new_root)
	{
	#ifdef MULTITHREAD_SUPPORT
		//use WriteLock to be safe
		Concurrency::WriteLock lock(managerAttributesMutex);
	#endif

		//iteratively search forward; this will be fast for newly created entities but potentially slow for those that are not
		// however, this should be rarely called on those entities since it's basically clearing them out, so it should not generally be a performance issue
		auto location = std::find(begin(nodes), begin(nodes) + firstUnusedNodeIndex, new_root);

		//put the new root in the proper place
		if(location != end(nodes))
			std::swap(*begin(nodes), *location);
	}

	//returns true if any node is referenced other than root, which is an indication if there are
	// any interpreters operating on the nodes managed by this instance
	inline bool IsAnyNodeReferencedOtherThanRoot()
	{
		NodesReferenced &nr = GetNodesReferenced();

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::SingleLock lock(nr.mutex);
	#endif

		size_t num_nodes_currently_referenced = nr.nodesReferenced.size();
		return (num_nodes_currently_referenced > 0);
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
	// if ensure_nodes_in_enm is passed in, it will ensure all nodes are contained within this EvaluableNodeManager
	//asserts an error if it finds any
	//intended for debugging only
	static void ValidateEvaluableNodeTreeMemoryIntegrity(EvaluableNode *en,
		EvaluableNodeManager *ensure_nodes_in_enm = nullptr);

	//when numNodesToRunGarbageCollection are allocated, then it is time to run garbage collection
	size_t numNodesToRunGarbageCollection;

protected:
	//allocates an EvaluableNode of the respective memory type in the appropriate way
	// returns an uninitialized EvaluableNode -- care must be taken to set fields properly
	EvaluableNode *AllocUninitializedNode();

	//frees everything execpt those nodes referenced by nodesCurrentlyReferenced
	//cur_first_unused_node_index represents the first unused index and will set firstUnusedNodeIndex
	//to the reduced value
	//note that this method does not read from firstUnusedNodeIndex, as it may be cleared to indicate threads
	//to stop spinlocks
	void FreeAllNodesExceptReferencedNodes(size_t cur_first_unused_node_index);

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

	//sets all referenced nodes that are in use as such
	void MarkAllReferencedNodesInUse(size_t estimated_nodes_in_use);

	//computes whether the code is cycle free and idempotent and updates all nodes appropriately
	// returns flags for whether cycle free and idempotent
	// requires tree not be nullptr; the first tree should have nullptr as parent
	static std::pair<bool, bool> UpdateFlagsForNodeTreeRecurse(EvaluableNode *tree, EvaluableNode *parent,
		EvaluableNode::ReferenceAssocType &checked_to_parent);

	//sets or clears all referenced nodes' in use flags
	//if set_in_use is true, then it will set the value, if false, it will clear the value
	//note that tree cannot be nullptr and it should already be inserted into the references prior to calling
	static void MarkAllReferencedNodesInUseRecurse(EvaluableNode *tree);

#ifdef MULTITHREAD_SUPPORT
	static void MarkAllReferencedNodesInUseRecurseConcurrent(EvaluableNode* tree);
#endif

	//helper method for ValidateEvaluableNodeTreeMemoryIntegrity
	//returns a tuple of whether it is cycle free and whether it is idempotent
	static std::pair<bool, bool> ValidateEvaluableNodeTreeMemoryIntegrityRecurse(EvaluableNode *en,
		EvaluableNode::ReferenceSetType &checked, FastHashSet<EvaluableNode *> *existing_nodes);

#ifdef MULTITHREAD_SUPPORT
	//mutex to manage attributes of manager, including operations such as
	// memory allocation, reference management, etc.
	Concurrency::ReadWriteMutex managerAttributesMutex;

	std::atomic<size_t> firstUnusedNodeIndex;

public:
	//global mutex to manage whether memory nodes are being modified
	//concurrent modifications can occur as long as there is only one unique thread
	// that has allocated the memory
	//garbage collection or destruction of the manager require a unique lock
	// so that the memory can be traversed
	//note that this is a global lock because nodes may be mixed among more than one
	// EvaluableNodeManager and so garbage collection should not happening while memory is being modified
	static Concurrency::ReadWriteMutex memoryModificationMutex;

protected:

#else
	size_t firstUnusedNodeIndex;
#endif

	//nodes that have been allocated and may be in use
	// all nodes in use are below firstUnusedNodeIndex, such that all above that index are free for use
	// nodes cannot be nullptr for lower indices than firstUnusedNodeIndex
	std::vector<EvaluableNode *> nodes;

	//keeps track of all of the nodes currently referenced by any resource or interpreter
	//only allocated if needed
	std::unique_ptr<NodesReferenced> nodesCurrentlyReferenced;

	//buffer used for updating EvaluableNodeFlags, particularly UpdateFlagsForNodeTree
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		static EvaluableNode::ReferenceAssocType nodeToParentNodeCache;


	//extra space to allocate when allocating
	static const double allocExpansionFactor;
};
