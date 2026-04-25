#pragma once
//project headers:
#include "EvaluableNode.h"

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

	constexpr bool NoValueRequested() const noexcept
	{
		return requestedValueTypes == Type::NULL_VALUE;
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
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(bool bool_value)
		: boolValue(bool_value)
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(double _number)
		: number(_number)
	{}

	__forceinline EvaluableNodeImmediateValue(StringInternPool::StringID string_id)
		: stringID(string_id)
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(EvaluableNode *_code)
		: code(_code)
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(const EvaluableNodeImmediateValue &eniv)
		: code(eniv.code)
	{}

	__forceinline constexpr EvaluableNodeImmediateValue(size_t indirection_index)
		: indirectionIndex(indirection_index)
	{}

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
	inline static bool AreEqual(EvaluableNodeImmediateValueType type_1,
		const EvaluableNodeImmediateValue &value_1,
		EvaluableNodeImmediateValueType type_2,
		const EvaluableNodeImmediateValue &value_2)
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
	inline static bool AreEqualGivenImmediateValuesNotCode(EvaluableNodeImmediateValueType type_1,
		const EvaluableNodeImmediateValue &value_1,
		EvaluableNodeImmediateValueType type_2,
		const EvaluableNodeImmediateValue &value_2)
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
	static __forceinline constexpr bool IsNull(EvaluableNodeImmediateValueType type,
		const EvaluableNodeImmediateValue &value)
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
	{}

	__forceinline EvaluableNodeImmediateValueWithType(EvaluableNodeImmediateValue node_value,
		EvaluableNodeImmediateValueType node_type)
		: nodeValue(node_value), nodeType(node_type)
	{}

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
	{}

	constexpr EvaluableNodeImmediateValueWithType(const EvaluableNodeImmediateValueWithType &enimvwt)
		: nodeValue(enimvwt.nodeValue), nodeType(enimvwt.nodeType)
	{}

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

	static inline bool AreEqual(const EvaluableNodeImmediateValueWithType &a,
		const EvaluableNodeImmediateValueWithType &b)
	{
		return EvaluableNodeImmediateValue::AreEqual(a.nodeType, a.nodeValue, b.nodeType, b.nodeValue);
	}

	static inline bool AreEqualGivenImmediateValuesNotCode(const EvaluableNodeImmediateValueWithType &a,
		const EvaluableNodeImmediateValueWithType &b)
	{
		return EvaluableNodeImmediateValue::AreEqualGivenImmediateValuesNotCode(a.nodeType, a.nodeValue, b.nodeType, b.nodeValue);
	}

	//returns true if it is a null or null equivalent
	constexpr bool IsNull() const
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

//describes an EvaluableNode value and whether it is uniquely referenced
//this is mostly used for actual EvaluableNode *'s, and so most of the methods are built as such
//however, if it may contain an immediate value, then that must be checked via IsImmediateValue()
class EvaluableNodeReference
{
public:
	constexpr EvaluableNodeReference()
		: value(), unique(true), uniqueUnreferencedTopNode(true)
	{}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique)
		: value(_reference), unique(_unique), uniqueUnreferencedTopNode(_unique)
	{}

	constexpr EvaluableNodeReference(EvaluableNode *_reference, bool _unique, bool top_node_unique)
		: value(_reference), unique(_unique), uniqueUnreferencedTopNode(top_node_unique)
	{}

	constexpr EvaluableNodeReference(const EvaluableNodeReference &inr)
		: value(inr.value), unique(inr.unique), uniqueUnreferencedTopNode(inr.uniqueUnreferencedTopNode)
	{}

	__forceinline EvaluableNodeReference(bool value)
		: value(value), unique(true), uniqueUnreferencedTopNode(true)
	{}

	__forceinline EvaluableNodeReference(double value)
		: value(value), unique(true), uniqueUnreferencedTopNode(true)
	{}

	//if reference_handoff is true, it will assume ownership rather than creating a new reference
	__forceinline EvaluableNodeReference(StringInternPool::StringID string_id, bool reference_handoff = false)
		: value(reference_handoff ? string_id : string_intern_pool.CreateStringReference(string_id)),
		unique(true), uniqueUnreferencedTopNode(true)
	{}

	__forceinline EvaluableNodeReference(const std::string &str)
		: value(string_intern_pool.CreateStringReference(str)), unique(true), uniqueUnreferencedTopNode(true)
	{}

	__forceinline EvaluableNodeReference(std::string_view str)
		: value(string_intern_pool.CreateStringReference(str)), unique(true), uniqueUnreferencedTopNode(true)
	{}

	__forceinline EvaluableNodeReference(EvaluableNodeImmediateValueWithType enivwt, bool _unique)
		: value(enivwt), unique(_unique), uniqueUnreferencedTopNode(_unique)
	{}

	//constructs an EvaluableNodeReference with an immediate type and true if possible to coerce node
	//into one of the immediate request types, or returns a non-unique EvaluableNodeReference and false if not
	static inline EvaluableNodeReference CoerceNonUniqueEvaluableNodeToImmediateIfPossible(EvaluableNode *en,
		EvaluableNodeRequestedValueTypes immediate_result)
	{
		if(en == nullptr)
		{
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NULL_VALUE))
				return EvaluableNodeReference(EvaluableNodeImmediateValueWithType(), true);
			return EvaluableNodeReference::Null();
		}

		if(immediate_result.AnyImmediateType())
		{
			//first check for null, since it's not an immediate value
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NULL_VALUE))
			{
				if(en->GetType() == ENT_NULL)
				{
					if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NULL_VALUE))
						return EvaluableNodeReference(EvaluableNodeImmediateValueWithType(), true);
					return EvaluableNodeReference::Null();
				}
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

			//if it doesn't allow code, then coerce into the the most general type
			if(!immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::CODE))
			{
				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::EXISTING_KEY_STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDIfExists(en, true));

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::KEY_STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDWithReference(en, true), true);

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::EXISTING_STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDIfExists(en));

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::STRING_ID))
					return EvaluableNodeReference(EvaluableNode::ToStringIDWithReference(en), true);

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::NUMBER))
					return EvaluableNodeReference(EvaluableNode::ToNumber(en), true);

				if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::BOOL))
					return EvaluableNodeReference(EvaluableNode::ToBool(en), true);

				//otherwise EvaluableNodeRequestedValueTypes::Type::NULL_VALUE
				return EvaluableNodeReference::Null();
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
	{
		return value.nodeValue.code;
	}

	//allow to use as an EvaluableNode *
	__forceinline EvaluableNode *operator->()
	{
		return value.nodeValue.code;
	}

	//forbid implicit assignment from a raw pointer, since things can go wrong
	EvaluableNodeReference &operator=(EvaluableNode *) = delete;

	__forceinline EvaluableNodeReference &operator =(const EvaluableNodeReference &enr)
	{
		//perform a memcpy because it's a union, to be safe; the compiler should optimize this out
		value = enr.value;
		unique = enr.unique;
		uniqueUnreferencedTopNode = enr.uniqueUnreferencedTopNode;
		return *this;
	}

	EvaluableNodeReference &operator=(EvaluableNodeReference &&enr)
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
