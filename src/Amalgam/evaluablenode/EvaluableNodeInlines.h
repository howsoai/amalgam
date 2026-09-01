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
	if(!HasExtendedValue())
	{
		GetAnnotationsAndCommentsStorage().Clear();
		return;
	}

	if(GetType() == ENT_ASSOC)
	{
		if(value.extendedMappedChildNodes.mappedChildNodes->size() > largestSmallAssocSize)
		{
			//need to keep extended
			GetAnnotationsAndCommentsStorage().Clear();
		}
		else //reduce to small
		{
			SmallAssocType temp_mcn = value.extendedMappedChildNodes.mappedChildNodes->ExtractVectorMap();
			value.extendedMappedChildNodes.mappedChildNodes.reset();
			new (&value.mappedChildNodes) SmallAssocType(std::move(temp_mcn));

			SetExtendedValue(false);
		}
	}
	else //ordered
	{
		OrderedType temp_ocn = std::move(*value.extendedOrderedChildNodes.orderedChildNodes);
		value.extendedOrderedChildNodes.orderedChildNodes.~unique_ptr<OrderedType>();
		new (&value.orderedChildNodes) OrderedType(std::move(temp_ocn));

		SetExtendedValue(false);
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

__forceinline bool EvaluableNode::CanRepresentValueAsANumber(EvaluableNode *e)
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

inline void EvaluableNode::SetTypeViaNumberValue(double v, bool clear_metadata)
{
	if(clear_metadata)
		ClearMetadata();

	if(FastIsNaN(v))
	{
		SetType(ENT_NULL, false);
	}
	else
	{
		SetType(ENT_NUMBER, false);
		GetNumberValueReference() = v;
	}
}

inline void EvaluableNode::SetTypeViaStringIdValue(std::string &v, bool clear_metadata)
{
	if(clear_metadata)
		ClearMetadata();

	SetType(ENT_STRING, false);
	GetStringIDReference() = string_intern_pool.CreateStringReference(v);
}

//changes the type by setting it to the string id value specified
inline void EvaluableNode::SetTypeViaStringIdValue(StringInternPool::StringID v, bool clear_metadata)
{
	if(clear_metadata)
		ClearMetadata();

	if(v == string_intern_pool.NOT_A_STRING_ID)
	{
		SetType(ENT_NULL, false);
	}
	else
	{
		SetType(ENT_STRING, false);
		GetStringIDReference() = string_intern_pool.CreateStringReference(v);
	}
}

//changes the type by setting it to the string id value specified, handing off the reference
inline void EvaluableNode::SetTypeViaStringIdValueWithReferenceHandoff(
	StringInternPool::StringID v, bool clear_metadata)
{
	if(clear_metadata)
		ClearMetadata();

	if(v == string_intern_pool.NOT_A_STRING_ID)
	{
		SetType(ENT_NULL, false);
	}
	else
	{
		SetType(ENT_STRING, false);
		GetStringIDReference() = v;
	}
}

inline void EvaluableNode::InitStringValue()
{
	DestructValue();
	value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
	AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
}

__forceinline StringInternPool::StringID EvaluableNode::GetStringID()
{
	if(DoesEvaluableNodeTypeUseStringData(GetType()))
		return GetStringIDReference();

	return StringInternPool::NOT_A_STRING_ID;
}

#ifdef MULTITHREAD_SUPPORT
__forceinline bool EvaluableNode::HasAttributeAtomic(Attribute attr)
{
	std::atomic_ref atomic_ref(attributes);
	AttributeStorageType cur = atomic_ref.load(std::memory_order_seq_cst);
	return (cur & static_cast<AttributeStorageType>(attr)) != 0;
}

__forceinline void EvaluableNode::SetAttributeAtomic(Attribute attr, bool enable)
{
	AttributeStorageType mask = static_cast<AttributeStorageType>(attr);

	std::atomic_ref atomic_ref(attributes);
	if(enable)
		atomic_ref.fetch_or(mask, std::memory_order_seq_cst);
	else
		atomic_ref.fetch_and(~mask, std::memory_order_seq_cst);
}

__forceinline bool EvaluableNode::TrySetAttributeAtomic(Attribute attr)
{
	constexpr AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::KNOWN_TO_BE_IN_USE);

	//check if already set, relaxed is fine for early-out
	std::atomic_ref atomic_ref(attributes);
	AttributeStorageType current_flags = atomic_ref.load(std::memory_order_relaxed);
	if(current_flags & mask)
		return false;

	//slow path to actually set the value
	while(!atomic_ref.compare_exchange_weak(
		current_flags, current_flags | mask, std::memory_order_acquire, std::memory_order_relaxed))
	{
		//see if another thread set it
		if(current_flags & mask)
			return false;
	}

	return true;
}

__forceinline bool EvaluableNode::SetIsFreeableAtomic(bool is_freeable)
{
	AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::FREEABLE);
	std::atomic_ref atomic_ref(attributes);

	if(is_freeable)
	{
		AttributeStorageType previous_value = atomic_ref.fetch_or(mask);
		return (previous_value & mask) != 0;
	}
	else
	{
		AttributeStorageType previous_value = atomic_ref.fetch_and(~mask);
		return (previous_value & mask) != 0;
	}
}

__forceinline bool EvaluableNode::SetIsFreeableTopNodeAtomic(bool is_freeable)
{
	AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::FREEABLE_TOP_NODE);

	std::atomic_ref atomic_ref(attributes);
	if(is_freeable)
	{
		AttributeStorageType previous_value = atomic_ref.fetch_or(mask);
		return (previous_value & mask) != 0;
	}
	else
	{
		AttributeStorageType previous_value = atomic_ref.fetch_and(~mask);
		return (previous_value & mask) != 0;
	}
}

#endif

__forceinline std::pair<bool, bool> EvaluableNode::SetIsFreeableAndIsFreeableTopNode(bool is_freeable)
{
	AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::FREEABLE) |
								static_cast<AttributeStorageType>(Attribute::FREEABLE_TOP_NODE);

	AttributeStorageType previous_value = attributes;

	if(is_freeable)
		attributes |= static_cast<AttributeStorageType>(mask);
	else
		attributes &= ~static_cast<AttributeStorageType>(mask);

	return {(previous_value & static_cast<AttributeStorageType>(Attribute::FREEABLE)) != 0,
		(previous_value & static_cast<AttributeStorageType>(Attribute::FREEABLE_TOP_NODE)) != 0};
}

#ifdef MULTITHREAD_SUPPORT
__forceinline std::pair<bool, bool> EvaluableNode::SetIsFreeableAndIsFreeableTopNodeAtomic(bool is_freeable)
{
	AttributeStorageType mask = static_cast<AttributeStorageType>(Attribute::FREEABLE) |
								static_cast<AttributeStorageType>(Attribute::FREEABLE_TOP_NODE);
	AttributeStorageType previous_value;

	std::atomic_ref atomic_ref(attributes);
	if(is_freeable)
		previous_value = atomic_ref.fetch_or(mask);
	else
		previous_value = atomic_ref.fetch_and(~mask);

	return {(previous_value & static_cast<AttributeStorageType>(Attribute::FREEABLE)) != 0,
		(previous_value & static_cast<AttributeStorageType>(Attribute::FREEABLE_TOP_NODE)) != 0};
}
#endif

__forceinline void EvaluableNode::UpdateFlagsBasedOnNewChildNode(EvaluableNode *new_child)
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
__forceinline void EvaluableNode::UpdateAllFlagsBasedOnNoReferencingChildNodes()
{
	bool is_idempotent = IsEvaluableNodeTypePotentiallyIdempotent(GetType());
	bool need_cycle_check = false;

	if(IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : GetMappedChildNodesViewOnAssoc())
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
	else if(!IsTerminal())
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

inline void EvaluableNode::InitOrderedChildNodes()
{
	DestructValue();

	if(!HasExtendedValue())
		value.ConstructOrderedChildNodes();
	else
		value.extendedOrderedChildNodes.Construct();
}

template<typename StoreValueFunction>
inline void EvaluableNode::ConvertChildNodesAndStoreValue(EvaluableNode *node,
	std::vector<StringInternPool::StringID> &element_names, size_t num_expected_elements,
	StoreValueFunction store_value)
{
	if(EvaluableNode::IsTerminal(node))
	{
		//fill in with the node's value
		for(size_t i = 0; i < num_expected_elements; i++)
			store_value(i, true, node);
	}
	else if(node->IsAssociativeArray())
	{
		auto mcn = node->GetMappedChildNodesViewOnAssoc();
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

template<typename ContainerIterator>
inline void EvaluableNode::SetOrderedChildNodes(
	ContainerIterator first, ContainerIterator last, bool need_cycle_check, bool is_idempotent)
{
	if(!IsOrderedArray())
		return;

	auto &ocn = GetOrderedChildNodesReference();
	ocn.assign(first, last);

	SetNeedCycleCheck(need_cycle_check);

	if(is_idempotent && !IsEvaluableNodeTypePotentiallyIdempotent(type))
		SetIsIdempotent(false);
	else
		SetIsIdempotent(is_idempotent);
}

//if the OrderedChildNodes list was using extra memory (if it were resized to be smaller), this would attempt to free extra memory
inline void EvaluableNode::ReleaseOrderedChildNodesExtraMemory()
{
	if(IsOrderedArray())
		GetOrderedChildNodesReference().shrink_to_fit();
}

inline void EvaluableNode::InitMappedChildNodes(size_t num_elements)
{
	DestructValue();

	if(!HasExtendedValue() && num_elements <= largestSmallAssocSize)
	{
		value.ConstructMappedChildNodes();
		value.mappedChildNodes.reserve(num_elements);
	}
	else
	{
		value.extendedMappedChildNodes.Construct();
		value.extendedMappedChildNodes.mappedChildNodes->reserve(num_elements);
		SetExtendedValue(true);
	}
}

//preallocates to_reserve for appending, etc.
inline void EvaluableNode::ReserveMappedChildNodes(size_t to_reserve)
{
	if(IsAssociativeArray())
		GetMappedChildNodesViewOnAssoc().reserve(to_reserve);
}

__forceinline EvaluableNode::AssocRef EvaluableNode::GetMappedChildNodesView()
{
	if(IsAssociativeArray())
		return GetMappedChildNodesViewOnAssoc();

	return emptyMappedChildNodesNode.GetMappedChildNodesViewOnAssoc();
}

__forceinline EvaluableNode::AssocRef EvaluableNode::GetMappedChildNodesView(EvaluableNode *en)
{
	if(en == nullptr || !en->IsAssociativeArray())
		return emptyMappedChildNodesNode.GetMappedChildNodesViewOnAssoc();
	return en->GetMappedChildNodesViewOnAssoc();
}

//if the id exists, returns a pointer to the pointer of the child node
// returns nullptr if the id doesn't exist
inline EvaluableNode **EvaluableNode::GetMappedChildNode(const std::string &id)
{
	StringInternPool::StringID sid = string_intern_pool.GetIDFromString(id);
	return GetMappedChildNode(sid);
}
//if the id exists, returns a pointer to the pointer of the child node
// returns nullptr if the id doesn't exist
inline EvaluableNode **EvaluableNode::GetMappedChildNode(const StringInternPool::StringID sid)
{
	auto mcn = GetMappedChildNodesView();
	auto node_iter = mcn.find(sid);
	if(node_iter == end(mcn))
		return nullptr;

	//return the location of the child pointer
	return &node_iter->second;
}

template<typename T>
void EvaluableNode::GetValueFromMappedChildNodesReference(
	EvaluableNode::AssocRef mcn, EvaluableNodeBuiltInStringId key, T &value)
{
	auto found_value = mcn.find(GetStringIdFromBuiltInStringId(key));
	if(found_value != end(mcn))
	{
		if constexpr(std::is_same<T, bool>::value)
			value = EvaluableNode::ToBool(found_value->second);
		else if constexpr(std::is_same<T, double>::value)
			value = EvaluableNode::ToNumber(found_value->second);
		else if constexpr(std::is_same<T, size_t>::value)
			value = static_cast<size_t>(EvaluableNode::ToNumber(found_value->second, 0));
		else if constexpr(std::is_same<T, int64_t>::value)
			value = static_cast<int64_t>(EvaluableNode::ToNumber(found_value->second, 0));
		else if constexpr(std::is_same<T, std::string>::value)
			value = EvaluableNode::ToString(found_value->second);
		else if constexpr(std::is_same<T, StringInternPool::StringID>::value)
			value = EvaluableNode::ToStringIDIfExists(found_value->second);
		else
			value = found_value->second;
	}
}

__forceinline bool &EvaluableNode::GetBoolValueReference()
{
	return value.boolValueContainer.boolValue;
}

__forceinline double &EvaluableNode::GetNumberValueReference()
{
	return value.numberAndNullValueContainer.numberValue;
}

__forceinline StringInternPool::StringID &EvaluableNode::GetStringIDReference()
{
	return value.stringValueContainer.stringID;
}

__forceinline EvaluableNode::OrderedRef EvaluableNode::GetOrderedChildNodesReference()
{
	if(!HasExtendedValue())
		return value.orderedChildNodes;
	else
		return *value.extendedOrderedChildNodes.orderedChildNodes.get();
}

__forceinline EvaluableNode::AssocRef EvaluableNode::GetMappedChildNodesViewOnAssoc()
{
	return EvaluableNode::AssocRef(this);
}

__forceinline EvaluableNode::AnnotationsAndComments &EvaluableNode::GetAnnotationsAndCommentsStorage()
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

inline void EvaluableNode::DestructValue()
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
