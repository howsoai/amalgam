#pragma once

//project headers:
#include "FastMath.h"
#include "HashMaps.h"
#include "Opcodes.h"
#include "PlatformSpecific.h"
#include "StringInternPool.h"

//system headers:
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
		//if true, then known to be in use with regard to garbage collection
		KNOWN_TO_BE_IN_USE = 1 << 5,
		ALL = HAS_EXTENDED_VALUE | NEED_CYCLE_CHECK | IDEMPOTENT | CONCURRENT | FREEABLE | KNOWN_TO_BE_IN_USE
	};

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
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type));
	#endif

		type = _type;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		SetIsIdempotent(true);
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
	}

	inline void InitializeType(EvaluableNodeType _type, StringInternPool::StringID string_id)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type));
	#endif

		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		if(string_id == StringInternPool::NOT_A_STRING_ID)
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = _type;
			value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_id);
			AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
		}
	}

	//like InitializeType, but hands off the string reference to string_id
	inline void InitializeTypeWithReferenceHandoff(EvaluableNodeType _type, StringInternPool::StringID string_id)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type));
	#endif

		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		if(string_id == StringInternPool::NOT_A_STRING_ID)
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = _type;
			value.stringValueContainer.stringID = string_id;
			AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
		}
	}

	inline void InitializeType(double number_value)
	{
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		if(FastIsNaN(number_value))
		{
			type = ENT_NULL;
			value.ConstructOrderedChildNodes();
		}
		else
		{
			type = ENT_NUMBER;
			SetIsIdempotent(true);
			value.numberValueContainer.numberValue = number_value;
			AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
		}
	}

	inline void InitializeType(bool bool_value)
	{
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		type = ENT_BOOL;
		SetIsIdempotent(true);
		value.boolValueContainer.boolValue = bool_value;
		AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
	}

	//initializes to ENT_UNINITIALIZED
	//useful to mark a node in a hold state before it's ready so it isn't counted as ENT_DEALLOCATED
	//but also such that the fields don't need to be initialized or cleared
	__forceinline constexpr void InitializeUnallocated()
	{
		type = ENT_UNINITIALIZED;
	}

	inline void InitializeType(EvaluableNodeType _type)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type) || _type == ENT_DEALLOCATED);
	#endif

		type = _type;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		SetIsIdempotent(IsEvaluableNodeTypePotentiallyIdempotent(_type));

		if(DoesEvaluableNodeTypeUseBoolData(_type))
		{
			AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
			value.boolValueContainer.boolValue = false;
			SetIsIdempotent(true);
		}
		else if(DoesEvaluableNodeTypeUseNumberData(_type))
		{
			AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
			value.numberValueContainer.numberValue = 0.0;
			SetIsIdempotent(true);
		}
		else if(DoesEvaluableNodeTypeUseStringData(_type))
		{
			value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
			AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
			SetIsIdempotent(_type == ENT_STRING);
		}
		else if(DoesEvaluableNodeTypeUseAssocData(_type))
		{
			type = _type;
			SetIsIdempotent(true);
			value.ConstructMappedChildNodes();
		}
		else if(_type == ENT_DEALLOCATED)
		{
		#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
			//use a value that is more apparent that something went wrong
			value.numberValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
		#else
			value.numberValueContainer.numberValue = 0;
		#endif

			AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
		}
		else
		{
			value.ConstructOrderedChildNodes();
		}
	}

	//sets the value of the node to that of n and coppies metadata if copy_metadata is true
	void InitializeType(EvaluableNode *n, bool copy_metadata = true);

	//copies the EvaluableNode n into this.  Does not overwrite labels or comments.
	void CopyValueFrom(EvaluableNode *n);

	//copies the metadata of the node n into this
	void CopyMetadataFrom(EvaluableNode *n);

	//clears the node's metadata
	__forceinline void ClearMetadata()
	{
		GetAnnotationsAndCommentsStorage().Clear();
		SetConcurrency(false);
	}

	//returns true if the node has any metadata
	__forceinline bool HasMetadata()
	{
		auto &a_and_c = GetAnnotationsAndCommentsStorage();
		return (a_and_c.HasCommentOrAnnotation()  || GetConcurrency());
	}

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
		case ENT_BOOL:
		case ENT_NUMBER:
		case ENT_NULL:
			return true;
		default:
			return false;
		}
	}

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

	//converts the node to a key string that can be used in assocs
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	static std::string ToString(EvaluableNode *e, bool key_string = false);

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
	__forceinline constexpr bool IsNodeDeallocated()
	{
		return (type == ENT_DEALLOCATED);
	}

	//returns true if the node is a valid type and has valid data structures
	bool IsNodeValid();

	//transforms node to new_type, converting data if types are different
	// enm is used if it needs to allocate nodes when changing types
	// if enm is nullptr, then it will not necessarily keep child nodes
	//if attempt_to_preserve_immediate_value is true, then it will try to preserve any relevant immediate value
	// attempt_to_preserve_immediate_value should be set to false if the value will be immediately overwritten
	void SetType(EvaluableNodeType new_type, EvaluableNodeManager *enm,
		bool attempt_to_preserve_immediate_value);

	//sets up boolean value
	void InitBoolValue();

	//gets the value by reference
	__forceinline bool &GetBoolValue()
	{
		if(DoesEvaluableNodeTypeUseBoolData(GetType()))
			return GetBoolValueReference();

		//none of the above, return an empty one
		return falseBoolValue;
	}

	//changes the type by setting it to the number value specified
	inline void SetTypeViaBoolValue(bool v)
	{
		SetType(ENT_BOOL, nullptr, false);
		GetBoolValueReference() = v;
	}

	//sets up number value
	void InitNumberValue();

	//gets the value by reference
	__forceinline double &GetNumberValue()
	{
		if(DoesEvaluableNodeTypeUseNumberData(GetType()))
			return GetNumberValueReference();

		//none of the above, return an empty one
		return nanNumberValue;
	}

	//changes the type by setting it to the number value specified
	inline void SetTypeViaNumberValue(double v)
	{
		if(FastIsNaN(v))
		{
			SetType(ENT_NULL, nullptr, false);
		}
		else
		{
			SetType(ENT_NUMBER, nullptr, false);
			GetNumberValueReference() = v;
		}
	}

	//changes the type by setting it to the string id value specified
	inline void SetTypeViaStringIdValue(StringInternPool::StringID v)
	{
		if(v == string_intern_pool.NOT_A_STRING_ID)
		{
			SetType(ENT_NULL, nullptr, false);
		}
		else
		{
			SetType(ENT_STRING, nullptr, false);
			GetStringIDReference() = string_intern_pool.CreateStringReference(v);
		}
	}

	//changes the type by setting it to the string id value specified, handing off the reference
	inline void SetTypeViaStringIdValueWithReferenceHandoff(StringInternPool::StringID v)
	{
		if(v == string_intern_pool.NOT_A_STRING_ID)
		{
			SetType(ENT_NULL, nullptr, false);
		}
		else
		{
			SetType(ENT_STRING, nullptr, false);
			GetStringIDReference() = v;
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
	const std::string &GetStringValue();
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
		GetAnnotationsAndCommentsStorage().GetAnnotations();
	}

	//sets the annotation_string
	void SetAnnotationsString(std::string_view s)
	{
		GetAnnotationsAndCommentsStorage().SetAnnotations(s);
	}

	//splits annotations lines and returns a vector of strings of the comment
	std::vector<std::string> GetAnnotationsSeparateLines();

	inline void ClearAnnotations()
	{
		GetAnnotationsAndCommentsStorage().SetAnnotations("");
	}

	//appends annotations to the node
	void AppendAnnotations(std::string &annotations)
	{
		auto &a_and_c = GetAnnotationsAndCommentsStorage();
		std::string combined(a_and_c.GetAnnotations());
		combined.append(annotations);
		a_and_c.SetAnnotations(combined);
	}

	//functions for getting and setting node comments by string
	inline std::string_view GetCommentsString()
	{
		GetAnnotationsAndCommentsStorage().GetComments();
	}

	//returns true if has comments
	inline bool HasComments()
	{
		return GetAnnotationsAndCommentsStorage().HasComments();
	}

	//if handoff_reference is true, then it will not create a new reference but assume one has already been created
	void SetCommentsStringId(StringInternPool::StringID comments_string_id, bool handoff_reference = false);
	inline void SetComments(const std::string &comment)
	{
		GetAnnotationsAndCommentsStorage().SetComments(comment);
	}

	//splits comment lines and returns a vector of strings of the comment
	std::vector<std::string> GetCommentsSeparateLines();

	inline void ClearComments()
	{
		GetAnnotationsAndCommentsStorage().SetComments("");
	}

	//appends comments to the node
	void AppendComments(std::string &comments)
	{
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
	__forceinline bool HasAttributeAtomic(Attribute attr)
	{
		//TODO 15993: once C++20 is widely supported, change type to atomic_ref
		const std::atomic<AttributeStorageType> *atomic_ref
			= reinterpret_cast<const std::atomic<AttributeStorageType>*>(&attributes);
		AttributeStorageType cur = atomic_ref->load(std::memory_order_seq_cst);
		return (cur & static_cast<AttributeStorageType>(attr)) != 0;
	}

	__forceinline void SetAttributeAtomic(Attribute attr, bool enable = true)
	{
		AttributeStorageType mask = static_cast<AttributeStorageType>(attr);
		//TODO 15993: once C++20 is widely supported, change type to atomic_ref
		std::atomic<AttributeStorageType> *atomic_ref
			= reinterpret_cast<std::atomic<AttributeStorageType>*>(&attributes);
		if(enable)
			atomic_ref->fetch_or(mask, std::memory_order_seq_cst);
		else
			atomic_ref->fetch_and(~mask, std::memory_order_seq_cst);
	}
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

	//sets the EvaluableNode's idempotentcy flag
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
	__forceinline bool SetIsFreeableAtomic(bool is_freeable)
	{
		AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::FREEABLE);

		//TODO 15993: once C++20 is widely supported, change type to atomic_ref
		std::atomic<AttributeStorageType> *atomic_ref
			= reinterpret_cast<std::atomic<AttributeStorageType>*>(&attributes);

		if(is_freeable)
		{
			AttributeStorageType previous_value = atomic_ref->fetch_or(mask);
			return (previous_value & mask) != 0;
		}
		else
		{
			AttributeStorageType previous_value = atomic_ref->fetch_and(~mask);
			return (previous_value & mask) != 0;
		}
	}
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
		if(node == nullptr || node->IsImmediate())
		{
			//fill in with the node's value
			for(size_t i = 0; i < num_expected_elements; i++)
				store_value(i, true, node);
		}
		else if(node->IsAssociativeArray())
		{
			auto &mcn = node->GetMappedChildNodesReference();
			for(size_t i = 0; i < element_names.size(); i++)
			{
				EvaluableNode *value_en = nullptr;
				bool found = false;
				auto found_node = mcn.find(element_names[i]);
				if(found_node != end(mcn))
				{
					value_en = found_node->second;
					found = true;
				}

				store_value(i, found, value_en);
			}
		}
		else //ordered
		{
			auto &node_ocn = node->GetOrderedChildNodesReference();

			for(size_t i = 0; i < node_ocn.size(); i++)
				store_value(i, true, node_ocn[i]);
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
	//sets the ordered child nodes and updates flags, but can be used as an rvalue so that the memory doesn't
	//need to be reallocated if std::move is used for the input
	void SetOrderedChildNodes(std::vector<EvaluableNode *> &&ocn,
		bool need_cycle_check, bool is_idempotent);
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

	//helper function to obtain a typed value from mapped child nodes
	//note that it can only be used on string key lookups, no code or numeric keys
	template<typename T>
	static void GetValueFromMappedChildNodesReference(EvaluableNode::AssocType &mcn, EvaluableNodeBuiltInStringId key, T &value)
	{
		auto found_value = mcn.find(GetStringIdFromBuiltInStringId(key));
		if(found_value != end(mcn))
		{
			if constexpr(std::is_same<T, bool>::value)
				value = EvaluableNode::ToBool(found_value->second);
			else if constexpr(std::is_same<T, double>::value)
				value = EvaluableNode::ToNumber(found_value->second);
			else if constexpr(std::is_same<T, std::string>::value)
				value = EvaluableNode::ToString(found_value->second);
			else
				value = found_value->second;
		}
	}

protected:
	//defined since it is used as a pointer
	class AnnotationsAndComments;
public:

	//assumes that the EvaluableNode is of type ENT_BOOL, and returns the value by reference
	__forceinline bool &GetBoolValueReference()
	{
		if(!HasExtendedValue())
			return value.boolValueContainer.boolValue;
		else
			return value.extension.extendedValue->boolValueContainer.boolValue;
	}

	//assumes that the EvaluableNode is of type ENT_NUMBER, and returns the value by reference
	__forceinline double &GetNumberValueReference()
	{
		if(!HasExtendedValue())
			return value.numberValueContainer.numberValue;
		else
			return value.extension.extendedValue->numberValueContainer.numberValue;
	}

	//assumes that the EvaluableNode is of type that holds a string, and returns the value by reference
	__forceinline StringInternPool::StringID &GetStringIDReference()
	{
		if(!HasExtendedValue())
			return value.stringValueContainer.stringID;
		else
			return value.extension.extendedValue->stringValueContainer.stringID;
	}

	//assumes that the EvaluableNode has ordered child nodes, and returns the value by reference
	__forceinline std::vector<EvaluableNode *> &GetOrderedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.orderedChildNodes;
		else
			return value.extension.extendedValue->orderedChildNodes;
	}

	//assumes that the EvaluableNode is has mapped child nodes, and returns the value by reference
	__forceinline AssocType &GetMappedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.mappedChildNodes;
		else
			return value.extension.extendedValue->mappedChildNodes;
	}

	//if it is storing an immediate value and has room to store a label
	bool HasCompactSingleLabelStorage()
	{
		return ((type == ENT_BOOL || type == ENT_NUMBER || type == ENT_STRING || type == ENT_SYMBOL) && !HasExtendedValue());
	}

	//returns a reference to the storage location for the annotation and comment storage
	// will only return valid results if HasCompactSingleLabelStorage() is true, so that should be called first
	__forceinline AnnotationsAndComments &GetAnnotationsAndCommentsStorage()
	{
		if(type == ENT_BOOL)
			return value.boolValueContainer.annotationsAndComments;
		if(type == ENT_NUMBER)
			return value.numberValueContainer.annotationsAndComments;
		//else assume type == ENT_STRING || type == ENT_SYMBOL
		return value.stringValueContainer.annotationsAndComments;
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

	class AnnotationsAndComments
	{
	public:
		__forceinline static void Construct(AnnotationsAndComments &a_and_c)
		{
			new (&a_and_c) AnnotationsAndComments;
		}

		__forceinline static void Destruct(AnnotationsAndComments &a_and_c)
		{
			a_and_c.~AnnotationsAndComments();
		}

		AnnotationsAndComments() = default;
		AnnotationsAndComments(std::string_view annotation, std::string_view comment)
		{
			SetAnnotationsAndComments(annotation, comment);
		}

		void Clear()
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
		void SetAnnotations(std::string_view new_annotations)
		{
			SetAnnotationsAndComments(new_annotations, GetComments());
		}

		//replace only the comments
		void SetComments(std::string_view new_comments)
		{
			SetAnnotationsAndComments(GetAnnotations(), new_comments);
		}

		bool HasAnnotations()
		{
			return buffer && buffer[0] != '\0';
		}

		bool HasComments()
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
			if(*p != '\0') return true;

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
		// default to numberValueContainer constructor to allow constexpr
		__forceinline  EvaluableNodeValue() { }
		__forceinline  ~EvaluableNodeValue() { }

		__forceinline void ConstructOrderedChildNodes()
		{	new (&orderedChildNodes) std::vector<EvaluableNode *>;	}

		__forceinline void DestructOrderedChildNodes()
		{	orderedChildNodes.~vector();	}

		__forceinline void ConstructMappedChildNodes()
		{	new (&mappedChildNodes) AssocType;	}

		__forceinline void DestructMappedChildNodes()
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

			AnnotationsAndComments annotationsAndComments;
		} stringValueContainer;

		//when type represents a number, holds the corresponding value
		struct EvaluableNodeValueNumber
		{
			//number value
			double numberValue;

			AnnotationsAndComments annotationsAndComments;
		} numberValueContainer;

		//when type represents a bool, holds the corresponding value
		struct EvaluableNodeValueBool
		{
			//bool value
			bool boolValue;

			AnnotationsAndComments annotationsAndComments;
		} boolValueContainer;

		struct EvaluableNodeExtension
		{
			//pointer to store any extra data if EvaluableNode needs multiple fields
			EvaluableNodeValue *extendedValue;

			AnnotationsAndComments annotationsAndComments;
		} extension;
	};
#pragma pack(pop)

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

	//fields contained within the current set of data
	AttributeStorageType attributes;

	//values used to be able to return a reference
	static bool falseBoolValue;
	static double nanNumberValue;
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
	ENIVT_BOOL,					//bool
	ENIVT_NUMBER,				//number
	ENIVT_STRING_ID,			//stringID
	ENIVT_CODE,					//code (more general than any of the above)
	ENIVT_NUMBER_INDIRECTION_INDEX,		//not a real EvaluableNode type, but an index to some data structure that has a number
	ENIVT_STRING_ID_INDIRECTION_INDEX	//not a real EvaluableNode type, but an index to some data structure that has a stringID
};

//when an EvaluableNodeImmediateValue is requested, this class can indicate which types of values are allowed
// EvaluableNodeRequestedValueTypes.h
class EvaluableNodeRequestedValueTypes
{
public:
	using StorageType = uint8_t;

	enum class Type : StorageType
	{
		NONE = 0,
		NULL_VALUE = 1 << 0,
		BOOL = 1 << 1,
		NUMBER = 1 << 2,
		EXISTING_STRING_ID = 1 << 3,
		STRING_ID = 1 << 4,
		EXISTING_KEY_STRING_ID = 1 << 5,
		KEY_STRING_ID = 1 << 6,
		CODE = 1 << 7,

		//composite types which can include NULL_VALUE
		REQUEST_BOOL = BOOL | NULL_VALUE,
		REQUEST_NUMBER = NUMBER | NULL_VALUE,
		REQUEST_EXISTING_STRING_ID = EXISTING_STRING_ID | NULL_VALUE,
		REQUEST_STRING_ID = STRING_ID | NULL_VALUE,
		REQUEST_EXISTING_KEY_STRING_ID = EXISTING_KEY_STRING_ID | NULL_VALUE,
		REQUEST_KEY_STRING_ID = KEY_STRING_ID | NULL_VALUE,

		ALL = NULL_VALUE | BOOL | NUMBER | EXISTING_STRING_ID | STRING_ID | CODE
	};

	constexpr EvaluableNodeRequestedValueTypes() noexcept
		: requestedValueTypes(Type::NONE)
	{}

	constexpr EvaluableNodeRequestedValueTypes(Type t) noexcept
		: requestedValueTypes(t)
	{}

	constexpr EvaluableNodeRequestedValueTypes(StorageType raw) noexcept
		: requestedValueTypes(static_cast<Type>(raw))
	{}

	//boolean implies all or none
	constexpr EvaluableNodeRequestedValueTypes(bool all_or_none) noexcept
		: requestedValueTypes(all_or_none ? Type::ALL : Type::NONE)
	{}

	//bit‑wise operators
	constexpr EvaluableNodeRequestedValueTypes &operator|=(EvaluableNodeRequestedValueTypes rhs) noexcept
	{
		requestedValueTypes = static_cast<Type>(static_cast<StorageType>(requestedValueTypes) |
								 static_cast<StorageType>(rhs.requestedValueTypes));
		return *this;
	}

	constexpr EvaluableNodeRequestedValueTypes &operator&=(EvaluableNodeRequestedValueTypes rhs) noexcept
	{
		requestedValueTypes = static_cast<Type>(static_cast<StorageType>(requestedValueTypes) &
								 static_cast<StorageType>(rhs.requestedValueTypes));
		return *this;
	}

	constexpr friend EvaluableNodeRequestedValueTypes operator|(
		EvaluableNodeRequestedValueTypes lhs,
		EvaluableNodeRequestedValueTypes rhs) noexcept
	{
		return EvaluableNodeRequestedValueTypes(
			static_cast<Type>(static_cast<StorageType>(lhs.requestedValueTypes) |
				static_cast<StorageType>(rhs.requestedValueTypes)));
	}

	constexpr friend EvaluableNodeRequestedValueTypes operator&(
		EvaluableNodeRequestedValueTypes lhs,
		EvaluableNodeRequestedValueTypes rhs) noexcept
	{
		return EvaluableNodeRequestedValueTypes(
			static_cast<Type>(static_cast<StorageType>(lhs.requestedValueTypes) &
				static_cast<StorageType>(rhs.requestedValueTypes)));
	}

	constexpr friend EvaluableNodeRequestedValueTypes operator~(
		EvaluableNodeRequestedValueTypes v) noexcept
	{
		return EvaluableNodeRequestedValueTypes(
			static_cast<Type>(~static_cast<StorageType>(v.requestedValueTypes)));
	}

	constexpr bool Allows(Type flag) const noexcept
	{
		return (static_cast<StorageType>(requestedValueTypes) &
				static_cast<StorageType>(flag)) != 0;
	}

	//returns true if any immediate is allowed
	constexpr bool AnyImmediateType() const noexcept
	{
		return (static_cast<StorageType>(requestedValueTypes) & ~static_cast<StorageType>(Type::CODE)) != 0;
	}

	//returns true if an immediate is allowed
	constexpr bool ImmediateValue() const noexcept
	{
		return requestedValueTypes != Type::NONE;
	}

	//returns true if an immediate value is allowed but immediate types are not allowed
	constexpr bool ImmediateValueButNotImmediateType() const noexcept
	{
		return (static_cast<StorageType>(requestedValueTypes) & ~static_cast<StorageType>(Type::CODE)) == 0;
	}

private:
	Type requestedValueTypes;
};


//structure that can hold the most immediate value type of an EvaluableNode
// EvaluableNodeImmediateValueType can be used to communicate which type of data is being held
union EvaluableNodeImmediateValue
{
	__forceinline constexpr EvaluableNodeImmediateValue()
		: code(nullptr)
	{	}

	__forceinline constexpr EvaluableNodeImmediateValue(bool bool_value)
		: boolValue(bool_value)
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(double _number)
		: number(_number)
	{	}

	__forceinline EvaluableNodeImmediateValue(StringInternPool::StringID string_id)
		: stringID(string_id)
	{	}

	__forceinline constexpr EvaluableNodeImmediateValue(EvaluableNode *_code)
		: code(_code)
	{	}

	__forceinline constexpr EvaluableNodeImmediateValue(const EvaluableNodeImmediateValue &eniv)
		: code(eniv.code)
	{	}

	__forceinline constexpr EvaluableNodeImmediateValue(size_t indirection_index)
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

		if(en_type == ENT_BOOL)
		{
			boolValue = en->GetBoolValueReference();
			return ENIVT_BOOL;
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

	//returns true if the values are equal, which can include ENIVT_CODE containing null, etc.
	inline static bool AreEqual(EvaluableNodeImmediateValueType type_1, EvaluableNodeImmediateValue &value_1,
		EvaluableNodeImmediateValueType type_2, EvaluableNodeImmediateValue &value_2)
	{
		if(type_1 != type_2)
		{
			if(type_1 == ENIVT_CODE)
			{
				if(type_2 == ENIVT_NULL)
					return EvaluableNode::IsNull(value_1.code);
				else if(type_2 == ENIVT_BOOL)
					return (value_2.boolValue == EvaluableNode::ToBool(value_1.code));
				else if(type_2 == ENIVT_NUMBER)
					return (value_2.number == EvaluableNode::ToNumber(value_1.code));
				else if(type_2 == ENIVT_STRING_ID)
					return (value_2.stringID == EvaluableNode::ToStringIDIfExists(value_1.code));
			}
			else if(type_2 == ENIVT_CODE)
			{
				if(type_1 == ENIVT_NULL)
					return EvaluableNode::IsNull(value_2.code);
				else if(type_1 == ENIVT_BOOL)
					return (value_1.boolValue == EvaluableNode::ToBool(value_2.code));
				else if(type_1 == ENIVT_NUMBER)
					return (value_1.number == EvaluableNode::ToNumber(value_2.code));
				else if(type_1 == ENIVT_STRING_ID)
					return (value_1.stringID == EvaluableNode::ToStringIDIfExists(value_2.code));
			}

			return false;
		}

		//types are the same, just use type_1 for reference
		if(type_1 == ENIVT_NULL)
			return true;
		else if(type_1 == ENIVT_BOOL)
			return (value_1.boolValue == value_2.boolValue);
		else if(type_1 == ENIVT_NUMBER)
			return (value_1.number == value_2.number);
		else if(type_1 == ENIVT_STRING_ID)
			return (value_1.stringID == value_2.stringID);
		else if(type_1 == ENIVT_NUMBER_INDIRECTION_INDEX || type_1 == ENIVT_STRING_ID_INDIRECTION_INDEX)
			return (value_1.indirectionIndex == value_2.indirectionIndex);
		else
			return EvaluableNode::AreDeepEqual(value_1.code, value_2.code);
	}

	//like AreEqual but requires that immediate values are already transformed into immediate representations,
	//e.g., ENIVT_CODE would not contain a null
	inline static bool AreEqualGivenImmediateValuesNotCode(EvaluableNodeImmediateValueType type_1, EvaluableNodeImmediateValue &value_1,
		EvaluableNodeImmediateValueType type_2, EvaluableNodeImmediateValue &value_2)
	{
		if(type_1 != type_2)
			return false;

		//types are the same, just use type_1 for reference
		if(type_1 == ENIVT_NULL)
			return true;
		else if(type_1 == ENIVT_BOOL)
			return (value_1.boolValue == value_2.boolValue);
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
	static __forceinline constexpr bool IsNull(EvaluableNodeImmediateValueType type, EvaluableNodeImmediateValue &value)
	{
		if(type != ENIVT_CODE)
			return type == ENIVT_NULL;

		return EvaluableNode::IsNull(value.code);
	}

	__forceinline operator double()
	{
		return number;
	}

	__forceinline operator StringInternPool::StringID()
	{
		return stringID;
	}

	bool boolValue;
	double number;
	StringInternPool::StringID stringID;
	EvaluableNode *code;
	size_t indirectionIndex;
};

//used for storing a value and type together
class EvaluableNodeImmediateValueWithType
{
public:
	__forceinline constexpr EvaluableNodeImmediateValueWithType()
		: nodeValue(static_cast<EvaluableNode *>(nullptr)), nodeType(ENIVT_NULL)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType(EvaluableNodeImmediateValue node_value,
		EvaluableNodeImmediateValueType node_type)
		: nodeValue(node_value), nodeType(node_type)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType(bool value)
	{
		if(value)
			nodeValue.boolValue = true;
		else
			nodeValue.boolValue = false;
		nodeType = ENIVT_BOOL;
	}

	__forceinline EvaluableNodeImmediateValueWithType(double number)
	{
		if(FastIsNaN(number))
			nodeType = ENIVT_NULL;
		else
		{
			nodeValue = number;
			nodeType = ENIVT_NUMBER;
		}
	}

	__forceinline EvaluableNodeImmediateValueWithType(StringInternPool::StringID string_id)
	{
		if(string_id == StringInternPool::NOT_A_STRING_ID)
			nodeType = ENIVT_NULL;
		else
		{
			nodeValue = string_id;
			nodeType = ENIVT_STRING_ID;
		}
	}

	constexpr EvaluableNodeImmediateValueWithType(EvaluableNode *code)
		: nodeValue(code), nodeType(ENIVT_CODE)
	{	}

	constexpr EvaluableNodeImmediateValueWithType(const EvaluableNodeImmediateValueWithType &enimvwt)
		: nodeValue(enimvwt.nodeValue), nodeType(enimvwt.nodeType)
	{	}

	__forceinline EvaluableNodeImmediateValueWithType &operator =(const EvaluableNodeImmediateValueWithType &enimvwt)
	{
		nodeValue = enimvwt.nodeValue;
		nodeType = enimvwt.nodeType;
		return *this;
	}

	//copies the value from en and returns the EvaluableNodeConcreteValueType
	//if enm is not nullptr, then it will make a copy of any code or string ids
	void CopyValueFromEvaluableNode(EvaluableNode *en, EvaluableNodeManager *enm = nullptr);

	bool GetValueAsBoolean(bool value_if_null = false);

	double GetValueAsNumber(double value_if_null = std::numeric_limits<double>::quiet_NaN());

	std::pair<bool, std::string> GetValueAsString(bool key_string = false);

	StringInternPool::StringID GetValueAsStringIDIfExists(bool key_string = false);

	StringInternPool::StringID GetValueAsStringIDWithReference(bool key_string = false);

	static inline bool AreEqual(EvaluableNodeImmediateValueWithType &a, EvaluableNodeImmediateValueWithType &b)
	{
		return EvaluableNodeImmediateValue::AreEqual(a.nodeType, a.nodeValue, b.nodeType, b.nodeValue);
	}

	static inline bool AreEqualGivenImmediateValuesNotCode(EvaluableNodeImmediateValueWithType &a, EvaluableNodeImmediateValueWithType &b)
	{
		return EvaluableNodeImmediateValue::AreEqualGivenImmediateValuesNotCode(a.nodeType, a.nodeValue, b.nodeType, b.nodeValue);
	}

	//returns true if it is a null or null equivalent
	constexpr bool IsNull()
	{
		return EvaluableNodeImmediateValue::IsNull(nodeType, nodeValue);
	}

	EvaluableNodeImmediateValue nodeValue;
	EvaluableNodeImmediateValueType nodeType;
};

//copies ocn into immediate values and value_types
inline void CopyOrderedChildNodesToImmediateValuesAndTypes(std::vector<EvaluableNode *> &ocn,
	std::vector<EvaluableNodeImmediateValue> &values, std::vector<EvaluableNodeImmediateValueType> &value_types)
{
	values.clear();
	value_types.clear();
	values.reserve(ocn.size());
	value_types.reserve(ocn.size());
	for(EvaluableNode *en : ocn)
	{
		EvaluableNodeImmediateValue imm_val;
		auto value_type = imm_val.CopyValueFromEvaluableNode(en);
		value_types.push_back(value_type);
		values.push_back(imm_val);
	}
}
