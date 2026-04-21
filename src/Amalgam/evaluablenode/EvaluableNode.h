#pragma once

//project headers:
#include "FastMath.h"
#include "HashMaps.h"
#include "OpcodeDetails.h"
#include "Opcodes.h"
#include "PlatformSpecific.h"
#include "StringInternPool.h"

//system headers:
#include <string>
#include <vector>
#define AMALGAM_FAST_MEMORY_INTEGRITY
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
	__forceinline EvaluableNode(EvaluableNode *n, bool copy_metadata = true) { InitializeType(n, copy_metadata); }

	__forceinline ~EvaluableNode()
	{
		if(!IsNodeDeallocated())
			Invalidate();
	}

	//clears out all data and makes the unusable in the ENT_DEALLOCATED state
	inline void Invalidate()
	{
		DestructValue();

		type = ENT_DEALLOCATED;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);

	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		//use a value that is more apparent that something went wrong
		value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
	#else
		value.numberAndNullValueContainer.numberValue = 0;
	#endif

		AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
	}

	///////////////////////////////////////////
	//Each InitializeType* sets up a given type with appropriate data
	inline void InitializeType(EvaluableNodeType _type, const std::string &string_value)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type));
	#endif

		type = _type;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);

		SetIsIdempotent(type == ENT_STRING);
		SetNeedCycleCheck(false);
	}

	inline void InitializeType(EvaluableNodeType _type, const std::string_view string_value)
	{
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		assert(IsEvaluableNodeTypeValid(_type));
	#endif

		type = _type;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);

		SetIsIdempotent(type == ENT_STRING);
		SetNeedCycleCheck(false);
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

		SetIsIdempotent(type == ENT_STRING);
		SetNeedCycleCheck(false);
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

		SetIsIdempotent(type == ENT_STRING);
		SetNeedCycleCheck(false);
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
			value.numberAndNullValueContainer.numberValue = number_value;
			AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
		}

		SetIsIdempotent(true);
		SetNeedCycleCheck(false);
	}

	inline void InitializeType(bool bool_value)
	{
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
		type = ENT_BOOL;
		value.boolValueContainer.boolValue = bool_value;
		AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);

		SetIsIdempotent(true);
		SetNeedCycleCheck(false);
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

		if(DoesEvaluableNodeTypeUseNullData(_type))
		{
			value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
			AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
			SetIsIdempotent(true);
			SetNeedCycleCheck(false);
		}
		if(DoesEvaluableNodeTypeUseBoolData(_type))
		{
			AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
			value.boolValueContainer.boolValue = false;
			SetIsIdempotent(true);
			SetNeedCycleCheck(false);
		}
		else if(DoesEvaluableNodeTypeUseNumberData(_type))
		{
			AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
			value.numberAndNullValueContainer.numberValue = 0.0;
			SetIsIdempotent(true);
			SetNeedCycleCheck(false);
		}
		else if(DoesEvaluableNodeTypeUseStringData(_type))
		{
			value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
			AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
			SetIsIdempotent(_type == ENT_STRING);
			SetNeedCycleCheck(false);
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
			value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
		#else
			value.numberAndNullValueContainer.numberValue = 0;
		#endif

			AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);
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

	//clears annotations and comments
	__forceinline void ClearAnnotationsAndComments()
	{
		if(HasExtendedValue())
		{
			if(GetType() == ENT_ASSOC)
			{
				AssocType temp_mcn = std::move(*value.extendedMappedChildNodes.mappedChildNodes);
				value.extendedMappedChildNodes.mappedChildNodes.~unique_ptr<AssocType>();
				new (&value.mappedChildNodes) AssocType(std::move(temp_mcn));
			}
			else //ordered
			{
				std::vector<EvaluableNode *> temp_ocn = std::move(*value.extendedOrderedChildNodes.orderedChildNodes);
				value.extendedOrderedChildNodes.orderedChildNodes.~unique_ptr<std::vector<EvaluableNode *>>();
				new (&value.orderedChildNodes) std::vector<EvaluableNode *>(std::move(temp_ocn));
			}

			SetExtendedValue(false);
		}
		else
		{
			GetAnnotationsAndCommentsStorage().Clear();
		}
	}

	//clears the node's metadata
	__forceinline void ClearMetadata()
	{
		ClearAnnotationsAndComments();
		SetConcurrency(false);
	}

	//returns true if the node has any metadata
	__forceinline bool HasMetadata()
	{
		auto &a_and_c = GetAnnotationsAndCommentsStorage();
		return (a_and_c.HasCommentOrAnnotation()  || GetConcurrency());
	}

	//Returns true if the immediate data structure of a is equal to b
	static inline bool AreShallowEqual(EvaluableNode *a, EvaluableNode *b)
	{
		EvaluableNodeType a_type = (a == nullptr ? ENT_NULL : a->GetType());
		EvaluableNodeType b_type = (b == nullptr ? ENT_NULL : b->GetType());

		//check both types are the same
		if(a_type != b_type)
			return false;

		//since both types are the same, only need to check one for the type of data
		//check string equality
		if(DoesEvaluableNodeTypeUseStringData(a_type))
			return a->GetStringIDReference() == b->GetStringIDReference();

		//check numeric equality
		if(DoesEvaluableNodeTypeUseNumberData(a_type))
			return a->GetNumberValueReference() == b->GetNumberValueReference();

		if(DoesEvaluableNodeTypeUseBoolData(a_type))
			return a->GetBoolValueReference() == b->GetBoolValueReference();

		//if made it here, then it's an instruction, and they're of equal type
		return true;
	}

	//Returns true if the entire data structure of a is equal in value to the data structure of b
	static inline bool AreDeepEqual(EvaluableNode *a, EvaluableNode *b)
	{
		//if pointers are the same, then they are the same
		if(a == b)
			return true;

		//first check if the immediate values are equal
		if(!AreShallowEqual(a, b))
			return false;

		//since they are shallow equal, check for quick exit
		if(a == nullptr || b == nullptr || IsEvaluableNodeTypeImmediate(a->GetType()))
			return true;

		//only need cycle checks if both a and b need cycle checks,
		// otherwise, one will become exhausted and end the comparison
		if(a->GetNeedCycleCheck() && b->GetNeedCycleCheck())
		{
			ReferenceAssocType checked;
			return AreDeepEqualGivenShallowEqualAndNotImmediate(a, b, &checked);
		}
		else
		{
			return AreDeepEqualGivenShallowEqualAndNotImmediate(a, b, nullptr);
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

		return CanNodeTreeBeFlattenedRecurse(n, reusableBuffer);
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
	inline void SetTypeViaBoolValue(bool v)
	{
		SetType(ENT_BOOL, nullptr, false);
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
	inline void InitStringValue()
	{
		DestructValue();
		value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
	}

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
		EnsureHasAnnotationsAndCommentsStorage();
		GetAnnotationsAndCommentsStorage().SetAnnotations(s);
	}

	inline void ClearAnnotations()
	{
		GetAnnotationsAndCommentsStorage().SetAnnotations("");
	}

	//appends annotations to the node
	void AppendAnnotations(std::string &annotations)
	{
		EnsureHasAnnotationsAndCommentsStorage();

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
		EnsureHasAnnotationsAndCommentsStorage();
		GetAnnotationsAndCommentsStorage().SetComments(comment);
	}

	inline void ClearComments()
	{
		GetAnnotationsAndCommentsStorage().SetComments("");
	}

	//appends comments to the node
	void AppendComments(std::string &comments)
	{
		EnsureHasAnnotationsAndCommentsStorage();

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

	//updates all flags as appropriate given that a newly allocated
	// child_node is being added as a child to this node
	__forceinline void UpdateFlagsBasedOnNewChildNode(EvaluableNode *new_child)
	{
		if(new_child == nullptr)
			return;

		//if cycles, propagate upward
		if(new_child->GetNeedCycleCheck())
			SetNeedCycleCheck(true);

		//propagate idempotency
		if(!new_child->GetIsIdempotent())
			SetIsIdempotent(false);
	}

	//assumes all child nodes (if any) do not reference this node and all their
	//flags are correct and updates this node's flags
	__forceinline void UpdateAllFlagsBasedOnNoReferencingChildNodes()
	{
		bool is_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(GetType());
		bool need_cycle_check = false;

		if(IsAssociativeArray())
		{
			for(auto &[cn_id, cn] : GetMappedChildNodesReference())
			{
				if(cn == nullptr)
					continue;

				//update flags for tree
				if(cn->GetNeedCycleCheck())
					need_cycle_check = true;

				if(!cn->GetIsIdempotent())
					is_idempotent = false;

				//if both are triggered, no need to continue
				if(!is_idempotent && need_cycle_check)
					break;
			}
		}
		else if(!IsImmediate())
		{
			for(auto cn : GetOrderedChildNodesReference())
			{
				if(cn == nullptr)
					continue;

				//update flags for tree
				if(cn->GetNeedCycleCheck())
					need_cycle_check = true;

				if(!cn->GetIsIdempotent())
					is_idempotent = false;

				//if both are triggered, no need to continue
				if(!is_idempotent && need_cycle_check)
					break;
			}
		}

		SetNeedCycleCheck(need_cycle_check);
		SetIsIdempotent(is_idempotent);
	}

	inline void InitOrderedChildNodes()
	{
		DestructValue();

		if(!HasExtendedValue())
			value.ConstructOrderedChildNodes();
		else
			value.extendedOrderedChildNodes.Construct();
	}

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
		if(EvaluableNode::IsNull(node) || node->IsImmediate())
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

	inline void InitMappedChildNodes()
	{
		DestructValue();

		if(!HasExtendedValue())
			value.ConstructMappedChildNodes();
		else
			value.extendedMappedChildNodes.Construct();
	}

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
			else if constexpr(std::is_same<T, StringInternPool::StringID>::value)
				value = EvaluableNode::ToStringIDIfExists(found_value->second);
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
		return value.boolValueContainer.boolValue;
	}

	//assumes that the EvaluableNode is of type ENT_NUMBER, and returns the value by reference
	__forceinline double &GetNumberValueReference()
	{
		return value.numberAndNullValueContainer.numberValue;
	}

	//assumes that the EvaluableNode is of type that holds a string, and returns the value by reference
	__forceinline StringInternPool::StringID &GetStringIDReference()
	{
		return value.stringValueContainer.stringID;
	}

	//assumes that the EvaluableNode has ordered child nodes, and returns the value by reference
	__forceinline std::vector<EvaluableNode *> &GetOrderedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.orderedChildNodes;
		else
			return *value.extendedOrderedChildNodes.orderedChildNodes.get();
	}

	//assumes that the EvaluableNode is has mapped child nodes, and returns the value by reference
	__forceinline AssocType &GetMappedChildNodesReference()
	{
		if(!HasExtendedValue())
			return value.mappedChildNodes;
		else
			return *value.extendedMappedChildNodes.mappedChildNodes.get();
	}

	//if it is storing an immediate value and has room to store a label
	inline bool HasCompactAnnotationsAndCommentsStorage()
	{
		return (type == ENT_NULL || type == ENT_BOOL || type == ENT_NUMBER || type == ENT_STRING || type == ENT_SYMBOL);
	}

	//returns a reference to the storage location for the annotation and comment storage
	// will only return valid results if HasCompactAnnotationsAndCommentsStorage() is true, so that should be called first
	__forceinline AnnotationsAndComments &GetAnnotationsAndCommentsStorage()
	{
		switch(GetType())
		{
		case ENT_BOOL:
			return value.boolValueContainer.annotationsAndComments;
		case ENT_NULL:
		case ENT_NUMBER:
			return value.numberAndNullValueContainer.annotationsAndComments;
		case ENT_STRING:
		case ENT_SYMBOL:
			return value.stringValueContainer.annotationsAndComments;
		case ENT_ASSOC:
			if(!HasExtendedValue())
				return emptyAnnotationsAndComments;
			else
				return value.extendedMappedChildNodes.annotationsAndComments;
		//otherwise ordered
		default:
			if(!HasExtendedValue())
				return emptyAnnotationsAndComments;
			else
				return value.extendedOrderedChildNodes.annotationsAndComments;
		}
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

	//combines annotations and comments into a single string to minimize storage overhead rather
	// than minimize compute time; each retrieval time is linear in the length of the strings
	//the two strings are separated by a null terminator and end with a null terminator,
	// so it is faster to retrieve both together than one and then the other
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
				new (&orderedChildNodes) std::unique_ptr<std::vector<EvaluableNode *>>(
					std::make_unique<std::vector<EvaluableNode *>>());

				AnnotationsAndComments::Construct(annotationsAndComments);
			}

			__forceinline void Destruct()
			{
				orderedChildNodes.~unique_ptr<std::vector<EvaluableNode *>>();
				AnnotationsAndComments::Destruct(annotationsAndComments);
			}

			//external orderedChildNodes
			std::unique_ptr<std::vector<EvaluableNode *>> orderedChildNodes;

			AnnotationsAndComments annotationsAndComments;
		} extendedOrderedChildNodes;

		struct EvaluableNodeValueMappedChildNodesWithAnnotationsAndComments
		{
			__forceinline void Construct()
			{
				new (&mappedChildNodes) std::unique_ptr<AssocType>(std::make_unique<AssocType>());

				AnnotationsAndComments::Construct(annotationsAndComments);
			}

			__forceinline void Destruct()
			{
				string_intern_pool.DestroyStringReferences(*mappedChildNodes, [](auto n) { return n.first; });
				mappedChildNodes.~unique_ptr<AssocType>();
				AnnotationsAndComments::Destruct(annotationsAndComments);
			}

			//external orderedChildNodes
			std::unique_ptr<AssocType> mappedChildNodes;

			AnnotationsAndComments annotationsAndComments;
		} extendedMappedChildNodes;
	};
#pragma pack(pop)

	//makes sure that the extendedValue is set appropriately so that it can be used to hold additional data
	void EnsureHasAnnotationsAndCommentsStorage();

	//destructs the value so that the node can be reused
	// note that the value should be considered uninitialized
	inline void DestructValue()
	{
		switch(GetType())
		{
		case ENT_BOOL:
			AnnotationsAndComments::Destruct(value.boolValueContainer.annotationsAndComments);
			break;
		case ENT_NULL:
		case ENT_NUMBER:
			AnnotationsAndComments::Destruct(value.numberAndNullValueContainer.annotationsAndComments);
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReference(value.stringValueContainer.stringID);
			AnnotationsAndComments::Destruct(value.stringValueContainer.annotationsAndComments);
			break;
		case ENT_ASSOC:
			if(!HasExtendedValue())
			{
				value.DestructMappedChildNodes();
			}
			else
			{
				value.extendedMappedChildNodes.Destruct();
				SetExtendedValue(false);
			}
			break;
			//otherwise ordered
		default:
			if(!HasExtendedValue())
			{
				value.DestructOrderedChildNodes();
			}
			else
			{
				value.extendedOrderedChildNodes.Destruct();
				SetExtendedValue(false);
			}
			break;
		}
	}

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
	static AnnotationsAndComments emptyAnnotationsAndComments;

public:
	//reusable memory pool for local operations
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		inline static std::vector<EvaluableNode *> reusableBuffer;
protected:

	//field for watching EvaluableNodes for debugging
	static FastHashSet<EvaluableNode *> debugWatch;
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	static Concurrency::SingleMutex debugWatchMutex;
#endif
};
