//project headers:
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "Interpreter.h"
#include "Merger.h"

//system headers:
#include <cmath>

EvaluableNodeTreeManipulation::NodesMixMethod::NodesMixMethod(RandomStream random_stream, EvaluableNodeManager *_enm,
	double fraction_a, double fraction_b, double similar_mix_chance) : NodesMergeMethod(_enm, true, false)
{
	randomStream = random_stream;

	//clamp each to the appropriate range, 0 to 1 for fractions, -1 to 1 for similarMixChance
	if(FastIsNaN(fraction_a))
		fractionA = 0.0;
	else
		fractionA = std::min(1.0, std::max(0.0, fraction_a));

	if(FastIsNaN(fraction_b))
		fractionB = 0.0;
	else
		fractionB = std::min(1.0, std::max(0.0, fraction_b));

	fractionAOrB = fractionA + fractionB - fractionA * fractionB;
	fractionAInsteadOfB = fractionA / (fractionA + fractionB);

	//similarMixChance can go from -1 to 1
	if(FastIsNaN(similar_mix_chance))
		similarMixChance = 0.0;
	else
		similarMixChance = std::min(1.0, std::max(-1.0, similar_mix_chance));
}

//returns a mix of a and b based on their fractions
inline double MixNumberValues(double a, double b, double fraction_a, double fraction_b)
{
	//quick exit for when they match
	if(EqualIncludingNaN(a, b))
		return a;

	//handle nans
	if(FastIsNaN(a))
	{
		if(fraction_a > 0)
			return std::numeric_limits<double>::quiet_NaN();
		else
			return b;
	}

	if(FastIsNaN(b))
	{
		if(fraction_b > 0)
			return std::numeric_limits<double>::quiet_NaN();
		else
			return a;
	}

	//normalize fractions
	fraction_a = fraction_a / (fraction_a + fraction_b);
	return a * fraction_a + b * (1 - fraction_a);
}

//returns a mix of a and b based on their fractions
inline StringInternPool::StringID MixStringValues(StringInternPool::StringID a, StringInternPool::StringID b,
	RandomStream random_stream, double fraction_a, double fraction_b)
{
	//quick exit for when they match
	if(a == b)
		return string_intern_pool.CreateStringReference(a);

	if(a == StringInternPool::NOT_A_STRING_ID)
		return string_intern_pool.CreateStringReference(b);

	if(b == StringInternPool::NOT_A_STRING_ID)
		return string_intern_pool.CreateStringReference(a);
	
	const auto &a_str = string_intern_pool.GetStringFromID(a);
	const auto &b_str = string_intern_pool.GetStringFromID(b);
	std::string result = EvaluableNodeTreeManipulation::MixStrings(a_str, b_str,
		random_stream, fraction_a, fraction_b);

	return string_intern_pool.CreateStringReference(result);
}

bool EvaluableNodeTreeManipulation::NodesMergeMethod::AreMergeable(EvaluableNode *a, EvaluableNode *b)
{
	size_t num_common_labels;
	size_t num_unique_labels;
	EvaluableNode::GetNodeCommonAndUniqueLabelCounts(a, b, num_common_labels, num_unique_labels);

	auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(a, b, true);

	return (commonality == 1.0 && num_unique_labels == 0);
}

EvaluableNode *EvaluableNodeTreeManipulation::NodesMixMethod::MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge)
{
	//early out
	if(a == nullptr && b == nullptr)
		return nullptr;

	if(AreMergeable(a, b) || must_merge)
	{
		EvaluableNode *merged = MergeTrees(this, a, b);

		//if the original and merged, check to see if mergeable of same type, and if so, interpolate
		if(merged != nullptr && a != nullptr && b != nullptr)
		{
			if(merged->IsNativelyNumeric() && a->IsNativelyNumeric() && b->IsNativelyNumeric())
			{
				double a_value = a->GetNumberValue();
				double b_value = b->GetNumberValue();
				double mixed_value = MixNumberValues(a_value, b_value, fractionA, fractionB);
				merged->SetNumberValue(mixed_value);
			}
			else if(merged->GetType() == ENT_STRING && a->GetType() == ENT_STRING && b->GetType() == ENT_STRING)
			{
				auto a_value = a->GetStringID();
				auto b_value = b->GetStringID();
				auto mixed_value = MixStringValues(a_value, b_value,
					randomStream.CreateOtherStreamViaRand(), fractionA, fractionB);
				merged->SetStringIDWithReferenceHandoff(mixed_value);
			}
		}

		return merged;
	}

	if(KeepNonMergeableAInsteadOfB())
		return MergeTrees(this, a, nullptr);
	else
		return MergeTrees(this, nullptr, b);
}

bool EvaluableNodeTreeManipulation::NodesMixMethod::AreMergeable(EvaluableNode *a, EvaluableNode *b)
{
	size_t num_common_labels;
	size_t num_unique_labels;
	EvaluableNode::GetNodeCommonAndUniqueLabelCounts(a, b, num_common_labels, num_unique_labels);

	auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(a, b);

	//if the immediate nodes are in fact a match, then just merge them
	if(commonality == 1.0 && num_unique_labels == 0)
		return true;

	//assess overall commonality between value commonality and label commonality
	double overall_commonality = (commonality + num_common_labels)
		/ (1 + num_common_labels + num_unique_labels);

	double prob_of_match = overall_commonality;
	if(commonality > 0)
	{
		if(similarMixChance > 0.0)
		{
			//probability of match is commonality OR similarMixChance
			// however, these are not mutually exclusive, so need to remove the conjunction of the
			// probability of both to prevent double-counting
			prob_of_match = overall_commonality + similarMixChance - overall_commonality * similarMixChance;
		}
		else if(similarMixChance < 0)
		{
			//probability of match is commonality AND not (negative similarMixChance)
			// because similarMixChance is negative, adding to 1 is the same as NOT
			prob_of_match = overall_commonality * (1.0 + similarMixChance);
		}
		//else 0.0 or NaN, just leave as overall_commonality
	}

	return randomStream.Rand() < prob_of_match;
}

MergeMetricResults<std::string *> EvaluableNodeTreeManipulation::StringSequenceMergeMetric::MergeMetric(std::string *a, std::string *b)
{
	if(a == b || (a != nullptr && b != nullptr && *a == *b))
		return MergeMetricResults(1.0, a, b);
	else
		return MergeMetricResults(0.0, a, b);
}

std::string *EvaluableNodeTreeManipulation::StringSequenceMergeMetric::MergeValues(std::string *a, std::string *b, bool must_merge)
{
	if(keepAllOfBoth)
	{
		if(a != nullptr)
			return a;
		return b;
	}

	//pick one, so select a
	return a;
}

EvaluableNodeTreeManipulation::StringsMixMethodUtf8::StringsMixMethodUtf8(RandomStream random_stream,
	double fraction_a, double fraction_b)
{
	randomStream = random_stream;

	//clamp each to the appropriate range of [0,1]
	if(FastIsNaN(fraction_a))
		fractionA = 0.0;
	else
		fractionA = std::min(1.0, std::max(0.0, fraction_a));

	if(FastIsNaN(fraction_b))
		fractionB = 0.0;
	else
		fractionB = std::min(1.0, std::max(0.0, fraction_b));

	fractionAOrB = fractionA + fractionB - fractionA * fractionB;
	fractionAInsteadOfB = fractionA / (fractionA + fractionB);
}

EvaluableNode *EvaluableNodeTreeManipulation::IntersectTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2)
{
	NodesMergeMethod mm(enm, false, true);
	return mm.MergeValues(tree1, tree2);
}

EvaluableNode *EvaluableNodeTreeManipulation::UnionTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2)
{
	NodesMergeMethod mm(enm, true, true);
	return mm.MergeValues(tree1, tree2);
}

EvaluableNode *EvaluableNodeTreeManipulation::MixTrees(RandomStream random_stream, EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2,
	double fraction_a, double fraction_b, double similar_mix_chance)
{
	NodesMixMethod mm(random_stream, enm, fraction_a, fraction_b, similar_mix_chance);
	return mm.MergeValues(tree1, tree2);
}

EvaluableNode *EvaluableNodeTreeManipulation::MixTreesByCommonLabels(Interpreter *interpreter, EvaluableNodeManager *enm,
	EvaluableNodeReference tree1, EvaluableNodeReference tree2, RandomStream &rs, double fraction_a, double fraction_b)
{
	//can't merge anything into an empty tree
	if(tree1 == nullptr)
		return nullptr;

	EvaluableNodeReference result_tree = enm->DeepAllocCopy(tree1);

	//if nothing to merge into the first tree, then just return unmodified copy
	if(tree2 == nullptr)
		return result_tree;

	auto index1 = RetrieveLabelIndexesFromTree(tree1.reference);
	auto index2 = RetrieveLabelIndexesFromTree(tree2.reference);

	//normalize fraction to be less than 1
	double total_fraction = fraction_a + fraction_b;
	if(total_fraction > 1.0)
	{
		fraction_a /= total_fraction;
		fraction_b /= total_fraction;
	}

	//get only labels that are in both trees

	//get list of labels from both
	CompactHashSet<StringInternPool::StringID> common_labels(index1.size() + index2.size());
	for(auto &[node_id, _] : index1)
		common_labels.insert(node_id);
	for(auto &[node_id, _] : index2)
		common_labels.insert(node_id);

	//get number of labels from each
	std::vector<StringInternPool::StringID> all_labels(begin(common_labels), end(common_labels));
	size_t num_from_2 = static_cast<size_t>(fraction_b * all_labels.size());
	size_t num_to_remove = static_cast<size_t>((1.0 - fraction_a - fraction_b) * all_labels.size());

	//remove labels from the first that are not used
	for(size_t i = 0; i < num_to_remove; i++)
	{
		//take a random string
		size_t index_to_remove = rs.RandSize(all_labels.size());
		StringInternPool::StringID label_id = all_labels[index_to_remove];
		all_labels.erase(begin(all_labels) + index_to_remove);

		//remove its label. Reuse enm for temporary since used it to create the new tree
		ReplaceLabelInTree(result_tree.reference, label_id, nullptr);
	}

	//replace labels from the second
	for(size_t i = 0; i < num_from_2; i++)
	{
		//take a random string
		size_t index_to_remove = rs.RandSize(all_labels.size());
		StringInternPool::StringID label_id = all_labels[index_to_remove];
		all_labels.erase(begin(all_labels) + index_to_remove);

		//replace with something from the other tree. Reuse enm for temporary since used it to create the new tree
		const auto replacement_index = index2.find(label_id);
		if(replacement_index != end(index2))
		{
			EvaluableNode *replacement = enm->DeepAllocCopy(replacement_index->second);
			ReplaceLabelInTree(result_tree.reference, label_id, replacement);
		}
	}

	return result_tree;
}

std::string EvaluableNodeTreeManipulation::MixStrings(const std::string &a, const std::string &b,
	RandomStream random_stream, double fraction_a, double fraction_b)
{
	StringManipulation::ExplodeUTF8Characters(a, aCharsBuffer);
	StringManipulation::ExplodeUTF8Characters(b, bCharsBuffer);

	StringsMixMethodUtf8 smm(random_stream, fraction_a, fraction_b);
	auto destCharsBuffer = smm.MergeSequences(aCharsBuffer, bCharsBuffer);

	std::string result = StringManipulation::ConcatUTF8Characters(destCharsBuffer);
	return result;
}

bool EvaluableNodeTreeManipulation::DoesTreeContainLabels(EvaluableNode *en)
{
	if(en == nullptr)
		return false;

	if(en->GetNumChildNodes() == 0)
		return (en->GetNumLabels() > 0);

	if(!en->GetNeedCycleCheck())
		return NonCycleDoesTreeContainLabels(en);

	EvaluableNode::ReferenceSetType checked;
	return DoesTreeContainLabels(en, checked);
}

std::pair<Entity::LabelsAssocType, bool> EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTreeAndNormalize(EvaluableNode *en)
{
	Entity::LabelsAssocType index;
	EvaluableNode::ReferenceSetType checked;

	//can check faster if don't need to check for cycles
	bool en_cycle_free = (en == nullptr || !en->GetNeedCycleCheck());
	bool label_collision = CollectLabelIndexesFromNormalTree(en, index, en_cycle_free ? nullptr : &checked);

	//if no collision, return
	if(!label_collision)
		return std::make_pair(index, false);

	//keep replacing until don't need to replace anymore
	EvaluableNode *to_replace = nullptr;
	while(true)
	{
		index.clear();
		checked.clear();
		bool replacement = CollectLabelIndexesFromTreeAndMakeLabelNormalizationPass(en, index, checked, to_replace);

		if(!replacement)
			break;
	}

	//things have been replaced, so anything might need to be updated
	EvaluableNodeManager::UpdateFlagsForNodeTree(en, checked);

	return std::make_pair(index, true);
}

void EvaluableNodeTreeManipulation::ReplaceLabelInTreeRecurse(EvaluableNode *&tree, StringInternPool::StringID label_id,
	EvaluableNode *replacement, EvaluableNode::ReferenceSetType &checked)
{
	//validate input
	if(tree == nullptr || label_id == StringInternPool::NOT_A_STRING_ID)
		return;

	//try to insert. if fails, then it has already been inserted, so ignore
	if(checked.insert(tree).second == false)
		return;

	size_t num_node_labels = tree->GetNumLabels();
	if(num_node_labels > 0)
	{
		//see if this node either has multiple labels or is a match; if so, need to replace it
		if(num_node_labels > 1 || tree->GetLabelStringId(0) == label_id)
		{
			//get the labels in case we'll need to merge them
			const auto &tree_node_label_sids = tree->GetLabelsStringIds();
			if(std::find(begin(tree_node_label_sids), end(tree_node_label_sids), label_id) != end(tree_node_label_sids))
			{
				EvaluableNode *result = replacement;
				if(result != nullptr)
				{
					//copy over relevant labels to the new node
					std::vector<StringInternPool::StringID> new_labels;
					if(replacement != nullptr)
						new_labels = replacement->GetLabelsStringIds();

					result->SetLabelsStringIds(UnionStringIDVectors(tree_node_label_sids, new_labels));
				}

				//don't free anything, because it could be referred to by other locations
				tree = result;
				return;
			}
		}
	}

	//update all ordered child nodes
	for(auto &cn : tree->GetOrderedChildNodes())
		ReplaceLabelInTreeRecurse(cn, label_id, replacement, checked);

	//update all mapped child nodes
	for(auto &[_, cn] : tree->GetMappedChildNodes())
		ReplaceLabelInTreeRecurse(cn, label_id, replacement, checked);
}

EvaluableNode *EvaluableNodeTreeManipulation::CreateGeneralizedNode(NodesMergeMethod *mm, EvaluableNode *n1, EvaluableNode *n2)
{
	if(n1 == nullptr && n2 == nullptr)
		return nullptr;

	EvaluableNodeManager *enm = mm->enm;

	//if want to keep all of both and only one exists, copy it
	if(mm->KeepSomeNonMergeableValues())
	{
		if(n1 != nullptr && n2 == nullptr)
			return enm->AllocNode(n1);
		else if(n1 == nullptr && n2 != nullptr)
			return enm->AllocNode(n2);
	}

	auto [node, commonality] = EvaluableNodeTreeManipulation::CommonalityBetweenNodeTypesAndValues(n1, n2);

	//if both are nullptr, nothing more to do
	if(node == nullptr)
		return nullptr;

	//see if need exact commonality
	if(mm->RequireExactMatches() && commonality != 1.0)
		return nullptr;

	//make a new copy of it
	auto common_type = node->GetType();
	EvaluableNode *n = enm->AllocNode(common_type);

	//if immediate, copy value
	if(DoesEvaluableNodeTypeUseNumberData(common_type))
		n->SetNumberValue(node->GetNumberValue());
	else if(DoesEvaluableNodeTypeUseStringData(common_type))
		n->SetStringID(node->GetStringID());

	//merge labels
	size_t n1_num_labels = n1->GetNumLabels();
	size_t n2_num_labels = n2->GetNumLabels();
	if(mm->KeepSomeNonMergeableValues())
	{
		if(n1_num_labels > 0 || n2_num_labels > 0)
			n->SetLabelsStringIds(UnionStringIDVectors(n1->GetLabelsStringIds(), n2->GetLabelsStringIds()));
	}
	else
	{
		if(n1_num_labels > 0 && n2_num_labels > 0)
			n->SetLabelsStringIds(IntersectStringIDVectors(n1->GetLabelsStringIds(), n2->GetLabelsStringIds()));
	}

	//merge comments if they exist
	if(n1->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID || n2->GetCommentsStringId() != StringInternPool::NOT_A_STRING_ID)
	{
		//convert from vectors of strings to vectors of pointers to strings so can merge on them
		auto n1_comment_strings = n1->GetCommentsSeparateLines();
		std::vector<std::string *> n1_comment_string_ptrs(n1_comment_strings.size());
		for(size_t i = 0; i < n1_comment_strings.size(); i++)
			n1_comment_string_ptrs[i] = &n1_comment_strings[i];

		auto n2_comment_strings = n2->GetCommentsSeparateLines();
		std::vector<std::string *> n2_comment_string_ptrs(n2_comment_strings.size());
		for(size_t i = 0; i < n2_comment_strings.size(); i++)
			n2_comment_string_ptrs[i] = &n2_comment_strings[i];

		StringSequenceMergeMetric ssmm(mm->KeepSomeNonMergeableValues());
		auto merged_comment_lines = ssmm.MergeSequences(n1_comment_string_ptrs, n2_comment_string_ptrs);
		
		//append back to one string
		std::string merged_comments;
		for(auto &line : merged_comment_lines)
		{
			//if already have comments, append a newline
			if(merged_comments.size() > 0)
				merged_comments.append("\r\n");
			merged_comments.append(*line);
		}

		n->SetComments(merged_comments);
	}

	return n;
}

std::vector<StringInternPool::StringID> EvaluableNodeTreeManipulation::UnionStringIDVectors(const std::vector<StringInternPool::StringID> &label_list_a, const std::vector<StringInternPool::StringID> &label_list_b)
{
	//quick shortcuts in case either list is empty
	if(label_list_a.size() == 0)
		return std::vector<StringInternPool::StringID>(label_list_b);
	if(label_list_b.size() == 0)
		return std::vector<StringInternPool::StringID>(label_list_a);

	//create list of unique labels included in either
	auto all_labels = CompactHashSet<StringInternPool::StringID>(label_list_a.size() + label_list_b.size());
	all_labels.insert(begin(label_list_a), end(label_list_a));
	all_labels.insert(begin(label_list_b), end(label_list_b));
	return std::vector<StringInternPool::StringID>(begin(all_labels), end(all_labels));
}

std::vector<StringInternPool::StringID> EvaluableNodeTreeManipulation::IntersectStringIDVectors(const std::vector<StringInternPool::StringID> &label_list_a, const std::vector<StringInternPool::StringID> &label_list_b)
{
	//quick shortcut in case either list is empty
	if(label_list_a.size() == 0 || label_list_b.size() == 0)
		return std::vector<StringInternPool::StringID>();

	std::vector<StringInternPool::StringID> labels_in_1(begin(label_list_a), end(label_list_a));
	std::vector<StringInternPool::StringID> labels_in_2(begin(label_list_b), end(label_list_b));
	std::vector<StringInternPool::StringID> common_labels(label_list_a.size() + label_list_b.size());	//hold enough in case all are unique

	//sort both of the lists as required before passing into set_intersection
	std::sort(begin(labels_in_1), end(labels_in_1));
	std::sort(begin(labels_in_2), end(labels_in_2));

	//create a new clean set and insert the set intersection
	auto common_end = std::set_intersection(begin(labels_in_1), end(labels_in_1), begin(labels_in_2), end(labels_in_2), begin(common_labels));

	//get rid of any unused entries
	common_labels.resize(common_end - begin(common_labels));
	return common_labels;
}

EvaluableNode *EvaluableNodeTreeManipulation::MergeTrees(NodesMergeMethod *mm, EvaluableNode *tree1, EvaluableNode *tree2)
{
	//shortcut for merging empty trees
	if(tree1 == nullptr && tree2 == nullptr)
		return nullptr;

	//if it's already been merged, then return the previous merged version
	auto &references = mm->GetReferences();

	auto find_tree1 = references.find(tree1);
	if(find_tree1 != end(references))
		return find_tree1->second;

	auto find_tree2 = references.find(tree2);
	if(find_tree2 != end(references))
		return find_tree2->second;

	//find best node to combine from each tree
	auto best_shared_nodes_match = mm->MergeMetric(tree1, tree2);
	//if not keeping any nonmergeable values, then just cut out anything that isn't common
	if(!mm->KeepSomeNonMergeableValues())
	{
		tree1 = best_shared_nodes_match.elementA;
		tree2 = best_shared_nodes_match.elementB;
	}
	else if( (tree1 != best_shared_nodes_match.elementA && mm->KeepNonMergeableA())
		|| (tree2 != best_shared_nodes_match.elementB && mm->KeepNonMergeableB()) )
	{
		//might keep one or the other, so make a merge which will be kept in references
		//if the reference is hit, it will be used
		//this result may be not used some of the time due to being cut out, but most of the time this will be efficient
		MergeTrees(mm, best_shared_nodes_match.elementA, best_shared_nodes_match.elementB);

		//whichever one doesn't match, set that one to null and merge on the one that did
		if(tree1 != best_shared_nodes_match.elementA)
			tree2 = nullptr;
		else if(tree2 != best_shared_nodes_match.elementB)
			tree1 = nullptr;
	}

	//get new generalized node of all
	EvaluableNode *generalized_node = EvaluableNodeTreeManipulation::CreateGeneralizedNode(mm, tree1, tree2);

	//if nothing, then don't keep processing
	if(generalized_node == nullptr)
		return nullptr;

	//put it in the references list for both trees
	if(tree1 != nullptr)
		references[tree1] = generalized_node;
	if(tree2 != nullptr)
		references[tree2] = generalized_node;

	//if the generalized_node is assoc and at least one is an assoc,
	// make sure are initialized (or initialize) and merge
	if( generalized_node->IsAssociativeArray() &&
		( (tree1 != nullptr && tree1->IsAssociativeArray())
		 || (tree2 != nullptr && tree2->IsAssociativeArray())) )
	{
		//get or convert the nodes to an assoc for tree1
		EvaluableNode::AssocType tree1_conversion_assoc;
		auto *tree1_mapped_childs = &tree1_conversion_assoc;
		if(tree1 != nullptr && tree1->IsAssociativeArray())
			tree1_mapped_childs = &tree1->GetMappedChildNodesReference();

		//get or convert the nodes to an assoc for tree2
		EvaluableNode::AssocType tree2_conversion_assoc;
		auto *tree2_mapped_childs = &tree2_conversion_assoc;
		if(tree2 != nullptr && tree2->IsAssociativeArray())
			tree2_mapped_childs = &tree2->GetMappedChildNodesReference();

		EvaluableNode::AssocType merged = mm->MergeMaps(*tree1_mapped_childs, *tree2_mapped_childs);
		//hand off merged allocation into the generalized_node (hence the false parameter)
		generalized_node->SetMappedChildNodes(merged, false);	

		return generalized_node;
	}

	std::vector<EvaluableNode *> empty_vector;

	auto *tree1_ordered_childs = &empty_vector;
	if(tree1 != nullptr && tree1->IsOrderedArray())
		tree1_ordered_childs = &tree1->GetOrderedChildNodesReference();

	auto *tree2_ordered_childs = &empty_vector;
	if(tree2 != nullptr && tree2->IsOrderedArray())
		tree2_ordered_childs = &tree2->GetOrderedChildNodesReference();

	//see if both trees have ordered child nodes
	if(tree1_ordered_childs->size() > 0 || tree2_ordered_childs->size() > 0)
	{
		auto iocnt = GetInstructionOrderedChildNodeType(generalized_node->GetType());
		switch(iocnt)
		{
		case OCNT_UNORDERED:
			generalized_node->SetOrderedChildNodes(mm->MergeUnorderedSets(*tree1_ordered_childs, *tree2_ordered_childs));
			break;

		case OCNT_ORDERED:
			generalized_node->SetOrderedChildNodes(mm->MergeSequences(*tree1_ordered_childs, *tree2_ordered_childs));
			break;

		case OCNT_ONE_POSITION_THEN_ORDERED:
		case OCNT_ONE_POSITION_THEN_PAIRED:
		{
			//start from a clean slate
			generalized_node->ClearOrderedChildNodes();

			//make arrays of just the first node
			std::vector<EvaluableNode *> a1;
			std::vector<EvaluableNode *> a2;
			if(tree1_ordered_childs->size() > 0)
				a1.emplace_back((*tree1_ordered_childs)[0]);
			if(tree2_ordered_childs->size() > 0)
				a2.emplace_back((*tree2_ordered_childs)[0]);

			//put on the first position
			auto merged = mm->MergePositions(a1, a2);
			generalized_node->GetOrderedChildNodes().insert(end(generalized_node->GetOrderedChildNodes()), begin(merged), end(merged));

			//make new arrays without first position
			a1.clear();
			a2.clear();
			if(tree1_ordered_childs->size() > 0)
				a1.insert(begin(a1), begin(*tree1_ordered_childs), end(*tree1_ordered_childs));
			if(tree2_ordered_childs->size() > 0)
				a2.insert(begin(a2), begin(*tree2_ordered_childs), end(*tree2_ordered_childs));
			if(a1.size() > 0)
				a1.erase(begin(a1));
			if(a2.size() > 0)
				a2.erase(begin(a2));

			//append the rest
			if(iocnt == OCNT_ONE_POSITION_THEN_ORDERED)
				merged = mm->MergeSequences(a1, a2);
			else if(iocnt == OCNT_ONE_POSITION_THEN_PAIRED)
				merged = mm->MergeUnorderedSetsOfPairs(a1, a2);
			generalized_node->GetOrderedChildNodes().insert(end(generalized_node->GetOrderedChildNodes()), begin(merged), end(merged));
			break;
		}

		case OCNT_PAIRED:
			generalized_node->SetOrderedChildNodes(mm->MergeUnorderedSetsOfPairs(*tree1_ordered_childs, *tree2_ordered_childs));
			break;

		case OCNT_POSITION:
			generalized_node->SetOrderedChildNodes(mm->MergePositions(*tree1_ordered_childs, *tree2_ordered_childs));
			break;

		default:
			break;
		}
	}

	return generalized_node;
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateTree(Interpreter *interpreter, EvaluableNodeManager *enm,
	EvaluableNode *tree, double mutation_rate,
	CompactHashMap<StringInternPool::StringID, double> *mutation_weights,
	CompactHashMap<EvaluableNodeType, double> *evaluable_node_weights)
{
	std::vector<std::string> strings;
	EvaluableNode::ReferenceSetType checked;
	GetStringsFromTree(tree, strings, checked);

	MutationParameters::WeightedRandEvaluableNodeType operation_type_wrs;
	if(evaluable_node_weights != nullptr && !evaluable_node_weights->empty())
		operation_type_wrs.Initialize(*evaluable_node_weights, true);

	MutationParameters::WeightedRandMutationType rand_mutation_type;
	if(mutation_weights != nullptr && !mutation_weights->empty())
		rand_mutation_type.Initialize(*mutation_weights, true);

	MutationParameters mp(interpreter, enm, mutation_rate, &strings, operation_type_wrs.IsInitialized() ? &operation_type_wrs : &evaluableNodeTypeRandomStream, rand_mutation_type.IsInitialized() ? &rand_mutation_type : &mutationOperationTypeRandomStream);
	EvaluableNode *ret = MutateTree(mp, tree);

	return ret;
}

void EvaluableNodeTreeManipulation::ReplaceStringsInTree(EvaluableNode *tree, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &to_replace)
{
	EvaluableNode::ReferenceSetType checked;
	ReplaceStringsInTree(tree, to_replace, checked);
}

EvaluableNodeType EvaluableNodeTreeManipulation::GetRandomEvaluableNodeType(RandomStream *rs)
{
	if(rs == nullptr)
		return ENT_NOT_A_BUILT_IN_TYPE;

	return evaluableNodeTypeRandomStream.WeightedDiscreteRand(*rs);
}

MergeMetricResults<EvaluableNode *> EvaluableNodeTreeManipulation::NumberOfSharedNodes(EvaluableNode *tree1, EvaluableNode *tree2,
	MergeMetricResultsCache &memoized, EvaluableNode::ReferenceSetType *checked)
{
	if(tree1 == nullptr && tree2 == nullptr)
		return MergeMetricResults(1.0, tree1, tree2, false, true);

	//if one is null and the other isn't, then stop
	if( (tree1 == nullptr && tree2 != nullptr) || (tree1 != nullptr && tree2 == nullptr) )
		return MergeMetricResults(0.0, tree1, tree2, false, false);

	//if the pair of nodes has already been computed, then just return the result
	auto found = memoized.find(std::make_pair(tree1, tree2));
	if(found != end(memoized))
		return found->second;

	if(checked != nullptr)
	{
		//if either is already checked, then neither adds shared nodes
		if(checked->find(tree1) != end(*checked) || checked->find(tree2) != end(*checked))
			return MergeMetricResults(0.0, tree1, tree2, false, true);
	}

	//if the trees are the same, then just return the size
	if(tree1 == tree2)
	{
		MergeMetricResults results(static_cast<double>(EvaluableNode::GetDeepSize(tree1)), tree1, tree2, true, true);
		memoized.emplace(std::make_pair(tree1, tree2), results);
		return results;
	}

	//check current top nodes
	auto commonality = CommonalityBetweenNodes(tree1, tree2);

	//see if can exit early, before inserting the nodes into the checked list and then removing them
	size_t tree1_ordered_nodes_size = 0;
	size_t tree1_mapped_nodes_size = 0;
	size_t tree2_ordered_nodes_size = 0;
	size_t tree2_mapped_nodes_size = 0;

	if(tree1->IsAssociativeArray())
		tree1_mapped_nodes_size = tree1->GetMappedChildNodesReference().size();
	else if(!tree1->IsImmediate())
		tree1_ordered_nodes_size = tree1->GetOrderedChildNodesReference().size();

	if(tree2->IsAssociativeArray())
		tree2_mapped_nodes_size = tree2->GetMappedChildNodesReference().size();
	else if(!tree2->IsImmediate())
		tree2_ordered_nodes_size = tree2->GetOrderedChildNodesReference().size();

	if(tree1_ordered_nodes_size == 0 && tree2_ordered_nodes_size == 0
		&& tree1_mapped_nodes_size == 0 && tree2_mapped_nodes_size == 0)
	{
		memoized.emplace(std::make_pair(tree1, tree2), commonality);
		return commonality;
	}

	if(checked != nullptr)
	{
		//remember that it has already checked when traversing tree, and then remove from checked at the end of the function
		checked->insert(tree1);
		checked->insert(tree2);
	}

	if(tree1_ordered_nodes_size > 0 && tree2_ordered_nodes_size > 0)
	{
		auto iocnt = GetInstructionOrderedChildNodeType(tree1->GetType());

		//if there's only one node in each, then just use OCNT_POSITION because
		// it's more efficient and the pairing doesn't matter
		if(tree1_ordered_nodes_size < 2 && tree2_ordered_nodes_size < 2)
			iocnt = OCNT_POSITION;

		switch(iocnt)
		{
		case OCNT_UNORDERED:
		{
			std::vector<EvaluableNode *> a2(tree2->GetOrderedChildNodesReference());

			//for every element in a1, check to see if there's any in a2
			for(auto &a1_current : tree1->GetOrderedChildNodesReference())
			{
				//find the node that best matches this one, greedily
				bool best_match_found = false;
				size_t best_match_index = 0;
				MergeMetricResults best_match_value(0.0, tree1, tree2, false, false);
				for(size_t match_index = 0; match_index < a2.size(); match_index++)
				{
					auto match_value = NumberOfSharedNodes(a1_current, a2[match_index], memoized, checked);
					if(!best_match_found || match_value > best_match_value)
					{
						best_match_found = true;
						best_match_value = match_value;
						best_match_index = match_index;

						if(best_match_value.mustMatch || best_match_value.exactMatch)
							break;
					}
				}

				//if found a match, then remove it from the match list and put it in the list
				if(best_match_found)
				{
					//count this for whatever match it is
					commonality += best_match_value;

					a2.erase(begin(a2) + best_match_index);
				}
			}
			break;
		}

		case OCNT_ORDERED:
		case OCNT_ONE_POSITION_THEN_ORDERED:
		{
			auto &ocn1 = tree1->GetOrderedChildNodesReference();
			auto &ocn2 = tree2->GetOrderedChildNodesReference();
			auto size1 = ocn1.size();
			auto size2 = ocn2.size();

			size_t starting_index = 0;

			if(iocnt == OCNT_ONE_POSITION_THEN_ORDERED)
			{
				auto smallest_list_size = std::min(size1, size2);
				if(smallest_list_size >= 1)
					commonality += NumberOfSharedNodes(ocn1[0], ocn2[0], memoized, checked);

				starting_index = 1;
			}

			FlatMatrix<MergeMetricResults<EvaluableNode *>> sequence_commonality;
			ComputeSequenceCommonalityMatrix(sequence_commonality, ocn1, ocn2,
				[&memoized, checked]
				(EvaluableNode *a, EvaluableNode *b)
				{
					return EvaluableNodeTreeManipulation::NumberOfSharedNodes(a, b, memoized, checked);
				}, starting_index);

			commonality += sequence_commonality.At(size1, size2);
			break;
		}

		case OCNT_PAIRED:
		case OCNT_ONE_POSITION_THEN_PAIRED:
		{
			std::vector<EvaluableNode *> a1(tree1->GetOrderedChildNodesReference());
			std::vector<EvaluableNode *> a2(tree2->GetOrderedChildNodesReference());

			if(iocnt == OCNT_ONE_POSITION_THEN_PAIRED)
			{
				auto smallest_list_size = std::min(a1.size(), a2.size());
				if(smallest_list_size >= 1)
				{
					commonality += NumberOfSharedNodes(a1[0], a2[0], memoized, checked);

					a1.erase(begin(a1));
					a2.erase(begin(a2));
				}
			}

			//for every element in a1, check to see if there's any in a2
			while(a1.size() > 0 && a2.size() > 0)
			{
				//find the key (even numbered) node that best matches this one, greedily
				bool best_match_found = false;
				size_t best_match_index = 0;
				MergeMetricResults<EvaluableNode *> best_match_key(0.0, nullptr, nullptr, false, false);
				MergeMetricResults<EvaluableNode *> best_match_value(0.0, nullptr, nullptr, false, false);

				for(size_t match_index = 0; match_index < a2.size(); match_index += 2)
				{
					auto match_key = NumberOfSharedNodes(a1[0], a2[match_index], memoized, checked);

					// key match dominates value match
					if(!best_match_found || match_key > best_match_key)
					{
						best_match_found = true;
						best_match_key = match_key;
						best_match_index = match_index;

						//count the value node commonality as long as it exists and is nontrivial
						if(match_key.IsNontrivialMatch() && a1.size() > 1 && a2.size() > match_index + 1)
							best_match_value = NumberOfSharedNodes(a1[1], a2[match_index + 1], memoized, checked);
						else
							best_match_value = MergeMetricResults<EvaluableNode *>(0.0, nullptr, nullptr, false, false);
					}
				}

				//if found a match, then remove it from the match list and put it in the list
				if(best_match_found)
				{
					//remove the key node
					a2.erase(begin(a2) + best_match_index);
					//also remove the value node if it exists; use same index because it will be shifted down
					if(a2.size() > 0 && a2.size() > best_match_index)
						a2.erase(begin(a2) + best_match_index);

					//count this for whatever match it is
					commonality += best_match_key;
					commonality += best_match_value;
				}

				//remove a potential pair from the first list
				a1.erase(begin(a1));
				if(a1.size() > 0)	//make sure that there's a second in the pair
					a1.erase(begin(a1));
			}
			break;
		}

		case OCNT_POSITION:
		{
			auto &ocn1 = tree1->GetOrderedChildNodesReference();
			auto &ocn2 = tree2->GetOrderedChildNodesReference();
			//use size of smallest list
			auto smallest_list_size = std::min(ocn1.size(), ocn2.size());
			for(size_t i = 0; i < smallest_list_size; i++)
				commonality += NumberOfSharedNodes(ocn1[i], ocn2[i], memoized, checked);

			break;
		}

		}
	}
	
	if(tree1_mapped_nodes_size > 0 && tree2_mapped_nodes_size > 0)
	{
		//use keys from first node
		for(auto &[node_id, node] : tree1->GetMappedChildNodes())
		{
			//skip unless both trees have the key
			auto other_node = tree2->GetMappedChildNodes().find(node_id);
			if(other_node == end(tree2->GetMappedChildNodes()))
				continue;

			commonality += NumberOfSharedNodes(node, other_node->second, memoized, checked);
		}
	}

	//if not exact match of nodes and all child nodes, then check all child nodes for better submatches
	if(!commonality.exactMatch)
	{
		if(tree1_ordered_nodes_size > 0)
		{
			for(auto node : tree1->GetOrderedChildNodesReference())
			{
				auto sub_match = NumberOfSharedNodes(tree2, node, memoized, checked);
				if(sub_match > commonality)
					commonality = sub_match;
			}
		}
		else if(tree1_mapped_nodes_size > 0)
		{
			for(auto &[node_id, node] : tree1->GetMappedChildNodes())
			{
				auto sub_match = NumberOfSharedNodes(tree2, node, memoized, checked);
				if(sub_match > commonality)
					commonality = sub_match;
			}
		}

		if(tree2_ordered_nodes_size > 0)
		{
			for(auto cn : tree2->GetOrderedChildNodesReference())
			{
				auto sub_match = NumberOfSharedNodes(tree1, cn, memoized, checked);
				if(sub_match > commonality)
					commonality = sub_match;
			}
		}
		else if(tree2_mapped_nodes_size > 0)
		{
			for(auto &[node_id, node] : tree2->GetMappedChildNodes())
			{
				auto sub_match = NumberOfSharedNodes(tree1, node, memoized, checked);
				if(sub_match > commonality)
					commonality = sub_match;
			}
		}
	}

	if(checked != nullptr)
	{
		//remove from the checked list so don't block other traversals
		checked->erase(tree1);
		checked->erase(tree2);
	}

	memoized.emplace(std::make_pair(tree1, tree2), commonality);
	return commonality;
}

bool EvaluableNodeTreeManipulation::NonCycleDoesTreeContainLabels(EvaluableNode *en)
{
	if(en->GetNumLabels() > 0)
		return true;

	for(auto cn : en->GetOrderedChildNodes())
	{
		if(cn == nullptr)
			continue;

		if(NonCycleDoesTreeContainLabels(cn))
			return true;
	}

	for(auto &[_, cn] : en->GetMappedChildNodes())
	{
		if(cn == nullptr)
			continue;

		if(NonCycleDoesTreeContainLabels(cn))
			return true;
	}

	return false;
}

bool EvaluableNodeTreeManipulation::DoesTreeContainLabels(EvaluableNode *en, EvaluableNode::ReferenceSetType &checked)
{
	auto [_, inserted] = checked.insert(en);
	if(!inserted)
		return false;

	if(en->GetNumLabels() > 0)
		return true;

	for(auto cn : en->GetOrderedChildNodes())
	{
		if(cn == nullptr)
			continue;

		if(DoesTreeContainLabels(cn, checked))
			return true;
	}

	for(auto &[_, cn] : en->GetMappedChildNodes())
	{
		if(cn == nullptr)
			continue;

		if(DoesTreeContainLabels(cn, checked))
			return true;
	}

	return false;
}

bool EvaluableNodeTreeManipulation::CollectLabelIndexesFromNormalTree(EvaluableNode *tree, Entity::LabelsAssocType &index, EvaluableNode::ReferenceSetType *checked)
{
	if(tree == nullptr)
		return false;

	//attempt to insert, but if has already been checked and in checked list (circular code), then return false
	if(checked != nullptr && checked->insert(tree).second == false)
		return false;

	size_t num_labels = tree->GetNumLabels();
	for(size_t i = 0; i < num_labels; i++)
	{
		auto label_sid = tree->GetLabelStringId(i);
		const std::string &label_name = string_intern_pool.GetStringFromID(label_sid);

		if(label_name.size() == 0)
			continue;

		//ignore labels that have a # in the beginning
		if(label_name[0] == '#')
			continue;

		//attempt to put the label in the index
		auto [_, inserted] = index.insert(std::make_pair(label_sid, tree));

		//if label already exists
		if(!inserted)
			return true;
	}

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
		{
			if(CollectLabelIndexesFromNormalTree(e, index, checked))
				return true;
		}
	}
	else if(tree->IsOrderedArray())
	{
		for(auto &e : tree->GetOrderedChildNodesReference())
		{
			if(CollectLabelIndexesFromNormalTree(e, index, checked))
				return true;
		}
	}

	return false;
}

void EvaluableNodeTreeManipulation::CollectAllLabelIndexesFromTree(EvaluableNode *tree, Entity::LabelsAssocType &index, EvaluableNode::ReferenceSetType *checked)
{
	if(tree == nullptr)
		return;

	//attempt to insert, but if has already been checked and in checked list (circular code), then return false
	if(checked != nullptr && checked->insert(tree).second == false)
		return;

	size_t num_labels = tree->GetNumLabels();
	for(size_t i = 0; i < num_labels; i++)
	{
		auto label_sid = tree->GetLabelStringId(i);
		const std::string &label_name = string_intern_pool.GetStringFromID(label_sid);

		if(label_name.size() == 0)
			continue;

		//ignore labels that have a # in the beginning
		if(label_name[0] == '#')
			continue;

		//attempt to put the label in the index
		index.insert(std::make_pair(label_sid, tree));
	}

	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
			CollectAllLabelIndexesFromTree(e, index, checked);
	}
	else if(tree->IsOrderedArray())
	{
		for(auto &e : tree->GetOrderedChildNodesReference())
			CollectAllLabelIndexesFromTree(e, index, checked);
	}
}

bool EvaluableNodeTreeManipulation::CollectLabelIndexesFromTreeAndMakeLabelNormalizationPass(EvaluableNode *tree, Entity::LabelsAssocType &index,
	EvaluableNode::ReferenceSetType &checked, EvaluableNode *&replace_tree_by)
{
	if(tree == nullptr)
		return false;
	
	//attempt to insert, but if has already been checked and in checked list (circular code), then return false
	if(checked.insert(tree).second == false)
		return false;

	//if this node has any labels, insert them and check for collisions
	size_t num_labels = tree->GetNumLabels();
	for(size_t i = 0; i < num_labels; i++)
	{
		auto label_sid = tree->GetLabelStringId(i);
		const std::string &label_name = string_intern_pool.GetStringFromID(label_sid);

		if(label_name.size() == 0)
			continue;

		//ignore labels that have a # in the beginning
		if(label_name[0] == '#')
			continue;

		//attempt to put the label in the index
		const auto &[inserted_value, inserted] = index.insert(std::make_pair(label_sid, tree));
		
		//if label already exists
		if(!inserted)
		{
			replace_tree_by = inserted_value->second;

			//add any labels from this tree if they are not on the existing node that has the label
			if(replace_tree_by != nullptr)
				replace_tree_by->SetLabelsStringIds(EvaluableNodeTreeManipulation::UnionStringIDVectors(tree->GetLabelsStringIds(), replace_tree_by->GetLabelsStringIds()));

			//more than one thing points to this label
			return true;
		}
	}

	//traverse child nodes. If find a replacement, then mark as such to return, and if need immediate replacement of a node, then do so
	// continue to iterate over all children even if have a replacement, to reduce the total number of passes needed over the tree
	bool had_any_replacement = false;
	if(tree->IsAssociativeArray())
	{
		for(auto &[_, e] : tree->GetMappedChildNodesReference())
		{
			EvaluableNode *replace_node_by = nullptr;
			auto replacement = CollectLabelIndexesFromTreeAndMakeLabelNormalizationPass(e, index, checked, replace_node_by);

			if(replacement)
			{
				had_any_replacement = true;
				if(replace_node_by != nullptr)
					e = replace_node_by;
			}
		}
	}
	else if(tree->IsOrderedArray())
	{
		for(auto &e : tree->GetOrderedChildNodes())
		{
			EvaluableNode *replace_node_by = nullptr;
			bool replacement = CollectLabelIndexesFromTreeAndMakeLabelNormalizationPass(e, index, checked, replace_node_by);

			if(replacement)
			{
				had_any_replacement = true;
				if(replace_node_by != nullptr)
					e = replace_node_by;
			}
		}
	}

	return had_any_replacement;
}

MergeMetricResults<EvaluableNode *> EvaluableNodeTreeManipulation::CommonalityBetweenNodes(EvaluableNode *n1, EvaluableNode *n2)
{
	if(n1 == nullptr && n2 == nullptr)
		return MergeMetricResults(1.0, n1, n2, false, true);

	if(n1 == nullptr || n2 == nullptr)
		return MergeMetricResults(0.0, n1, n2, false, false);

	size_t num_common_labels;
	size_t num_unique_labels;
	EvaluableNode::GetNodeCommonAndUniqueLabelCounts(n1, n2, num_common_labels, num_unique_labels);

	auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(n1, n2);

	//if no labels, as is usually the case, then just address normal commonality
	// and if the nodes are exactly equal
	if(num_unique_labels == 0)
		return MergeMetricResults(commonality, n1, n2, false, commonality == 1.0);

	return MergeMetricResults(commonality + num_common_labels, n1, n2, num_common_labels == num_unique_labels, commonality == 1.0);
}

std::pair<EvaluableNode *, double> EvaluableNodeTreeManipulation::CommonalityBetweenNodeTypesAndValues(
	EvaluableNode *n1, EvaluableNode *n2, bool require_exact_node_match)
{
	bool n1_null = EvaluableNode::IsNull(n1);
	bool n2_null = EvaluableNode::IsNull(n2);
	if(n1_null && n2_null)
		return std::make_pair(n1, 1.0);

	//if either is nullptr, then use an actual EvaluableNode
	if(n1 == nullptr)
		n1 = &nullEvaluableNode;
	if(n2 == nullptr)
		n2 = &nullEvaluableNode;

	auto n1_type = n1->GetType();
	auto n2_type = n2->GetType();

	//can have much faster and lighter computations if only checking for exact matches
	if(require_exact_node_match)
	{
		if(n1_type != n2_type)
			return std::make_pair(n1, 0.0);

		if(n1_type == ENT_NUMBER)
		{
			double n1_value = n1->GetNumberValueReference();
			double n2_value = n2->GetNumberValueReference();
			return std::make_pair(n1, EqualIncludingNaN(n1_value, n2_value) ? 1.0 : 0.0);
		}
		if(n1->IsStringValue())
		{
			auto n1_sid = n1->GetStringID();
			auto n2_sid = n2->GetStringID();
			return std::make_pair(n1, n1_sid == n2_sid ? 1.0 : 0.0);
		}
		return std::make_pair(n1, 1.0);
	}

	//compare similar types that are not the same, or types that have immediate comparisons
	//if the types are the same, it'll be caught below
	switch(n1_type)
	{
	case ENT_SEQUENCE:
		if(n2_type == ENT_PARALLEL)				return std::make_pair(n1, 0.25);
		if(n2_type == ENT_NULL)					return std::make_pair(n2, 0.125);
		if(n2_type == ENT_LIST)					return std::make_pair(n2, 0.125);
		break;

	case ENT_PARALLEL:
		if(n2_type == ENT_SEQUENCE)				return std::make_pair(n2, 0.25);
		if(n2_type == ENT_NULL)					return std::make_pair(n2, 0.125);
		if(n2_type == ENT_LIST)					return std::make_pair(n2, 0.125);
		break;

	case ENT_CALL:
		if(n2_type == ENT_CALL_SANDBOXED)		return std::make_pair(n1, 0.25);
		break;

	case ENT_CALL_SANDBOXED:
		if(n2_type == ENT_CALL)					return std::make_pair(n2, 0.25);
		break;

	case ENT_LET:
		if(n2_type == ENT_DECLARE)				return std::make_pair(n2, 0.5);
		break;

	case ENT_DECLARE:
		if(n2_type == ENT_LET)					return std::make_pair(n1, 0.5);
		break;

	case ENT_REDUCE:
		if(n2_type == ENT_APPLY)				return std::make_pair(n1, 0.125);
		break;

	case ENT_APPLY:
		if(n2_type == ENT_REDUCE)				return std::make_pair(n2, 0.125);
		break;

	case ENT_SET:
		if(n2_type == ENT_REPLACE)				return std::make_pair(n2, 0.5);
		break;

	case ENT_REPLACE:
		if(n2_type == ENT_SET)					return std::make_pair(n1, 0.5);
		break;

	case ENT_ASSOC:
		if(n2_type == ENT_ASSOCIATE)			return std::make_pair(n1, 0.25);
		break;

	case ENT_ASSOCIATE:
		if(n2_type == ENT_ASSOC)				return std::make_pair(n2, 0.25);
		break;

	case ENT_TRUE:
		if(n2_type == ENT_FALSE)				return std::make_pair(n1, 0.375);
		if(n2_type == ENT_NUMBER || n2_type == ENT_NULL)
		{
			double n2_value = EvaluableNode::ToNumber(n2);
			if(n2_value)
				return std::make_pair(n2, 0.875);
			return std::make_pair(n2, 0.125);
		}
		break;

	case ENT_FALSE:
		if(n2_type == ENT_TRUE)					return std::make_pair(n1, 0.375);
		if(n2_type == ENT_NUMBER || n2_type == ENT_NULL)
		{
			double n2_value = EvaluableNode::ToNumber(n2);
			if(n2_value == 0.0)
				return std::make_pair(n2, 0.875);
			if(FastIsNaN(n2_value))
				return std::make_pair(n2, 0.5);
			return std::make_pair(n2, 0.375);
		}
		break;

	case ENT_NULL:
		if(n2_type == ENT_TRUE)					return std::make_pair(n1, 0.25);
		if(n2_type == ENT_FALSE)				return std::make_pair(n1, 0.5);
		if(n2_type == ENT_NUMBER)
		{
			double n2_value = EvaluableNode::ToNumber(n2);
			if(n2_value == 0.0)
				return std::make_pair(n2, 0.5);
			if(FastIsNaN(n2_value))
				return std::make_pair(n2, 0.875);
			return std::make_pair(n2, 0.375);
		}
		if(n2_type == ENT_SEQUENCE)			return std::make_pair(n1, 0.125);
		if(n2_type == ENT_PARALLEL)			return std::make_pair(n1, 0.125);
		if(n2_type == ENT_LIST)				return std::make_pair(n1, 0.125);
		break;

	case ENT_LIST:
		if(n2_type == ENT_SEQUENCE)			return std::make_pair(n1, 0.125);
		if(n2_type == ENT_PARALLEL)			return std::make_pair(n1, 0.125);
		if(n2_type == ENT_NULL)				return std::make_pair(n1, 0.125);
		break;

	case ENT_NUMBER:
	{
		double n1_value = n1->GetNumberValueReference();

		if(n2_type == ENT_TRUE)
		{
			if(n1_value)
				return std::make_pair(n2, 0.875);
			return std::make_pair(n1, 0.375);
		}

		if(n2_type == ENT_FALSE)
		{
			if(n1_value == 0.0)
				return std::make_pair(n1, 0.875);
			if(FastIsNaN(n1_value))
				return std::make_pair(n1, 0.5);
			return std::make_pair(n1, 0.375);
		}

		if(n2_type == ENT_NULL)
		{
			if(n1_value == 0.0)
				return std::make_pair(n1, 0.5);
			if(FastIsNaN(n1_value))
				return std::make_pair(n1, 0.875);
			return std::make_pair(n1, 0.375);
		}

		if(n2_type == ENT_NUMBER)
		{
			double n2_value = n2->GetNumberValueReference();
			if(EqualIncludingNaN(n1_value, n2_value))
				return std::make_pair(n1, 1.0);

			if(FastIsNaN(n1_value) || FastIsNaN(n2_value))
				return std::make_pair(n1, 0.25);

			double commonality = CommonalityBetweenNumbers(n1_value, n2_value);
			double commonality_including_type = std::max(0.25, commonality);

			if(n1_type == ENT_NUMBER)
				return std::make_pair(n1, commonality_including_type);
			else
				return std::make_pair(n2, commonality_including_type);
		}

		if(n2_type == ENT_RAND)
			return std::make_pair(n1, 0.25);

		//can't match with any other type
		return std::make_pair(nullptr, 0.0);
	}

	case ENT_RAND:
		if(n2_type == ENT_NUMBER)						
			return std::make_pair(n1, 0.125);
		break;

	case ENT_STRING:
		if(n2_type == ENT_STRING)
		{
			auto n1sid = n1->GetStringID();
			auto n2sid = n2->GetStringID();
			return std::make_pair(n1, CommonalityBetweenStrings(n1sid, n2sid));
		}

		//can't match with any other type
		return std::make_pair(nullptr, 0.0);

	case ENT_SYMBOL:
		if(n2_type == ENT_SYMBOL)
		{
			if(n2->GetStringID() == n1->GetStringID())													
				return std::make_pair(n1, 1.0);
			else																						
				return std::make_pair(n1, 0.25);
		}
		break;

	default:
		break;
	}
	
	if(n1_type == n2_type)
		return std::make_pair(n1, 1.0);

	//different type, how close?
	if(IsEvaluableNodeTypeQuery(n1_type) && IsEvaluableNodeTypeQuery(n2_type))
		return std::make_pair(n1, 0.25);
		
	//see if compatible opcode ordering
	if(GetInstructionOrderedChildNodeType(n1_type) == GetInstructionOrderedChildNodeType(n2_type))
		return std::make_pair(n1, 0.125);
		
	return std::make_pair(nullptr, 0.0);
}

std::string GenerateRandomString(RandomStream &rs)
{
	//make the length between 1 and 32, with a mean of 6
	int string_length = std::min(32, static_cast<int>(rs.ExponentialRand(3.0)) + 1 + static_cast<int>(rs.Rand() * 4));
	std::string retval;
	retval.reserve(string_length);
	static const std::string samples("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
	for(int i = 0; i < string_length; i++)
	{
		auto sample = samples[rs.RandSize(samples.length())];
		retval.push_back(sample);
	}
	return retval;
}

std::string GenerateRandomStringGivenStringSet(RandomStream &rs, std::vector<std::string> &strings, double novel_chance = 0.08)
{
	if(strings.size() == 0 || rs.Rand() < novel_chance) //small but nontrivial chance of making a new string
	{
		std::string s = GenerateRandomString(rs);
		//put the string into the list of considered strings
		strings.emplace_back(s);
		return s;
	}
	else //use randomly chosen existing string
	{
		size_t rand_index = rs.RandSize(strings.size());
		return std::string(strings[rand_index]);
	}
}

//helper function for EvaluableNodeTreeManipulation::MutateNode to populate immediate data
void MutateImmediateNode(EvaluableNode *n, RandomStream &rs, std::vector<std::string> &strings)
{
	if(DoesEvaluableNodeTypeUseNumberData(n->GetType()))
	{
		double cur_value = n->GetNumberValue();

		//if it's a NaN, then sometimes randomly replace it with a non-null value (which can be mutated further below)
		if(FastIsNaN(cur_value) && rs.Rand() < 0.9)
			cur_value = rs.Rand();

		//50% chance of being negative if negative, 50% of that 50% if positive (minimizing assumptions - a number can be either)
		bool is_negative = (cur_value < 0.0);
		bool new_number_negative = (rs.Rand() < (is_negative ? 0.5 : 0.25));
		double new_value = rs.ExponentialRand(fabs(cur_value));

		//chance to keep it an integer if it is already an integer
		double int_part;
		bool is_integer = (std::modf(cur_value, &int_part) == 0.0);
		if(is_integer && (rs.Rand() < 0.5))
			new_value = std::round(new_value);

		if(rs.Rand() < 0.01)
		{
			if(rs.Rand() < 0.5)
				new_value = std::numeric_limits<double>::infinity();
			else
				new_value = std::numeric_limits<double>::quiet_NaN();
		}

		n->SetNumberValue((new_number_negative ? -1 : 1) * new_value);
	}
	else if(DoesEvaluableNodeTypeUseStringData(n->GetType()))
	{
		n->SetStringValue(GenerateRandomStringGivenStringSet(rs, strings));
	}
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateNode(EvaluableNode *n, MutationParameters &mp)
{
	if(n == nullptr)
		return nullptr;

	//if immediate type (after initial mutation), see if should mutate value
	bool is_immediate = n->IsImmediate();
	if(is_immediate)
	{
		if(mp.interpreter->randomStream.Rand() < 0.5)
			MutateImmediateNode(n, mp.interpreter->randomStream, *mp.strings);
	}

	StringInternPool::StringID mutation_type = mp.randMutationType->WeightedDiscreteRand(mp.interpreter->randomStream);
	//only mark for likely deletion if null has no parameters
	if(n->GetType() == ENT_NULL && n->GetOrderedChildNodes().size() == 0 && n->GetMappedChildNodes().size() && mp.interpreter->randomStream.Rand() < 0.5)
		mutation_type = ENBISI_delete;

	//if immediate, can't perform most of the mutations, just mutate it
	if(is_immediate && (mutation_type != ENBISI_change_label && mutation_type != ENBISI_change_type))
		mutation_type = ENBISI_change_type;

	switch(mutation_type)
	{
		case ENBISI_change_type:
			n->SetType(mp.randEvaluableNodeType->WeightedDiscreteRand(mp.interpreter->randomStream), mp.enm);
			if(IsEvaluableNodeTypeImmediate(n->GetType()))
				MutateImmediateNode(n, mp.interpreter->randomStream, *mp.strings);
			break;

		case ENBISI_delete:
			if(n->GetOrderedChildNodes().size() > 0)
			{
				size_t num_children = n->GetOrderedChildNodes().size();
				size_t replace_with = mp.interpreter->randomStream.RandSize(num_children);
				n = mp.enm->AllocNode(n->GetOrderedChildNodes()[replace_with]);
			}
			else if(n->GetMappedChildNodes().size() > 0)
			{
				size_t num_children = n->GetMappedChildNodes().size();
				double replace_with = mp.interpreter->randomStream.Rand() * num_children;
				//iterate over child nodes until find the right index
				for(auto &[_, cn] : n->GetMappedChildNodes())
				{
					if(replace_with < 1.0)
					{
						n = mp.enm->AllocNode(cn);
						break;
					}
					replace_with--;
				}

			}
			else
				n->SetType(ENT_NULL, mp.enm);
			break;

		case ENBISI_insert:
		{
			//use some heuristics to generate some random immediate value
			EvaluableNode *new_node = mp.enm->AllocNode(mp.randEvaluableNodeType->WeightedDiscreteRand(mp.interpreter->randomStream));

			//give it a respectable default before randomizing
			if(DoesEvaluableNodeTypeUseNumberData(new_node->GetType()))
				n->SetNumberValue(50);
			if(DoesEvaluableNodeTypeUseStringData(new_node->GetType()))
				n->SetStringValue("string");

			MutateImmediateNode(n, mp.interpreter->randomStream, *mp.strings);
			if(n->IsAssociativeArray())
			{
				// get a random key
				std::string key = GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings);
				n->SetMappedChildNode(key, new_node);
			}
			else
				n->AppendOrderedChildNode(new_node);
			break;
		}

		case ENBISI_swap_elements:
			if(n->GetOrderedChildNodes().size() > 0)
			{
				size_t num_child_nodes = n->GetOrderedChildNodesReference().size();
				auto first_index = mp.interpreter->randomStream.RandSize(num_child_nodes);
				auto second_index = mp.interpreter->randomStream.RandSize(num_child_nodes);
				std::swap(n->GetOrderedChildNodes()[first_index], n->GetOrderedChildNodes()[second_index]);
			}
			else if(n->GetMappedChildNodes().size() > 1)
			{
				auto &n_mcn = n->GetMappedChildNodesReference();
				size_t num_child_nodes = n_mcn.size();
				auto first_index = mp.interpreter->randomStream.RandSize(num_child_nodes);
				auto second_index = mp.interpreter->randomStream.RandSize(num_child_nodes);

				auto first_entry = begin(n_mcn);
				auto first_key = string_intern_pool.EMPTY_STRING_ID;
				while(first_index > 0 && first_entry != end(n_mcn))
				{
					first_entry++;
					first_index++;
				}
				first_key = first_entry->first;

				auto second_entry = begin(n_mcn);
				auto second_key = string_intern_pool.EMPTY_STRING_ID;
				while(second_index > 0 && second_entry != end(n_mcn))
				{
					second_entry++;
					second_index++;
				}
				second_key = second_entry->first;

				//need to do a manual swap because the first iterator is invalidated
				EvaluableNode *temp = n_mcn[second_key];
				n_mcn[second_key] = n_mcn[first_key];
				n_mcn[first_key] = temp;

			}
			break;

		case ENBISI_deep_copy_elements:
			if(n->GetOrderedChildNodes().size() > 0)
			{
				size_t num_children = n->GetOrderedChildNodesReference().size();
				size_t source_index = mp.interpreter->randomStream.RandSize(num_children);
				size_t destination_index = mp.interpreter->randomStream.RandSize(num_children + 1);
				if(destination_index >= num_children)
					n->AppendOrderedChildNode(mp.enm->DeepAllocCopy(n->GetOrderedChildNodes()[source_index]));
				else
					n = n->GetOrderedChildNodes()[destination_index] = mp.enm->DeepAllocCopy(n->GetOrderedChildNodes()[source_index]);
			}
			else if(n->GetMappedChildNodes().size() > 0)
			{
				auto num_children = n->GetMappedChildNodesReference().size();
				size_t source_index = mp.interpreter->randomStream.RandSize(num_children);
				EvaluableNode *source_node = nullptr;
				size_t destination_index = mp.interpreter->randomStream.RandSize(num_children + 1);
				//iterate over child nodes until find the right index
				for(auto &[_, cn] : n->GetMappedChildNodes())
				{
					if(source_index < 1)
					{
						source_node = cn;
						break;
					}
					source_index--;
				}
			
				for(auto &[_, cn] : n->GetMappedChildNodes())
				{
					if(destination_index < 1)
					{
						cn = mp.enm->DeepAllocCopy(source_node);
						destination_index--;
						break;
					}
					destination_index--;
				}
			
				//need to create a new key
				if(destination_index > 0)
				{
					std::string new_key = GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings, 0.6);
					n->SetMappedChildNode(new_key, mp.enm->DeepAllocCopy(source_node));
				}
			}
			break;

		case ENBISI_delete_elements:
			n->ClearOrderedChildNodes();
			n->ClearMappedChildNodes();
			break;

		case ENBISI_change_label:
			//affect labels
			if(n != nullptr)
			{
				//see if can delete a label, and delete all if the option is available and chosen to keep new label creation balanced
				if(n->GetNumLabels() > 0 && mp.interpreter->randomStream.Rand() < 0.875)
				{
					n->ClearLabels();
				}
				else
				{
					//add new label
					std::string new_label = GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings);
					n->AppendLabel(new_label);
				}
			}
			break;

		default:
			//error, don't do anything
			break;
	}

	//clear excess nulls (with no child nodes) in lists
	if(n != nullptr)
	{
		while(!n->GetOrderedChildNodes().empty()
			&& (n->GetOrderedChildNodes().back() == nullptr
					|| (n->GetOrderedChildNodes().back()->GetOrderedChildNodes().size() == 0
						&& n->GetOrderedChildNodes().back()->GetMappedChildNodes().size() == 0) ))
		{
			//either remove this one or stop removing
			if(mp.interpreter->randomStream.Rand() > 0.125)
				n->GetOrderedChildNodes().pop_back();
			else
				break;
		}
	}

	return n;	
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateTree(MutationParameters &mp, EvaluableNode *tree)
{
	if(tree == nullptr)
		return nullptr;

	//if this object has already been copied, then just return the reference to the new copy
	auto found_copy = mp.references.find(tree);
	if(found_copy != end(mp.references))
		return found_copy->second;

	EvaluableNode *copy = mp.enm->AllocNode(tree);
	auto node_stack = mp.interpreter->CreateInterpreterNodeStackStateSaver(copy);

	//shouldn't happen, but just to be safe
	if(copy == nullptr)
		return nullptr;

	if(mp.interpreter->randomStream.Rand() < mp.mutation_rate)
	{
		EvaluableNode *new_node = MutateNode(copy, mp);
		//make sure have the right node to reference if it's a new node
		if(new_node != copy)
		{
			copy = new_node;

			node_stack.PopEvaluableNode();
			node_stack.PushEvaluableNode(new_node);
		}
	}
	
	(mp.references)[tree] = copy;

	//this shouldn't happen - it should be a node of type ENT_NULL, but check just in case
	if(copy == nullptr)
		return nullptr;

	if(copy->IsAssociativeArray())
	{
		//for any mapped children, copy and update
		for(auto &[_, s] : copy->GetMappedChildNodesReference())
		{
			//get current item in list
			EvaluableNode *n = s;
			if(n == nullptr)
				continue;

			//turn into a copy and mutate
			n = MutateTree(mp, n);

			//replace current item in list with copy
			s = n;
		}
	}
	else
	{
		//for any ordered children, copy and update
		auto &ocn = copy->GetOrderedChildNodes();
		for(size_t i = 0; i < ocn.size(); i++)
		{
			//get current item in list
			EvaluableNode *n = ocn[i];
			if(n == nullptr)
				continue;

			//turn into a copy and mutate
			n = MutateTree(mp, n);

			//replace current item in list with copy
			ocn[i] = n;
		}
	}	

	return copy;
}

void EvaluableNodeTreeManipulation::ReplaceStringsInTree(EvaluableNode *tree, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &to_replace, EvaluableNode::ReferenceSetType &checked)
{
	if(tree == nullptr)
		return;

	//try to record, but if already checked, then don't do anything
	auto [_, inserted] = checked.insert(tree);
	if(!inserted)
		return;

	if(tree->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : tree->GetMappedChildNodesReference())
			ReplaceStringsInTree(cn, to_replace, checked);
	}
	else if(tree->IsImmediate())
	{
		if(tree->GetType() == ENT_STRING)
		{
			auto replacement = to_replace.find(tree->GetStringID());
			if(replacement != end(to_replace))
				tree->SetStringID(replacement->second);
		}
	}
	else //ordered
	{
		for(auto cn : tree->GetOrderedChildNodesReference())
			ReplaceStringsInTree(cn, to_replace, checked);
	}
}

void EvaluableNodeTreeManipulation::GetStringsFromTree(EvaluableNode *tree, std::vector<std::string> &strings, EvaluableNode::ReferenceSetType &checked)
{
	if(tree == nullptr)
		return;

	//try to record, but if already checked, then don't do anything
	auto [_, inserted] = checked.insert(tree);
	if(!inserted)
		return;

	if(tree->IsAssociativeArray())
	{
		for(auto &[cn_id, cn] : tree->GetMappedChildNodesReference())
			GetStringsFromTree(cn, strings, checked);
	}
	else if(tree->IsImmediate())
	{
		if(DoesEvaluableNodeTypeUseStringData(tree->GetType()))
			strings.emplace_back(tree->GetStringValue());
	}
	else //ordered
	{
		for(auto &cn : tree->GetOrderedChildNodesReference())
			GetStringsFromTree(cn, strings, checked);
	}
}

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local std::vector<uint32_t> EvaluableNodeTreeManipulation::aCharsBuffer;
thread_local std::vector<uint32_t> EvaluableNodeTreeManipulation::bCharsBuffer;
thread_local FlatMatrix<size_t> EvaluableNodeTreeManipulation::sequenceCommonalityBuffer;
#else
std::vector<uint32_t> EvaluableNodeTreeManipulation::aCharsBuffer;
std::vector<uint32_t> EvaluableNodeTreeManipulation::bCharsBuffer;
FlatMatrix<size_t> EvaluableNodeTreeManipulation::sequenceCommonalityBuffer;
#endif

EvaluableNode EvaluableNodeTreeManipulation::nullEvaluableNode(ENT_NULL);

CompactHashMap<StringInternPool::StringID, double> EvaluableNodeTreeManipulation::mutationOperationTypeProbabilities
{
	{ ENBISI_change_type,		0.28 },
	{ ENBISI_delete,			0.12 },
	{ ENBISI_insert,			0.23 },
	{ ENBISI_swap_elements,		0.24 },
	{ ENBISI_deep_copy_elements,0.05 },
	{ ENBISI_delete_elements,	0.04 },
	{ ENBISI_change_label,		0.04 }
};

EvaluableNodeTreeManipulation::MutationParameters::WeightedRandMutationType  EvaluableNodeTreeManipulation::mutationOperationTypeRandomStream(mutationOperationTypeProbabilities, true);

CompactHashMap<EvaluableNodeType, double> EvaluableNodeTreeManipulation::evaluableNodeTypeProbabilities
{
	//built-in / system specific
	{ENT_SYSTEM,										0.05},
	{ENT_GET_DEFAULTS,									0.01},

	//parsing
	{ENT_PARSE,											0.05},
	{ENT_UNPARSE,										0.05},

	//core control
	{ENT_IF,											1.0},
	{ENT_SEQUENCE,										0.5},
	{ENT_PARALLEL,										0.5},
	{ENT_LAMBDA,										1.5},
	{ENT_CONCLUDE,										0.05},
	{ENT_CALL,											1.5},
	{ENT_CALL_SANDBOXED,								0.25},
	{ENT_WHILE,											0.1},

	//definitions
	{ENT_LET,											0.95},
	{ENT_DECLARE,										0.5},
	{ENT_ASSIGN,										0.95},
	{ENT_ACCUM,											0.25},
	{ENT_RETRIEVE,										0.1},

	//retrieval
	{ENT_GET,											3.0},
	{ENT_SET,											0.35},
	{ENT_REPLACE,										0.1},

	//stack and node manipulation
	{ENT_TARGET,										0.1},
	{ENT_TARGET_INDEX,									0.1},
	{ENT_TARGET_VALUE,									0.1},
	{ENT_STACK,											0.05},
	{ENT_ARGS,											0.08},

	//simulation and operations
	{ENT_RAND,											0.4},
	{ENT_WEIGHTED_RAND,									0.02},
	{ENT_GET_RAND_SEED,									0.02},
	{ENT_SET_RAND_SEED,									0.02},
	{ENT_SYSTEM_TIME,									0.01},

	//base math
	{ENT_ADD,											0.9},
	{ENT_SUBTRACT,										0.65},
	{ENT_MULTIPLY,										0.65},
	{ENT_DIVIDE,										0.6},
	{ENT_MODULUS,										0.2},
	{ENT_GET_DIGITS,									0.1},
	{ENT_SET_DIGITS,									0.1},
	{ENT_FLOOR,											0.6},
	{ENT_CEILING,										0.6},
	{ENT_ROUND,											0.6},

	//extended math
	{ENT_EXPONENT,										0.4},
	{ENT_LOG,											0.4},

	{ENT_SIN,											0.2},
	{ENT_ASIN,											0.2},
	{ENT_COS,											0.2},
	{ENT_ACOS,											0.2},
	{ENT_TAN,											0.2},
	{ENT_ATAN,											0.2},

	{ENT_SINH,											0.07},
	{ENT_ASINH,											0.07},
	{ENT_COSH,											0.07},
	{ENT_ACOSH,											0.07},
	{ENT_TANH,											0.07},
	{ENT_ATANH,											0.07},

	{ENT_ERF,											0.05},
	{ENT_TGAMMA,										0.07},
	{ENT_LGAMMA,										0.07},

	{ENT_SQRT,											0.2},
	{ENT_POW,											0.2},
	{ENT_ABS,											0.4},
	{ENT_MAX,											0.4},
	{ENT_MIN,											0.4},
	{ENT_DOT_PRODUCT,									0.2},
	{ENT_GENERALIZED_DISTANCE,							0.15},

	//list manipulation
	{ENT_FIRST,											0.65},
	{ENT_TAIL,											0.65},
	{ENT_LAST,											0.65},
	{ENT_TRUNC,											0.65},
	{ENT_APPEND,										0.65},
	{ENT_SIZE,											0.6},
	{ENT_RANGE,											0.5},

	//transformation
	{ENT_REWRITE,										0.1},
	{ENT_MAP,											1.1},
	{ENT_FILTER,										0.5},
	{ENT_WEAVE,											0.2},
	{ENT_REDUCE,										0.7},
	{ENT_APPLY,											0.5},
	{ENT_REVERSE,										0.4},
	{ENT_SORT,											0.5},

	//associative list manipulation
	{ENT_INDICES,										0.5},
	{ENT_VALUES,										0.5},
	{ENT_CONTAINS_INDEX,								0.5},
	{ENT_CONTAINS_VALUE,								0.5},
	{ENT_REMOVE,										0.5},
	{ENT_KEEP,											0.5},
	{ENT_ASSOCIATE,										0.8},
	{ENT_ZIP,											0.35},
	{ENT_UNZIP,											0.25},

	//logic
	{ENT_AND,											0.75},
	{ENT_OR,											0.75},
	{ENT_XOR,											0.75},
	{ENT_NOT,											0.75},

	//equivalence
	{ENT_EQUAL,											1.2},
	{ENT_NEQUAL,										0.65},
	{ENT_LESS,											0.85},
	{ENT_LEQUAL,										0.85},
	{ENT_GREATER,										0.85},
	{ENT_GEQUAL,										0.85},
	{ENT_TYPE_EQUALS,									0.1},
	{ENT_TYPE_NEQUALS,									0.1},

	//built-in constants and variables
	{ENT_TRUE,											0.1},
	{ENT_FALSE,											0.1},
	{ENT_NULL,											0.75},

	//data types
	{ENT_LIST,											2.5},
	{ENT_ASSOC,											3.0},
	{ENT_NUMBER,										8.0},
	{ENT_STRING,										4.0},
	{ENT_SYMBOL,										25.0},

	//node types
	{ENT_GET_TYPE,										0.25},
	{ENT_GET_TYPE_STRING,								0.25},
	{ENT_SET_TYPE,										0.35},
	{ENT_FORMAT,										0.05},

	//labels and comments
	{ENT_GET_LABELS,									0.1},
	{ENT_GET_ALL_LABELS,								0.05},
	{ENT_SET_LABELS,									0.1},
	{ENT_ZIP_LABELS,									0.02},

	{ENT_GET_COMMENTS,									0.05},
	{ENT_SET_COMMENTS,									0.05},

	{ENT_GET_CONCURRENCY,								0.01},
	{ENT_SET_CONCURRENCY,								0.01},

	{ENT_GET_VALUE,										0.15},
	{ENT_SET_VALUE,										0.15},

	//string
	{ENT_EXPLODE,										0.02},
	{ENT_SPLIT,											0.2},
	{ENT_SUBSTR,										0.2},
	{ENT_CONCAT,										0.2},

	//encryption
	{ENT_CRYPTO_SIGN,									0.01},
	{ENT_CRYPTO_SIGN_VERIFY,							0.01},
	{ENT_ENCRYPT,										0.01},
	{ENT_DECRYPT,										0.01},

	//I/O
	{ENT_PRINT,											0.01},

	//tree merging
	{ENT_TOTAL_SIZE,									0.2},
	{ENT_MUTATE,										0.2},
	{ENT_COMMONALITY,									0.2},
	{ENT_EDIT_DISTANCE,									0.2},
	{ENT_INTERSECT,										0.2},
	{ENT_UNION,											0.2},
	{ENT_DIFFERENCE,									0.2},
	{ENT_MIX,											0.2},
	{ENT_MIX_LABELS,									0.2},

	//entity merging
	{ENT_TOTAL_ENTITY_SIZE,								0.02},
	{ENT_FLATTEN_ENTITY,								0.02},
	{ENT_MUTATE_ENTITY,									0.02},
	{ENT_COMMONALITY_ENTITIES,							0.02},
	{ENT_EDIT_DISTANCE_ENTITIES,						0.02},
	{ENT_INTERSECT_ENTITIES,							0.02},
	{ENT_UNION_ENTITIES,								0.02},
	{ENT_DIFFERENCE_ENTITIES,							0.02},
	{ENT_MIX_ENTITIES,									0.02},

	//entity details
	{ENT_GET_ENTITY_COMMENTS,							0.01},
	{ENT_RETRIEVE_ENTITY_ROOT,							0.01},
	{ENT_ASSIGN_ENTITY_ROOTS,							0.01},
	{ENT_ACCUM_ENTITY_ROOTS,							0.01},
	{ENT_GET_ENTITY_RAND_SEED,							0.01},
	{ENT_SET_ENTITY_RAND_SEED,							0.01},
	{ENT_GET_ENTITY_ROOT_PERMISSION,					0.01},
	{ENT_SET_ENTITY_ROOT_PERMISSION,					0.01},

	//entity base actions
	{ENT_CREATE_ENTITIES,								0.1},
	{ENT_CLONE_ENTITIES,								0.1},
	{ENT_MOVE_ENTITIES,									0.15},
	{ENT_DESTROY_ENTITIES,								0.1},
	{ENT_LOAD,											0.01},
	{ENT_LOAD_ENTITY,									0.01},
	{ENT_LOAD_PERSISTENT_ENTITY,						0.01},
	{ENT_STORE,											0.01},
	{ENT_STORE_ENTITY,									0.01},
	{ENT_CONTAINS_ENTITY,								0.1},

	//entity query
	{ENT_CONTAINED_ENTITIES,							0.3},
	{ENT_COMPUTE_ON_CONTAINED_ENTITIES,					0.3},
	{ENT_QUERY_SELECT,									0.2},
	{ENT_QUERY_SAMPLE,									0.2},
	{ENT_QUERY_WEIGHTED_SAMPLE,							0.2},
	{ENT_QUERY_IN_ENTITY_LIST,							0.2},
	{ENT_QUERY_NOT_IN_ENTITY_LIST,						0.2},
	{ENT_QUERY_COUNT,									0.2},
	{ENT_QUERY_EXISTS,									0.2},
	{ENT_QUERY_NOT_EXISTS,								0.2},
	{ENT_QUERY_EQUALS,									0.2},
	{ENT_QUERY_NOT_EQUALS,								0.2},
	{ENT_QUERY_BETWEEN,									0.2},
	{ENT_QUERY_NOT_BETWEEN,								0.2},
	{ENT_QUERY_AMONG,									0.2},
	{ENT_QUERY_NOT_AMONG,								0.2},
	{ENT_QUERY_MAX,										0.2},
	{ENT_QUERY_MIN,										0.2},
	{ENT_QUERY_SUM,										0.2},
	{ENT_QUERY_MODE,									0.2},
	{ENT_QUERY_QUANTILE,								0.2},
	{ENT_QUERY_GENERALIZED_MEAN,						0.2},
	{ENT_QUERY_MIN_DIFFERENCE,							0.2},
	{ENT_QUERY_MAX_DIFFERENCE,							0.2},
	{ENT_QUERY_VALUE_MASSES,							0.2},
	{ENT_QUERY_GREATER_OR_EQUAL_TO,						0.2},
	{ENT_QUERY_LESS_OR_EQUAL_TO,						0.2},
	{ENT_QUERY_WITHIN_GENERALIZED_DISTANCE,				0.2},
	{ENT_QUERY_NEAREST_GENERALIZED_DISTANCE,			0.2},

	{ENT_COMPUTE_ENTITY_CONVICTIONS,					0.2},
	{ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE,				0.2},
	{ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS,			0.2},
	{ENT_COMPUTE_ENTITY_KL_DIVERGENCES,					0.2},

	//entity access
	{ENT_CONTAINS_LABEL,								0.5},
	{ENT_ASSIGN_TO_ENTITIES,							0.5},
	{ENT_DIRECT_ASSIGN_TO_ENTITIES,						0.01},
	{ENT_ACCUM_TO_ENTITIES,								0.5},
	{ENT_RETRIEVE_FROM_ENTITY,							0.5},
	{ENT_DIRECT_RETRIEVE_FROM_ENTITY,					0.01},
	{ENT_CALL_ENTITY,									0.5},
	{ENT_CALL_ENTITY_GET_CHANGES,						0.05},
	{ENT_CALL_CONTAINER,								0.5}
};

EvaluableNodeTreeManipulation::MutationParameters::WeightedRandEvaluableNodeType EvaluableNodeTreeManipulation::evaluableNodeTypeRandomStream(evaluableNodeTypeProbabilities, true);
