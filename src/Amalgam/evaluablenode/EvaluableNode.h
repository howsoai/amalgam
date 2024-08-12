#pragma once

//project headers:
#include "FastMath.h"
#include "HashMaps.h"
#include "Opcodes.h"
#include "PlatformSpecific.h"
#include "StringInternPool.h"
#include "StringManipulation.h"

//system headers:
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

//if the macro AMALGAM_MEMORY_INTEGRITY is defined, then it will continuously verify memory, at a high cost of performance
//this is useful for diagnosing and debugging memory issues
//if the macro AMALGAM_FAST_MEMORY_INTEGRITY is defined, then only the checks that are fast will be made
#ifdef AMALGAM_MEMORY_INTEGRITY
#define AMALGAM_FAST_MEMORY_INTEGRITY
#endif

//forward declarations:
class EvaluableNodeManager;

class EvaluableNode
{
public:
	//set associative container types based on performance needs

	//referencing one EvaluableNode to another
	using ReferenceAssocType = FastHashMap<EvaluableNode *, EvaluableNode *>;

	//a set of EvaluableNode pointers
	using ReferenceSetType = FastHashSet<EvaluableNode *>;

	//EvaluableNode pointer to count
	using ReferenceCountType = FastHashMap<EvaluableNode *, size_t>;

	//lookup a keyword string and find the type
	using KeywordLookupType = FastHashMap<std::string, EvaluableNodeType>;

	//EvaluableNode assoc storage
	using AssocType = CompactHashMap<StringInternPool::StringID, EvaluableNode *>;

	//Storage for labels
	using LabelsAssocType = CompactHashMap<StringInternPool::StringID, EvaluableNode *>;

	//constructors
	__forceinline EvaluableNode() { InitializeUnallocated(); }
	__forceinline EvaluableNode(EvaluableNodeType type, const std::string &string_value) { InitializeType(type, string_value); }
	__forceinline EvaluableNode(double value) { InitializeType(value); }
	__forceinline EvaluableNode(EvaluableNodeType type) { InitializeType(type); }
	__forceinline EvaluableNode(EvaluableNode *n) { InitializeType(n); }

	__forceinline ~EvaluableNode()
	{
		if(!IsNodeDeallocated())
			Invalidate();
	}

	//clears out all data and makes the unusable in the ENT_DEALLOCATED state
	void Invalidate();

	///////////////////////////////////////////
	//Each InitializeType* sets up a given type with appropriate data
	inline void InitializeType(EvaluableNodeType _type, const std::string &string_value)
	{
		type = _type;
		attributes.allAttributes = 0;
		attributes.individualAttribs.isIdempotent = true;
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
		value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
	}

	inline void InitializeType(EvaluableNodeType _type, StringInternPool::StringID string_id)
	{
		attributes.allAttributes = 0;
		if(string_id == StringInternPool::NOT_A_STRING_ID)
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = _type;
			value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_id);
			value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
		}
	}

	//like InitializeType, but hands off the string reference to string_id
	inline void InitializeTypeWithReferenceHandoff(EvaluableNodeType _type, StringInternPool::StringID string_id)
	{
		attributes.allAttributes = 0;
		if(string_id == StringInternPool::NOT_A_STRING_ID)
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = _type;
			value.stringValueContainer.stringID = string_id;
			value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
		}
	}

	inline void InitializeType(double number_value)
	{
		attributes.allAttributes = 0;
		if(FastIsNaN(number_value))
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = ENT_NUMBER;
			attributes.individualAttribs.isIdempotent = true;
			value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
			value.numberValueContainer.numberValue = number_value;
		}
	}

	//initializes to ENT_UNINITIALIZED
	//useful to mark a node in a hold state before it's ready so it isn't counted as ENT_DEALLOCATED
	//but also such that the fields don't need to be initialized or cleared
	constexpr void InitializeUnallocated()
	{
		type = ENT_UNINITIALIZED;
	}

	inline void InitializeType(EvaluableNodeType _type)
	{
		type = _type;
		attributes.allAttributes = 0;
		attributes.individualAttribs.isIdempotent = IsEvaluableNodeTypePotentiallyIdempotent(_type);

		if(DoesEvaluableNodeTypeUseNumberData(_type))
		{
			value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
			value.numberValueContainer.numberValue = 0.0;
			attributes.individualAttribs.isIdempotent = true;
		}
		else if(DoesEvaluableNodeTypeUseStringData(_type))
		{
			value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
			value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
			attributes.individualAttribs.isIdempotent = (_type == ENT_STRING);
		}
		else if(DoesEvaluableNodeTypeUseAssocData(_type))
		{
			type = _type;
			attributes.allAttributes = 0;
			attributes.individualAttribs.isIdempotent = true;
			value.ConstructMappedChildNodes();
		}
		else
		{
			value.ConstructOrderedChildNodes();
		}
	}

	//sets the value of the node to that of n and the copy_* parameters indicate what metadata should be copied
	void InitializeType(EvaluableNode *n, bool copy_labels = true, bool copy_comments_and_concurrency = true);

	//copies the EvaluableNode n into this.  Does not overwrite labels or comments.
	void CopyValueFrom(EvaluableNode *n);

	//copies the metadata of the node n into this
	void CopyMetadataFrom(EvaluableNode *n);

	//clears the node's metadata
	__forceinline void ClearMetadata()
	{
		ClearComments();
		ClearLabels();
		SetConcurrency(false);
	}

	//Evaluates the fraction of the labels of nodes that are the same, 1.0 if no labels on either
	//num_common_labels and num_unique_labels are set to the appropriate number in common and number of labels that are unique when the two sets are merged
	static void GetNodeCommonAndUniqueLabelCounts(EvaluableNode *n1, EvaluableNode *n2, size_t &num_common_labels, size_t &num_unique_labels);

	//Returns true if the immediate data structure of a is equal to b
	static bool AreShallowEqual(EvaluableNode *a, EvaluableNode *b);

	//Returns true if the entire data structure of a is equal in value to the data structure of b
	static inline bool AreDeepEqual(EvaluableNode *a, EvaluableNode *b)
	{
		//if pointers are the same, then they are the same
		if(a == b)
			return true;

		//first check if the immediate values are equal
		if(!AreShallowEqual(a, b))
			return false;

		bool need_cycle_checks = false;

		//since they are shallow equal, check for quick exit
		if(a != nullptr && b != nullptr)
		{
			if(IsEvaluableNodeTypeImmediate(a->GetType())
					&& IsEvaluableNodeTypeImmediate(b->GetType()))
				return true;

			//only need cycle checks if both a and b need cycle checks,
			// otherwise, one will become exhausted and end the comparison
			if(a->GetNeedCycleCheck() && b->GetNeedCycleCheck())
				need_cycle_checks = true;
		}

		if(need_cycle_checks)
		{
			ReferenceAssocType checked;
			return AreDeepEqualGivenShallowEqual(a, b, &checked);
		}
		else
		{
			return AreDeepEqualGivenShallowEqual(a, b, nullptr);
		}
	}

	//Returns true if this node evaluates to true
	static bool IsTrue(EvaluableNode *n);

	//Returns true if the node is some form of associative array
	__forceinline bool IsAssociativeArray()
	{
		return DoesEvaluableNodeTypeUseAssocData(GetType());
	}

	//Returns true if the node is some form of associative array
	static __forceinline bool IsAssociativeArray(EvaluableNode *n)
	{
		if(n == nullptr)
			return false;
		return n->IsAssociativeArray();
	}

	//returns true if the type is immediate
	__forceinline bool IsImmediate()
	{
		return IsEvaluableNodeTypeImmediate(GetType());
	}

	//Returns true if the node is some form of ordered array
	__forceinline bool IsOrderedArray()
	{
		return DoesEvaluableNodeTypeUseOrderedData(GetType());
	}

	//Returns true if the node is some form of ordered array
	static __forceinline bool IsOrderedArray(EvaluableNode *n)
	{
		if(n == nullptr)
			return false;
		return n->IsOrderedArray();
	}

	//returns true if the EvaluableNode is of a query type
	static __forceinline bool IsQuery(EvaluableNode *n)
	{
		return (n != nullptr && IsEvaluableNodeTypeQuery(n->GetType()));
	}

	//Returns positive if a is less than b,
	// negative if greater, or 0 if equal or not numerically comparable
	static int Compare(EvaluableNode *a, EvaluableNode *b);

	//Returns true if the node b is less than node a.  If or_equal_to is true, then also returns true if equal
	static inline bool IsLessThan(EvaluableNode *a, EvaluableNode *b, bool or_equal_to)
	{
		int r = Compare(a, b);
		if(r < 0)
			return true;
		if(or_equal_to && r == 0)
			return true;
		return false;
	}

	static inline bool IsStrictlyLessThan(EvaluableNode *a, EvaluableNode *b)
	{
		return IsLessThan(a, b, false);
	}

	static inline bool IsStrictlyGreaterThan(EvaluableNode *a, EvaluableNode *b)
	{
		return !IsLessThan(a, b, true);
	}

	//if the node's contents can be represented as a number, which includes numbers, infinity, then return true
	// otherwise returns false
	static __forceinline bool CanRepresentValueAsANumber(EvaluableNode *e)
	{
		if(e == nullptr)
			return true;

		switch(e->GetType())
		{
		case ENT_NUMBER:
		case ENT_TRUE:
		case ENT_FALSE:
		case ENT_NULL:
			return true;
		default:
			return false;
		}
	}

	//returns true is node pointer e is nullptr or value of e has type ENT_NULL
	static __forceinline bool IsNull(EvaluableNode *e)
	{
		return (e == nullptr || e->GetType() == ENT_NULL);
	}

	//Converts the node to a number
	//if null, then will return value_if_null
	static double ToNumber(EvaluableNode *e, double value_if_null = std::numeric_limits<double>::quiet_NaN());

	//returns true if the node can directly be interpreted as a number
	static __forceinline bool IsNumericOrNull(EvaluableNode *e)
	{
		if(e == nullptr)
			return true;

		auto type = e->GetType();
		if(type == ENT_NUMBER || type == ENT_NULL)
			return true;

		return false;
	}

	//returns true if the EvaluableNode uses numeric data
	__forceinline bool IsNumericOrNull()
	{
		return DoesEvaluableNodeTypeUseNumberData(GetType());
	}

	//Converts a number to a string in a consistent way that should be used for anything dealing with EvaluableNode
	static __forceinline std::string NumberToString(double value)
	{
		return StringManipulation::NumberToString(value);
	}

	static __forceinline std::string NumberToString(size_t value)
	{
		return StringManipulation::NumberToString(value);
	}

	//converts the node to a string that represents the opcode
	const static std::string ToStringPreservingOpcodeType(EvaluableNode *e);

	//converts the node to a string, returning true if valid.  If it doesn't exist or it's null, it returns false
	static std::pair<bool, std::string> ToString(EvaluableNode *e);

	//converts node to an existing string. If it doesn't exist or it's null, it returns NOT_A_STRING_ID
	static StringInternPool::StringID ToStringIDIfExists(EvaluableNode *e);

	//converts node to a string. Creates a reference to the string that must be destroyed, regardless of whether the string existed or not (if it did not exist, then it creates one)
	static StringInternPool::StringID ToStringIDWithReference(EvaluableNode *e);

	//converts node to a string. Creates a reference to the string that must be destroyed, regardless of whether the string existed or not
	// if e is a string, it will clear it and hand the reference to the caller
	static StringInternPool::StringID ToStringIDTakingReferenceAndClearing(EvaluableNode *e);

	//returns the comments as a new string
	static inline StringInternPool::StringID GetCommentsStringId(EvaluableNode *e)
	{
		if(e == nullptr)
			return StringInternPool::NOT_A_STRING_ID;
		return e->GetCommentsStringId();
	}

	//Converts the node to an ENT_ASSOC where the keys are the numbers of the indices
	void ConvertOrderedListToNumberedAssoc();

	//returns true if the node can be flattened,
	// that is, contains no cycles when traversing downward and potentially
	// duplicating nodes if they are referenced more than once
	static inline bool CanNodeTreeBeFlattened(EvaluableNode *n)
	{
		if(n == nullptr)
			return true;

		if(!n->GetNeedCycleCheck())
			return true;

		std::vector<EvaluableNode *> stack;
		return CanNodeTreeBeFlattenedRecurse(n, stack);
	}

	//Returns the number of nodes in the data structure
	static inline size_t GetDeepSize(EvaluableNode *n)
	{
		if(n == nullptr)
			return 1;

		if(!n->GetNeedCycleCheck())
		{
			return GetDeepSizeNoCycleRecurse(n);
		}
		else
		{
			ReferenceSetType checked;
			return GetDeepSizeRecurse(n, checked);
		}
	}

	//Returns the number of bytes of memory that node is currently using
	static size_t GetEstimatedNodeSizeInBytes(EvaluableNode *n);

	//gets current type
	__forceinline EvaluableNodeType &GetType()
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(type != ENT_DEALLOCATED);
	#endif
		return type;
	}

	//returns true if the node is currently deallocated
	constexpr bool IsNodeDeallocated()
	{
		return (type == ENT_DEALLOCATED);
	}

	//transforms node to new_type, converting data if types are different
	// enm is used if it needs to allocate nodes when changing types
	// if enm is nullptr, then it will not necessarily keep child nodes
	//if attempt_to_preserve_immediate_value is true, then it will try to preserve any relevant immediate value
	// attempt_to_preserve_immediate_value should be set to false if the value will be immediately overwritten
	void SetType(EvaluableNodeType new_type, EvaluableNodeManager *enm = nullptr,
		bool attempt_to_preserve_immediate_value = true);

	//fully clears node and sets it to new_type
	inline void ClearAndSetType(EvaluableNodeType new_type)
	{
		ClearMetadata();
		DestructValue();
		InitializeType(new_type);
	}

	//sets up number value
	void InitNumberValue();

	//gets the value by reference
	__forceinline double &GetNumberValue()
	{
		if(DoesEvaluableNodeTypeUseNumberData(GetType()))
			return GetNumberValueReference();

		//none of the above, return an empty one
		return zeroNumberValue;
	}

	//sets the number value
	inline void SetNumberValue(double v)
	{
		if(FastIsNaN(v))
		{
			SetType(ENT_NULL);
		}
		else
		{
			if(DoesEvaluableNodeTypeUseNumberData(GetType()))
				GetNumberValueReference() = v;
		}
	}

	//sets up the ability to contain a string
	void InitStringValue();
	__forceinline StringInternPool::StringID GetStringID()
	{
		if(DoesEvaluableNodeTypeUseStringData(GetType()))
			return GetStringIDReference();

		return StringInternPool::NOT_A_STRING_ID;
	}
	void SetStringID(StringInternPool::StringID id);
	std::string GetStringValue();
	void SetStringValue(const std::string &v);
	//gets the string ID and clears the node's string ID, but does not destroy the string reference,
	// leaving the reference handling up to the caller
	StringInternPool::StringID GetAndClearStringIDWithReference();
	//sets the string but does not create a new reference because the reference has already been created
	void SetStringIDWithReferenceHandoff(StringInternPool::StringID id);

	//functions for getting and setting labels by string or by StringID
	// all Label functions perform any reference counting management necessary when setting and clearing
	std::vector<StringInternPool::StringID> GetLabelsStringIds();
	std::vector<std::string> GetLabelsStrings();
	void SetLabelsStringIds(const std::vector<StringInternPool::StringID> &label_string_ids);
	size_t GetNumLabels();
	std::string GetLabel(size_t label_index);
	const StringInternPool::StringID GetLabelStringId(size_t label_index);
	void RemoveLabel(size_t label_index);
	void ClearLabels();
	//reserves the specified number of labels
	void ReserveLabels(size_t num_labels);
	//if handoff_reference is true, then it will not create a new reference but assume one has already been created
	void AppendLabelStringId(StringInternPool::StringID label_string_id, bool handoff_reference = false);
	void AppendLabel(const std::string &label);

	//functions for getting and setting node comments by string or by StringID
	// all Comment functions perform any reference counting management necessary when setting and clearing
	StringInternPool::StringID GetCommentsStringId();
	inline std::string GetCommentsString()
	{
		return string_intern_pool.GetStringFromID(GetCommentsStringId());
	}

	//returns true if has comments
	inline bool HasComments()
	{
		return GetCommentsStringId() != string_intern_pool.NOT_A_STRING_ID;
	}

	//splits comment lines and returns a vector of strings of the comment
	std::vector<std::string> GetCommentsSeparateLines();
	//if handoff_reference is true, then it will not create a new reference but assume one has already been created
	void SetCommentsStringId(StringInternPool::StringID comments_string_id, bool handoff_reference = false);
	void SetComments(const std::string &comments);
	void ClearComments();
	void AppendCommentsStringId(StringInternPool::StringID comments_string_id);
	void AppendComments(const std::string &comments);

	//returns true if the EvaluableNode is marked with preference for concurrency
	constexpr bool GetConcurrency()
	{
		return attributes.individualAttribs.concurrent;
	}

	//sets the EvaluableNode's preference for concurrency
	constexpr void SetConcurrency(bool concurrent)
	{
		attributes.individualAttribs.concurrent = concurrent;
	}

	//returns true if the EvaluableNode and all its dependents need to be checked for cycles
	constexpr bool GetNeedCycleCheck()
	{
		return attributes.individualAttribs.needCycleCheck;
	}

	//sets the EvaluableNode's needCycleCheck flag
	constexpr void SetNeedCycleCheck(bool need_cycle_check)
	{
		attributes.individualAttribs.needCycleCheck = need_cycle_check;
	}

	//returns true if the EvaluableNode and all its dependents are idempotent
	constexpr bool GetIsIdempotent()
	{
		return attributes.individualAttribs.isIdempotent;
	}

	//sets the EvaluableNode's idempotentcy flag
	constexpr void SetIsIdempotent(bool is_idempotent)
	{
		attributes.individualAttribs.isIdempotent = is_idempotent;
	}

	//returns whether this node has been marked as known to be currently in use
	constexpr bool GetKnownToBeInUse()
	{
		return attributes.individualAttribs.knownToBeInUse;
	}

	//sets whether this node is currently known to be in use
	constexpr void SetKnownToBeInUse(bool in_use)
	{
		attributes.individualAttribs.knownToBeInUse = in_use;
	}

#ifdef MULTITHREAD_SUPPORT
	//returns whether this node has been marked as known to be currently in use
	__forceinline bool GetKnownToBeInUseAtomic()
	{
		EvaluableNodeAttributesType attrib_with_known_true;
		attrib_with_known_true.allAttributes = 0;
		attrib_with_known_true.individualAttribs.knownToBeInUse = true;

		//TODO 15993: once C++20 is widely supported, change type to atomic_ref
		uint8_t all_attributes = reinterpret_cast<std::atomic<uint8_t>&>(attributes.allAttributes);
		return (all_attributes & attrib_with_known_true.allAttributes);
	}

	//sets whether this node is currently known to be in use
	__forceinline void SetKnownToBeInUseAtomic(bool in_use)
	{
		if(in_use)
		{
			EvaluableNodeAttributesType attrib_with_known_true;
			attrib_with_known_true.allAttributes = 0;
			attrib_with_known_true.individualAttribs.knownToBeInUse = true;

			//TODO 15993: once C++20 is widely supported, change type to atomic_ref
			reinterpret_cast<std::atomic<uint8_t>&>(attributes.allAttributes).fetch_or(attrib_with_known_true.allAttributes);
		}
		else
		{
			EvaluableNodeAttributesType attrib_with_known_false;
			attrib_with_known_false.allAttributes = 0xFF;
			attrib_with_known_false.individualAttribs.knownToBeInUse = false;

			//TODO 15993: once C++20 is widely supported, change type to atomic_ref
			reinterpret_cast<std::atomic<uint8_t>&>(attributes.allAttributes).fetch_and(attrib_with_known_false.allAttributes);
		}
	}
#endif

	//returns the number of child nodes regardless of mapped or ordered
	size_t GetNumChildNodes();

	void InitOrderedChildNodes();
	//preallocates to_reserve for appending, etc.
	inline void ReserveOrderedChildNodes(size_t to_reserve)
	{
		if(IsOrderedArray())
			GetOrderedChildNodesReference().reserve(to_reserve);
	}

	__forceinline std::vector<EvaluableNode *> &GetOrderedChildNodes()
	{
		if(IsOrderedArray())
			return GetOrderedChildNodesReference();

		return emptyOrderedChildNodes;
	}

	//using ordered or mapped child nodes as appropriate, transforms into numeric values and passes into store_value
	// if node is mapped child nodes, it will use element_names to order populate out and use default_value if any given id is not found
	//will use num_expected_elements for immediate values
	//store_value takes in 3 parameters, the index, a bool if the value was found, and the EvaluableNode of the value
	template<typename StoreValueFunction = void(size_t, bool, EvaluableNode *)>
	static inline void ConvertChildNodesAndStoreValue(EvaluableNode *node, std::vector<StringInternPool::StringID> &element_names,
		size_t num_expected_elements, StoreValueFunction store_value)
	{
		if(node != nullptr)
		{
			if(node->IsAssociativeArray())
			{
				auto &wn_mcn = node->GetMappedChildNodesReference();
				for(size_t i = 0; i < element_names.size(); i++)
				{
					EvaluableNode *value_en = nullptr;
					bool found = false;
					auto found_node = wn_mcn.find(element_names[i]);
					if(found_node != end(wn_mcn))
					{
						value_en = found_node->second;
						found = true;
					}

					store_value(i, found, value_en);
				}
			}
			else if(node->IsImmediate())
			{
				//fill in with the node's value
				for(size_t i = 0; i < num_expected_elements; i++)
					store_value(i, true, node);
			}
			else //ordered
			{
				auto &node_ocn = node->GetOrderedChildNodesReference();

				for(size_t i = 0; i < node_ocn.size(); i++)
					store_value(i, true, node_ocn[i]);
			}
		}
	}

	//Note that ResizeOrderedChildNodes does not initialize new nodes, so they must be initialized by caller
	inline void SetOrderedChildNodesSize(size_t new_size)
	{
		if(IsOrderedArray())
			GetOrderedChildNodesReference().resize(new_size);
	}

	//sets the ordered child nodes and updates flags
	void SetOrderedChildNodes(const std::vector<EvaluableNode *> &ocn,
		bool need_cycle_check = true, bool is_idempotent = false);
	void ClearOrderedChildNodes();
	void AppendOrderedChildNode(EvaluableNode *cn);
	void AppendOrderedChildNodes(const std::vector<EvaluableNode *> &ocn_to_append);
	//if the OrderedChildNodes list was using extra memory (if it were resized to be smaller), this would attempt to free extra memory
	inline void ReleaseOrderedChildNodesExtraMemory()
	{
		if(IsOrderedArray())
			GetOrderedChildNodesReference().shrink_to_fit();
	}

	void InitMappedChildNodes();
	//preallocates to_reserve for appending, etc.
	inline void ReserveMappedChildNodes(size_t to_reserve)
	{
		if(IsAssociativeArray())
			GetMappedChildNodesReference().reserve(to_reserve);
	}

	__forceinline AssocType &GetMappedChildNodes()
	{
		if(IsAssociativeArray())
			return GetMappedChildNodesReference();

		return emptyMappedChildNodes;
	}

	//if the id exists, returns a pointer to the pointer of the child node
	// returns nullptr if the id doesn't exist
	inline EvaluableNode **GetMappedChildNode(const std::string &id)
	{
		StringInternPool::StringID sid = string_intern_pool.GetIDFromString(id);
		return GetMappedChildNode(sid);
	}
	//if the id exists, returns a pointer to the pointer of the child node
	// returns nullptr if the id doesn't exist
	EvaluableNode **GetMappedChildNode(const StringInternPool::StringID sid);
	//returns a pointer to the pointer of the child node, creating it if necessary and populating it with a nullptr
	EvaluableNode **GetOrCreateMappedChildNode(const std::string &id);
	//returns a pointer to the pointer of the child node, creating it if necessary and populating it with a nullptr
	EvaluableNode **GetOrCreateMappedChildNode(const StringInternPool::StringID sid);
	// if copy is set to true, then it will copy the map, otherwise it will swap
	void SetMappedChildNodes(AssocType &new_mcn, bool copy,
		bool need_cycle_check = true, bool is_idempotent = false);
	//if overwrite is true, then it will overwrite the value, otherwise it will only set it if it does not exist
	// will return true if it was successfully written (false if overwrite is set to false and the key already exists),
	// as well as a pointer to where the pointer is stored
	std::pair<bool, EvaluableNode **> SetMappedChildNode(const std::string &id, EvaluableNode *node, bool overwrite = true);
	std::pair<bool, EvaluableNode **> SetMappedChildNode(const StringInternPool::StringID sid, EvaluableNode *node, bool overwrite = true);
	//like SetMappedChildNode, except the sid already has a reference that is being handed off to this EvaluableNode to manage
	bool SetMappedChildNodeWithReferenceHandoff(const StringInternPool::StringID sid, EvaluableNode *node, bool overwrite = true);
	void ClearMappedChildNodes();
	//returns the node erased
	EvaluableNode *EraseMappedChildNode(const StringInternPool::StringID sid);
	void AppendMappedChildNodes(AssocType &mcn_to_append);

protected:
	//defined since it is used as a pointer in EvaluableNodeValue
	struct EvaluableNodeExtendedValue;
public:

	//returns true if value contains an extended type
	constexpr bool HasExtendedValue()
	{	return attributes.individualAttribs.hasExtendedValue;	}

	//assumes that the EvaluableNode is of type ENT_NUMBER, and returns the value by reference
	constexpr double &GetNumberValueReference()
	{
		if(!HasExtendedValue())
			return value.numberValueContainer.numberValue;
		else
			return value.extension.extendedValue->value.numberValueContainer.numberValue;
	}

	//assumes that the EvaluableNode is of type that holds a string, and returns the value by reference
	constexpr StringInternPool::StringID &GetStringIDReference()
	{
		if(!HasExtendedValue())
			return value.stringValueContainer.stringID;
		else
			return value.extension.extendedValue->value.stringValueContainer.stringID;
	}

	//assumes that the EvaluableNode has ordered child nodes, and returns the value by reference
	constexpr std::vector<EvaluableNode *> &GetOrderedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.orderedChildNodes;
		else
			return value.extension.extendedValue->value.orderedChildNodes;
	}

	//assumes that the EvaluableNode is has mapped child nodes, and returns the value by reference
	constexpr AssocType &GetMappedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.mappedChildNodes;
		else
			return value.extension.extendedValue->value.mappedChildNodes;
	}

	//if it is storing an immediate value and has room to store a label
	constexpr bool HasCompactSingleLabelStorage()
	{
		return ((type == ENT_NUMBER || type == ENT_STRING || type == ENT_SYMBOL) && !HasExtendedValue());
	}

	//returns a reference to the storage location for a single label
	// will only return valid results if HasCompactSingleLabelStorage() is true, so that should be called first
	constexpr StringInternPool::StringID &GetCompactSingleLabelStorage()
	{
		if(type == ENT_NUMBER)
			return value.numberValueContainer.labelStringID;
		//else assume type == ENT_STRING || type == ENT_SYMBOL
		return value.stringValueContainer.labelStringID;
	}

	//registers and unregisters an EvaluableNode for debug watching
	static inline void RegisterEvaluableNodeForDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		debugWatch.emplace(en);
	}

	static inline void UnregisterEvaluableNodeForDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		debugWatch.erase(en);
	}

	//returns true if the EvaluableNode is in the debug watch
	static inline void AssertIfInDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		if(debugWatch.find(en) != end(debugWatch))
		{
			assert(false);
		}
	}

protected:

	//align to the nearest 2-bytes to minimize alignment issues but reduce the overall memory footprint
	// while maintaining some alignment
#pragma pack(push, 2)
	union EvaluableNodeValue
	{
		//take care of all setup and cleanup outside of the union
		// default to numberValueContainer constructor to allow constexpr
		inline EvaluableNodeValue() { }
		inline ~EvaluableNodeValue() { }

		inline void ConstructOrderedChildNodes()
		{	new (&orderedChildNodes) std::vector<EvaluableNode *>;	}

		inline void DestructOrderedChildNodes()
		{	orderedChildNodes.~vector();	}

		inline void ConstructMappedChildNodes()
		{	new (&mappedChildNodes) AssocType;	}

		inline void DestructMappedChildNodes()
		{
			string_intern_pool.DestroyStringReferences(mappedChildNodes, [](auto n) { return n.first; });
			mappedChildNodes.~AssocType();
		}

		//ordered child nodes (when type requires it), meaning and number of childNodes is based on the type of the node
		std::vector<EvaluableNode *> orderedChildNodes;

		//hash-mapped child nodes (when type requires it), meaning and number of childNodes is based on the type of the node
		AssocType mappedChildNodes;

		//when type represents a string, holds the corresponding values
		struct EvaluableNodeValueString
		{
			//string value
			StringInternPool::StringID stringID;

			//allow up to one label -- only used when not part of an extended value
			StringInternPool::StringID labelStringID;
		} stringValueContainer;

		//when type represents a number, holds the corresponding value
		struct EvaluableNodeValueNumber
		{
			//number value
			double numberValue;

			//allow up to one label -- only used when not part of an extended value
			StringInternPool::StringID labelStringID;
		} numberValueContainer;

		struct EvaluableNodeExtension
		{
			//pointer to store any extra data if EvaluableNode needs multiple fields
			EvaluableNodeExtendedValue *extendedValue;

			//comments that appear just above the code represented by this node
			StringInternPool::StringID commentsStringId;
		} extension;
	};
#pragma pack(pop)

	struct EvaluableNodeExtendedValue
	{
		//value stored here
		EvaluableNodeValue value;

		//labels of the node for referencing and querying
		std::vector<StringInternPool::StringID> labelsStringIds;
	};

	//makes sure that the extendedValue is set appropriately so that it can be used to hold additional data
	void EnsureEvaluableNodeExtended();

	//destructs the value so that the node can be reused
	// note that the value should be considered uninitialized
	void DestructValue();

	//Returns true if the entire data structure of a is equal in value to the data structure of b
	// but does not check the immediate nodes a and b to see if they are shallow equal (this is assumed to be done by the caller for performance)
	// Assists the public function AreDeepEqual
	// if checked is nullptr, then it won't check for cycles
	static bool AreDeepEqualGivenShallowEqual(EvaluableNode *a, EvaluableNode *b, ReferenceAssocType *checked);

	//recursive helper function for CanNodeTreeBeFlattened
	// assumes n is not nullptr
	static bool CanNodeTreeBeFlattenedRecurse(EvaluableNode *n, std::vector<EvaluableNode *> &stack);

	//Returns the deep size, excluding nodes already checked
	// Assists the public function GetDeepSize
	static size_t GetDeepSizeRecurse(EvaluableNode *n, ReferenceSetType &checked);

	//Like GetDeepSizeRecurse, but assumes there are no cycles in n
	static size_t GetDeepSizeNoCycleRecurse(EvaluableNode *n);

	EvaluableNodeValue value;

	//Executable/data type of the node
	EvaluableNodeType type;

	//make sure this only takes up one byte
#pragma pack(push, 1)
	union EvaluableNodeAttributesType
	{
		//quick way to initialize all attributes to 0
		uint8_t allAttributes;
		struct
		{
			//if true, then contains an extended type
			bool hasExtendedValue : 1;
			//if true, then this node and any nodes it contains may have a cycle so needs to be checked
			bool needCycleCheck : 1;
			//if true, then this node and any nodes it contains are idempotent
			bool isIdempotent : 1;
			//if true, then the node is marked for concurrency
			bool concurrent : 1;
			//if true, then known to be in use with regard to garbage collection
			bool knownToBeInUse : 1;
		} individualAttribs;
	};
#pragma pack(pop)

	//fields contained within the current set of data
	EvaluableNodeAttributesType attributes;

	//values used to be able to return a reference
	static double zeroNumberValue;
	static std::string emptyStringValue;
	static EvaluableNode *emptyEvaluableNodeNullptr;
	static std::vector<std::string> emptyStringVector;
	static std::vector<StringInternPool::StringID> emptyStringIdVector;
	static std::vector<EvaluableNode *> emptyOrderedChildNodes;
	static AssocType emptyMappedChildNodes;

	//field for watching EvaluableNodes for debugging
	static FastHashSet<EvaluableNode *> debugWatch;
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	static Concurrency::SingleMutex debugWatchMutex;
#endif
};

//EvaluableNode type upper taxonomy for determining the most generic way
// concrete values can be stored for the EvaluableNode.  It is intended to
// group types into the highest specificity that it is worth using to
// compare two values based on their collective types
enum EvaluableNodeImmediateValueType : uint8_t
{
	ENIVT_NOT_EXIST,			//there is nothing to even hold the data
	ENIVT_NULL,					//no data being held
	ENIVT_NUMBER,				//number
	ENIVT_STRING_ID,			//stringID
	ENIVT_CODE,					//code (more general than any of the above)
	ENIVT_NUMBER_INDIRECTION_INDEX,		//not a real EvaluableNode type, but an index to some data structure that has a number
	ENIVT_STRING_ID_INDIRECTION_INDEX	//not a real EvaluableNode type, but an index to some data structure that has a stringID
};

//structure that can hold the most immediate value type of an EvaluableNode
// EvaluableNodeImmediateValueType can be used to communicate which type of data is being held
union EvaluableNodeImmediateValue
{
	constexpr EvaluableNodeImmediateValue()
		: number(std::numeric_limits<double>::quiet_NaN())
	{	}

	constexpr EvaluableNodeImmediateValue(double _number)
		: number(_number)
	{	}

	__forceinline EvaluableNodeImmediateValue(StringInternPool::StringID string_id)
		: stringID(string_id)
	{	}

	constexpr EvaluableNodeImmediateValue(EvaluableNode *_code)
		: code(_code)
	{	}

	constexpr EvaluableNodeImmediateValue(const EvaluableNodeImmediateValue &eniv)
		: code(eniv.code)
	{	}

	constexpr EvaluableNodeImmediateValue(size_t indirection_index)
		: indirectionIndex(indirection_index)
	{	}

	__forceinline EvaluableNodeImmediateValue &operator =(const EvaluableNodeImmediateValue &eniv)
	{
		//perform a memcpy because it's a union, to be safe; the compiler should optimize this out
		std::memcpy(this, &eniv, sizeof(*this));
		return *this;
	}

	//copies the value from en and returns the EvaluableNodeConcreteValueType
	inline EvaluableNodeImmediateValueType CopyValueFromEvaluableNode(EvaluableNode *en)
	{
		if(en == nullptr)
		{
			number = std::numeric_limits<double>::quiet_NaN();
			return ENIVT_NULL;
		}

		auto en_type = en->GetType();
		if(en_type == ENT_NULL)
		{
			number = std::numeric_limits<double>::quiet_NaN();
			return ENIVT_NULL;
		}

		if(en_type == ENT_NUMBER)
		{
			number = en->GetNumberValueReference();
			return ENIVT_NUMBER;
		}

		if(en_type == ENT_STRING)
		{
			stringID = en->GetStringIDReference();
			return ENIVT_STRING_ID;
		}

		code = en;
		return ENIVT_CODE;
	}

	static bool AreEqual(EvaluableNodeImmediateValueType type_1, EvaluableNodeImmediateValue &value_1,
		EvaluableNodeImmediateValueType type_2, EvaluableNodeImmediateValue &value_2)
	{
		if(type_1 != type_2)
			return false;

		//types are the same, just use type_1 for reference
		if(type_1 == ENIVT_NULL)
			return true;
		else if(type_1 == ENIVT_NUMBER)
			return (value_1.number == value_2.number);
		else if(type_1 == ENIVT_STRING_ID)
			return (value_1.stringID == value_2.stringID);
		else if(type_1 == ENIVT_NUMBER_INDIRECTION_INDEX || type_1 == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return (value_1.indirectionIndex == value_2.indirectionIndex);
		else
			return EvaluableNode::AreDeepEqual(value_1.code, value_2.code);
	}

	//returns true if it is a null or null equivalent
	static constexpr bool IsNull(EvaluableNodeImmediateValueType type, EvaluableNodeImmediateValue &value)
	{
		return type == ENIVT_NULL;
	}

	operator double()
	{
		return number;
	}

	operator StringInternPool::StringID()
	{
		return stringID;
	}

	double number;
	StringInternPool::StringID stringID;
	EvaluableNode *code;
	size_t indirectionIndex;
};

//used for storing a value and type together
class EvaluableNodeImmediateValueWithType
{
public:
	constexpr EvaluableNodeImmediateValueWithType()
		: nodeType(ENIVT_NULL)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType(EvaluableNodeImmediateValue node_value,
		EvaluableNodeImmediateValueType node_type)
		: nodeType(node_type), nodeValue(node_value)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType(bool value)
	{
		nodeType = ENIVT_NUMBER;
		if(value)
			nodeValue.number = 1.0;
		else
			nodeValue.number = 0.0;
	}

	__forceinline EvaluableNodeImmediateValueWithType(double number)
	{
		if(FastIsNaN(number))
			nodeType = ENIVT_NULL;
		else
		{
			nodeType = ENIVT_NUMBER;
			nodeValue = number;
		}
	}

	__forceinline EvaluableNodeImmediateValueWithType(StringInternPool::StringID string_id)
	{
		if(string_id == StringInternPool::NOT_A_STRING_ID)
			nodeType = ENIVT_NULL;
		else
		{
			nodeType = ENIVT_STRING_ID;
			nodeValue = string_id;
		}
	}

	constexpr EvaluableNodeImmediateValueWithType(EvaluableNode *code)
		: nodeType(ENIVT_CODE), nodeValue(code)
	{	}

	constexpr EvaluableNodeImmediateValueWithType(const EvaluableNodeImmediateValueWithType &enimvwt)
		: nodeType(enimvwt.nodeType), nodeValue(enimvwt.nodeValue)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType &operator =(const EvaluableNodeImmediateValueWithType &enimvwt)
	{
		nodeType = enimvwt.nodeType;
		nodeValue = enimvwt.nodeValue;
		return *this;
	}

	//copies the value from en and returns the EvaluableNodeConcreteValueType
	void CopyValueFromEvaluableNode(EvaluableNode *en)
	{
		if(en == nullptr)
		{
			nodeType = ENIVT_NULL;
			nodeValue = EvaluableNodeImmediateValue(std::numeric_limits<double>::quiet_NaN());
			return;
		}

		auto en_type = en->GetType();
		if(en_type == ENT_NULL)
		{
			nodeType = ENIVT_NULL;
			nodeValue = EvaluableNodeImmediateValue(std::numeric_limits<double>::quiet_NaN());
			return;
		}

		if(en_type == ENT_NUMBER)
		{
			nodeType = ENIVT_NUMBER;
			nodeValue = EvaluableNodeImmediateValue(en->GetNumberValueReference());
			return;
		}

		if(en_type == ENT_STRING)
		{
			nodeType = ENIVT_STRING_ID;
			nodeValue = EvaluableNodeImmediateValue(en->GetStringIDReference());
			return;
		}

		nodeType = ENIVT_CODE;
		nodeValue = EvaluableNodeImmediateValue(en);
	}

	bool GetValueAsBoolean()
	{
		if(nodeType == ENIVT_NUMBER)
		{
			if(nodeValue.number == 0.0)
				return false;
			return true;
		}

		if(nodeType == ENIVT_STRING_ID)
		{
			if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID
					|| nodeValue.stringID == string_intern_pool.emptyStringId)
				return false;
			return true;
		}

		if(nodeType == ENIVT_CODE)
			return EvaluableNode::IsTrue(nodeValue.code);

		//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
		return false;
	}

	double GetValueAsNumber(double value_if_null = std::numeric_limits<double>::quiet_NaN())
	{
		if(nodeType == ENIVT_NUMBER)
			return nodeValue.number;

		if(nodeType == ENIVT_STRING_ID)
		{
			if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID)
				return value_if_null;

			auto str = string_intern_pool.GetStringFromID(nodeValue.stringID);
			auto [value, success] = Platform_StringToNumber(str);
			if(success)
				return value;
			return value_if_null;
		}

		if(nodeType == ENIVT_CODE)
			return EvaluableNode::ToNumber(nodeValue.code);

		//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
		return value_if_null;
	}

	std::pair<bool, std::string> GetValueAsString()
	{
		if(nodeType == ENIVT_NUMBER)
			return std::make_pair(true, EvaluableNode::NumberToString(nodeValue.number));

		if(nodeType == ENIVT_STRING_ID)
		{
			if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID)
				return std::make_pair(false, "");

			auto str = string_intern_pool.GetStringFromID(nodeValue.stringID);
			return std::make_pair(true, str);
		}

		if(nodeType == ENIVT_CODE)
			return std::make_pair(true, EvaluableNode::ToStringPreservingOpcodeType(nodeValue.code));

		//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
		return std::make_pair(false, "");
	}

	StringInternPool::StringID GetValueAsStringIDIfExists()
	{
		if(nodeType == ENIVT_NUMBER)
		{
			const std::string str_value = EvaluableNode::NumberToString(nodeValue.number);
			//will return empty string if not found
			return string_intern_pool.GetIDFromString(str_value);
		}

		if(nodeType == ENIVT_STRING_ID)
			return nodeValue.stringID;

		if(nodeType == ENIVT_CODE)
			return EvaluableNode::ToStringIDIfExists(nodeValue.code);

		//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
		return string_intern_pool.NOT_A_STRING_ID;
	}

	StringInternPool::StringID GetValueAsStringIDWithReference()
	{
		if(nodeType == ENIVT_NUMBER)
		{
			const std::string str_value = EvaluableNode::NumberToString(nodeValue.number);
			//will return empty string if not found
			return string_intern_pool.CreateStringReference(str_value);
		}

		if(nodeType == ENIVT_STRING_ID)
			return string_intern_pool.CreateStringReference(nodeValue.stringID);

		if(nodeType == ENIVT_CODE)
			return EvaluableNode::ToStringIDWithReference(nodeValue.code);

		//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
		return string_intern_pool.NOT_A_STRING_ID;
	}

	static inline bool AreEqual(EvaluableNodeImmediateValueWithType &a, EvaluableNodeImmediateValueWithType &b)
	{
		return EvaluableNodeImmediateValue::AreEqual(a.nodeType, a.nodeValue, b.nodeType, b.nodeValue);
	}

	//returns true if it is a null or null equivalent
	constexpr bool IsNull()
	{
		return EvaluableNodeImmediateValue::IsNull(nodeType, nodeValue);
	}

	EvaluableNodeImmediateValueType nodeType;
	EvaluableNodeImmediateValue nodeValue;
};
