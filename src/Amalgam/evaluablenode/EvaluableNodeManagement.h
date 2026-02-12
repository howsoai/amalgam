#pragma once

//project headers:
#include "Concurrency.h"
#include "EvaluableNode.h"

//system headers:
#include <memory>

//if the macro PEDANTIC_GARBAGE_COLLECTION is defined, then garbage collection will be performed
//after every opcode, to help find and debug memory issues
//if the macro DEBUG_REPORT_LAB_USAGE is defined, then the local allocation buffer storage will be
//profiled and printed

typedef int64_t ExecutionCycleCount;
typedef int32_t ExecutionCycleCountCompactDelta;

//describes an EvaluableNode value and whether it is uniquely referenced
//this is mostly used for actual EvaluableNode *'s, and so most of the methods are built as such
//however, if it may contain an immediate value, then that must be checked via IsImmediateValue()
class EvaluableNodeReference
{
public:
	constexpr EvaluableNodeReference()
		: value(), unique(true), uniqueUnreferencedTopNode(true)
	{	}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique)
		: value(_reference), unique(_unique), uniqueUnreferencedTopNode(_unique)
	{	}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique, bool top_node_unique)
		: value(_reference), unique(_unique), uniqueUnreferencedTopNode(top_node_unique)
	{}

	constexpr EvaluableNodeReference(const EvaluableNodeReference &inr)
		: value(inr.value), unique(inr.unique), uniqueUnreferencedTopNode(inr.uniqueUnreferencedTopNode)
	{	}

	__forceinline EvaluableNodeReference(bool value)
		: value(value), unique(true), uniqueUnreferencedTopNode(true)
	{	}

	__forceinline EvaluableNodeReference(double value)
		: value(value), unique(true), uniqueUnreferencedTopNode(true)
	{	}

	//if reference_handoff is true, it will assume ownership rather than creating a new reference
	__forceinline EvaluableNodeReference(StringInternPool::StringID string_id, bool reference_handoff = false)
		: value(reference_handoff ? string_id : string_intern_pool.CreateStringReference(string_id)),
		unique(true), uniqueUnreferencedTopNode(true)
	{	}

	__forceinline EvaluableNodeReference(const std::string &str)
		: value(string_intern_pool.CreateStringReference(str)), unique(true), uniqueUnreferencedTopNode(true)
	{	}

	__forceinline EvaluableNodeReference(std::string_view str)
		: value(string_intern_pool.CreateStringReference(str)), unique(true), uniqueUnreferencedTopNode(true)
	{}

	//constructs an EvaluableNodeReference with an immediate type and true if possible to coerce node
	//into one of the immediate request types, or returns a non-unique EvaluableNodeReference and false if not
	static inline EvaluableNodeReference CoerceNonUniqueEvaluableNodeToImmediateIfPossible(EvaluableNode *en,
		EvaluableNodeRequestedValueTypes immediate_result)
	{
		if(en == nullptr)
			return EvaluableNodeReference::Null();

		if(immediate_result.AnyImmediateType())
		{
			//first check for null, since it's not an immediate value
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NULL_VALUE))
			{
				if(en->GetType() == ENT_NULL)
					return EvaluableNodeReference::Null();
			}

			if(en->IsImmediate())
			{
				//first check for key strings
				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::EXISTING_KEY_STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDIfExists(en, true));

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::KEY_STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDWithReference(en, true), true);

				//if type matches the usable return type, then return that, otherwise fall back to returning
				//the node as the caller will know the most appropriate type change to apply
				auto type = en->GetType();
				if(type == ENT_BOOL)
				{
					if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::BOOL))
						return EvaluableNodeReference(en->GetBoolValueReference());
				}
				else if(type == ENT_NUMBER)
				{
					if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NUMBER))
						return EvaluableNodeReference(en->GetNumberValueReference());
				}
				else if(type == ENT_STRING)
				{
					if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::EXISTING_STRING_ID)
							|| immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::STRING_ID))
						return EvaluableNodeReference(en->GetStringIDReference());
				}
			}
		}

		return EvaluableNodeReference(en, false);
	}

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
	//if first_attachment_and_not_construction_stack_node is true, then it will not call SetNeedCycleCheck(true)
	// unless the attached node also needs cycle check.  Note that this parameter should not be set to true
	//if the node can be accessed in any other way, such as the construction stack
	void UpdatePropertiesBasedOnAttachedNode(EvaluableNodeReference &attached,
		bool first_attachment_and_not_construction_stack_node = false)
	{
		if(attached.value.nodeValue.code == nullptr)
			return;

		if(!attached.unique)
		{
			unique = false;

			//first attachment doesn't need to automatically require a cycle check
			if(first_attachment_and_not_construction_stack_node)
			{
				if(attached.value.nodeValue.code->GetNeedCycleCheck())
					value.nodeValue.code->SetNeedCycleCheck(true);
			}
			else //if new attachments aren't unique, then can't guarantee there isn't a cycle present
			{
				value.nodeValue.code->SetNeedCycleCheck(true);
			}
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
		return EvaluableNodeReference(static_cast<EvaluableNode *>(nullptr), true);
	}

	__forceinline void SetReference(EvaluableNode *_reference)
	{
		value = _reference;
	}

	__forceinline void SetReference(EvaluableNode *_reference, bool _unique)
	{
		value = _reference;
		unique = _unique;
		uniqueUnreferencedTopNode = _unique;
	}

	__forceinline void SetReference(EvaluableNode *_reference,
		bool _unique, bool unique_unreferenced_top_node)
	{
		value = _reference;
		unique = _unique;
		uniqueUnreferencedTopNode = unique_unreferenced_top_node;
	}

	__forceinline void SetReference(const EvaluableNodeImmediateValueWithType &enimvwt, bool _unique)
	{
		value = enimvwt;
		unique = _unique;
		uniqueUnreferencedTopNode = _unique;
	}

	__forceinline void SetReference(const EvaluableNodeImmediateValueWithType &enimvwt,
		bool _unique, bool unique_unreferenced_top_node)
	{
		value = enimvwt;
		unique = _unique;
		uniqueUnreferencedTopNode = unique_unreferenced_top_node;
	}

	//returns true if it is an immediate value stored in this EvaluableNodeReference
	__forceinline bool IsImmediateValue()
	{
		return (value.nodeType != ENIVT_CODE || value.nodeValue.code == nullptr);
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
		uniqueUnreferencedTopNode = enr.uniqueUnreferencedTopNode;
		return *this;
	}

protected:
	//align so the entire data structure takes up 16 bytes
#pragma pack(push, 4)
	EvaluableNodeImmediateValueWithType value;

public:

	//true if this is the only reference to the result
	bool unique;

	//true if this is the only reference to the top node, including no child nodes referencing it
	bool uniqueUnreferencedTopNode;
#pragma pack(pop)
};


//Uses an EvaluableNode as a stack which may already have elements in it
// upon destruction it restores the stack back to the state it was when constructed
class EvaluableNodeStackStateSaver
{
public:
	inline EvaluableNodeStackStateSaver()
		: stack(nullptr), originalStackSize(0)
	{	}

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
		assert(initial_element == nullptr || initial_element->IsNodeValid());
	#endif

		stack->push_back(initial_element);
	}

	__forceinline ~EvaluableNodeStackStateSaver()
	{
		stack->resize(originalStackSize);
	}

	//ensures that the stack is allocated to hold up to num_new_nodes
	__forceinline void ReserveNodes(size_t num_new_nodes)
	{
		stack->resize(stack->size() + num_new_nodes);
	}

	__forceinline void PushEvaluableNode(EvaluableNode *n)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(n == nullptr || n->IsNodeValid());
	#endif
		stack->push_back(n);
	}

	__forceinline void PopEvaluableNode()
	{
		stack->pop_back();
	}

	//returns the offset to the first element of this state saver
	__forceinline size_t GetIndexOfFirstElement()
	{
		return originalStackSize;
	}

	//returns the offset to the last element of this state saver
	__forceinline size_t GetIndexOfLastElement()
	{
		return stack->size();
	}

	//returns the corresponding element
	__forceinline EvaluableNode *GetStackElement(size_t location)
	{
		return (*stack)[location];
	}

	//replaces the position of the stack with new_value
	__forceinline void SetStackElement(size_t location, EvaluableNode *new_value)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(new_value == nullptr || new_value->IsNodeValid());
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
			auto [inserted_entry, inserted] = nodesReferenced.emplace(en, 1);

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

	//holds pointers to EvaluableNode's reserved for allocation by a specific thread
	//during garbage collection, these buffers need to be cleared because memory may be rearranged or reassigned
	//this also means that garbage collection processes may reuse this buffer as long as it is cleared
	class LocalAllocationBuffer
	{
	public:
		LocalAllocationBuffer()
			: lastEvaluableNodeManager(nullptr)
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::Lock lock(registryMutex);
			registry.push_back(this);
		#endif
		}

		~LocalAllocationBuffer()
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::Lock lock(registryMutex);

			auto it = std::find(registry.begin(), registry.end(), this);
			if(it != registry.end())
				registry.erase(it);
		#endif
		}

		//removes all EvaluableNodes from the local allocation buffer, leaving it empty
		//it will clear if only_clear_if_current_enm is nullptr or if
		// lastEvaluableNodeManager is the same as only_clear_if_current_enm
		inline void Clear(EvaluableNodeManager *only_clear_if_current_enm = nullptr)
		{
			if(only_clear_if_current_enm != nullptr && only_clear_if_current_enm != lastEvaluableNodeManager)
				return;

			buffer.clear();
			//set to null so nothing matches until more nodes are added
			lastEvaluableNodeManager = nullptr;
		}

		//gets a pointer to the next available node from the local allocation buffer
		//nullptr if it cannot
		inline EvaluableNode *GetNode(EvaluableNodeManager *cur_enm)
		{
			if(buffer.size() > 0 && cur_enm == lastEvaluableNodeManager)
			{
				EvaluableNode *node = buffer.back();
				buffer.pop_back();
				return node;
			}
			else //local allocation buffer is empty or irrelevant, clear so nothing matches until more nodes are added
			{
				buffer.clear();
				lastEvaluableNodeManager = nullptr;
				return nullptr;
			}
		}

		//adds a node to the local allocation buffer
		//if this is accessed by a different EvaluableNode manager than the last time it was called on this thread,
		// it will clear the buffer before adding the node
		inline void AddNode(EvaluableNode *en, EvaluableNodeManager *cur_enm)
		{
			if(cur_enm != lastEvaluableNodeManager)
			{
				buffer.clear();
				lastEvaluableNodeManager = cur_enm;
			}

			buffer.push_back(en);
		}

	#ifdef MULTITHREAD_SUPPORT
		//calls func on all registered local allocation buffers for each thread
		template<typename Func>
		static inline void IterateFunctionOverRegisteredLabs(Func func)
		{
			Concurrency::Lock lock(registryMutex);
			for(auto lab : LocalAllocationBuffer::registry)
				func(lab);
		}
	#endif

		// Keeps track of the the last EvaluableNodeManager that accessed 
		// the local allocation buffer for a each thread.
		// A given local allocation buffer should only have nodes associated with one manager.
		// If a different manager accesses the buffer, it is cleared to maintain this invariant.
		EvaluableNodeManager *lastEvaluableNodeManager;

		//the buffer itself
		std::vector<EvaluableNode *> buffer;

	protected:

	#ifdef MULTITHREAD_SUPPORT
		//registry that keeps track of all local allocation buffers
		static inline std::vector<LocalAllocationBuffer *> registry;
		static inline Concurrency::SingleMutex registryMutex;
	#endif
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

	inline EvaluableNode *AllocNode(EvaluableNodeType type, StringRef &sir)
	{	return AllocNode(type, static_cast<StringInternPool::StringID>(sir));	}

	inline EvaluableNode *AllocNode(bool bool_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(bool_value);
		return n;
	}

	inline EvaluableNode *AllocNode(double float_value)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(float_value);
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

	template<class Iterator>
	inline EvaluableNode *AllocNode(Iterator first, Iterator last,
		bool need_cycle_check = true, bool is_idempotent = false)
	{
		EvaluableNode *n = AllocNode(ENT_LIST);
		n->SetNeedCycleCheck(need_cycle_check);
		n->SetIsIdempotent(is_idempotent);

		auto &ocn = n->GetOrderedChildNodesReference();
		ocn = std::vector<EvaluableNode *>(first, last);
		return n;
	}

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
	//if ensure_copy_if_cycles, then it will also allocate a new node if there are cycles,
	//in case the top node is referenced by any of its node tree and it needs to ensure that structure is maintained
	inline void EnsureNodeIsModifiable(EvaluableNodeReference &original, bool ensure_copy_if_cycles = false,
		EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE)
	{
		if(original.uniqueUnreferencedTopNode && original != nullptr
				&& (!ensure_copy_if_cycles || !original.GetNeedCycleCheck()) )
			return;

		EvaluableNode *copy = AllocNode(original.GetReference(), metadata_modifier);
		//the copy will only be unique if there are no child nodes
		original = EvaluableNodeReference(copy, (copy->GetNumChildNodes() == 0), true);
	}

	//returns an EvaluableNodeReference for value, allocating if necessary based on if immediate result is needed
	template<typename T>
	__forceinline EvaluableNodeReference AllocIfNotImmediate(T value, EvaluableNodeRequestedValueTypes immediate_result)
	{
		if(immediate_result.AnyImmediateType())
			return EvaluableNodeReference(value);
		return EvaluableNodeReference(AllocNode(value), true);
	}

	//Copies the data structure and everything underneath it, modifying labels as specified
	EvaluableNodeReference DeepAllocCopy(EvaluableNode *tree, EvaluableNodeMetadataModifier metadata_modifier = ENMM_NO_CHANGE);

	//used to hold all of the references for DeepAllocCopy calls
	struct DeepAllocCopyParams
	{
		inline DeepAllocCopyParams(EvaluableNodeMetadataModifier metadata_modifier)
			: labelModifier(metadata_modifier)
		{	}

		EvaluableNode::ReferenceAssocType references;
		EvaluableNodeMetadataModifier labelModifier;
	};

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

		EvaluableNode::ReferenceAssocType node_to_parent_cache;
		UpdateFlagsForNodeTreeRecurse(tree, nullptr, node_to_parent_cache);
	}

	//updates idempotency flags for tree and returns true if tree is idempotent
	//assumes there are no cycles
	static bool UpdateIdempotencyFlagsForNonCyclicNodeTree(EvaluableNode *tree)
	{
		bool is_idempotent = (IsEvaluableNodeTypePotentiallyIdempotent(tree->GetType()) && (tree->GetNumLabels() == 0));

		if(tree->IsAssociativeArray())
		{
			for(auto &[cn_id, cn] : tree->GetMappedChildNodesReference())
			{
				if(cn == nullptr)
					continue;

				bool cn_is_idempotent = UpdateIdempotencyFlagsForNonCyclicNodeTree(cn);

				if(!cn_is_idempotent)
					is_idempotent = false;
			}

			tree->SetIsIdempotent(is_idempotent);
			return is_idempotent;
		}
		else if(!tree->IsImmediate())
		{
			for(auto cn : tree->GetOrderedChildNodesReference())
			{
				if(cn == nullptr)
					continue;

				auto cn_is_idempotent = UpdateIdempotencyFlagsForNonCyclicNodeTree(cn);

				if(!cn_is_idempotent)
					is_idempotent = false;
			}

			tree->SetIsIdempotent(is_idempotent);
			return is_idempotent;
		}
		else //immediate value
		{
			tree->SetIsIdempotent(is_idempotent);
			return is_idempotent;
		}
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

	//changes the garbage collection trigger so that the next call to RecommendGarbageCollection will be true
	inline void UpdateGarbageCollectionTriggerForImmediateCollection()
	{
		numNodesToRunGarbageCollection = GetNumberOfUsedNodes();
	}

	//collects garbage
	void CollectGarbage();

#ifdef MULTITHREAD_SUPPORT
	//if multithreaded, then memory_modification_lock is the lock used for memoryModificationMutex if not nullptr
	void CollectGarbageWithConcurrentAccess(Concurrency::ReadLock *memory_modification_lock);
#endif

	//frees any extra EvaluableNodes and shrinks memory to be appropriate for current use
	inline void ShrinkMemoryToCurrentUtilization()
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock write_lock(managerAttributesMutex);
	#endif

		for(size_t i = firstUnusedNodeIndex + 1; i < nodes.size(); i++)
		{
			//break at first empty slot
			if(nodes[i] == nullptr)
				break;

			delete nodes[i];
			nodes[i] = nullptr;
		}

		size_t new_size = std::min(nodes.size(), static_cast<size_t>(firstUnusedNodeIndex * allocExpansionFactor));
		nodes.resize(new_size);
		nodes.shrink_to_fit();
	}

	//frees an EvaluableNode (must be owned by this EvaluableNodeManager)
	// if place_nodes_in_lab is true, then it will update the local allocation buffer and place nodes in it
	inline void FreeNode(EvaluableNode *en, bool place_nodes_in_lab = true)
	{
		if(en == nullptr)
			return;

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(en->IsNodeValid());
	#endif

		en->Invalidate();
		if(place_nodes_in_lab)
			AddNodeToLocalAllocationBuffer(en);
	}

	//attempts to free the node reference
	__forceinline void FreeNodeIfPossible(EvaluableNodeReference &enr)
	{
		if(enr.IsImmediateValue())
		{
			enr.FreeImmediateResources();
			return;
		}

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(enr == nullptr || enr->IsNodeValid());
	#endif

		if( (enr.unique || enr.uniqueUnreferencedTopNode)
			&& enr != nullptr && !enr->GetNeedCycleCheck())
		{
			enr->Invalidate();
			AddNodeToLocalAllocationBuffer(enr);
		}
	}

	//frees all nodes
	void FreeAllNodes();

	//frees the entire tree in the respective ways for the corresponding permanence types allowed
	// if place_nodes_in_lab is true, then it will update the local allocation buffer and place nodes in it
	inline void FreeNodeTree(EvaluableNode *en, bool place_nodes_in_lab = true)
	{
		if(en == nullptr)
			return;

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(en->IsNodeValid());
	#endif

		if(IsEvaluableNodeTypeImmediate(en->GetType()))
		{
			en->Invalidate();
			if(place_nodes_in_lab)
				AddNodeToLocalAllocationBuffer(en);
		}
		else if(!en->GetNeedCycleCheck())
		{
			nodeMarkBuffer.clear();
			if(en != nullptr)
				nodeMarkBuffer.push_back(en);

			//perform depth-first traversal
			while(!nodeMarkBuffer.empty())
			{
				EvaluableNode *cur = nodeMarkBuffer.back();
				nodeMarkBuffer.pop_back();

			#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
				assert(cur->IsNodeValid());
				assert(!cur->GetNeedCycleCheck());
			#endif

				if(cur->IsAssociativeArray())
				{
					for(auto &[_, child] : cur->GetMappedChildNodesReference())
					{
						if(child != nullptr)
							nodeMarkBuffer.push_back(child);
					}
				}
				else if(!cur->IsImmediate())
				{
					for(auto &child : cur->GetOrderedChildNodesReference())
					{
						if(child != nullptr)
							nodeMarkBuffer.push_back(child);
					}
				}

				cur->Invalidate();
				if(place_nodes_in_lab)
					AddNodeToLocalAllocationBuffer(cur);
			}
		}
		else //more costly cyclic free
		{
			nodeMarkBuffer.clear();
			if(en != nullptr)
				nodeMarkBuffer.push_back(en);

		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			assert(en->IsNodeValid());
		#endif

			//perform depth-first traversal
			while(!nodeMarkBuffer.empty())
			{
				EvaluableNode *cur = nodeMarkBuffer.back();
				nodeMarkBuffer.pop_back();

				if(cur->IsNodeDeallocated())
					continue;

				if(cur->IsAssociativeArray())
				{
					for(auto &[_, e] : cur->GetMappedChildNodesReference())
					{
						if(e != nullptr && !e->IsNodeDeallocated())
							nodeMarkBuffer.push_back(e);
					}
				}
				else if(!cur->IsImmediate())
				{
					for(auto &e : cur->GetOrderedChildNodesReference())
					{
						if(e != nullptr && !e->IsNodeDeallocated())
							nodeMarkBuffer.push_back(e);
					}
				}

				cur->Invalidate();
				if(place_nodes_in_lab)
					AddNodeToLocalAllocationBuffer(cur);
			}
		}
	}

	//attempts to free the node reference
	// if place_nodes_in_lab is true, then it will update the local allocation buffer and place nodes in it
	__forceinline void FreeNodeTreeIfPossible(EvaluableNodeReference &enr, bool place_nodes_in_lab = true)
	{
		if(enr.IsImmediateValue())
			enr.FreeImmediateResources();
		else if(enr.unique)
			FreeNodeTree(enr, place_nodes_in_lab);
		else if(enr.uniqueUnreferencedTopNode)
			FreeNode(enr, place_nodes_in_lab);
	}

	//just frees the child nodes of tree, but not tree itself; assumes no cycles
	inline void FreeNodeChildNodes(EvaluableNode *tree)
	{
		nodeMarkBuffer.clear();

		// Seed the buffer with the direct children of *tree*.
		if(tree->IsAssociativeArray())
		{
			for(auto &[_, e] : tree->GetMappedChildNodesReference())
			{
				if(e != nullptr)
					nodeMarkBuffer.push_back(e);
			}
		}
		else if(tree->IsOrderedArray())
		{
			for(auto &e : tree->GetOrderedChildNodesReference())
			{
				if(e != nullptr)
					nodeMarkBuffer.push_back(e);
			}
		}

		//perform depth-first traversal
		while(!nodeMarkBuffer.empty())
		{
			EvaluableNode *cur = nodeMarkBuffer.back();
			nodeMarkBuffer.pop_back();

		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			assert(cur->IsNodeValid());
			assert(!cur->GetNeedCycleCheck());
		#endif

			if(cur->IsAssociativeArray())
			{
				for(auto &[_, child] : cur->GetMappedChildNodesReference())
				{
					if(child != nullptr)
						nodeMarkBuffer.push_back(child);
				}
			}
			else if(!cur->IsImmediate())
			{
				for(auto &child : cur->GetOrderedChildNodesReference())
				{
					if(child != nullptr)
						nodeMarkBuffer.push_back(child);
				}
			}

			cur->Invalidate();
			AddNodeToLocalAllocationBuffer(cur);
		}
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
	//to obtain a lock around all calls to this method
	template<typename ...EvaluableNodeReferenceType>
	inline void KeepNodeReferences(EvaluableNodeReferenceType... nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : { nodes... })
			nr.KeepNodeReference(en);
	}

	//removes the node from nodes referenced
	//if called within multithreading, GetNodeReferenceUpdateLock() needs to be called
	//to obtain a lock around all calls to this method
	template<typename ...EvaluableNodeReferenceType>
	void FreeNodeReferences(EvaluableNodeReferenceType... nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : { nodes... })
			nr.FreeNodeReference(en);
	}

	//removes the node from nodes referenced
	//if called within multithreading, GetNodeReferenceUpdateLock() needs to be called
	//to obtain a lock around all calls to this method
	template<typename NodeType>
	void FreeNodeReferences(std::vector<NodeType> &nodes)
	{
		NodesReferenced &nr = GetNodesReferenced();
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(nr.mutex);
	#endif

		for(EvaluableNode *en : nodes)
			nr.FreeNodeReference(en);
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
		Concurrency::Lock lock(nr.mutex);
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
	//if new_root is nullptr, then it allocates its own ENT_NULL node
	inline void SetRootNode(EvaluableNode *new_root)
	{
		if(new_root == nullptr)
			new_root = AllocNode(ENT_NULL);

	#ifdef MULTITHREAD_SUPPORT
		//use WriteLock to be safe
		Concurrency::WriteLock lock(managerAttributesMutex);
	#endif

		//iteratively search forward; this will be fast for newly created entities but potentially slow for those that are not
		// however, this should be rarely called on those entities since it's basically clearing them out, so it should not generally be a performance issue
		auto location = std::find(begin(nodes), begin(nodes) + firstUnusedNodeIndex, new_root);

		if(location == end(nodes))
		{
			assert(false);
			return;
		}

		//put the new root in the proper place
		std::swap(*begin(nodes), *location);
	}

	//returns true if any node is referenced other than root, which is an indication if there are
	// any interpreters operating on the nodes managed by this instance
	inline bool IsAnyNodeReferencedOtherThanRoot()
	{
		NodesReferenced &nr = GetNodesReferenced();

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(nr.mutex);
	#endif

		size_t num_nodes_currently_referenced = nr.nodesReferenced.size();
		return (num_nodes_currently_referenced > 0);
	}

	//returns all nodes still in use.  For debugging purposes
	std::vector<EvaluableNode *> GetUsedNodes()
	{	return std::vector<EvaluableNode *>(begin(nodes), begin(nodes) + firstUnusedNodeIndex);	}

	//returns an estimate of the amount of memory allocated by the nodes managed
	// only an estimate because the platform's underlying memory management system may need to allocate additional
	// memory that cannot be easily accounted for
	size_t GetEstimatedTotalReservedSizeInBytes();
	size_t GetEstimatedTotalUsedSizeInBytes();

	//makes sure that the evaluable node and everything referenced by it has not been deallocated
	// if ensure_nodes_in_enm is passed in, it will ensure all nodes are contained within this EvaluableNodeManager
	// if check_cycle_flag_consistency is set, it will ensure that all cycle flags are consistent
	// note that for stacks that are not accessible, it may be acceptable to not have cycle flags be consistent for
	// data unreachable by execution
	//asserts an error if it finds any issues
	//intended for debugging only
	static void ValidateEvaluableNodeTreeMemoryIntegrity(EvaluableNode *en,
		EvaluableNodeManager *ensure_nodes_in_enm = nullptr, bool check_cycle_flag_consistency = true);

	//when numNodesToRunGarbageCollection are allocated, then it is time to run garbage collection
	size_t numNodesToRunGarbageCollection;

	//Uses an EvaluableNode as a stack which may already have elements in it
	// upon destruction it restores the stack back to the state it was when constructed
	class LocalAllocationBufferPause
	{
	public:
		//if initialized without params, just leave unpaused
		inline LocalAllocationBufferPause()
			: localAllocationBufferLocation(nullptr), lastEvaluableNodeManagerLocation(nullptr),
			paused(false), prevLastEvaluableNodeManager(nullptr)
		{	}

		inline LocalAllocationBufferPause(LocalAllocationBuffer &lab)
		{
			prevLocalAllocationBuffer.clear();
			std::swap(prevLocalAllocationBuffer, lab.buffer);
			localAllocationBufferLocation = &lab.buffer;
			prevLastEvaluableNodeManager = lab.lastEvaluableNodeManager;
			lastEvaluableNodeManagerLocation = &lab.lastEvaluableNodeManager;
			paused = true;
		}

		inline ~LocalAllocationBufferPause()
		{
			Resume();
		}

		inline void Resume()
		{
			if(!paused)
				return;

			std::swap(prevLocalAllocationBuffer, *localAllocationBufferLocation);
			(*lastEvaluableNodeManagerLocation) = prevLastEvaluableNodeManager;
			paused = false;
		}

	protected:
		//current pointers
		std::vector<EvaluableNode *> *localAllocationBufferLocation;
		EvaluableNodeManager **lastEvaluableNodeManagerLocation;

		//true if paused
		bool paused;

		//used to store the previous value for pausing
		EvaluableNodeManager *prevLastEvaluableNodeManager;

		//stores previous buffer
		//one per thread to reduce memory churn
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		thread_local
	#endif
			static inline std::vector<EvaluableNode *> prevLocalAllocationBuffer;
	};

	//pauses the thread allocation buffer for the duration of the lifetime of the
	//returned object; no garbage collection or execution should occur while it is paused
	//this is intended only for allocations for other entities
	inline LocalAllocationBufferPause PauseLocalAllocationBuffer()
	{
		return LocalAllocationBufferPause(localAllocationBuffer);
	}

protected:
	//allocates an EvaluableNode of the respective memory type in the appropriate way
	// returns an uninitialized EvaluableNode -- care must be taken to set fields properly
	EvaluableNode *AllocUninitializedNode();

	//frees everything except those nodes referenced by nodesCurrentlyReferenced
	//cur_first_unused_node_index represents the first unused index and will set firstUnusedNodeIndex
	//to the reduced value
	//note that this method does not read from firstUnusedNodeIndex, as it may be cleared to indicate threads
	//to stop spinlocks
	void FreeAllNodesExceptReferencedNodes(size_t cur_first_unused_node_index);

	//modifies the labels of n with regard to metadata_modifier
	// assumes n is not nullptr
	static void ModifyLabels(EvaluableNode *n, EvaluableNodeMetadataModifier metadata_modifier);

	//implemented as a recursive method because the extra complexity of an iterative implementation
	// is not worth the very small performance benefit
	//returns a pair of the copy and true if the copy needs cycle check
	//assumes tree is not nullptr
	std::pair<EvaluableNode *, bool> DeepAllocCopyRecurse(EvaluableNode *tree, DeepAllocCopyParams &dacp);

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
	static void MarkAllReferencedNodesInUse(EvaluableNode *tree);

#ifdef MULTITHREAD_SUPPORT
	static void MarkAllReferencedNodesInUseConcurrent(EvaluableNode *tree);
#endif

	//helper method for ValidateEvaluableNodeTreeMemoryIntegrity
	//returns a tuple of whether it is cycle free and whether it is idempotent
	static std::pair<bool, bool> ValidateEvaluableNodeTreeMemoryIntegrityRecurse(
		EvaluableNode *en, EvaluableNode::ReferenceSetType &checked,
		FastHashSet<EvaluableNode *> *existing_nodes, bool check_cycle_flag_consistency);

	//gets a pointer to the next available node from the local allocation buffer
	//nullptr if it cannot
	inline EvaluableNode *GetNextNodeFromLocalAllocationBuffer()
	{
		return localAllocationBuffer.GetNode(this);
	}

	//adds en to the local allocation buffer
	inline void AddNodeToLocalAllocationBuffer(EvaluableNode *en)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(en->IsNodeDeallocated());
	#endif
		localAllocationBuffer.AddNode(en, this);
	}

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
	inline static Concurrency::ReadWriteMutex memoryModificationMutex;

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

	//extra space to allocate when allocating
	static const double allocExpansionFactor;

	//number of nodes to allocate at once for the local allocation buffer
	static const int labBlockAllocationSize = 24;

	//local memory pool for allocations
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		inline static LocalAllocationBuffer localAllocationBuffer;

	//local memory pool for garbage collection
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		inline static std::vector<EvaluableNode *> nodeMarkBuffer;

	//debug diagnostic variables for localAllocationBuffer
#ifdef DEBUG_REPORT_LAB_USAGE
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	inline static Concurrency::SingleMutex labCountMutex;
	inline static std::atomic<size_t> labSize = 0;
	inline static std::atomic<size_t> labSizeCount = 0;
	inline static std::atomic<double> rollingAveLABSize = 0.0;
#else
	inline static size_t labSize = 0;
	inline static size_t labSizeCount = 0;
	inline static double rollingAveLABSize = 0.0;
#endif
#endif
};
