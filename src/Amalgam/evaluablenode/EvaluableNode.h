#pragma once

//project headers:
#include "FastMath.h"
#include "OrderedHashMap.h"
#include "HashMaps.h"
#include "OpcodeDetails.h"
#include "Opcodes.h"
#include "PlatformSpecific.h"
#include "StringInternPool.h"

//system headers:
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
	using SmallAssocType = VectorMap<StringInternPool::StringID, EvaluableNode *>;
	using LargeAssocType = OrderedHashMap<CompactHashMap, CompactHashSet, StringInternPool::StringID, EvaluableNode *>;
	class AssocRef;

	//EvaluableNode ordered storage
	using OrderedType = std::vector<EvaluableNode *>;
	using OrderedRef = OrderedType &;

	//Storage for labels
	using LabelsAssocType = CompactHashMap<StringInternPool::StringID, EvaluableNode *>;

	using AttributeStorageType = uint8_t;
	enum class Attribute : AttributeStorageType
	{
		NONE = 0,
		//if true, then contains an extended type
		HAS_EXTENDED_VALUE = 1 << 0,
		//if true, then this node and any nodes it contains may have a cycle so needs to be checked
		NEED_CYCLE_CHECK = 1 << 1,
		//if true, then this node and any nodes it contains are idempotent
		IDEMPOTENT = 1 << 2,
		//if true, then the node is marked for concurrency
		CONCURRENT = 1 << 3,
		//if true, then the node has not yet been read/accessed and can be freed
		//used to optimize flows to avoid copies when there has been no other accesses
		FREEABLE = 1 << 4,
		//if true, then the top node has not yet been read/accessed and can be freed
		//used to optimize flows to avoid copies when there has been no other accesses
		FREEABLE_TOP_NODE = 1 << 5,
		//if true, then known to be in use with regard to garbage collection
		KNOWN_TO_BE_IN_USE = 1 << 6,
		ALL = HAS_EXTENDED_VALUE | NEED_CYCLE_CHECK | IDEMPOTENT | CONCURRENT
				| FREEABLE | FREEABLE_TOP_NODE | KNOWN_TO_BE_IN_USE
	};

	//constructors
	__forceinline EvaluableNode()
	{
		InitializeUnallocated();
	}
	__forceinline EvaluableNode(EvaluableNodeType type, const std::string &string_value)
	{
		InitializeType(type, string_value);
	}
	__forceinline EvaluableNode(double value)
	{
		InitializeType(value);
	}
	__forceinline EvaluableNode(EvaluableNodeType type)
	{
		InitializeType(type);
	}
	__forceinline EvaluableNode(EvaluableNode *n, bool copy_metadata = true)
	{
		InitializeType(n, copy_metadata);
	}

	__forceinline ~EvaluableNode()
	{
		if(!IsNodeDeallocated())
			Invalidate();
	}

	//clears out all data and makes the unusable in the ENT_DEALLOCATED state
	inline void Invalidate();

	///////////////////////////////////////////
	//Each InitializeType* sets up a given type with appropriate data
	inline void InitializeType(EvaluableNodeType _type, const std::string &string_value);

	inline void InitializeType(EvaluableNodeType _type, const std::string_view string_value);

	inline void InitializeType(EvaluableNodeType _type, StringInternPool::StringID string_id);

	//like InitializeType, but hands off the string reference to string_id
	inline void InitializeTypeWithReferenceHandoff(EvaluableNodeType _type, StringInternPool::StringID string_id);

	inline void InitializeType(double number_value);

	inline void InitializeType(bool bool_value);

	//initializes to ENT_UNINITIALIZED
	//useful to mark a node in a hold state before it's ready so it isn't counted as ENT_DEALLOCATED
	//but also such that the fields don't need to be initialized or cleared
	__forceinline constexpr void InitializeUnallocated();

	inline void InitializeType(EvaluableNodeType _type);

	//sets the value of the node to that of n and copies metadata if copy_metadata is true
	void InitializeType(EvaluableNode *n, bool copy_metadata = true);

	//copies the EvaluableNode n into this.  Does not overwrite labels or comments.
	void CopyValueFrom(EvaluableNode *n);

	//copies the metadata of the node n into this
	void CopyMetadataFrom(EvaluableNode *n);

	//copies the entire node n into this
	__forceinline void CopyNodeFrom(EvaluableNode *n)
	{
		CopyMetadataFrom(n);
		CopyValueFrom(n);
	}

	//clears annotations and comments
	__forceinline void ClearAnnotationsAndComments();

	//clears the node's metadata
	__forceinline void ClearMetadata();

	//returns true if the node has any metadata
	__forceinline bool HasMetadata();

	//Returns true if the immediate data structure of a is equal to b
	static inline bool AreShallowEqual(EvaluableNode *a, EvaluableNode *b);

	//Returns true if the entire data structure of a is equal in value to the data structure of b
	static inline bool AreDeepEqual(EvaluableNode *a, EvaluableNode *b);

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

	static __forceinline bool IsImmediate(EvaluableNode *n)
	{
		return (n == nullptr || n->IsImmediate());
	}

	//returns true if the type is terminal (immediate or symbol)
	__forceinline bool IsTerminal()
	{
		return IsEvaluableNodeTypeTerminalNode(GetType());
	}

	static __forceinline bool IsTerminal(EvaluableNode *n)
	{
		return (n == nullptr || n->IsTerminal());
	}

	//returns true if the node is some form of ordered array
	__forceinline bool IsOrderedArray()
	{
		return DoesEvaluableNodeTypeUseOrderedData(GetType());
	}

	//returns true if the node is some form of ordered array
	static __forceinline bool IsOrderedArray(EvaluableNode *n)
	{
		if(n == nullptr)
			return false;
		return n->IsOrderedArray();
	}

	//returns true if the node is a string
	static __forceinline bool IsString(EvaluableNode *n)
	{
		if(n == nullptr)
			return false;
		return n->GetType() == ENT_STRING;
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
	static __forceinline bool CanRepresentValueAsANumber(EvaluableNode *e);

	//returns true if e is nullptr or value of e has type ENT_NULL
	static __forceinline bool IsNull(EvaluableNode *e)
	{
		return (e == nullptr || e->GetType() == ENT_NULL);
	}

	//Returns true if this node evaluates to true
	static bool ToBool(EvaluableNode *n);

	//Converts a bool value to a string
	//if key_string is false, will return the appropriate string as code
	//if key_string is true, will return for use as a key string in an ENT_ASSOC
	static std::string BoolToString(bool value, bool key_string = false);
	static StringInternPool::StringID BoolToStringID(bool value, bool key_string = false);

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
	static std::string NumberToString(double value, bool key_string = false);
	static std::string NumberToString(size_t value, bool key_string = false);
	static StringInternPool::StringID NumberToStringIDIfExists(double value, bool key_string = false);
	static StringInternPool::StringID NumberToStringIDIfExists(size_t value, bool key_string = false);

	//converts the node to a string
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	static std::string ToString(EvaluableNode *e, bool key_string = false);

	//converts the node to a string, returning true if it is a valid string
	static std::pair<bool, std::string> ToValidString(EvaluableNode *e);

	//converts node to an existing string. If it doesn't exist or it's null, it returns NOT_A_STRING_ID
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	static StringInternPool::StringID ToStringIDIfExists(EvaluableNode *e, bool key_string = false);

	//converts node to a string. Creates a reference to the string that must be destroyed, regardless of whether the
	// string existed or not (if it did not exist, then it creates one)
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	static StringInternPool::StringID ToStringIDWithReference(EvaluableNode *e, bool key_string = false);

	//converts node to a string. Creates a reference to the string that must be destroyed, regardless of whether the
	// string existed or not
	//if e is a string, it will clear it and hand the reference to the caller
	//if include_symbol is true, then it will also apply to ENT_SYMBOL
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	static StringInternPool::StringID ToStringIDTakingReferenceAndClearing(EvaluableNode *e,
		bool include_symbol = false, bool key_string = false);

	//converts the node to an ENT_ASSOC where the keys are the numbers of the indices
	void ConvertListToNumberedAssoc();

	//converts the node from an ENT_ASSOC to an ENT_LIST
	void ConvertAssocToList();

	//returns true if the node can be flattened,
	// that is, contains no cycles when traversing downward and potentially
	// duplicating nodes if they are referenced more than once
	static inline bool CanNodeTreeBeFlattened(EvaluableNode *n)
	{
		if(n == nullptr)
			return true;

		if(!n->GetNeedCycleCheck())
			return true;

		return CanNodeTreeBeFlattenedRecurse(n, reusableBuffer);
	}

	//Returns the number of nodes in the data structure
	static inline size_t GetDeepSize(EvaluableNode *n)
	{
		if(n == nullptr || n->IsTerminal())
			return 1;

		if(!n->GetNeedCycleCheck())
		{
			return GetDeepSizeNoCycles(n);
		}
		else
		{
			ReferenceSetType checked;
			return GetDeepSizeWithCycles(n, checked);
		}
	}

	//Returns the number of bytes of memory that node is currently using
	static size_t GetEstimatedNodeSizeInBytes(EvaluableNode *n);

	//gets current type
	__forceinline EvaluableNodeType &GetType()
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		AmlgAssert(type != ENT_DEALLOCATED);
	#endif
		return type;
	}

	//returns true if the node is currently deallocated
	__forceinline constexpr bool IsNodeDeallocated()
	{
		return (type == ENT_DEALLOCATED);
	}

	//returns true if the node is a valid type and has valid data structures
	bool IsNodeValid();

	//transforms node to new_type, converting data if types are different
	//if attempt_to_preserve_value is true, then it will try to preserve any relevant value or values
	// attempt_to_preserve_value should be set to false if the value will be immediately overwritten
	void SetType(EvaluableNodeType new_type, bool attempt_to_preserve_value);

	//sets up null value
	inline void InitNullValue()
	{
		DestructValue();
		value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
		AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
	}

	//sets up boolean value
	inline void InitBoolValue()
	{
		DestructValue();
		value.boolValueContainer.boolValue = false;
		AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
	}

	//gets the value by reference
	__forceinline bool &GetBoolValue()
	{
		if(DoesEvaluableNodeTypeUseBoolData(GetType()))
			return GetBoolValueReference();

		//none of the above, return an empty one
		return falseBoolValue;
	}

	//changes the type by setting it to the number value specified
	inline void SetTypeViaBoolValue(bool v, bool clear_metadata = true)
	{
		if(clear_metadata)
			ClearMetadata();
		SetType(ENT_BOOL, false);
		GetBoolValueReference() = v;
	}

	//sets up number value
	inline void InitNumberValue()
	{
		DestructValue();
		value.numberAndNullValueContainer.numberValue = 0.0;
		AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
	}

	//gets the value by reference
	__forceinline double &GetNumberValue()
	{
		if(DoesEvaluableNodeTypeUseNumberData(GetType()))
			return GetNumberValueReference();

		//none of the above, return an empty one
		return nanNumberValue;
	}

	//changes the type by setting it to the number value specified
	inline void SetTypeViaNumberValue(double v, bool clear_metadata = true);

	//changes the type by setting it to the string value specified
	inline void SetTypeViaStringIdValue(std::string &v, bool clear_metadata = true);

	//changes the type by setting it to the string id value specified
	inline void SetTypeViaStringIdValue(StringInternPool::StringID v, bool clear_metadata = true);

	//changes the type by setting it to the string id value specified, handing off the reference
	inline void SetTypeViaStringIdValueWithReferenceHandoff(StringInternPool::StringID v, bool clear_metadata = true);

	//sets up the ability to contain a string
	inline void InitStringValue();

	__forceinline StringInternPool::StringID GetStringID();

	void SetStringID(StringInternPool::StringID id);
	std::string_view GetStringView();
	void SetStringValue(const std::string &v);
	//gets the string ID and clears the node's string ID, but does not destroy the string reference,
	// leaving the reference handling up to the caller
	StringInternPool::StringID GetAndClearStringIDWithReference();
	//sets the string but does not create a new reference because the reference has already been created
	void SetStringIDWithReferenceHandoff(StringInternPool::StringID id);

	//returns true if has annotation
	inline bool HasAnnotations()
	{
		return GetAnnotationsAndCommentsStorage().HasAnnotations();
	}

	//returns a string_view of the annotation string
	inline std::string_view GetAnnotationsString()
	{
		return GetAnnotationsAndCommentsStorage().GetAnnotations();
	}

	static inline std::string_view GetAnnotationsString(EvaluableNode *en)
	{
		if(en == nullptr)
			return std::string_view();
		return en->GetAnnotationsAndCommentsStorage().GetAnnotations();
	}

	//sets the annotation_string
	inline void SetAnnotationsString(std::string_view s)
	{
		EnsureHasExtendedValue();
		GetAnnotationsAndCommentsStorage().SetAnnotations(s);
	}

	inline void ClearAnnotations()
	{
		GetAnnotationsAndCommentsStorage().SetAnnotations("");
	}

	//appends annotations to the node
	void AppendAnnotations(std::string &annotations)
	{
		EnsureHasExtendedValue();

		auto &a_and_c = GetAnnotationsAndCommentsStorage();
		std::string combined(a_and_c.GetAnnotations());
		combined.append(annotations);
		a_and_c.SetAnnotations(combined);
	}

	//functions for getting and setting node comments by string
	inline std::string_view GetCommentsString()
	{
		return GetAnnotationsAndCommentsStorage().GetComments();
	}

	static inline std::string_view GetCommentsString(EvaluableNode *en)
	{
		if(en == nullptr)
			return std::string_view();
		return en->GetAnnotationsAndCommentsStorage().GetComments();
	}

	//returns true if has comments
	inline bool HasComments()
	{
		return GetAnnotationsAndCommentsStorage().HasComments();
	}

	inline void SetCommentsString(const std::string &comment)
	{
		EnsureHasExtendedValue();
		GetAnnotationsAndCommentsStorage().SetComments(comment);
	}

	inline void ClearComments()
	{
		GetAnnotationsAndCommentsStorage().SetComments("");
	}

	//appends comments to the node
	void AppendComments(std::string &comments)
	{
		EnsureHasExtendedValue();

		auto &a_and_c = GetAnnotationsAndCommentsStorage();
		std::string combined(a_and_c.GetComments());
		combined.append(comments);
		a_and_c.SetComments(combined);
	}

	__forceinline bool HasAttribute(Attribute attr) const
	{
		return (attributes & static_cast<AttributeStorageType>(attr)) != 0;
	}

	__forceinline void SetAttribute(Attribute attr, bool enable = true)
	{
		if(enable)
			attributes |= static_cast<AttributeStorageType>(attr);
		else
			attributes &= ~static_cast<AttributeStorageType>(attr);
	}

#ifdef MULTITHREAD_SUPPORT
	__forceinline bool HasAttributeAtomic(Attribute attr);

	__forceinline void SetAttributeAtomic(Attribute attr, bool enable = true);

	//returns true if the bit was successfully set (was previously unset)
	//returns false if the bit was already set
	__forceinline bool TrySetAttributeAtomic(Attribute attr);
#endif

	//returns true if the EvaluableNode is marked with preference for concurrency
	__forceinline bool GetConcurrency()
	{
		return HasAttribute(Attribute::CONCURRENT);
	}

	//sets the EvaluableNode's preference for concurrency
	__forceinline void SetConcurrency(bool concurrent)
	{
		SetAttribute(Attribute::CONCURRENT, concurrent);
	}

	//returns true if the EvaluableNode and all its dependents need to be checked for cycles
	__forceinline bool GetNeedCycleCheck()
	{
		return HasAttribute(Attribute::NEED_CYCLE_CHECK);
	}

	//sets the EvaluableNode's needCycleCheck flag
	__forceinline void SetNeedCycleCheck(bool need_cycle_check)
	{
		SetAttribute(Attribute::NEED_CYCLE_CHECK, need_cycle_check);
	}

	//returns true if the EvaluableNode and all its dependents are idempotent
	__forceinline bool GetIsIdempotent()
	{
		return HasAttribute(Attribute::IDEMPOTENT);
	}

	//sets the EvaluableNode's idempotency flag
	__forceinline void SetIsIdempotent(bool is_idempotent)
	{
		SetAttribute(Attribute::IDEMPOTENT, is_idempotent);
	}

	//returns true if the node has never been read / accessed
	__forceinline bool GetIsFreeable()
	{
		return HasAttribute(Attribute::FREEABLE);
	}

	//sets whether the node has never been read / accessed
	//returns the previous value
	__forceinline bool SetIsFreeable(bool is_freeable)
	{
		bool old_value = HasAttribute(Attribute::FREEABLE);
		SetAttribute(Attribute::FREEABLE, is_freeable);
		return old_value;
	}

#ifdef MULTITHREAD_SUPPORT
	//returns true if the node has never been read / accessed
	__forceinline bool GetIsFreeableAtomic()
	{
		return HasAttributeAtomic(Attribute::FREEABLE);
	}

	//sets whether the node has never been read / accessed
	//returns the previous value
	__forceinline bool SetIsFreeableAtomic(bool is_freeable);
#endif

	//returns true if the top node has never been read / accessed
	__forceinline bool GetIsFreeableTopNode()
	{
		return HasAttribute(Attribute::FREEABLE_TOP_NODE);
	}

	//sets whether the top node has never been read / accessed
	//returns the previous value
	__forceinline bool SetIsFreeableTopNode(bool is_freeable)
	{
		bool old_value = HasAttribute(Attribute::FREEABLE_TOP_NODE);
		SetAttribute(Attribute::FREEABLE_TOP_NODE, is_freeable);
		return old_value;
	}

#ifdef MULTITHREAD_SUPPORT
	//returns true if the top node has never been read / accessed
	__forceinline bool GetIsFreeableTopNodeAtomic()
	{
		return HasAttributeAtomic(Attribute::FREEABLE_TOP_NODE);
	}

	//sets whether the top node has never been read / accessed
	//returns the previous value
	__forceinline bool SetIsFreeableTopNodeAtomic(bool is_freeable);
#endif

	//sets both FREEABLE and FREEABLE_TOP_NODE
	//returns previous values of the flags in order
	__forceinline std::pair<bool, bool> SetIsFreeableAndIsFreeableTopNode(bool is_freeable);

#ifdef MULTITHREAD_SUPPORT
	//sets both FREEABLE and FREEABLE_TOP_NODE
	//returns previous values of the flags in order
	__forceinline std::pair<bool, bool> SetIsFreeableAndIsFreeableTopNodeAtomic(bool is_freeable);
#endif

	//returns whether this node has been marked as known to be currently in use
	__forceinline bool GetKnownToBeInUse()
	{
		return HasAttribute(Attribute::KNOWN_TO_BE_IN_USE);
	}

	//sets whether this node is currently known to be in use
	__forceinline void SetKnownToBeInUse(bool in_use)
	{
		SetAttribute(Attribute::KNOWN_TO_BE_IN_USE, in_use);
	}

#ifdef MULTITHREAD_SUPPORT
	//returns whether this node has been marked as known to be currently in use
	__forceinline bool GetKnownToBeInUseAtomic()
	{
		return HasAttributeAtomic(Attribute::KNOWN_TO_BE_IN_USE);
	}

	//sets whether this node is currently known to be in use
	__forceinline void SetKnownToBeInUseAtomic(bool in_use)
	{
		SetAttributeAtomic(Attribute::KNOWN_TO_BE_IN_USE, in_use);
	}

	//returns true if the bit was successfully set (was previously unset)
	//returns false if the bit was already set
	__forceinline bool TrySetKnownToBeInUseAtomic()
	{
		return TrySetAttributeAtomic(Attribute::KNOWN_TO_BE_IN_USE);
	}
#endif

	//returns true if value contains an extended type
	__forceinline bool HasExtendedValue()
	{
		return HasAttribute(Attribute::HAS_EXTENDED_VALUE);
	}

	//sets whether this node contains an extended type
	__forceinline void SetExtendedValue(bool extended_value)
	{
		SetAttribute(Attribute::HAS_EXTENDED_VALUE, extended_value);
	}

	//returns the number of child nodes regardless of mapped or ordered
	size_t GetNumChildNodes();

	//updates all flags as appropriate given that a newly allocated
	// child_node is being added as a child to this node
	__forceinline void UpdateFlagsBasedOnNewChildNode(EvaluableNode *new_child);

	//assumes all child nodes (if any) do not reference this node and all their
	//flags are correct and updates this node's flags
	__forceinline void UpdateAllFlagsBasedOnNoReferencingChildNodes();

	inline void InitOrderedChildNodes();

	//preallocates to_reserve for appending, etc.
	inline void ReserveOrderedChildNodes(size_t to_reserve)
	{
		if(IsOrderedArray())
			GetOrderedChildNodesReference().reserve(to_reserve);
	}

	__forceinline OrderedRef GetOrderedChildNodes()
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
	static inline void ConvertChildNodesAndStoreValue(EvaluableNode *node,
		std::vector<StringInternPool::StringID> &element_names, size_t num_expected_elements,
		StoreValueFunction store_value);

	//Note that ResizeOrderedChildNodes does not initialize new nodes, so they must be initialized by caller
	inline void SetOrderedChildNodesSize(size_t new_size)
	{
		if(IsOrderedArray())
			GetOrderedChildNodesReference().resize(new_size);
	}

	//sets the ordered child nodes and updates flags
	void SetOrderedChildNodes(OrderedRef ocn, bool need_cycle_check = true, bool is_idempotent = false);
	//sets the ordered child nodes and updates flags, but can be used as an rvalue so that the memory doesn't
	//need to be reallocated if std::move is used for the input
	void SetOrderedChildNodes(OrderedType &&ocn, bool need_cycle_check = true, bool is_idempotent = false);
	template<typename ContainerIterator>
	inline void SetOrderedChildNodes(
		ContainerIterator first, ContainerIterator last, bool need_cycle_check, bool is_idempotent);

	void ClearOrderedChildNodes();
	void AppendOrderedChildNode(EvaluableNode *cn);
	void AppendOrderedChildNodes(OrderedRef ocn_to_append);

	//if the OrderedChildNodes list was using extra memory (if it were resized to be smaller), this would attempt to free extra memory
	inline void ReleaseOrderedChildNodesExtraMemory();

	//sets up mapped child nodes
	//optionally can pass in the number of elements so it can
	//automatically set up large vs small assoc
	inline void InitMappedChildNodes(size_t num_elements = 0);

	//preallocates to_reserve for appending, etc.
	inline void ReserveMappedChildNodes(size_t to_reserve);

	//returns a view of the mapped child nodes; should be treated as read-only
	//(which means the receiving variables should not have an &)
	//if the node is of not of type ENT_ASSOC, will return a view to an empty map
	__forceinline AssocRef GetMappedChildNodesView();

	//if the id exists, returns a pointer to the pointer of the child node
	// returns nullptr if the id doesn't exist
	inline EvaluableNode **GetMappedChildNode(const std::string &id);

	//if the id exists, returns a pointer to the pointer of the child node
	// returns nullptr if the id doesn't exist
	inline EvaluableNode **GetMappedChildNode(const StringInternPool::StringID sid);

	//returns a pointer to the pointer of the child node, creating it if necessary and populating it with a nullptr
	EvaluableNode **GetOrCreateMappedChildNode(const std::string &id);
	//returns a pointer to the pointer of the child node, creating it if necessary and populating it with a nullptr
	EvaluableNode **GetOrCreateMappedChildNode(const StringInternPool::StringID sid);
	// if copy is set to true, then it will copy the map, otherwise it will swap
	void SetMappedChildNodes(AssocRef new_mcn, bool copy,
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
	void AppendMappedChildNodes(AssocRef mcn_to_append);

	//helper function to obtain a typed value from mapped child nodes
	//note that it can only be used on string key lookups, no code or numeric keys
	template<typename T>
	static void GetValueFromMappedChildNodesReference(
		EvaluableNode::AssocRef mcn, EvaluableNodeBuiltInStringId key, T &value);

protected:
	//defined since it is used as a pointer
	class AnnotationsAndComments;
public:

	//assumes that the EvaluableNode is of type ENT_BOOL, and returns the value by reference
	__forceinline bool &GetBoolValueReference();

	//assumes that the EvaluableNode is of type ENT_NUMBER, and returns the value by reference
	__forceinline double &GetNumberValueReference();

	//assumes that the EvaluableNode is of type that holds a string, and returns the value by reference
	__forceinline StringInternPool::StringID &GetStringIDReference();

	//assumes that the EvaluableNode has ordered child nodes, and returns the value by reference
	__forceinline OrderedRef GetOrderedChildNodesReference();

	//assumes that the EvaluableNode is has mapped child nodes and returns the view
	//(which means the receiving variables should not have an &)
	__forceinline AssocRef GetMappedChildNodesViewOnAssoc();

	//if it is storing an immediate value and has room to store a label
	inline bool HasCompactAnnotationsAndCommentsStorage()
	{
		return (type == ENT_NULL || type == ENT_BOOL || type == ENT_NUMBER || type == ENT_STRING || type == ENT_SYMBOL);
	}

	//returns a reference to the storage location for the annotation and comment storage
	// will only return valid results if HasCompactAnnotationsAndCommentsStorage() is true, so that should be called first
	__forceinline AnnotationsAndComments &GetAnnotationsAndCommentsStorage();

	//registers and unregisters an EvaluableNode for debug watching
	static inline void RegisterEvaluableNodeForDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		debugWatch.emplace(en);
	}

	static inline void UnregisterEvaluableNodeForDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		debugWatch.erase(en);
	}

	//returns true if the EvaluableNode is in the debug watch
	static inline void AssertIfInDebugWatch(EvaluableNode *en)
	{
	#if defined(MULTITHREAD_SUPPORT)
		Concurrency::SingleLock lock(debugWatchMutex);
	#endif
		if(debugWatch.find(en) != end(debugWatch)) [[unlikely]]
		{
			AmlgAssert(false);
		}
	}

protected:

	//combines annotations and comments into a single string to minimize storage overhead rather
	// than minimize compute time; each retrieval time is linear in the length of the strings
	//the two strings are separated by a null terminator and end with a null terminator,
	// so it is faster to retrieve both together than one and then the other
	class AnnotationsAndComments
	{
	public:
		static __forceinline void Construct(AnnotationsAndComments &a_and_c)
		{
			new (&a_and_c) AnnotationsAndComments;
		}

		static __forceinline void Destruct(AnnotationsAndComments &a_and_c)
		{
			a_and_c.~AnnotationsAndComments();
		}

		AnnotationsAndComments() = default;
		__forceinline AnnotationsAndComments(std::string_view annotation, std::string_view comment)
		{
			SetAnnotationsAndComments(annotation, comment);
		}

		__forceinline void Clear()
		{
			buffer.reset();
		}

		//returns a view of the annotations
		std::string_view GetAnnotations()
		{
			if(!buffer)
				return {};
			const char *p = buffer.get();
			std::size_t len = std::strlen(p);
			return std::string_view(p, len);
		}

		//returns a view of the comments
		std::string_view GetComments()
		{
			if(!buffer)
				return {};
			const char *p = buffer.get();
			//skip past annotation and its terminating '\0'
			p += std::strlen(p) + 1;
			std::size_t len = std::strlen(p);
			return std::string_view(p, len);
		}

		//gets both annotations and comments more efficiently than getting separately
		std::pair<std::string_view, std::string_view> GetAnnotationsAndComments()
		{
			if(!buffer)
				return { {}, {} };

			const char *p = buffer.get();
			std::size_t ann_len = std::strlen(p);
			const char *comment_ptr = p + ann_len + 1;
			std::size_t com_len = std::strlen(comment_ptr);
			return {
				std::string_view(p, ann_len),
				std::string_view(comment_ptr, com_len)
			};
		}

		//replace both strings
		void SetAnnotationsAndComments(std::string_view new_annotation, std::string_view new_comment)
		{
			if(new_annotation.empty() && new_comment.empty())
			{
				buffer.reset();
				return;
			}

			//total size includes two null terminators
			std::size_t total_size = new_annotation.size() + 1 + new_comment.size() + 1;
			auto tmp = std::make_unique<char[]>(total_size);

			char *dest = tmp.get();

			//copy annotation
			std::memcpy(dest, new_annotation.data(), new_annotation.size());
			dest[new_annotation.size()] = '\0';

			//copy comment
			std::memcpy(dest + new_annotation.size() + 1, new_comment.data(), new_comment.size());
			dest[total_size - 1] = '\0';

			buffer = std::move(tmp);
		}

		//replace only the annotations
		__forceinline void SetAnnotations(std::string_view new_annotations)
		{
			SetAnnotationsAndComments(new_annotations, GetComments());
		}

		//replace only the comments
		__forceinline void SetComments(std::string_view new_comments)
		{
			SetAnnotationsAndComments(GetAnnotations(), new_comments);
		}

		__forceinline bool HasAnnotations()
		{
			return buffer && buffer[0] != '\0';
		}

		inline bool HasComments()
		{
			if(!buffer)
				return false;

			const char *p = buffer.get() + std::strlen(buffer.get()) + 1;
			return *p != '\0';
		}

		//slightly more efficient than HasAnnotations() || HasComments()
		bool HasCommentOrAnnotation() const noexcept
		{
			if(!buffer)
				return false;

			const char *p = buffer.get();
			if(*p != '\0')
				return true;

			p += std::strlen(p) + 1;
			return *p != '\0';
		}

	private:
		std::unique_ptr<char[]> buffer;
	};

	//align to the nearest 2-bytes to minimize alignment issues but reduce the overall memory footprint
	// while maintaining some alignment
#pragma pack(push, 2)
	union EvaluableNodeValue
	{
		//take care of all setup and cleanup outside of the union
		// default to numberAndNullValueContainer constructor to allow constexpr
		__forceinline  EvaluableNodeValue()
		{}
		__forceinline  ~EvaluableNodeValue()
		{}

		__forceinline void ConstructOrderedChildNodes()
		{
			new (&orderedChildNodes) OrderedType;
		}

		__forceinline void DestructOrderedChildNodes()
		{
			orderedChildNodes.~OrderedType();
		}

		__forceinline void ConstructMappedChildNodes()
		{
			new (&mappedChildNodes) SmallAssocType;
		}

		__forceinline void DestructMappedChildNodes()
		{
			string_intern_pool.DestroyStringReferences(mappedChildNodes, [](auto n) { return n.first; });
			mappedChildNodes.~SmallAssocType();
		}

		//ordered child nodes (when type requires it), meaning and number of childNodes is based on the type of the node
		OrderedType orderedChildNodes;

		//hash-mapped child nodes (when type requires it), meaning and number of childNodes is based on the type of the node
		SmallAssocType mappedChildNodes;

		//when type represents a string, holds the corresponding values
		struct EvaluableNodeValueString
		{
			//string value
			StringInternPool::StringID stringID;

			AnnotationsAndComments annotationsAndComments;
		} stringValueContainer;

		//when type represents a number, holds the corresponding value
		//ENT_NULL also uses this with a NaN
		struct EvaluableNodeValueNumber
		{
			//number value
			double numberValue;

			AnnotationsAndComments annotationsAndComments;
		} numberAndNullValueContainer;

		//when type represents a bool, holds the corresponding value
		struct EvaluableNodeValueBool
		{
			//bool value
			bool boolValue;

			AnnotationsAndComments annotationsAndComments;
		} boolValueContainer;

		struct EvaluableNodeValueOrderedChildNodesWithAnnotationsAndComments
		{
			__forceinline void Construct()
			{
				new (&orderedChildNodes) std::unique_ptr<OrderedType>(std::make_unique<OrderedType>());
				AnnotationsAndComments::Construct(annotationsAndComments);
			}

			__forceinline void Destruct()
			{
				orderedChildNodes.~unique_ptr<OrderedType>();
				AnnotationsAndComments::Destruct(annotationsAndComments);
			}

			//external orderedChildNodes
			std::unique_ptr<OrderedType> orderedChildNodes;

			AnnotationsAndComments annotationsAndComments;
		} extendedOrderedChildNodes;

		struct EvaluableNodeValueMappedChildNodesWithAnnotationsAndComments
		{
			__forceinline void Construct()
			{
				mappedChildNodes = std::make_unique<LargeAssocType>();
				AnnotationsAndComments::Construct(annotationsAndComments);
			}

			__forceinline void Destruct()
			{
				string_intern_pool.DestroyStringReferences(*mappedChildNodes, [](auto n) { return n.first; });
				mappedChildNodes.~unique_ptr<LargeAssocType>();
				AnnotationsAndComments::Destruct(annotationsAndComments);
			}

			//external orderedChildNodes
			std::unique_ptr<LargeAssocType> mappedChildNodes;

			AnnotationsAndComments annotationsAndComments;
		} extendedMappedChildNodes;
	};
#pragma pack(pop)

	//makes sure that the data structure has an extended value so that it can be used to hold additional data
	void EnsureHasExtendedValue();

	//removes the extended value data structure if it is no longer needed
	void RemoveExtendedValueIfPossible();

	//destructs the value so that the node can be reused
	// note that the value should be considered uninitialized
	inline void DestructValue();

	//assists the public function AreDeepEqual
	//returns true if the entire data structure of a is equal in value to the data structure of b
	// but does not check if nodes a and b are not null or immediate and are shallow equal (this is assumed to be done by the caller for performance)
	//if checked is nullptr, then it won't check for cycles
	static bool AreDeepEqualGivenShallowEqualAndNotImmediate(EvaluableNode *a, EvaluableNode *b, ReferenceAssocType *checked);

	//recursive helper function for CanNodeTreeBeFlattened
	// assumes n is not nullptr
	static bool CanNodeTreeBeFlattenedRecurse(EvaluableNode *n, std::vector<EvaluableNode *> &stack);

	//Returns the deep size, excluding nodes already checked
	// Assists the public function GetDeepSize
	static size_t GetDeepSizeWithCycles(EvaluableNode *n, ReferenceSetType &checked);

	//Like GetDeepSizeWithCycles, but assumes there are no cycles in n
	static size_t GetDeepSizeNoCycles(EvaluableNode *n);

	EvaluableNodeValue value;

	//Executable/data type of the node
	EvaluableNodeType type;

	//fields contained within the current set of data
	AttributeStorageType attributes;

	//when the number of elements in a SmallAssocType exceeds this value, it should be promoted to a LargeAssocType
	static constexpr size_t largestSmallAssocSize = 8;

	//values used to be able to return a reference
	static bool falseBoolValue;
	static double nanNumberValue;
	static std::string emptyStringValue;
	static OrderedType emptyOrderedChildNodes;
	static EvaluableNode emptyMappedChildNodesNode;
	static AnnotationsAndComments emptyAnnotationsAndComments;

public:
	//reusable memory pool for local operations
#if defined(MULTITHREAD_SUPPORT)
	thread_local
	#endif
		static inline std::vector<EvaluableNode *> reusableBuffer;
protected:

	//field for watching EvaluableNodes for debugging
	static FastHashSet<EvaluableNode *> debugWatch;
#if defined(MULTITHREAD_SUPPORT)
	static Concurrency::SingleMutex debugWatchMutex;
#endif
};

#include "EvaluableNodeAssocRef.h"
#include "EvaluableNodeInlines.h"
