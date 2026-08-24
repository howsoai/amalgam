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
