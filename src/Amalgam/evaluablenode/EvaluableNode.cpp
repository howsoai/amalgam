//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeManagement.h"
#include "FastMath.h"
#include "Parser.h"
#include "StringInternPool.h"

//system headers:
#include <algorithm>
#include <cctype>
#include <iomanip>

double EvaluableNode::zeroNumberValue = 0.0;
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

std::pair<size_t, size_t> EvaluableNode::GetNodeCommonAndUniqueLabelCounts(EvaluableNode *n1, EvaluableNode *n2)
{
	if(n1 == nullptr)
	{
		if(n2 == nullptr)
			return std::make_pair(0, 0);

		return std::make_pair(0, n2->GetNumLabels());
	}

	if(n2 == nullptr)
		return std::make_pair(0, n1->GetNumLabels());

	size_t num_n1_labels = n1->GetNumLabels();
	size_t num_n2_labels = n2->GetNumLabels();

	//if no labels in one, just return the nonzero count as the total unique
	if(num_n1_labels == 0 || num_n2_labels == 0)
		return std::make_pair(0, std::max(num_n1_labels, num_n2_labels));

	//if only have one label in each, compare immediately for speed
	if(num_n1_labels == 1 && num_n2_labels == 1)
	{
		//if the same, only one common label, if unique, then two unique
		if(n1->GetLabel(0) == n2->GetLabel(0))
			return std::make_pair(1, 0);
		else
			return std::make_pair(0, 2);
	}

	//compare
	size_t num_common_labels = 0;
	for(auto s_id : n1->GetLabelsStringIds())
	{
		auto n2_label_sids = n2->GetLabelsStringIds();
		if(std::find(begin(n2_label_sids), end(n2_label_sids), s_id) != end(n2_label_sids))
			num_common_labels++;
	}

	//don't count the common labels in the uncommon
	return std::make_pair(num_common_labels, num_n1_labels + num_n2_labels - 2 * num_common_labels);
}

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
	{
		double av = EvaluableNode::ToNumber(a);
		double bv = EvaluableNode::ToNumber(b);
		return (av == bv);
	}

	//if made it here, then it's an instruction, and they're of equal type
	return true;
}

bool EvaluableNode::IsTrue(EvaluableNode *n)
{
	if(n == nullptr)
		return false;

	EvaluableNodeType node_type = n->GetType();
	if(node_type == ENT_TRUE)
		return true;
	if(node_type == ENT_FALSE)
		return false;
	if(node_type == ENT_NULL)
		return false;

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

	std::string a_str = EvaluableNode::ToKeyString(a);
	std::string b_str = EvaluableNode::ToKeyString(b);
	return StringManipulation::StringNaturalCompare(a_str, b_str);
}

double EvaluableNode::ToNumber(EvaluableNode *e, double value_if_null)
{
	if(e == nullptr)
		return value_if_null;

	switch(e->GetType())
	{
		case ENT_TRUE:
			return 1;
		case ENT_FALSE:
			return 0;
		case ENT_NULL:
			return value_if_null;
		case ENT_NUMBER:
			return e->GetNumberValueReference();
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

std::string EvaluableNode::ToKeyString(EvaluableNode *e)
{
	return Parser::UnparseToKeyString(e);
}

StringInternPool::StringID EvaluableNode::ToStringIDIfExists(EvaluableNode *e)
{
	if(EvaluableNode::IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING)
		return e->GetStringIDReference();

	std::string str_value = Parser::UnparseToKeyString(e);
	//will return empty string if not found
	return string_intern_pool.GetIDFromString(str_value);
}

StringInternPool::StringID EvaluableNode::ToStringIDWithReference(EvaluableNode *e)
{
	if(EvaluableNode::IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING)
		return string_intern_pool.CreateStringReference(e->GetStringIDReference());

	std::string str_value = Parser::UnparseToKeyString(e);
	return string_intern_pool.CreateStringReference(str_value);
}

StringInternPool::StringID EvaluableNode::ToStringIDTakingReferenceAndClearing(EvaluableNode *e)
{
	//null doesn't need a reference
	if(IsNull(e))
		return StringInternPool::NOT_A_STRING_ID;

	if(e->GetType() == ENT_STRING)
	{
		//clear the reference and return it
		StringInternPool::StringID &sid_reference = e->GetStringIDReference();
		StringInternPool::StringID sid_to_return = string_intern_pool.NOT_A_STRING_ID;
		std::swap(sid_reference, sid_to_return);
		return sid_to_return;
	}

	std::string str_value = Parser::UnparseToKeyString(e);
	return string_intern_pool.CreateStringReference(str_value);
}

void EvaluableNode::ConvertOrderedListToNumberedAssoc()
{
	//don't do anything if no child nodes
	if(!DoesEvaluableNodeTypeUseOrderedData(GetType()))
	{
		InitMappedChildNodes();
		type = ENT_ASSOC;
		return;
	}

	AssocType new_map;

	//convert ordered child nodes into index number -> value
	auto &ocn = GetOrderedChildNodes();
	new_map.reserve(ocn.size());
	for(size_t i = 0; i < ocn.size(); i++)
		new_map[string_intern_pool.CreateStringReference(NumberToString(i))] = ocn[i];

	InitMappedChildNodes();
	type = ENT_ASSOC;

	//swap for efficiency
	std::swap(GetMappedChildNodesReference(), new_map);
}

size_t EvaluableNode::GetEstimatedNodeSizeInBytes(EvaluableNode *n)
{
	if(n == nullptr)
		return 0;

	size_t total_size = 0;
	total_size += sizeof(EvaluableNode);
	if(n->HasExtendedValue())
		total_size += sizeof(EvaluableNode::EvaluableNodeExtendedValue);
	total_size += n->GetNumLabels() * sizeof(StringInternPool::StringID);

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

		return (sid->string.size() < max_size);
	}
	else //ordered
	{
		auto &ocn = GetOrderedChildNodesReference();
		return (ocn.size() < max_size);
	}

	//shouldn't make it here
	return false;
}

void EvaluableNode::InitializeType(EvaluableNode *n, bool copy_labels, bool copy_comments_and_concurrency)
{
	attributes.allAttributes = 0;
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
		string_intern_pool.CreateStringReferences(value.mappedChildNodes, [](auto n) { return n.first; });

		//update idempotency
		SetIsIdempotent(true);
		for(auto &[_, cn] : value.mappedChildNodes)
		{
			if(cn != nullptr && !cn->GetIsIdempotent())
			{
				SetIsIdempotent(false);
				break;
			}
		}
	}
	else if(DoesEvaluableNodeTypeUseNumberData(type))
	{
		value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
		value.numberValueContainer.numberValue = n->GetNumberValueReference();
		SetIsIdempotent(true);
	}
	else if(DoesEvaluableNodeTypeUseStringData(type))
	{
		value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(n->GetStringIDReference());
		value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
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

	if(copy_comments_and_concurrency)
		SetConcurrency(n->GetConcurrency());

	if(copy_labels || copy_comments_and_concurrency)
	{
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
	else if(DoesEvaluableNodeTypeUseNumberData(cur_type))
		GetNumberValueReference() = n->GetNumberValueReference();
	else if(DoesEvaluableNodeTypeUseStringData(cur_type))
		SetStringID(n->GetStringIDReference());
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

	if(    (DoesEvaluableNodeTypeUseNumberData(cur_type) && DoesEvaluableNodeTypeUseNumberData(new_type))
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
	if(DoesEvaluableNodeTypeUseNumberData(new_type))
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
				auto sid = ToStringIDWithReference(ocn[i]);

				EvaluableNode *value = nullptr;
				if(i + 1 < ocn.size())
					value = ocn[i + 1];

				//try to insert, but drop reference if couldn't
				if(!new_map.insert(std::make_pair(sid, value)).second)
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
				//keep the reference from when it was an assoc
				new_ordered.push_back(enm->AllocNodeWithReferenceHandoff(ENT_STRING, cn_id));
				new_ordered.push_back(cn);
			}

			//clear the mapped nodes here, because don't want to free the references
			// as they were handed off to the newly allocated ordered child nodes
			mcn.clear();
			InitOrderedChildNodes();
			//swap for efficiency
			swap(GetOrderedChildNodesReference(), new_ordered);
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

void EvaluableNode::InitNumberValue()
{
	DestructValue();

	if(HasExtendedValue())
	{
		value.extension.extendedValue->value.numberValueContainer.numberValue = 0.0;
	}
	else
	{
		value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
		value.numberValueContainer.numberValue = 0.0;
	}
}

void EvaluableNode::InitStringValue()
{
	DestructValue();

	if(HasExtendedValue())
	{
		value.extension.extendedValue->value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
	}
	else
	{
		value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
		value.stringValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
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
				StringInternPool::StringID cur_id = value.extension.extendedValue->value.stringValueContainer.stringID;
				if(id != cur_id)
				{
					string_intern_pool.DestroyStringReference(cur_id);
					value.extension.extendedValue->value.stringValueContainer.stringID = string_intern_pool.CreateStringReference(id);
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
			return string_intern_pool.GetStringFromID(value.extension.extendedValue->value.stringValueContainer.stringID);
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
			string_intern_pool.DestroyStringReference(value.extension.extendedValue->value.stringValueContainer.stringID);
			value.extension.extendedValue->value.stringValueContainer.stringID = new_id;
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
			sid = value.extension.extendedValue->value.stringValueContainer.stringID;
			value.extension.extendedValue->value.stringValueContainer.stringID = StringInternPool::NOT_A_STRING_ID;
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
				StringInternPool::StringID cur_id = value.extension.extendedValue->value.stringValueContainer.stringID;
				string_intern_pool.DestroyStringReference(cur_id);
				value.extension.extendedValue->value.stringValueContainer.stringID = id;
			}
		}
	}
}

std::vector<StringInternPool::StringID> EvaluableNode::GetLabelsStringIds()
{
	if(!HasExtendedValue())
	{
		if(HasCompactSingleLabelStorage())
		{
			if(GetCompactSingleLabelStorage() == StringInternPool::NOT_A_STRING_ID)
				return emptyStringIdVector;

			std::vector<StringInternPool::StringID> label_vec;
			label_vec.push_back(GetCompactSingleLabelStorage());
			return label_vec;
		}

		return emptyStringIdVector;
	}

	return value.extension.extendedValue->labelsStringIds;
}

std::vector<std::string> EvaluableNode::GetLabelsStrings()
{
	if(!HasExtendedValue())
	{
		if(HasCompactSingleLabelStorage())
		{
			if(GetCompactSingleLabelStorage() == StringInternPool::NOT_A_STRING_ID)
				return emptyStringVector;

			std::vector<std::string> label_vec;
			label_vec.push_back(GetLabel(0));
			return label_vec;
		}

		return emptyStringVector;
	}

	auto &sids = value.extension.extendedValue->labelsStringIds;
	std::vector<std::string> label_vec(sids.size());
	for(size_t i = 0; i < sids.size(); i++)
		label_vec[i] = string_intern_pool.GetStringFromID(sids[i]);

	return label_vec;
}

void EvaluableNode::SetLabelsStringIds(const std::vector<StringInternPool::StringID> &label_string_ids)
{
	if(label_string_ids.size() == 0)
	{
		ClearLabels();
		return;
	}

	//can no longer be idempotent because it could be altered by something collecting labels
	attributes.individualAttribs.isIdempotent = false;

	if(!HasExtendedValue())
	{
		if(label_string_ids.size() == 1 && HasCompactSingleLabelStorage())
		{
			StringInternPool::StringID cur_id = GetCompactSingleLabelStorage();
			if(label_string_ids[0] != cur_id)
			{
				string_intern_pool.DestroyStringReference(GetCompactSingleLabelStorage());
				GetCompactSingleLabelStorage() = string_intern_pool.CreateStringReference(label_string_ids[0]);
			}
			return;
		}

		//doesn't have enough storage, so extend and set below
		EnsureEvaluableNodeExtended();
	}

	//create new references before destroying old (so don't need to recreate strings if they are freed and then released
	string_intern_pool.CreateStringReferences(label_string_ids);

	//clear references to anything existing
	string_intern_pool.DestroyStringReferences(value.extension.extendedValue->labelsStringIds);

	value.extension.extendedValue->labelsStringIds = label_string_ids;
}

size_t EvaluableNode::GetNumLabels()
{
	if(!HasExtendedValue())
	{
		if(HasCompactSingleLabelStorage() && GetCompactSingleLabelStorage() != StringInternPool::NOT_A_STRING_ID)
			return 1;

		return 0;
	}

	auto &sids = value.extension.extendedValue->labelsStringIds;
	return sids.size();
}

std::string EvaluableNode::GetLabel(size_t label_index)
{
	if(!HasExtendedValue())
	{
		if(HasCompactSingleLabelStorage())
		{
			if(label_index != 0)
				return StringInternPool::EMPTY_STRING;

			return string_intern_pool.GetStringFromID(GetCompactSingleLabelStorage());
		}

		return StringInternPool::EMPTY_STRING;
	}

	auto &sids = value.extension.extendedValue->labelsStringIds;
	if(label_index >= sids.size())
		return StringInternPool::EMPTY_STRING;
	else
		return string_intern_pool.GetStringFromID(sids[label_index]);
}

const StringInternPool::StringID EvaluableNode::GetLabelStringId(size_t label_index)
{
	if(!HasExtendedValue())
	{
		if(HasCompactSingleLabelStorage())
		{
			if(label_index != 0)
				return StringInternPool::NOT_A_STRING_ID;

			return GetCompactSingleLabelStorage();
		}

		return StringInternPool::NOT_A_STRING_ID;
	}

	auto &sids = value.extension.extendedValue->labelsStringIds;
	if(label_index >= sids.size())
		return StringInternPool::NOT_A_STRING_ID;
	else
		return sids[label_index];
}

void EvaluableNode::RemoveLabel(size_t label_index)
{
	if(HasCompactSingleLabelStorage())
	{
		if(label_index == 0)
		{
			string_intern_pool.DestroyStringReference(GetCompactSingleLabelStorage());
			GetCompactSingleLabelStorage() = StringInternPool::NOT_A_STRING_ID;
		}

		return;
	}

	if(!HasExtendedValue())
		return;

	if(label_index >= value.extension.extendedValue->labelsStringIds.size())
		return;

	string_intern_pool.DestroyStringReference(value.extension.extendedValue->labelsStringIds[label_index]);
	value.extension.extendedValue->labelsStringIds.erase(begin(value.extension.extendedValue->labelsStringIds) + label_index);
}

void EvaluableNode::ClearLabels()
{
	if(HasCompactSingleLabelStorage())
	{
		string_intern_pool.DestroyStringReference(GetCompactSingleLabelStorage());
		GetCompactSingleLabelStorage() = StringInternPool::NOT_A_STRING_ID;
		return;
	}

	if(!HasExtendedValue())
		return;

	string_intern_pool.DestroyStringReferences(value.extension.extendedValue->labelsStringIds);
	value.extension.extendedValue->labelsStringIds.clear();
}

void EvaluableNode::ReserveLabels(size_t num_labels)
{
	if(num_labels == 0)
		return;

	//see if compact storage is good enough
	if(HasCompactSingleLabelStorage() && num_labels <= 1)
		return;

	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	value.extension.extendedValue->labelsStringIds.reserve(num_labels);
}

void EvaluableNode::AppendLabelStringId(StringInternPool::StringID label_string_id, bool handoff_reference)
{
	//can no longer be idempotent because it could be altered by something collecting labels
	attributes.individualAttribs.isIdempotent = false;

	if(!handoff_reference)
		string_intern_pool.CreateStringReference(label_string_id);

	if(HasCompactSingleLabelStorage() && GetCompactSingleLabelStorage() == StringInternPool::NOT_A_STRING_ID)
	{
		GetCompactSingleLabelStorage() = label_string_id;
		return;
	}

	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	value.extension.extendedValue->labelsStringIds.push_back(label_string_id);
}

void EvaluableNode::AppendLabel(const std::string &label)
{
	//can no longer be idempotent because it could be altered by something collecting labels
	attributes.individualAttribs.isIdempotent = false;

	if(HasCompactSingleLabelStorage() && GetCompactSingleLabelStorage() == StringInternPool::NOT_A_STRING_ID)
	{
		GetCompactSingleLabelStorage() = string_intern_pool.CreateStringReference(label);
		return;
	}

	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	value.extension.extendedValue->labelsStringIds.push_back(string_intern_pool.CreateStringReference(label));
}

StringInternPool::StringID EvaluableNode::GetCommentsStringId()
{
	if(!HasExtendedValue())
		return StringInternPool::NOT_A_STRING_ID;

	return value.extension.commentsStringId;
}

std::vector<std::string> EvaluableNode::GetCommentsSeparateLines()
{
	std::vector<std::string> comment_lines;

	StringInternPool::StringID comment_sid = GetCommentsStringId();
	if(comment_sid == string_intern_pool.NOT_A_STRING_ID || comment_sid == string_intern_pool.emptyStringId)
		return comment_lines;

	auto &full_comments = string_intern_pool.GetStringFromID(comment_sid);

	//early exit
	if(full_comments.empty())
		return comment_lines;

	size_t cur = 0;
	size_t prev = 0;
	while((cur = full_comments.find('\n', prev)) != std::string::npos)
	{
		//skip carriage return if found prior to the newline
		int carriage_return_offset = 0;
		if(prev < cur && full_comments[cur - 1] == '\r')
			carriage_return_offset = 1;

		comment_lines.emplace_back(full_comments.substr(prev, cur - prev - carriage_return_offset));
		prev = cur + 1;
	}

	//get whatever is left
	if(prev < full_comments.size())
		comment_lines.emplace_back(full_comments.substr(prev));

	return comment_lines;
}

void EvaluableNode::SetCommentsStringId(StringInternPool::StringID comments_string_id, bool handoff_reference)
{
	if(comments_string_id == StringInternPool::NOT_A_STRING_ID)
	{
		ClearComments();
		return;
	}

	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	if(!handoff_reference)
		string_intern_pool.CreateStringReference(comments_string_id);

	//clear references to anything existing
	string_intern_pool.DestroyStringReference(value.extension.commentsStringId);

	value.extension.commentsStringId = comments_string_id;
}

void EvaluableNode::SetComments(const std::string &comments)
{
	if(comments.empty())
	{
		ClearComments();
		return;
	}

	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	//create new references before destroying old (so don't need to recreate strings if they are freed and then released)
	StringInternPool::StringID new_reference = string_intern_pool.CreateStringReference(comments);

	//clear references to anything existing
	string_intern_pool.DestroyStringReference(value.extension.commentsStringId);

	value.extension.commentsStringId = new_reference;
}

void EvaluableNode::ClearComments()
{
	if(!HasExtendedValue())
		return;

	string_intern_pool.DestroyStringReference(value.extension.commentsStringId);

	value.extension.commentsStringId = StringInternPool::NOT_A_STRING_ID;
}

void EvaluableNode::AppendCommentsStringId(StringInternPool::StringID comments_string_id)
{
	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	if(GetCommentsStringId() == string_intern_pool.NOT_A_STRING_ID)
	{
		SetCommentsStringId(comments_string_id);
	}
	else //already has comments, so append more
	{
		std::string appended = GetCommentsString();
		appended.append(string_intern_pool.GetStringFromID(comments_string_id));

		SetComments(appended);
	}
}

void EvaluableNode::AppendComments(const std::string &comment)
{
	if(!HasExtendedValue())
		EnsureEvaluableNodeExtended();

	if(GetCommentsStringId() == string_intern_pool.NOT_A_STRING_ID)
	{
		SetComments(comment);
	}
	else //already has comments, so append more
	{
		std::string appended = GetCommentsString();
		appended.append(comment);

		SetComments(appended);
	}
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
		value.extension.extendedValue->value.ConstructOrderedChildNodes();
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

void EvaluableNode::ClearOrderedChildNodes()
{
	if(!IsOrderedArray())
		return;

	GetOrderedChildNodes().clear();

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
		value.extension.extendedValue->value.ConstructMappedChildNodes();
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

	auto [inserted_node, inserted] = mcn.insert(std::make_pair(sid, nullptr));

	//if the node was not inserted, then don't need the reference created
	if(!inserted)
		string_intern_pool.DestroyStringReference(sid);

	//return the location of the child pointer
	return &inserted_node->second;
}

EvaluableNode **EvaluableNode::GetOrCreateMappedChildNode(const StringInternPool::StringID sid)
{
	auto &mcn = GetMappedChildNodesReference();
	auto [inserted_node, inserted] = mcn.insert(std::make_pair(sid, nullptr));

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
	auto [inserted_node, inserted] = mcn.insert(std::make_pair(sid, node));
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

	auto [inserted_node, inserted] = mcn.insert(std::make_pair(sid, node));

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

	auto [inserted_node, inserted] = mcn.insert(std::make_pair(sid, node));

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
		auto [inserted_node, inserted] = mcn.insert(std::make_pair(n_id, n));

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

	EvaluableNodeExtendedValue *ev = new EvaluableNodeExtendedValue;

	switch(GetType())
	{
	case ENT_NUMBER:
		ev->value.numberValueContainer.numberValue = value.numberValueContainer.numberValue;
		if(value.numberValueContainer.labelStringID != StringInternPool::NOT_A_STRING_ID)
			ev->labelsStringIds.push_back(value.numberValueContainer.labelStringID);
		break;
	case ENT_STRING:
	case ENT_SYMBOL:
		ev->value.stringValueContainer.stringID = value.stringValueContainer.stringID;
		if(value.stringValueContainer.labelStringID != StringInternPool::NOT_A_STRING_ID)
			ev->labelsStringIds.push_back(value.stringValueContainer.labelStringID);
		break;
	case ENT_ASSOC:
		ev->value.ConstructMappedChildNodes();	//construct an empty mappedChildNodes to swap out
		ev->value.mappedChildNodes = std::move(value.mappedChildNodes);
		value.DestructMappedChildNodes();
		break;
	//otherwise it's uninitialized, so treat as ordered
	default: //all other opcodes
		ev->value.ConstructOrderedChildNodes();	//construct an empty orderedChildNodes to swap out
		ev->value.orderedChildNodes = std::move(value.orderedChildNodes);
		value.DestructOrderedChildNodes();
		break;
	}

	attributes.individualAttribs.hasExtendedValue = true;
	value.extension.extendedValue = ev;
	value.extension.commentsStringId = StringInternPool::NOT_A_STRING_ID;
}

void EvaluableNode::DestructValue()
{
	if(!HasExtendedValue())
	{
		switch(GetType())
		{
		case ENT_NUMBER:
			string_intern_pool.DestroyStringReference(value.numberValueContainer.labelStringID);
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReferences(value.stringValueContainer.stringID, value.stringValueContainer.labelStringID);
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
		switch(GetType())
		{
		case ENT_NUMBER:
			//don't need to do anything
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReference(value.extension.extendedValue->value.stringValueContainer.stringID);
			break;
		case ENT_ASSOC:
			value.extension.extendedValue->value.DestructMappedChildNodes();
			break;
		//otherwise it's uninitialized, so treat as ordered
		default:
			value.extension.extendedValue->value.DestructOrderedChildNodes();
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
		case ENT_NUMBER:
			string_intern_pool.DestroyStringReference(value.numberValueContainer.labelStringID);
			break;
		case ENT_STRING:
		case ENT_SYMBOL:
			string_intern_pool.DestroyStringReferences(value.stringValueContainer.stringID, value.stringValueContainer.labelStringID);
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
		attributes.allAttributes = 0;
	#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
		//use a value that is more apparent that something went wrong
		value.numberValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
	#else
		value.numberValueContainer.numberValue = 0;
	#endif

		value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
		return;
	}

	//has extended type
	switch(GetType())
	{
	case ENT_NUMBER:
		//don't need to do anything
		break;
	case ENT_STRING:
	case ENT_SYMBOL:
		string_intern_pool.DestroyStringReference(value.extension.extendedValue->value.stringValueContainer.stringID);
		break;
	case ENT_ASSOC:
		value.extension.extendedValue->value.DestructMappedChildNodes();
		break;
	//otherwise it's uninitialized, so treat as ordered
	default:
		value.extension.extendedValue->value.DestructOrderedChildNodes();
		break;
	}

	//delete extended if haven't returned yet
	string_intern_pool.DestroyStringReferences(value.extension.extendedValue->labelsStringIds);
	string_intern_pool.DestroyStringReference(value.extension.commentsStringId);

	delete value.extension.extendedValue;

	type = ENT_DEALLOCATED;
	attributes.allAttributes = 0;
	
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	//use a value that is more apparent that something went wrong
	value.numberValueContainer.numberValue = std::numeric_limits<double>::quiet_NaN();
#else
	value.numberValueContainer.numberValue = 0;
#endif

	value.numberValueContainer.labelStringID = StringInternPool::NOT_A_STRING_ID;
}

bool EvaluableNode::AreDeepEqualGivenShallowEqual(EvaluableNode *a, EvaluableNode *b, ReferenceAssocType *checked)
{
	//if either is a null and have same number of child nodes, then equal
	if(a == nullptr || b == nullptr)
		return true;

	if(checked != nullptr)
	{
		//try to record this as a new pair that is checked
		auto [inserted_entry, inserted] = checked->insert(std::make_pair(a, b));

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

	for(size_t i = 0; i < a_ocn.size(); i++)
	{
		EvaluableNode *a_child = a_ocn[i];
		EvaluableNode *b_child = b_ocn[i];

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

	//count any labels
	size += n->GetNumLabels();

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

	//count any labels
	size += n->GetNumLabels();

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

void EvaluableNodeImmediateValueWithType::CopyValueFromEvaluableNode(EvaluableNode *en)
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

bool EvaluableNodeImmediateValueWithType::GetValueAsBoolean()
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

double EvaluableNodeImmediateValueWithType::GetValueAsNumber(double value_if_null)
{
	if(nodeType == ENIVT_NUMBER)
		return nodeValue.number;

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

std::pair<bool, std::string> EvaluableNodeImmediateValueWithType::GetValueAsString()
{
	if(nodeType == ENIVT_NUMBER)
		return std::make_pair(true, EvaluableNode::NumberToString(nodeValue.number));

	if(nodeType == ENIVT_STRING_ID)
	{
		if(nodeValue.stringID == string_intern_pool.NOT_A_STRING_ID)
			return std::make_pair(false, "");

		auto &str = string_intern_pool.GetStringFromID(nodeValue.stringID);
		return std::make_pair(true, str);
	}

	if(nodeType == ENIVT_CODE)
		return std::make_pair(true, Parser::Unparse(nodeValue.code));

	//nodeType is one of ENIVT_NOT_EXIST, ENIVT_NULL, ENIVT_NUMBER_INDIRECTION_INDEX
	return std::make_pair(false, "");
}

StringInternPool::StringID EvaluableNodeImmediateValueWithType::GetValueAsStringIDIfExists()
{
	if(nodeType == ENIVT_STRING_ID)
		return nodeValue.stringID;

	auto [valid, str_value] = GetValueAsString();
	if(!valid)
		return string_intern_pool.NOT_A_STRING_ID;

	return string_intern_pool.GetIDFromString(str_value);
}

StringInternPool::StringID EvaluableNodeImmediateValueWithType::GetValueAsStringIDWithReference()
{
	if(nodeType == ENIVT_STRING_ID)
		return string_intern_pool.CreateStringReference(nodeValue.stringID);

	auto [valid, str_value] = GetValueAsString();
	if(!valid)
		return string_intern_pool.NOT_A_STRING_ID;

	return string_intern_pool.CreateStringReference(str_value);
}