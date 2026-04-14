#pragma once

//project headers:
#include "Concurrency.h"
#include "EvaluableNodeReference.h"

//system headers:
#include <memory>

//if the macro PEDANTIC_GARBAGE_COLLECTION is defined, then garbage collection will be performed
//after every opcode, to help find and debug memory issues
//if the macro DEBUG_REPORT_LAB_USAGE is defined, then the local allocation buffer storage will be
//profiled and printed

//forward declaration
class Interpreter;

//memory pooled manager for allocating EvaluableNodes
class EvaluableNodeManager
{
public:

	struct ActiveInterpreters
	{
	#ifdef MULTITHREAD_SUPPORT
		//mutex to manage whether memory nodes are being modified
		//concurrent modifications can occur as long as there is only one unique thread
		// that has allocated the memory
		//garbage collection or destruction of the manager require a unique lock
		// so that the memory can be traversed
		Concurrency::ReadWriteMutex memoryModificationMutex;

		//mutex to modify activeInterpreters
		Concurrency::SingleMutex activeInterpretersMutex;
	#endif
		CompactHashSet<Interpreter *> activeInterpreters;
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

	inline EvaluableNode *AllocNode(const std::string_view string_value)
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

	//allocates a node identical to original
	inline EvaluableNode *AllocNode(EvaluableNode *original, bool copy_metadata = true)
	{
		EvaluableNode *n = AllocUninitializedNode();
		n->InitializeType(original, copy_metadata);
		return n;
	}

	//ensures that the top node is modifiable -- will allocate the node if necessary,
	// and if the result and any child nodes are all unique, then it will return an EvaluableNodeReference that is unique
	//if ensure_copy_if_top_node_in_cycle, then it will also allocate a new node if the top node is in a cycle
	//in case the top node is referenced by any of its node tree and it needs to ensure that structure is maintained
	inline void EnsureNodeIsModifiable(EvaluableNodeReference &original, bool ensure_copy_if_top_node_in_cycle = false,
		bool copy_metadata = true)
	{
		if(original != nullptr
				&& (original.uniqueUnreferencedTopNode || (original.unique && !ensure_copy_if_top_node_in_cycle) ) )
			return;

		EvaluableNode *copy = AllocNode(original.GetReference(), copy_metadata);
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
	EvaluableNodeReference DeepAllocCopy(EvaluableNode *tree, bool copy_metadata = true);

	//used to hold all of the references for DeepAllocCopy calls
	struct DeepAllocCopyParams
	{
		inline DeepAllocCopyParams(bool copy_metadata = true)
			: copyMetadata(copy_metadata)
		{	}

		EvaluableNode::ReferenceAssocType references;
		bool copyMetadata;
	};

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
		bool is_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(tree->GetType());

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

		if( (enr.unique || enr.uniqueUnreferencedTopNode) && enr != nullptr)
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
			auto &node_stack = EvaluableNode::reusableBuffer;
			node_stack.clear();
			if(en != nullptr)
				node_stack.push_back(en);

			//perform depth-first traversal
			while(!node_stack.empty())
			{
				EvaluableNode *cur = node_stack.back();
				node_stack.pop_back();

			#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
				assert(cur->IsNodeValid());
				assert(!cur->GetNeedCycleCheck());
			#endif

				if(cur->IsAssociativeArray())
				{
					for(auto &[_, child] : cur->GetMappedChildNodesReference())
					{
						if(child != nullptr)
							node_stack.push_back(child);
					}
				}
				else if(!cur->IsImmediate())
				{
					for(auto &child : cur->GetOrderedChildNodesReference())
					{
						if(child != nullptr)
							node_stack.push_back(child);
					}
				}

				cur->Invalidate();
				if(place_nodes_in_lab)
					AddNodeToLocalAllocationBuffer(cur);
			}
		}
		else //more costly cyclic free
		{
			auto &node_stack = EvaluableNode::reusableBuffer;
			node_stack.clear();
			if(en != nullptr)
				node_stack.push_back(en);

		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			assert(en->IsNodeValid());
		#endif

			//perform depth-first traversal
			while(!node_stack.empty())
			{
				EvaluableNode *cur = node_stack.back();
				node_stack.pop_back();

				if(cur->IsNodeDeallocated())
					continue;

				if(cur->IsAssociativeArray())
				{
					for(auto &[_, e] : cur->GetMappedChildNodesReference())
					{
						if(e != nullptr && !e->IsNodeDeallocated())
							node_stack.push_back(e);
					}
				}
				else if(!cur->IsImmediate())
				{
					for(auto &e : cur->GetOrderedChildNodesReference())
					{
						if(e != nullptr && !e->IsNodeDeallocated())
							node_stack.push_back(e);
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
		auto &node_stack = EvaluableNode::reusableBuffer;
		node_stack.clear();

		// Seed the buffer with the direct children of *tree*.
		if(tree->IsAssociativeArray())
		{
			for(auto &[_, e] : tree->GetMappedChildNodesReference())
			{
				if(e != nullptr)
					node_stack.push_back(e);
			}
		}
		else if(tree->IsOrderedArray())
		{
			for(auto &e : tree->GetOrderedChildNodesReference())
			{
				if(e != nullptr)
					node_stack.push_back(e);
			}
		}

		//perform depth-first traversal
		while(!node_stack.empty())
		{
			EvaluableNode *cur = node_stack.back();
			node_stack.pop_back();

		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			assert(cur->IsNodeValid());
			assert(!cur->GetNeedCycleCheck());
		#endif

			if(cur->IsAssociativeArray())
			{
				for(auto &[_, child] : cur->GetMappedChildNodesReference())
				{
					if(child != nullptr)
						node_stack.push_back(child);
				}
			}
			else if(!cur->IsImmediate())
			{
				for(auto &child : cur->GetOrderedChildNodesReference())
				{
					if(child != nullptr)
						node_stack.push_back(child);
				}
			}

			cur->Invalidate();
			AddNodeToLocalAllocationBuffer(cur);
		}
	}

	//sets the root node ensuring that the memory has been flushed so it is ready for reading
	__forceinline void SetRootNode(EvaluableNode *new_root)
	{
	#ifdef MULTITHREAD_SUPPORT
		//fence memory flushing by using an atomic store
		//TODO 15993: once C++20 is widely supported, change type to atomic_ref
		std::atomic<EvaluableNode *> *atomic_ref
			= reinterpret_cast<std::atomic<EvaluableNode *> *>(&rootNode);
		atomic_ref->store(new_root, std::memory_order_release);
	#else
		rootNode = new_root;
	#endif
	}

	//returns the memory modification mutex for garbage collection, etc.
	Concurrency::ReadWriteMutex &GetMemoryModificationMutex()
	{
		if(activeInterpreters.get() == nullptr)
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::WriteLock write_lock(managerAttributesMutex);

			//double check that it's still nullptr in case another thread created it
			if(activeInterpreters.get() == nullptr)
		#endif
				activeInterpreters = std::make_unique<ActiveInterpreters>();
		}

		return activeInterpreters->memoryModificationMutex;
	}

	//returns true if any interpreters are operating on the nodes managed by this instance
	inline bool AreAnyInterpretersRunning()
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock write_lock(managerAttributesMutex);
	#endif

		return (activeInterpreters.get() != nullptr && activeInterpreters->activeInterpreters.size() > 0);
	}

	//adds the interpreter to the active list for tracking EvaluableNode references
	void AddActiveInterpreter(Interpreter *interpreter)
	{
		if(activeInterpreters.get() == nullptr)
		{
		#ifdef MULTITHREAD_SUPPORT
			Concurrency::WriteLock write_lock(managerAttributesMutex);

			//double check that it's still nullptr in case another thread created it
			if(activeInterpreters.get() == nullptr)
		#endif
				activeInterpreters = std::make_unique<ActiveInterpreters>();
		}

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(activeInterpreters->activeInterpretersMutex);
	#endif

		activeInterpreters->activeInterpreters.insert(interpreter);
	}

	//removes the interpreter from the active list for tracking EvaluableNode references
	void RemoveActiveInterpreter(Interpreter *interpreter)
	{
	#ifdef MULTITHREAD_SUPPORT
		Concurrency::Lock lock(activeInterpreters->activeInterpretersMutex);
	#endif

		activeInterpreters->activeInterpreters.erase(interpreter);
	}

	//returns the number of nodes currently being used that have not been freed yet
	__forceinline size_t GetNumberOfUsedNodes()
	{	return firstUnusedNodeIndex;		}

	__forceinline size_t GetNumberOfUnusedNodes()
	{	return nodes.size() - firstUnusedNodeIndex;		}

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

	//verifies integrity of all referenced nodes
	void VerifyEvaluableNodeIntegretyForAllReferencedNodes();

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

	//frees everything except those nodes referenced by rootNode and activeInterpreters
	//cur_first_unused_node_index represents the first unused index and will set firstUnusedNodeIndex
	//to the reduced value
	//note that this method does not read from firstUnusedNodeIndex, as it may be cleared to indicate threads
	//to stop spinlocks
	void FreeAllNodesExceptReferencedNodes(size_t cur_first_unused_node_index);

	//implemented as a recursive method because the extra complexity of an iterative implementation
	// is not worth the very small performance benefit
	//returns a pair of the copy and true if the copy needs cycle check
	//assumes tree is not nullptr
	std::pair<EvaluableNode *, bool> DeepAllocCopyRecurse(EvaluableNode *tree, DeepAllocCopyParams &dacp);

	//sets all referenced nodes that are in use as such
	void MarkAllReferencedNodesInUse(size_t estimated_nodes_in_use);

	//computes whether the code is cycle free and idempotent and updates all nodes appropriately
	// returns flags for whether cycle free and idempotent
	// requires tree not be nullptr; the first tree should have nullptr as parent
	static std::pair<bool, bool> UpdateFlagsForNodeTreeRecurse(EvaluableNode *tree, EvaluableNode *parent,
		EvaluableNode::ReferenceAssocType &checked_to_parent);

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

public:
	//root of the entity
	EvaluableNode *rootNode;

protected:

#ifdef MULTITHREAD_SUPPORT
	//mutex to manage attributes of manager, including operations such as
	// memory allocation, reference management, etc.
	Concurrency::ReadWriteMutex managerAttributesMutex;

	std::atomic<size_t> firstUnusedNodeIndex;
#else
	size_t firstUnusedNodeIndex;
#endif

	//nodes that have been allocated and may be in use
	// all nodes in use are below firstUnusedNodeIndex, such that all above that index are free for use
	// nodes cannot be nullptr for lower indices than firstUnusedNodeIndex
	std::vector<EvaluableNode *> nodes;

	//keeps track of all of the nodes currently referenced by any resource or interpreter
	//only allocated if needed
	std::unique_ptr<ActiveInterpreters> activeInterpreters;

	//extra space to allocate when allocating
	static const double allocExpansionFactor;

	//number of nodes to allocate at once for the local allocation buffer
	static const int labBlockAllocationSize = 24;

	//local memory pool for allocations
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		inline static LocalAllocationBuffer localAllocationBuffer;

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
