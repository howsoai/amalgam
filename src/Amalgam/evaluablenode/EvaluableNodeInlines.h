#pragma once
//this file is intended to be included only by EvaluableNode.h

inline void EvaluableNode::Invalidate()
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

inline void EvaluableNode::InitializeType(EvaluableNodeType _type, const std::string &string_value)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(IsEvaluableNodeTypeValid(_type));
#endif

	type = _type;
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
	AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);

	SetIsIdempotent(type == ENT_STRING);
	SetNeedCycleCheck(false);
}

inline void EvaluableNode::InitializeType(EvaluableNodeType _type, const std::string_view string_value)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(IsEvaluableNodeTypeValid(_type));
#endif

	type = _type;
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(string_value);
	AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);

	SetIsIdempotent(type == ENT_STRING);
	SetNeedCycleCheck(false);
}

inline void EvaluableNode::InitializeType(EvaluableNodeType _type, StringInternPool::StringID string_id)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(IsEvaluableNodeTypeValid(_type));
#endif

	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	if(string_id == StringInternPool::NOT_A_STRING_ID)
	{
		type = ENT_NULL;
		value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
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

inline void EvaluableNode::InitializeTypeWithReferenceHandoff(EvaluableNodeType _type, StringInternPool::StringID string_id)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(IsEvaluableNodeTypeValid(_type));
#endif

	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	if(string_id == StringInternPool::NOT_A_STRING_ID)
	{
		type = ENT_NULL;
		value.numberAndNullValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
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

inline void EvaluableNode::InitializeType(double number_value)
{
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	value.numberAndNullValueContainer.numberValue = number_value;
	AnnotationsAndComments::Construct(value.numberAndNullValueContainer.annotationsAndComments);

	if(!FastIsNaN(number_value))
		type = ENT_NUMBER;
	else
		type = ENT_NULL;


	SetIsIdempotent(true);
	SetNeedCycleCheck(false);
}

inline void EvaluableNode::InitializeType(bool bool_value)
{
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	type = ENT_BOOL;
	value.boolValueContainer.boolValue = bool_value;
	AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);

	SetIsIdempotent(true);
	SetNeedCycleCheck(false);
}

__forceinline constexpr void EvaluableNode::InitializeUnallocated()
{
	type = ENT_UNINITIALIZED;
}

inline void EvaluableNode::InitializeType(EvaluableNodeType _type)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	AmlgAssert(IsEvaluableNodeTypeValid(_type) || _type == ENT_DEALLOCATED);
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

__forceinline void EvaluableNode::ClearAnnotationsAndComments()
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
			OrderedType temp_ocn = std::move(*value.extendedOrderedChildNodes.orderedChildNodes);
			value.extendedOrderedChildNodes.orderedChildNodes.~unique_ptr<OrderedType>();
			new (&value.orderedChildNodes) OrderedType(std::move(temp_ocn));
		}

		SetExtendedValue(false);
	}
	else
	{
		GetAnnotationsAndCommentsStorage().Clear();
	}
}

__forceinline void EvaluableNode::ClearMetadata()
{
	ClearAnnotationsAndComments();
	SetConcurrency(false);
}

__forceinline bool EvaluableNode::HasMetadata()
{
	auto &a_and_c = GetAnnotationsAndCommentsStorage();
	return (a_and_c.HasCommentOrAnnotation() || GetConcurrency());
}

inline bool EvaluableNode::AreShallowEqual(EvaluableNode *a, EvaluableNode *b)
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

inline bool EvaluableNode::AreDeepEqual(EvaluableNode *a, EvaluableNode *b)
{
	//if pointers are the same, then they are the same
	if(a == b)
		return true;

	//first check if the immediate values are equal
	if(!AreShallowEqual(a, b))
		return false;

	//since they are shallow equal, check for quick exit
	if(a == nullptr || b == nullptr || IsEvaluableNodeTypeTerminalNode(a->GetType()))
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

