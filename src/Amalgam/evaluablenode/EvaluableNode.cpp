//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeManagement.h"
#include "FastMath.h"
#include "Opcodes.h"
#include "Parser.h"
#include "StringInternPool.h"

//system headers:
#include <algorithm>

bool EvaluableNode::falseBoolValue = false;
double EvaluableNode::nanNumberValue = std::numeric_limits<double>::quiet_NaN();
std::string EvaluableNode::emptyStringValue = "";
EvaluableNode *EvaluableNode::emptyEvaluableNodeNullptr = nullptr;
std::vector<std::string> EvaluableNode::emptyStringVector;
std::vector<StringInternPool::StringID> EvaluableNode::emptyStringIdVector;
std::vector<EvaluableNode *> EvaluableNode::emptyOrderedChildNodes;
EvaluableNode::AssocType EvaluableNode::emptyMappedChildNodes;

//field for watching EvaluableNodes for debugging
FastHashSet<EvaluableNode *> EvaluableNode::debugWatch;
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
Concurrency::SingleMutex EvaluableNode::debugWatchMutex;
#endif

bool EvaluableNode::AreShallowEqual(EvaluableNode *a, EvaluableNode *b)
{
	//check if one is null, then make sure both are null
	bool a_is_null = EvaluableNode::IsNull(a);
	bool b_is_null = EvaluableNode::IsNull(b);
	if(a_is_null || b_is_null)
	{
		if(a_is_null == b_is_null)
			return true;

		//one is null and the other isn't
		return false;
	}

	EvaluableNodeType a_type = a->GetType();
	EvaluableNodeType b_type = b->GetType();

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

int EvaluableNode::Compare(EvaluableNode *a, EvaluableNode *b)
{
	//try numerical comparison first
	if(CanRepresentValueAsANumber(a) && CanRepresentValueAsANumber(b))
	{
		double n_a = EvaluableNode::ToNumber(a);
		double n_b = EvaluableNode::ToNumber(b);

		bool a_nan = FastIsNaN(n_a);
		bool b_nan = FastIsNaN(n_b);
		if(a_nan && b_nan)
			return 0;
		if(a_nan)
			return -1;
		if(b_nan)
			return 1;

		if(n_a < n_b)
			return -1;
		else if(n_b < n_a)
			return 1;
		else
			return 0;
	}

	//compare via strings
	//first check if they're the same
	if(a != nullptr && b != nullptr)
	{
		if(DoesEvaluableNodeTypeUseStringData(a->GetType()) && DoesEvaluableNodeTypeUseStringData(b->GetType())
			&& a->GetStringIDReference() == b->GetStringIDReference())
			return 0;
	}

	std::string a_str = EvaluableNode::ToString(a, true);
	std::string b_str = EvaluableNode::ToString(b, true);
	return StringManipulation::StringNaturalCompare(a_str, b_str);
}

bool EvaluableNode::ToBool(EvaluableNode *n)
{
	if(n == nullptr)
		return false;

	EvaluableNodeType node_type = n->GetType();
	if(node_type == ENT_NULL)
		return false;

	if(DoesEvaluableNodeTypeUseBoolData(node_type))
		return n->GetBoolValueReference();

	if(DoesEvaluableNodeTypeUseNumberData(node_type))
	{
		double &num = n->GetNumberValueReference();
		if(num == 0.0)
			return false;
		return true;
	}

	if(DoesEvaluableNodeTypeUseStringData(node_type))
	{
		auto sid = n->GetStringIDReference();
		if(sid == string_intern_pool.NOT_A_STRING_ID || sid == string_intern_pool.emptyStringId)
			return false;
		return true;
	}

	return true;
}

std::string EvaluableNode::BoolToString(bool value, bool key_string)
{
	if(key_string)
		return GetStringIdFromBuiltInStringId(value ? ENBISI_true_key : ENBISI_false_key)->string;
	return GetStringIdFromBuiltInStringId(value ? ENBISI_true : ENBISI_false)->string;
}

StringInternPool::StringID EvaluableNode::BoolToStringID(bool value, bool key_string)
{
	if(key_string)
		return GetStringIdFromBuiltInStringId(value ? ENBISI_true_key : ENBISI_false_key);
	return GetStringIdFromBuiltInStringId(value ? ENBISI_true : ENBISI_false);
}

double EvaluableNode::ToNumber(EvaluableNode *e, double value_if_null)
{
	if(e == nullptr)
		return value_if_null;

	auto e_type = e->GetType();

	//check the most common case first
	if(e_type == ENT_NUMBER)
		return e->GetNumberValueReference();

	switch(e_type)
	{
		case ENT_BOOL:
			return (e->GetBoolValueReference() ? 1 : 0);
		case ENT_NULL:
			return value_if_null;
		case ENT_STRING:
		case ENT_SYMBOL:
		{
			auto sid = e->GetStringIDReference();
			if(sid == string_intern_pool.NOT_A_STRING_ID)
				return value_if_null;
			auto &str = string_intern_pool.GetStringFromID(sid);
			auto [value, success] = Platform_StringToNumber(str);
			if(success)
				return value;
			return value_if_null;
		}
		default:
			return static_cast<double>(e->GetNumChildNodes());
	}
}

std::string EvaluableNode::NumberToString(double value, bool key_string)
{
	if(key_string)
		return Parser::UnparseNumberToKeyString(value);
	else
		return StringManipulation::NumberToString(value);
}

std::string EvaluableNode::NumberToString(size_t value, bool key_string)
{
	if(key_string)
		return Parser::UnparseNumberToKeyString(value);
	return StringManipulation::NumberToString(value);
}

StringInternPool::StringID EvaluableNode::NumberToStringIDIfExists(double value, bool key_string)
{
	std::string num_str;
	if(key_string)
		num_str = Parser::UnparseNumberToKeyString(value);
	else
		num_str = StringManipulation::NumberToString(value);

	return string_intern_pool.GetIDFromString(num_str);
}

StringInternPool::StringID EvaluableNode::NumberToStringIDIfExists(size_t value, bool key_string)
{
	std::string num_str;
	if(key_string)
		num_str = Parser::UnparseNumberToKeyString(value);
	else
		num_str = StringManipulation::NumberToString(value);

	return string_intern_pool.GetIDFromString(num_str);
}

std::string EvaluableNode::ToString(EvaluableNode *e, bool key_string)
{
	if(key_string)
		return Parser::UnparseToKeyString(e);

	if(EvaluableNode::IsNull(e))
		return "(null)";

	if(e->GetType() == ENT_STRING)
		return e->GetStringValue();

	if(e->GetType() == ENT_NUMBER)
		return StringManipulation::NumberToString(e->GetNumberValueReference());

	return Parser::Unparse(e, false, false, true);
}

StringInternPool::StringID EvaluableNode::ToStringIDIfExists(EvaluableNode *e, bool key_string)
{
	if(EvaluableNode::IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING)
		return e->GetStringIDReference();

	std::string str_value = ToString(e, key_string);
	//will return empty string if not found
	return string_intern_pool.GetIDFromString(str_value);
}

StringInternPool::StringID EvaluableNode::ToStringIDWithReference(EvaluableNode *e, bool key_string)
{
	if(EvaluableNode::IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(e->GetStringIDReference());

	std::string str_value = ToString(e, key_string);
	return string_intern_pool.CreateStringReference(str_value);
}

StringInternPool::StringID EvaluableNode::ToStringIDTakingReferenceAndClearing(EvaluableNode *e, bool include_symbol, bool key_string)
{
	//null doesn't need a reference
	if(IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING || (include_symbol && e->GetType() == ENT_SYMBOL))
	{
		//clear the reference and return it
		StringInternPool::StringID &sid_reference = e->GetStringIDReference();
		StringInternPool::StringID sid_to_return = string_intern_pool.NOT_A_STRING_ID;
		std::swap(sid_reference, sid_to_return);
		return sid_to_return;
	}

	std::string str_value = ToString(e, key_string);
	return string_intern_pool.CreateStringReference(str_value);
}

void EvaluableNode::ConvertListToNumberedAssoc()
{
	//don't do anything if no child nodes
	if(!DoesEvaluableNodeTypeUseOrderedData(GetType()))
	{
		InitMappedChildNodes();
		type = ENT_ASSOC;
		return;
	}

	AssocType new_mcn;

	//convert ordered child nodes into index number -> value
	auto &ocn = GetOrderedChildNodesReference();
	new_mcn.reserve(ocn.size());
	for(size_t i = 0; i < ocn.size(); i++)
	{
		std::string s = NumberToString(i, true);
		new_mcn.emplace(string_intern_pool.CreateStringReference(s), ocn[i]);
	}

	InitMappedChildNodes();
	type = ENT_ASSOC;

	//swap for efficiency
	std::swap(GetMappedChildNodesReference(), new_mcn);
}

void EvaluableNode::ConvertAssocToList()
{
	//don't do anything if no child nodes
	if(!IsAssociativeArray())
		return;

	std::vector<EvaluableNode *> new_ocn;

	auto &mcn = GetMappedChildNodesReference();
	new_ocn.reserve(mcn.size());
	for(auto &[_, cn] : mcn)
		new_ocn.push_back(cn);

	InitOrderedChildNodes();
	type = ENT_LIST;

	//swap for efficiency
	std::swap(GetOrderedChildNodesReference(), new_ocn);
}

size_t EvaluableNode::GetEstimatedNodeSizeInBytes(EvaluableNode *n)
{
	if(n == nullptr)
		return 0;

	size_t total_size = 0;
	total_size += sizeof(EvaluableNode);
	if(n->HasExtendedValue())
		total_size += sizeof(EvaluableNode::EvaluableNodeValue);

	auto &a_and_c = n->GetAnnotationsAndCommentsStorage();
	size_t annotation_size = a_and_c.GetAnnotations().size();
	//count null terminator
	if(annotation_size > 0)
		annotation_size++;
	size_t comment_size = a_and_c.GetComments().size();
	//count null terminator
	if(comment_size > 0)
		comment_size++;
	total_size += annotation_size + comment_size;

	total_size += n->GetOrderedChildNodes().capacity() * sizeof(EvaluableNode *);
	total_size += n->GetMappedChildNodes().size() * (sizeof(StringInternPool::StringID) + sizeof(EvaluableNode *));

	return total_size;
}

bool EvaluableNode::IsNodeValid()
{
	if(!IsEvaluableNodeTypeValid(type))
		return false;

	//set a maximum number of valid elements of 100 million
	//this is not a hard limit, but a heuristic to detect issues
	size_t max_size = 100000000;
	if(DoesEvaluableNodeTypeUseAssocData(type))
	{
		auto &mcn = GetMappedChildNodesReference();
		return (mcn.size() < max_size);
	}
	else if(DoesEvaluableNodeTypeUseNumberData(type))
	{
		double number = GetNumberValueReference();
		return !FastIsNaN(number);
	}
	else if(DoesEvaluableNodeTypeUseStringData(type))
	{
		auto sid = GetStringIDReference();
		if(sid == string_intern_pool.NOT_A_STRING_ID)
			return true;

		return (sid->string.size() < 2000000000);
	}
	else if(DoesEvaluableNodeTypeUseBoolData(type))
	{
		return true;
	}
	else //ordered
	{
		auto &ocn = GetOrderedChildNodesReference();
		return (ocn.size() < max_size);
	}

	//shouldn't make it here
	return false;
}

void EvaluableNode::InitializeType(EvaluableNode *n, bool copy_metadata)
{
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	if(n == nullptr)
	{
		type = ENT_NULL;
		value.ConstructOrderedChildNodes();
		return;
	}

	type = n->GetType();

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(IsEvaluableNodeTypeValid(type));
#endif

	if(DoesEvaluableNodeTypeUseAssocData(type))
	{
		value.ConstructMappedChildNodes();
		value.mappedChildNodes = n->GetMappedChildNodesReference();

		SetIsIdempotent(true);
		for(auto &[sid, cn] : value.mappedChildNodes)
		{
			string_intern_pool.CreateStringReference(sid);
			if(cn != nullptr && !cn->GetIsIdempotent())
				SetIsIdempotent(false);
		}
	}
	else if(DoesEvaluableNodeTypeUseBoolData(type))
	{
		AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
		value.boolValueContainer.boolValue = n->GetBoolValueReference();
		SetIsIdempotent(true);
	}
	else if(DoesEvaluableNodeTypeUseNumberData(type))
	{
		AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
		value.numberValueContainer.numberValue = n->GetNumberValueReference();
		SetIsIdempotent(true);
	}
	else if(DoesEvaluableNodeTypeUseStringData(type))
	{
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(n->GetStringIDReference());
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
		SetIsIdempotent(type == ENT_STRING);
	}
	else //ordered
	{
		value.ConstructOrderedChildNodes();
		value.orderedChildNodes = n->GetOrderedChildNodesReference();

		//update idempotency
		if(IsEvaluableNodeTypePotentiallyIdempotent(type))
		{
			SetIsIdempotent(true);
			for(auto &cn : value.orderedChildNodes)
			{
				if(cn != nullptr && !cn->GetIsIdempotent())
				{
					SetIsIdempotent(false);
					break;
				}
			}
		}
		else
		{
			SetIsIdempotent(false);
		}
	}

	//child nodes were copied, so propagate whether cycle free
	SetNeedCycleCheck(n->GetNeedCycleCheck());

	if(copy_metadata)
	{
		SetConcurrency(n->GetConcurrency());

		if(n->HasExtendedValue())
		{
			EnsureEvaluableNodeExtended();
			if(copy_labels)
				SetLabelsStringIds(n->GetLabelsStringIds());
			if(copy_comments_and_concurrency)
				SetCommentsStringId(n->GetCommentsStringId());
		}
		//copy_comments doesn't matter because if made it here, there aren't any
		else if(copy_labels && HasCompactSingleLabelStorage())
		{
			StringInternPool::StringID id = n->GetCompactSingleLabelStorage();
			if(id != StringInternPool::NOT_A_STRING_ID)
				GetCompactSingleLabelStorage() = string_intern_pool.CreateStringReference(id);
		}
	}
}

void EvaluableNode::CopyValueFrom(EvaluableNode *n)
{
	//don't do anything if copying from itself (note that some flat hash map structures don't copy well onto themselves)
	if(n == this)
		return;

	if(n == nullptr)
	{
		ClearOrderedChildNodes();
		ClearMappedChildNodes();
		//doesn't need an EvaluableNodeManager because not converting child nodes from one type to another
		SetType(ENT_NULL, nullptr, false);
		return;
	}

	auto cur_type = n->GetType();

#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(IsEvaluableNodeTypeValid(cur_type));
#endif

	//doesn't need an EvaluableNodeManager because not converting child nodes from one type to another
	SetType(cur_type, nullptr, false);

	if(DoesEvaluableNodeTypeUseAssocData(cur_type))
	{
		auto &n_mcn = n->GetMappedChildNodesReference();
		if(n_mcn.empty())
			ClearMappedChildNodes();
		else
			SetMappedChildNodes(n_mcn, true, n->GetNeedCycleCheck(), n->GetIsIdempotent());
	}
	else if(DoesEvaluableNodeTypeUseBoolData(cur_type))
	{
		GetBoolValueReference() = n->GetBoolValueReference();
	}
	else if(DoesEvaluableNodeTypeUseNumberData(cur_type))
	{
		GetNumberValueReference() = n->GetNumberValueReference();
	}
	else if(DoesEvaluableNodeTypeUseStringData(cur_type))
	{
		SetStringID(n->GetStringIDReference());
	}
	else //ordered
	{
		auto &n_ocn = n->GetOrderedChildNodesReference();
		if(n_ocn.empty())
			ClearOrderedChildNodes();
		else
			SetOrderedChildNodes(n_ocn, n->GetNeedCycleCheck(), n->GetIsIdempotent());
	}

	if(GetNumLabels() > 0)
		SetIsIdempotent(false);
	else
		SetIsIdempotent(n->GetIsIdempotent());
}

void EvaluableNode::CopyMetadataFrom(EvaluableNode *n)
{
	//don't do anything if copying from itself
	if(n == this)
		return;

	//copy labels (different ways based on type)
	if(HasCompactSingleLabelStorage() && n->HasCompactSingleLabelStorage())
	{
		auto string_id = GetCompactSingleLabelStorage();
		auto n_string_id = n->GetCompactSingleLabelStorage();

		if(string_id != n_string_id)
		{
			string_intern_pool.DestroyStringReference(string_id);
			GetCompactSingleLabelStorage() = string_intern_pool.CreateStringReference(n_string_id);
			SetIsIdempotent(false);
		}
	}
	else
	{
		auto label_sids = n->GetLabelsStringIds();
		if(label_sids.size() > 0)
		{
			SetLabelsStringIds(label_sids);
			SetIsIdempotent(false);
		}
		else
			ClearLabels();
	}

	SetCommentsStringId(n->GetCommentsStringId());
	SetConcurrency(n->GetConcurrency());
}

void EvaluableNode::SetType(EvaluableNodeType new_type, EvaluableNodeManager *enm,
	bool attempt_to_preserve_immediate_value)
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(IsEvaluableNodeTypeValid(new_type));
#endif

	EvaluableNodeType cur_type = GetType();
	if(new_type == cur_type)
		return;

	if(    (DoesEvaluableNodeTypeUseBoolData(cur_type) && DoesEvaluableNodeTypeUseBoolData(new_type))
		|| (DoesEvaluableNodeTypeUseNumberData(cur_type) && DoesEvaluableNodeTypeUseNumberData(new_type))
		|| (DoesEvaluableNodeTypeUseStringData(cur_type) && DoesEvaluableNodeTypeUseStringData(new_type))
		|| (DoesEvaluableNodeTypeUseAssocData(cur_type)  && DoesEvaluableNodeTypeUseAssocData(new_type))
		|| (DoesEvaluableNodeTypeUseOrderedData(cur_type) && DoesEvaluableNodeTypeUseOrderedData(new_type)) )
	{
		type = new_type;

		//lose idempotency if the new type isn't
		if(GetIsIdempotent() && !IsEvaluableNodeTypePotentiallyIdempotent(type))
			SetIsIdempotent(false);

		return;
	}

	//need to preserve the extra label if it exists
	StringInternPool::StringID extra_label = StringInternPool::NOT_A_STRING_ID;
	if(HasCompactSingleLabelStorage())
	{
		extra_label = GetCompactSingleLabelStorage();
		GetCompactSingleLabelStorage() = StringInternPool::NOT_A_STRING_ID;
	}

	//transform as appropriate
	if(DoesEvaluableNodeTypeUseBoolData(new_type))
	{
		bool bool_value = false;
		if(attempt_to_preserve_immediate_value)
			bool_value = EvaluableNode::ToBool(this);

		InitBoolValue();
		GetBoolValueReference() = bool_value;

		//will check below if any reason to not be idempotent
		SetIsIdempotent(true);
	}
	else if(DoesEvaluableNodeTypeUseNumberData(new_type))
	{
		double number_value = 0.0;
		if(attempt_to_preserve_immediate_value)
			number_value = EvaluableNode::ToNumber(this);

		if(FastIsNaN(number_value))
		{
			new_type = ENT_NULL;
			InitOrderedChildNodes();
			SetNeedCycleCheck(false);
		}
		else
		{
			InitNumberValue();
			GetNumberValueReference() = number_value;

			//will check below if any reason to not be idempotent
			SetIsIdempotent(true);
		}
	}
	else if(DoesEvaluableNodeTypeUseStringData(new_type))
	{
		StringInternPool::StringID sid = string_intern_pool.emptyStringId;
		if(attempt_to_preserve_immediate_value)
			sid = EvaluableNode::ToStringIDWithReference(this);

		if(sid == string_intern_pool.NOT_A_STRING_ID)
		{
			new_type = ENT_NULL;
			InitOrderedChildNodes();
			SetNeedCycleCheck(false);
		}
		else
		{
			InitStringValue();
			GetStringIDReference() = sid;

			//will check below if any reason to not be idempotent
			SetIsIdempotent(new_type == ENT_STRING);
		}
	}
	else if(DoesEvaluableNodeTypeUseAssocData(new_type))
	{
		if(DoesEvaluableNodeTypeUseOrderedData(cur_type))
		{
			//convert ordered pairs to assoc
			AssocType new_map;

			auto &ocn = GetOrderedChildNodesReference();
			new_map.reserve((ocn.size() + 1) / 2);
			for(size_t i = 0; i < ocn.size(); i += 2)
			{
				auto sid = ToStringIDWithReference(ocn[i], true);

				EvaluableNode *value = nullptr;
				if(i + 1 < ocn.size())
					value = ocn[i + 1];

				//try to insert, but drop reference if couldn't
				if(!new_map.emplace(sid, value).second)
					string_intern_pool.DestroyStringReference(sid);
			}

			//set up mapped nodes
			InitMappedChildNodes();
			//swap for efficiency
			std::swap(GetMappedChildNodesReference(), new_map);
		}
		else //just set up empty assoc
		{
			InitMappedChildNodes();
			SetNeedCycleCheck(false);
		}
	}
	else //ordered pairs
	{
		//will need a valid enm to convert this
		if(DoesEvaluableNodeTypeUseAssocData(cur_type) && enm != nullptr)
		{
			std::vector<EvaluableNode *> new_ordered;
			auto &mcn = GetMappedChildNodesReference();
			new_ordered.reserve(2 * mcn.size());
			for(auto &[cn_id, cn] : mcn)
			{
				EvaluableNode *key = Parser::ParseFromKeyStringId(cn_id, enm);
				new_ordered.push_back(key);
				new_ordered.push_back(cn);
			}

			InitOrderedChildNodes();
			//swap for efficiency
			std::swap(GetOrderedChildNodesReference(), new_ordered);
		}
		else //just set up empty ordered
		{
			InitOrderedChildNodes();
			SetNeedCycleCheck(false);
		}
	}

	type = new_type;

	//put the extra label back on if exists (already have the reference)
	if(extra_label != StringInternPool::NOT_A_STRING_ID)
		AppendLabelStringId(extra_label, true);

	//reset idempotency if applicable
	// can only go one way with idempotency, because if it's not idempotent
	if(GetNumLabels() == 0)
	{
		if(GetIsIdempotent())
			SetIsIdempotent(IsEvaluableNodeTypePotentiallyIdempotent(new_type));
	}
	else
		SetIsIdempotent(false);
}

void EvaluableNode::InitBoolValue()
{
	DestructValue();

	if(HasExtendedValue())
	{
		value.extension.extendedValue->boolValueContainer.boolValue = false;
	}
	else
	{
		AnnotationsAndComments::Construct(value.boolValueContainer.annotationsAndComments);
		value.boolValueContainer.boolValue = false;
	}
}

void EvaluableNode::InitNumberValue()
{
	DestructValue();

	if(HasExtendedValue())
	{
		value.extension.extendedValue->numberValueContainer.numberValue = 0.0;
	}
	else
	{
		AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
		value.numberValueContainer.numberValue = 0.0;
	}
}

void EvaluableNode::InitStringValue()
{
	DestructValue();

	if(HasExtendedValue())
	{
		value.extension.extendedValue->stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
	}
	else
	{
		value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
		AnnotationsAndComments::Construct(value.stringValueContainer.annotationsAndComments);
	}
}

void EvaluableNode::SetStringID(StringInternPool::StringID id)
{
	if(id == StringInternPool::NOT_A_STRING_ID)
	{
		SetType(ENT_NULL, nullptr, false);
	}
	else
	{
		if(DoesEvaluableNodeTypeUseStringData(GetType()))
		{
			if(!HasExtendedValue())
			{
				StringInternPool::StringID cur_id = value.stringValueContainer.stringID;
				if(id != cur_id)
				{
					string_intern_pool.DestroyStringReference(cur_id);
					value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(id);
				}
			}
			else
			{
				StringInternPool::StringID cur_id = value.extension.extendedValue->stringValueContainer.stringID;
				if(id != cur_id)
				{
					string_intern_pool.DestroyStringReference(cur_id);
					value.extension.extendedValue->stringValueContainer.stringID = string_intern_pool.CreateStringReference(id);
				}
			}
		}
	}
}

const std::string &EvaluableNode::GetStringValue()
{
	if(DoesEvaluableNodeTypeUseStringData(GetType()))
	{
		if(!HasExtendedValue())
			return string_intern_pool.GetStringFromID(value.stringValueContainer.stringID);
		else
			return string_intern_pool.GetStringFromID(value.extension.extendedValue->stringValueContainer.stringID);
	}

	//none of the above, return an empty one
	return emptyStringValue;
}

//Note: this function is logically equivalent to SetStringValueID
// After string interning is implemented throughout, this should be revisited to see if these two functions should be combined.
void EvaluableNode::SetStringValue(const std::string &v)
{
	if(DoesEvaluableNodeTypeUseStringData(GetType()))
	{
		//create a new reference before destroying so don't accidentally destroy something that will then need to be recreated
		auto new_id = string_intern_pool.CreateStringReference(v);
		if(!HasExtendedValue())
		{	
			//destroy anything that was already in there
			string_intern_pool.DestroyStringReference(value.stringValueContainer.stringID);
			value.stringValueContainer.stringID = new_id;
		}
		else
		{
			//destroy anything that was already in there
			string_intern_pool.DestroyStringReference(value.extension.extendedValue->stringValueContainer.stringID);
			value.extension.extendedValue->stringValueContainer.stringID = new_id;
		}
	}
}

StringInternPool::StringID EvaluableNode::GetAndClearStringIDWithReference()
{
	StringInternPool::StringID sid = StringInternPool::NOT_A_STRING_ID;
	if(DoesEvaluableNodeTypeUseStringData(GetType()))
	{
		//retrieve id and just clear it, as the caller will take care of the reference
		if(!HasExtendedValue())
		{
			sid = value.stringValueContainer.stringID;
			value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
		}
		else
		{
			sid = value.extension.extendedValue->stringValueContainer.stringID;
			value.extension.extendedValue->stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
		}
	}

	return sid;
}

void EvaluableNode::SetStringIDWithReferenceHandoff(StringInternPool::StringID id)
{
	if(id == StringInternPool::NOT_A_STRING_ID)
	{
		SetType(ENT_NULL, nullptr, false);
	}
	else
	{
		if(DoesEvaluableNodeTypeUseStringData(GetType()))
		{
			if(!HasExtendedValue())
			{
				StringInternPool::StringID cur_id = value.stringValueContainer.stringID;
				string_intern_pool.DestroyStringReference(cur_id);
				value.stringValueContainer.stringID = id;
			}
			else
			{
				StringInternPool::StringID cur_id = value.extension.extendedValue->stringValueContainer.stringID;
				string_intern_pool.DestroyStringReference(cur_id);
				value.extension.extendedValue->stringValueContainer.stringID = id;
			}
		}
	}
}

static inline std::vector<std::string> BreakApartSeparateLines(std::string_view full_string)
{
	std::vector<std::string> comment_lines;

	//early exit
	if(full_string.empty())
		return comment_lines;

	size_t cur = 0;
	size_t prev = 0;
	while((cur = full_string.find('\n', prev)) != std::string::npos)
	{
		//skip carriage return if found prior to the newline
		int carriage_return_offset = 0;
		if(prev < cur && full_string[cur - 1] == '\r')
			carriage_return_offset = 1;

		comment_lines.emplace_back(full_string.substr(prev, cur - prev - carriage_return_offset));
		prev = cur + 1;
	}

	//get whatever is left
	if(prev < full_string.size())
		comment_lines.emplace_back(full_string.substr(prev));

	return comment_lines;
}

std::vector<std::string> EvaluableNode::GetAnnotationsSeparateLines()
{
	return BreakApartSeparateLines(GetAnnotationsString());
}

std::vector<std::string> EvaluableNode::GetCommentsSeparateLines()
{
	return BreakApartSeparateLines(GetCommentsString());
}

size_t EvaluableNode::GetNumChildNodes()
{
	if(IsEvaluableNodeTypeImmediate(GetType()))
		return 0;

	if(IsAssociativeArray())
		return GetMappedChildNodesReference().size();
	else
		return GetOrderedChildNodesReference().size();

	return 0;
}

void EvaluableNode::InitOrderedChildNodes()
{
	DestructValue();

	if(HasExtendedValue())
		value.extension.extendedValue->ConstructOrderedChildNodes();
	else
		value.ConstructOrderedChildNodes();
}

void EvaluableNode::SetOrderedChildNodes(const std::vector<EvaluableNode *> &ocn,
	bool need_cycle_check, bool is_idempotent)
{
	if(!IsOrderedArray())
		return;

	GetOrderedChildNodesReference() = ocn;

	SetNeedCycleCheck(need_cycle_check);

	if(is_idempotent && (GetNumLabels() > 0 || !IsEvaluableNodeTypePotentiallyIdempotent(type)))
		SetIsIdempotent(false);
	else
		SetIsIdempotent(is_idempotent);
}

void EvaluableNode::SetOrderedChildNodes(std::vector<EvaluableNode *> &&ocn,
	bool need_cycle_check, bool is_idempotent)
{
	if(!IsOrderedArray())
		return;

	GetOrderedChildNodesReference() = std::move(ocn);

	SetNeedCycleCheck(need_cycle_check);

	if(is_idempotent && (GetNumLabels() > 0 || !IsEvaluableNodeTypePotentiallyIdempotent(type)))
		SetIsIdempotent(false);
	else
		SetIsIdempotent(is_idempotent);
}

void EvaluableNode::ClearOrderedChildNodes()
{
	if(!IsOrderedArray())
		return;

	GetOrderedChildNodesReference().clear();

	SetNeedCycleCheck(false);

	if(GetNumLabels() == 0)
		SetIsIdempotent(IsEvaluableNodeTypePotentiallyIdempotent(type));
}

void EvaluableNode::AppendOrderedChildNode(EvaluableNode *cn)
{
	if(!IsOrderedArray())
		return;

	GetOrderedChildNodesReference().push_back(cn);

	if(cn != nullptr)
	{
		//if cycles, propagate upward
		if(cn->GetNeedCycleCheck())
			SetNeedCycleCheck(true);

		//propagate idempotency
		if(!cn->GetIsIdempotent())
			SetIsIdempotent(false);
	}
}

void EvaluableNode::AppendOrderedChildNodes(const std::vector<EvaluableNode *> &ocn_to_append)
{
	if(!IsOrderedArray())
		return;

	auto &ocn = GetOrderedChildNodesReference();
	ocn.insert(end(ocn), begin(ocn_to_append), end(ocn_to_append));

	//if cycles, propagate upward
	for(auto cn : ocn_to_append)
	{
		if(cn != nullptr && cn->GetNeedCycleCheck())
		{
			SetNeedCycleCheck(true);
			break;
		}
	}

	//propagate idempotency
	if(GetIsIdempotent())
	{
		for(auto cn : ocn_to_append)
		{
			if(cn != nullptr && !cn->GetIsIdempotent())
			{
				SetIsIdempotent(false);
				break;
			}
		}
	}
}

void EvaluableNode::InitMappedChildNodes()
{
	DestructValue();

	if(!HasExtendedValue())
		value.ConstructMappedChildNodes();
	else
		value.extension.extendedValue->ConstructMappedChildNodes();
}

EvaluableNode **EvaluableNode::GetMappedChildNode(const StringInternPool::StringID sid)
{
	auto &mcn = GetMappedChildNodes();
	auto node_iter = mcn.find(sid);
	if(node_iter == end(mcn))
		return nullptr;

	//return the location of the child pointer
	return &node_iter->second;
}

EvaluableNode **EvaluableNode::GetOrCreateMappedChildNode(const std::string &id)
{
	auto &mcn = GetMappedChildNodesReference();

	//create a reference in case it doesn't exist yet
	StringInternPool::StringID sid = string_intern_pool.CreateStringReference(id);

	auto [inserted_node, inserted] = mcn.emplace(sid, nullptr);

	//if the node was not inserted, then don't need the reference created
	if(!inserted)
		string_intern_pool.DestroyStringReference(sid);

	//return the location of the child pointer
	return &inserted_node->second;
}

EvaluableNode **EvaluableNode::GetOrCreateMappedChildNode(const StringInternPool::StringID sid)
{
	auto &mcn = GetMappedChildNodesReference();
	auto [inserted_node, inserted] = mcn.emplace(sid, nullptr);

	//if the node was inserted, then create a reference
	if(inserted)
		string_intern_pool.CreateStringReference(sid);

	return &inserted_node->second;
}

void EvaluableNode::SetMappedChildNodes(AssocType &new_mcn, bool copy, bool need_cycle_check, bool is_idempotent)
{
	if(!IsAssociativeArray())
		return;

	auto &mcn = GetMappedChildNodesReference();

	//create new references before freeing old ones
	string_intern_pool.CreateStringReferences(new_mcn, [](auto n) { return n.first; });

	//destroy any string refs for map
	string_intern_pool.DestroyStringReferences(mcn, [](auto n) { return n.first; });

	//swap map heap memory with new_mcn
	if(copy)
		mcn = new_mcn;
	else
		mcn.swap(new_mcn);

	SetNeedCycleCheck(need_cycle_check);

	if(is_idempotent && (GetNumLabels() > 0 || !IsEvaluableNodeTypePotentiallyIdempotent(type)))
		SetIsIdempotent(false);
	else
		SetIsIdempotent(is_idempotent);
}

std::pair<bool, EvaluableNode **> EvaluableNode::SetMappedChildNode(const std::string &id, EvaluableNode *node, bool overwrite)
{
	if(!IsAssociativeArray())
		return std::make_pair(false, nullptr);

	auto &mcn = GetMappedChildNodesReference();

	StringInternPool::StringID sid = string_intern_pool.CreateStringReference(id);

	//try to insert; if fail, then need to remove extra reference and update node
	auto [inserted_node, inserted] = mcn.emplace(sid, node);
	if(!inserted)
	{
		string_intern_pool.DestroyStringReference(sid);
		if(!overwrite)
			return std::make_pair(false, &inserted_node->second);
	}

	//set node regardless of whether it was added
	inserted_node->second = node;

	if(node != nullptr)
	{
		//if cycles, propagate upward
		if(node->GetNeedCycleCheck())
			SetNeedCycleCheck(true);

		//propagate idempotency
		if(!node->GetIsIdempotent())
			SetIsIdempotent(false);
	}

	return std::make_pair(true, &inserted_node->second);
}

std::pair<bool, EvaluableNode **> EvaluableNode::SetMappedChildNode(const StringInternPool::StringID sid, EvaluableNode *node, bool overwrite)
{
	if(!IsAssociativeArray())
		return std::make_pair(false, nullptr);

	auto &mcn = GetMappedChildNodesReference();

	auto [inserted_node, inserted] = mcn.emplace(sid, node);

	if(inserted)
		string_intern_pool.CreateStringReference(sid); //create string reference if pair was successfully set/added
	else
	{
		//if not overwriting, return if sid is already found
		if(!overwrite)
			return std::make_pair(false, &inserted_node->second);

		//update the value
		inserted_node->second = node;
	}

	if(node != nullptr)
	{
		//if cycles, propagate upward
		if(node->GetNeedCycleCheck())
			SetNeedCycleCheck(true);

		//propagate idempotency
		if(!node->GetIsIdempotent())
			SetIsIdempotent(false);
	}

	return std::make_pair(true, &inserted_node->second);
}

bool EvaluableNode::SetMappedChildNodeWithReferenceHandoff(const StringInternPool::StringID sid, EvaluableNode *node, bool overwrite)
{
	if(!IsAssociativeArray())
	{
		string_intern_pool.DestroyStringReference(sid);
		return false;
	}

	auto &mcn = GetMappedChildNodesReference();

	auto [inserted_node, inserted] = mcn.emplace(sid, node);

	if(!inserted)
	{
		//destroy the reference that was passed in, since this node already has a reference
		string_intern_pool.DestroyStringReference(sid);
		if(!overwrite)
			return false; //if not overwriting, return if sid is already found

		//update the value
		inserted_node->second = node;
	}

	if(node != nullptr)
	{
		//if cycles, propagate upward
		if(node->GetNeedCycleCheck())
			SetNeedCycleCheck(true);

		//propagate idempotency
		if(!node->GetIsIdempotent())
			SetIsIdempotent(false);
	}

	return true;
}

void EvaluableNode::ClearMappedChildNodes()
{
	if(!IsAssociativeArray())
		return;

	auto &map = GetMappedChildNodesReference();
	string_intern_pool.DestroyStringReferences(map, [](auto n) { return n.first; });
	map.clear();

	SetNeedCycleCheck(false);

	if(GetNumLabels() == 0)
		SetIsIdempotent(IsEvaluableNodeTypePotentiallyIdempotent(type));
}

EvaluableNode *EvaluableNode::EraseMappedChildNode(const StringInternPool::StringID sid)
{
	auto &mcn = GetMappedChildNodes();
	//attempt to find
	auto found = mcn.find(sid);
	if(found == end(mcn))
		return nullptr;

	//erase and return the value
	string_intern_pool.DestroyStringReference(sid);
	EvaluableNode *erased_value = found->second;
	mcn.erase(found);
	return erased_value;
}

void EvaluableNode::AppendMappedChildNodes(AssocType &mcn_to_append)
{
	if(!IsAssociativeArray())
		return;

	auto &mcn = GetMappedChildNodesReference();
	mcn.reserve(mcn.size() + mcn_to_append.size());

	//insert everything
	for(auto &[n_id, n] : mcn_to_append)
	{
		auto [inserted_node, inserted] = mcn.emplace(n_id, n);

		if(inserted)
			string_intern_pool.CreateStringReference(n_id); //create string reference if pair was successfully set/added
		else //overwrite
			inserted_node->second = n;

		if(n != nullptr)
		{
			//if cycles, propagate upward
			if(n->GetNeedCycleCheck())
				SetNeedCycleCheck(true);

			//propagate idempotency
			if(!n->GetIsIdempotent())
				SetIsIdempotent(false);
		}
	}
}

void EvaluableNode::EnsureEvaluableNodeExtended()
{
	if(HasExtendedValue())
		return;

	EvaluableNodeValue *ev = new EvaluableNodeValue;

	switch(GetType())
	{
	case ENT_BOOL:
		ev->boolValueContainer.boolValue = value.boolValueContainer.boolValue;
		if(value.boolValueContainer.labelStringID != StringInternPool::NOT_A_STRING_ID)
			ev->labelsStringIds.push_back(value.boolValueContainer.labelStringID);
		break;
	case ENT_NUMBER:
		ev->numberValueContainer.numberValue = value.numberValueContainer.numberValue;
		if(value.numberValueContainer.labelStringID != StringInternPool::NOT_A_STRING_ID)
			ev->labelsStringIds.push_back(value.numberValueContainer.labelStringID);
		break;
	case ENT_STRING:
	case ENT_SYMBOL:
		ev->stringValueContainer.stringID = value.stringValueContainer.stringID;
		if(value.stringValueContainer.labelStringID != StringInternPool::NOT_A_STRING_ID)
			ev->labelsStringIds.push_back(value.stringValueContainer.labelStringID);
		break;
	case ENT_ASSOC:
		ev->ConstructMappedChildNodes();	//construct an empty mappedChildNodes to swap out
		ev->mappedChildNodes = std::move(value.mappedChildNodes);
		value.DestructMappedChildNodes();
		break;
	//otherwise it's uninitialized, so treat as ordered
	default: //all other opcodes
		ev->ConstructOrderedChildNodes();	//construct an empty orderedChildNodes to swap out
		ev->orderedChildNodes = std::move(value.orderedChildNodes);
		value.DestructOrderedChildNodes();
		break;
	}

	SetExtendedValue(true);
	value.extension.extendedValue = ev;
	value.extension.commentsStringId = StringInternPool::NOT_A_STRING_ID;
}

void EvaluableNode::DestructValue()
{
	if(!HasExtendedValue())
	{
		switch(GetType())
		{
		case ENT_BOOL:
			AnnotationsAndComments::Destruct(value.boolValueContainer.annotationsAndComments);
			break;
		case ENT_NUMBER:
			AnnotationsAndComments::Destruct(value.numberValueContainer.annotationsAndComments);
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReference(value.stringValueContainer.stringID);
			AnnotationsAndComments::Destruct(value.stringValueContainer.annotationsAndComments);
			break;
		case ENT_ASSOC:
			value.DestructMappedChildNodes();
			break;
		//otherwise it's uninitialized, so treat as ordered
		default:
			value.DestructOrderedChildNodes();
			break;
		}
	}
	else
	{
		//TODO 24298: can this be streamlined and/or merged with above above?
		switch(GetType())
		{
		case ENT_BOOL:
		case ENT_NUMBER:
			//don't need to do anything
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReference(value.extension.extendedValue->stringValueContainer.stringID);
			break;
		case ENT_ASSOC:
			value.extension.extendedValue->DestructMappedChildNodes();
			break;
		//otherwise it's uninitialized, so treat as ordered
		default:
			value.extension.extendedValue->DestructOrderedChildNodes();
			break;
		}
	}
}

void EvaluableNode::Invalidate()
{
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	assert(!IsNodeDeallocated());
#endif

	if(!HasExtendedValue())
	{
		switch(GetType())
		{
		case ENT_BOOL:
			AnnotationsAndComments::Destruct(value.boolValueContainer.annotationsAndComments);
			break;
		case ENT_NUMBER:
			AnnotationsAndComments::Destruct(value.numberValueContainer.annotationsAndComments);
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReference(value.stringValueContainer.stringID);
			AnnotationsAndComments::Destruct(value.stringValueContainer.annotationsAndComments);
			break;
		case ENT_ASSOC:
			value.DestructMappedChildNodes();
			break;
		//otherwise it's uninitialized, so treat as ordered
		default:
			value.DestructOrderedChildNodes();
			break;
		}

		//return early if no extended value, make sure to clear out data so it isn't double-deleted
		type = ENT_DEALLOCATED;
		attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		//use a value that is more apparent that something went wrong
		value.numberValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
	#else
		value.numberValueContainer.numberValue = 0;
	#endif

		AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
		return;
	}

	//has extended type
	switch(GetType())
	{
	case ENT_BOOL:
	case ENT_NUMBER:
		//don't need to do anything
		break;
	case ENT_STRING:
	case ENT_SYMBOL:
		string_intern_pool.DestroyStringReference(value.extension.extendedValue->stringValueContainer.stringID);
		break;
	case ENT_ASSOC:
		value.extension.extendedValue->DestructMappedChildNodes();
		break;
	//otherwise it's uninitialized, so treat as ordered
	default:
		value.extension.extendedValue->DestructOrderedChildNodes();
		break;
	}

	//delete extended if haven't returned yet
	string_intern_pool.DestroyStringReferences(value.extension.extendedValue->labelsStringIds);
	string_intern_pool.DestroyStringReference(value.extension.commentsStringId);

	delete value.extension.extendedValue;

	type = ENT_DEALLOCATED;
	attributes = static_cast<AttributeStorageType>(Attribute::NONE);
	
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	//use a value that is more apparent that something went wrong
	value.numberValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
#else
	value.numberValueContainer.numberValue = 0;
#endif

	AnnotationsAndComments::Construct(value.numberValueContainer.annotationsAndComments);
}

bool EvaluableNode::AreDeepEqualGivenShallowEqual(EvaluableNode *a, EvaluableNode *b, ReferenceAssocType *checked)
{
	//if either is a null and have same number of child nodes, then equal
	if(a == nullptr || b == nullptr)
		return true;

	if(checked != nullptr)
	{
		//try to record this as a new pair that is checked
		auto [inserted_entry, inserted] = checked->emplace(a, b);

		//if the entry for a already exists
		if(!inserted)
		{
			//if it doesn't match, then there's an odd cycle and the graph structures don't match
			if(inserted_entry->second != b)
				return false;

			//already validated that these were equal
			return true;
		}
	}

	//immediate values have no child nodes, so since shallow equal, they're equal
	if(a->IsImmediate())
		return true;

	if(a->IsAssociativeArray())
	{
		//if a is associative, b must be too, since they're shallow equal
		auto &a_mcn = a->GetMappedChildNodesReference();
		auto &b_mcn = b->GetMappedChildNodesReference();
		size_t a_size = a_mcn.size();
		if(a_size != b_mcn.size())
			return false;

		//both empty, so equal
		if(a_size == 0)
			return true;

		for(auto &[s_id, s] : a_mcn)
		{
			//make sure it can be found
			auto b_found = b_mcn.find(s_id);
			if(b_found == end(b_mcn))
				return false;

			EvaluableNode *a_child = s;
			EvaluableNode *b_child = b_found->second;

			//if pointers are the same, then they are the same
			if(a_child == b_child)
				continue;

			//first check if the immediate values are equal
			if(!AreShallowEqual(a_child, b_child))
				return false;

			//now check deep values
			if(!EvaluableNode::AreDeepEqualGivenShallowEqual(a_child, b_child, checked))
				return false;
		}

		//all child nodes are equal
		return true;
	}

	//if made it here, then both types are ordered
	auto &a_ocn = a->GetOrderedChildNodesReference();
	auto &b_ocn = b->GetOrderedChildNodesReference();
	size_t a_size = a_ocn.size();
	if(a_size != b_ocn.size())
		return false;

	//both empty, so equal
	if(a_size == 0)
		return true;

	size_t index = 0;
	for(; index < a_size; index++)
	{
		EvaluableNode *a_child = a_ocn[index];
		EvaluableNode *b_child = b_ocn[index];

		//if pointers are the same, then they are the same
		if(a_child == b_child)
			continue;

		//first check if the immediate values are equal
		if(!AreShallowEqual(a_child, b_child))
			break;

		//now check deep values
		if(!EvaluableNode::AreDeepEqualGivenShallowEqual(a_child, b_child, checked))
			break;
	}

	//all are equal
	if(index == a_size)
		return true;

	if(a->GetType() != ENT_UNORDERED_LIST)
		return false;

	//if it's small with immediate types, then do a quick O(n^2) match,
	//otherwise do an expensive hash-based O(n) match
	bool use_immediate_method = false;
	if(a_size - index < 4)
	{
		use_immediate_method = true;
		for(size_t i = index; i < a_size; i++)
		{
			if((a_ocn[i] != nullptr && !a_ocn[i]->IsImmediate())
				|| (b_ocn[i] != nullptr && !b_ocn[i]->IsImmediate()))
			{
				use_immediate_method = false;
				break;
			}
		}
	}

	if(use_immediate_method)
	{
		std::vector<EvaluableNode *> b_unmatched;
		b_unmatched.reserve(a_size - index);
		for(size_t i = index; i < a_size; i++)
			b_unmatched.push_back(b_ocn[i]);

		//find a match for each remaining node
		for(; index < a_size; index++)
		{
			EvaluableNode *a_child = a_ocn[index];

			//look for a match among b's remaining unmatched
			bool found = false;
			for(size_t b_unmatched_index = 0; b_unmatched_index < b_unmatched.size(); b_unmatched_index++)
			{
				EvaluableNode *b_child = b_unmatched[b_unmatched_index];

				//if pointers are the same, then they are the same
				if(a_child == b_child)
				{
					found = true;
					b_unmatched.erase(begin(b_unmatched) + b_unmatched_index);
					break;
				}

				//because all nodes are immediate, just need shallow check
				if(AreShallowEqual(a_child, b_child))
				{
					found = true;
					b_unmatched.erase(begin(b_unmatched) + b_unmatched_index);
					break;
				}
			}

			//a's node of index did not have a match in b, so not equal
			if(!found)
				return false;
		}

		//all child nodes are equal
		return true;
	}
	else //compare hashed unparse strings
	{
		FastHashMap<std::string, size_t> unmatched_a_children;
		unmatched_a_children.reserve(a_size - index);
		for(size_t i = index; i < a_size; i++)
		{
			std::string a_unparsed = Parser::Unparse(a_ocn[i], false, false, true);
			auto entry = unmatched_a_children.emplace(std::move(a_unparsed), 1);
			//if already exists, increment
			if(!entry.second)
				entry.first->second++;
		}

		//for each of b's nodes, remove 
		for(size_t i = index; i < a_size; i++)
		{
			std::string b_unparsed = Parser::Unparse(b_ocn[i], false, false, true);
			auto found = unmatched_a_children.find(b_unparsed);
			if(found == end(unmatched_a_children))
				return false;

			if(found->second > 1)
				found->second--;
			else
				unmatched_a_children.erase(found);
		}

		//all had a match
		return true;
	}
}

bool EvaluableNode::CanNodeTreeBeFlattenedRecurse(EvaluableNode *n, std::vector<EvaluableNode *> &stack)
{
	//do a linear find because the logarithmic size of depth should be small enough to make this faster
	// than a ReferenceSetType
	if(std::find(begin(stack), end(stack), n) != end(stack))
		return false;

	stack.push_back(n);

	//check child nodes
	if(n->IsAssociativeArray())
	{
		for(auto &[_, e] : n->GetMappedChildNodesReference())
		{
			if(e == nullptr)
				continue;

			if(!CanNodeTreeBeFlattenedRecurse(e, stack))
				return false;
		}
	}
	else if(!n->IsImmediate())
	{
		for(auto &e : n->GetOrderedChildNodesReference())
		{
			if(e == nullptr)
				continue;

			if(!CanNodeTreeBeFlattenedRecurse(e, stack))
				return false;
		}
	}

	stack.pop_back();

	//didn't find itself
	return true;
}

size_t EvaluableNode::GetDeepSizeRecurse(EvaluableNode *n, ReferenceSetType &checked)
{
	//try to insert. if fails, then it has already been inserted, so ignore
	if(checked.insert(n).second == false)
		return 0;

	//count this one
	size_t size = 1;

	//check child nodes
	if(n->IsAssociativeArray())
	{
		for(auto &[_, e] : n->GetMappedChildNodesReference())
		{
			if(e != nullptr)
				size += GetDeepSizeRecurse(e, checked);
		}
	}
	else if(!n->IsImmediate())
	{
		for(auto &e : n->GetOrderedChildNodesReference())
		{
			if(e != nullptr)
				size += GetDeepSizeRecurse(e, checked);
		}
	}

	return size;
}

size_t EvaluableNode::GetDeepSizeNoCycleRecurse(EvaluableNode *n)
{
	//count this one
	size_t size = 1;

	//check child nodes
	if(n->IsAssociativeArray())
	{
		for(auto &[_, e] : n->GetMappedChildNodesReference())
		{
			if(e != nullptr)
				size += GetDeepSizeNoCycleRecurse(e);
		}
	}
	else if(!n->IsImmediate())
	{
		for(auto &e : n->GetOrderedChildNodesReference())
		{
			if(e != nullptr)
				size += GetDeepSizeNoCycleRecurse(e);
		}
	}

	return size;
}

void EvaluableNodeImmediateValueWithType::CopyValueFromEvaluableNode(EvaluableNode *en, EvaluableNodeManager *enm)
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

	if(en_type == ENT_BOOL)
	{
		nodeType = ENIVT_BOOL;
		nodeValue = EvaluableNodeImmediateValue(en->GetBoolValueReference());
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
		
		//create copy
		if(enm != nullptr)
			string_intern_pool.CreateStringReference(nodeValue.stringID);

		return;
	}

	nodeType = ENIVT_CODE;
	if(enm == nullptr)
		nodeValue = EvaluableNodeImmediateValue(en);
	else
		nodeValue.code = enm->DeepAllocCopy(en, false);
}

bool EvaluableNodeImmediateValueWithType::GetValueAsBoolean(bool value_if_null)
{
	if(nodeType == ENIVT_BOOL)
		return nodeValue.boolValue;

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
		return EvaluableNode::ToBool(nodeValue.code);

	//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
	return value_if_null;
}

double EvaluableNodeImmediateValueWithType::GetValueAsNumber(double value_if_null)
{
	if(nodeType == ENIVT_NUMBER)
		return nodeValue.number;

	if(nodeType == ENIVT_BOOL)
		return (nodeValue.boolValue ? 1.0 : 0.0);

	if(nodeType == ENIVT_STRING_ID)
	{
		if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID)
			return value_if_null;

		auto &str = string_intern_pool.GetStringFromID(nodeValue.stringID);
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

std::pair<bool, std::string> EvaluableNodeImmediateValueWithType::GetValueAsString(bool key_string)
{
	if(nodeType == ENIVT_STRING_ID)
	{
		if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID)
			return std::make_pair(false, "");

		auto &str = string_intern_pool.GetStringFromID(nodeValue.stringID);
		return std::make_pair(true, str);
	}

	if(nodeType == ENIVT_BOOL)
		return std::make_pair(true, EvaluableNode::BoolToString(nodeValue.boolValue, key_string));

	if(nodeType == ENIVT_NUMBER)
		return std::make_pair(true, EvaluableNode::NumberToString(nodeValue.number, key_string));

	if(nodeType == ENIVT_CODE && !EvaluableNode::IsNull(nodeValue.code))
	{
		if(nodeValue.code != nullptr && nodeValue.code->GetType() == ENT_STRING)
			return std::make_pair(true, nodeValue.code->GetStringValue());

		if(key_string)
			return std::make_pair(true, Parser::UnparseToKeyString(nodeValue.code));
		else
			return std::make_pair(true, Parser::Unparse(nodeValue.code, false, false, true));
	}

	//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
	return std::make_pair(false, "");
}

StringInternPool::StringID EvaluableNodeImmediateValueWithType::GetValueAsStringIDIfExists(bool key_string)
{
	if(nodeType == ENIVT_STRING_ID)
		return nodeValue.stringID;

	if(nodeType == ENIVT_BOOL)
		return EvaluableNode::BoolToStringID(nodeValue.boolValue, key_string);

	if(nodeType == ENIVT_CODE && nodeValue.code != nullptr && nodeValue.code->GetType() == ENT_STRING)
		return nodeValue.code->GetStringIDReference();

	auto [valid, str_value] = GetValueAsString(key_string);
	if(!valid)
		return string_intern_pool.NOT_A_STRING_ID;

	return string_intern_pool.GetIDFromString(str_value);
}

StringInternPool::StringID EvaluableNodeImmediateValueWithType::GetValueAsStringIDWithReference(bool key_string)
{
	if(nodeType == ENIVT_STRING_ID)
		return string_intern_pool.CreateStringReference(nodeValue.stringID);

	if(nodeType == ENIVT_BOOL)
		return string_intern_pool.CreateStringReference(EvaluableNode::BoolToStringID(nodeValue.boolValue, key_string));

	if(nodeType == ENIVT_CODE && nodeValue.code != nullptr && nodeValue.code->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(nodeValue.code->GetStringIDReference());

	auto [valid, str_value] = GetValueAsString(key_string);
	if(!valid)
		return string_intern_pool.NOT_A_STRING_ID;

	return string_intern_pool.CreateStringReference(str_value);
}
