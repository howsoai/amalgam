//project headers:
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNode.h"
#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "Interpreter.h"
#include "Merger.h"
#include "OpcodeDetails.h"

//system headers:
#include <cmath>
#include <iterator>

EvaluableNodeTreeManipulation::NodesMixMethod::NodesMixMethod(RandomStream random_stream, EvaluableNodeManager *_enm,
	double fraction_a, double fraction_b, double similar_mix_chance,
	bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching)
	: NodesMergeMethod(_enm, true, types_must_match, nominal_numbers, nominal_strings, recursive_matching)
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

	recursiveMatching = recursive_matching;
}

//returns a mix of a and b based on their fractions
static inline double MixNumberValues(double a, double b, double fraction_a, double fraction_b)
{
	//quick exit for when they match
	if(a == b)
		return a;

	//normalize fractions
	fraction_a = fraction_a / (fraction_a + fraction_b);
	return a * fraction_a + b * (1 - fraction_a);
}

//returns a mix of a and b based on their fractions
static inline StringInternPool::StringID MixStringValues(StringInternPool::StringID a, StringInternPool::StringID b,
	RandomStream random_stream, double fraction_a, double fraction_b)
{
	//quick exit for when they match
	if(a == b)
		return string_intern_pool.CreateStringReference(a);

	if(a == StringInternPool::NOT_A_STRING_ID)
		return string_intern_pool.CreateStringReference(b);

	if(b == StringInternPool::NOT_A_STRING_ID)
		return string_intern_pool.CreateStringReference(a);

	auto &a_str = string_intern_pool.GetStringFromID(a);
	auto &b_str = string_intern_pool.GetStringFromID(b);
	std::string result = EvaluableNodeTreeManipulation::MixStrings(a_str, b_str,
		random_stream, fraction_a, fraction_b);

	return string_intern_pool.CreateStringReference(result);
}

EvaluableNode *EvaluableNodeTreeManipulation::NodesMixMethod::MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge)
{
	//early out
	if(a == nullptr && b == nullptr)
		return nullptr;

	EvaluableNode *merged = nullptr;
	if(AreMergeable(a, b) || must_merge)
	{
		merged = MergeTrees(this, a, b);

		//if the original and merged, check to see if mergeable of same type,
		// and if so and similarMixChance is large enough, interpolate
		if(merged != nullptr && a != nullptr && b != nullptr && similarMixChance > 0)
		{
			if(merged->IsNumericOrNull() && a->IsNumericOrNull() && b->IsNumericOrNull())
			{
				if(randomStream.Rand() < similarMixChance)
				{
					double a_value = a->GetNumberValue();
					double b_value = b->GetNumberValue();
					double mixed_value = MixNumberValues(a_value, b_value, fractionA, fractionB);
					merged->SetTypeViaNumberValue(mixed_value);
				}
			}
			else if(merged->GetType() == ENT_STRING && a->GetType() == ENT_STRING && b->GetType() == ENT_STRING)
			{
				if(randomStream.Rand() < similarMixChance)
				{
					auto a_value = a->GetStringIDReference();
					auto b_value = b->GetStringIDReference();
					auto mixed_value = MixStringValues(a_value, b_value,
						randomStream.CreateOtherStreamViaRand(), fractionA, fractionB);
					merged->SetStringIDWithReferenceHandoff(mixed_value);
				}
			}
		}
	}
	else if(KeepNonMergeableAInsteadOfB())
	{
		merged = MergeTrees(this, a, nullptr);
	}
	else
	{
		merged = MergeTrees(this, nullptr, b);
	}

	return merged;
}

bool EvaluableNodeTreeManipulation::NodesMixMethod::AreMergeable(EvaluableNode *a, EvaluableNode *b)
{
	auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(a, b, TypesMustMatch(), NominalNumbers(), NominalStrings());

	//if the immediate nodes are in fact a match, then just merge them
	if(commonality == 1.0)
		return true;

	double prob_of_match = commonality;
	if(commonality > 0)
	{
		if(similarMixChance > 0.0)
		{
			//probability of match is commonality OR similarMixChance
			// however, these are not mutually exclusive, so need to remove the conjunction of the
			// probability of both to prevent double-counting
			prob_of_match = commonality + similarMixChance - commonality * similarMixChance;
		}
		else if(similarMixChance < 0)
		{
			//probability of match is commonality AND not (negative similarMixChance)
			// because similarMixChance is negative, adding to 1 is the same as NOT
			prob_of_match = commonality * (1.0 + similarMixChance);
		}
		//else 0.0 or NaN, just leave as overall_commonality
	}

	return randomStream.Rand() < prob_of_match;
}

MergeMetricResults<std::string *> EvaluableNodeTreeManipulation::StringSequenceMergeMetric::MergeMetric(std::string *a, std::string *b)
{
	if(a == b || (a != nullptr && b != nullptr && *a == *b))
		return MergeMetricResults(1.0, a, b, false, true);
	else
		return MergeMetricResults(0.0, a, b, false, false);
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

static std::string MergeMultilineStrings(EvaluableNodeTreeManipulation::NodesMergeMethod *mm,
	std::string_view string_1, std::string_view string_2)
{
	//convert from vectors of strings to vectors of pointers to strings so can merge on them
	auto n1_strings = StringManipulation::SplitByLines(string_1);
	std::vector<std::string *> n1_comment_string_ptrs(n1_strings.size());
	for(size_t i = 0; i < n1_strings.size(); i++)
		n1_comment_string_ptrs[i] = &n1_strings[i];

	auto n2_strings = StringManipulation::SplitByLines(string_2);
	std::vector<std::string *> n2_comment_string_ptrs(n2_strings.size());
	for(size_t i = 0; i < n2_strings.size(); i++)
		n2_comment_string_ptrs[i] = &n2_strings[i];

	EvaluableNodeTreeManipulation::StringSequenceMergeMetric ssmm(mm->KeepSomeNonMergeableValues());
	auto merged_lines = ssmm.MergeSequences(n1_comment_string_ptrs, n2_comment_string_ptrs);

	//append back to one string
	std::string merged_string;
	for(auto &line : merged_lines)
	{
		//if already have comments, append a newline
		if(merged_string.size() > 0)
			merged_string.append("\r\n");
		merged_string.append(*line);
	}

	return merged_string;
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

	auto [node, commonality] = EvaluableNodeTreeManipulation::CommonalityBetweenNodeTypesAndValues(n1, n2,
		mm->TypesMustMatch(), mm->NominalNumbers(), mm->NominalStrings());

	//if both are nullptr, nothing more to do
	if(node == nullptr)
		return nullptr;

	auto common_type = node->GetType();

	//see if need exact commonality based on the type
	if(mm->TypesMustMatch() && commonality != 1.0)
	{
		if(common_type == ENT_NUMBER)
		{
			if(mm->NominalNumbers())
				return nullptr;
		}
		else if(common_type == ENT_STRING)
		{
			if(mm->NominalStrings())
				return nullptr;
		}
		else //anything other type must match exactly
		{
			return nullptr;
		}
	}

	//make a new copy of it
	EvaluableNode *n = enm->AllocNode(common_type);

	//if immediate, copy value
	if(DoesEvaluableNodeTypeUseNumberData(common_type))
		n->SetTypeViaNumberValue(node->GetNumberValue());
	else if(DoesEvaluableNodeTypeUseStringData(common_type))
		n->SetStringID(node->GetStringID());

	//if either is a null, then handle logic below by creating a null EvaluableNode
	EvaluableNode null_merge_node(ENT_NULL);
	if(n1 == nullptr)
		n1 = &null_merge_node;
	if(n2 == nullptr)
		n2 = &null_merge_node;

	auto [n1_annotations, n1_comments] = n1->GetAnnotationsAndCommentsStorage().GetAnnotationsAndComments();
	auto [n2_annotations, n2_comments] = n2->GetAnnotationsAndCommentsStorage().GetAnnotationsAndComments();

	//merge annotations and comments if they exist
	if(!n1_annotations.empty() || !n2_annotations.empty())
	{
		auto merged_string = MergeMultilineStrings(mm, n1_annotations, n2_annotations);
		n->SetCommentsString(merged_string);
	}

	if(!n1_comments.empty() || !n2_comments.empty())
	{
		auto merged_string = MergeMultilineStrings(mm, n1->GetCommentsString(), n2->GetCommentsString());
		n->SetCommentsString(merged_string);
	}

	return n;
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
	else if((tree1 != best_shared_nodes_match.elementA && mm->KeepNonMergeableA())
		|| (tree2 != best_shared_nodes_match.elementB && mm->KeepNonMergeableB()))
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
	if(generalized_node->IsAssociativeArray() &&
		(EvaluableNode::IsAssociativeArray(tree1) || EvaluableNode::IsAssociativeArray(tree2)))
	{
		//get or convert the nodes to an assoc for tree1
		EvaluableNode::AssocType tree1_conversion_assoc;
		auto *tree1_mapped_childs = &tree1_conversion_assoc;
		if(EvaluableNode::IsAssociativeArray(tree1))
			tree1_mapped_childs = &tree1->GetMappedChildNodesReference();

		//get or convert the nodes to an assoc for tree2
		EvaluableNode::AssocType tree2_conversion_assoc;
		auto *tree2_mapped_childs = &tree2_conversion_assoc;
		if(EvaluableNode::IsAssociativeArray(tree2))
			tree2_mapped_childs = &tree2->GetMappedChildNodesReference();

		EvaluableNode::AssocType merged = mm->MergeMaps(*tree1_mapped_childs, *tree2_mapped_childs);
		//hand off merged allocation into the generalized_node (hence the false parameter)
		generalized_node->SetMappedChildNodes(merged, false);

		return generalized_node;
	}

	EvaluableNode::OrderedType empty_vector;

	auto *tree1_ordered_childs = &empty_vector;
	if(tree1 != nullptr && tree1->IsOrderedArray())
		tree1_ordered_childs = &tree1->GetOrderedChildNodesReference();

	auto *tree2_ordered_childs = &empty_vector;
	if(tree2 != nullptr && tree2->IsOrderedArray())
		tree2_ordered_childs = &tree2->GetOrderedChildNodesReference();

	//see if both trees have ordered child nodes
	if(tree1_ordered_childs->size() > 0 || tree2_ordered_childs->size() > 0)
	{
		auto iocnt = GetOpcodeOrderedChildNodeType(generalized_node->GetType());
		switch(iocnt)
		{
		case OpcodeDetails::OrderedChildNodeType::NONE:
			break;
		case OpcodeDetails::OrderedChildNodeType::UNORDERED:
			generalized_node->SetOrderedChildNodes(std::move(mm->MergeUnorderedSets(*tree1_ordered_childs, *tree2_ordered_childs)));
			break;

		case OpcodeDetails::OrderedChildNodeType::ORDERED:
			generalized_node->SetOrderedChildNodes(std::move(mm->MergeSequences(*tree1_ordered_childs, *tree2_ordered_childs)));
			break;

		case OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED:
		case OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED:
		{
			//start from a clean slate
			generalized_node->ClearOrderedChildNodes();

			//make arrays of just the first node
			EvaluableNode::OrderedType a1;
			EvaluableNode::OrderedType a2;
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
			if(iocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED)
				merged = mm->MergeSequences(a1, a2);
			else if(iocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED)
				merged = mm->MergeUnorderedSetsOfPairs(a1, a2);
			generalized_node->GetOrderedChildNodes().insert(end(generalized_node->GetOrderedChildNodes()), begin(merged), end(merged));
			break;
		}

		case OpcodeDetails::OrderedChildNodeType::PAIRED:
			generalized_node->SetOrderedChildNodes(std::move(mm->MergeUnorderedSetsOfPairs(*tree1_ordered_childs, *tree2_ordered_childs)));
			break;

		case OpcodeDetails::OrderedChildNodeType::POSITION:
			generalized_node->SetOrderedChildNodes(std::move(mm->MergePositions(*tree1_ordered_childs, *tree2_ordered_childs)));
			break;

		default:
			break;
		}
	}

	return generalized_node;
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateTree(Interpreter *interpreter, EvaluableNodeManager *enm,
	EvaluableNode *tree, double mutation_rate,
	CompactHashMap<EvaluableNodeBuiltInStringId, double> *mutation_weights,
	CompactHashMap<EvaluableNodeType, double> *evaluable_node_weights, size_t preserve_type_depth,
	EvaluableNodeTreeManipulation::MutationParameters::WeightedRandValueType &imm_number_weights,
	EvaluableNodeTreeManipulation::MutationParameters::WeightedRandValueType &imm_string_weights)
{
	std::vector<std::string> strings;
	EvaluableNode::ReferenceSetType checked;
	GetStringsFromTree(tree, strings, checked);

	//initializes on first call, 
	static MutationParameters::WeightedRandEvaluableNodeType default_operation_type_wrs(
		_opcode_details, true,
		[](auto &e, size_t i)
		{
			return static_cast<EvaluableNodeType>(i);
		},
		[](auto &e) { return e.frequencyPer10000Opcodes; }
	);

	MutationParameters::WeightedRandEvaluableNodeType operation_type_wrs;
	if(evaluable_node_weights != nullptr && !evaluable_node_weights->empty())
		operation_type_wrs.Initialize(*evaluable_node_weights, true);

	MutationParameters::WeightedRandMutationType rand_mutation_type;
	if(mutation_weights != nullptr && !mutation_weights->empty())
		rand_mutation_type.Initialize(*mutation_weights, true);

	MutationParameters mp(interpreter, enm, mutation_rate, &strings,
		operation_type_wrs.IsInitialized() ? &operation_type_wrs : &default_operation_type_wrs,
		rand_mutation_type.IsInitialized() ? &rand_mutation_type : &mutationOperationTypeRandomStream,
		preserve_type_depth, imm_number_weights, imm_string_weights);
	EvaluableNode *ret = MutateTree(mp, tree, 0);

	return ret;
}

MergeMetricResults<EvaluableNode *> EvaluableNodeTreeManipulation::NumberOfSharedNodes(
	EvaluableNode *tree1, EvaluableNode *tree2, MergeMetricResultsParams &mmrp)
{
	if(tree1 == nullptr && tree2 == nullptr)
		return MergeMetricResults(1.0, tree1, tree2, false, true);

	//if the pair of nodes has already been computed, then just return the result
	auto [memoized_entry, new_memoization_inserted] =
		mmrp.memoizedNodeMergePairs.emplace(std::make_pair(tree1, tree2),
			MergeMetricResults<EvaluableNode *>());
	if(!new_memoization_inserted)
		return memoized_entry->second;

	if(mmrp.checked != nullptr)
	{
		//if either is already checked, then neither adds shared nodes
		if(mmrp.checked->find(tree1) != end(*(mmrp.checked)) || mmrp.checked->find(tree2) != end(*(mmrp.checked)))
			return MergeMetricResults(0.0, tree1, tree2, false, true);
	}

	//if the trees are the same, then just return the size
	if(tree1 == tree2)
	{
		MergeMetricResults results(static_cast<double>(EvaluableNode::GetDeepSize(tree1)), tree1, tree2, true, true);
		memoized_entry->second = results;
		return results;
	}

	//check current top nodes
	auto commonality = CommonalityBetweenNodes(tree1, tree2, mmrp.typesMustMatch, mmrp.nominalNumbers, mmrp.nominalStrings);

	//see if can exit early, before inserting the nodes into the checked list and then removing them
	size_t tree1_ordered_nodes_size = 0;
	size_t tree1_mapped_nodes_size = 0;
	size_t tree2_ordered_nodes_size = 0;
	size_t tree2_mapped_nodes_size = 0;

	if(EvaluableNode::IsAssociativeArray(tree1))
		tree1_mapped_nodes_size = tree1->GetMappedChildNodesReference().size();
	else if(EvaluableNode::IsOrderedArray(tree1))
		tree1_ordered_nodes_size = tree1->GetOrderedChildNodesReference().size();

	if(EvaluableNode::IsAssociativeArray(tree2))
		tree2_mapped_nodes_size = tree2->GetMappedChildNodesReference().size();
	else if(EvaluableNode::IsOrderedArray(tree2))
		tree2_ordered_nodes_size = tree2->GetOrderedChildNodesReference().size();

	if(tree1_ordered_nodes_size == 0 && tree2_ordered_nodes_size == 0
		&& tree1_mapped_nodes_size == 0 && tree2_mapped_nodes_size == 0)
	{
		memoized_entry->second = commonality;
		return commonality;
	}

	//note that memoized_entry may be invalidated for subsequent calls to emplace because memoizedNodeMergePairs
	//may be a flat hash map

	if(mmrp.checked != nullptr)
	{
		//remember that it has already checked when traversing tree, and then remove from checked at the end of the function
		mmrp.checked->emplace(tree1);
		mmrp.checked->emplace(tree2);
	}

	if(tree1_ordered_nodes_size > 0 && tree2_ordered_nodes_size > 0)
	{
		auto iocnt = GetOpcodeOrderedChildNodeType(tree1->GetType());

		//if there's only one node in each, then just use OpcodeDetails::OrderedChildNodeType::POSITION because
		// it's more efficient and the pairing doesn't matter
		if(tree1_ordered_nodes_size < 2 && tree2_ordered_nodes_size < 2)
			iocnt = OpcodeDetails::OrderedChildNodeType::POSITION;

		switch(iocnt)
		{
		case OpcodeDetails::OrderedChildNodeType::NONE:
			break;

		case OpcodeDetails::OrderedChildNodeType::UNORDERED:
		{
			EvaluableNode::OrderedType a2(tree2->GetOrderedChildNodesReference());

			//for every element in a1, check to see if there's any in a2
			for(auto &a1_current : tree1->GetOrderedChildNodesReference())
			{
				//find the node that best matches this one, greedily
				bool best_match_found = false;
				size_t best_match_index = 0;
				MergeMetricResults best_match_value(0.0, tree1, tree2, false, false);
				for(size_t match_index = 0; match_index < a2.size(); match_index++)
				{
					auto match_value = NumberOfSharedNodes(a1_current, a2[match_index], mmrp);
					//if either an exact match
					// or it doesn't need an exact match and it's a better match than previously found
					if(match_value.exactMatch
						|| (!match_value.mustMatch && (!best_match_found || match_value > best_match_value)))
					{
						best_match_found = true;
						best_match_value = match_value;
						best_match_index = match_index;

						if(best_match_value.exactMatch)
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

		case OpcodeDetails::OrderedChildNodeType::ORDERED:
		case OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED:
		{
			auto &ocn1 = tree1->GetOrderedChildNodesReference();
			auto &ocn2 = tree2->GetOrderedChildNodesReference();
			auto size1 = ocn1.size();
			auto size2 = ocn2.size();

			size_t starting_index = 0;

			if(iocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_ORDERED)
			{
				auto smallest_list_size = std::min(size1, size2);
				if(smallest_list_size >= 1)
					commonality += NumberOfSharedNodes(ocn1[0], ocn2[0], mmrp);

				starting_index = 1;
			}

			FlatMatrix<MergeMetricResults<EvaluableNode *>> sequence_commonality;
			ComputeSequenceCommonalityMatrix(sequence_commonality, ocn1, ocn2,
				[&mmrp]
				(EvaluableNode *a, EvaluableNode *b)
				{
					return EvaluableNodeTreeManipulation::NumberOfSharedNodes(a, b, mmrp);
				}, starting_index);

			commonality += sequence_commonality.At(size1, size2);
			break;
		}

		case OpcodeDetails::OrderedChildNodeType::PAIRED:
		case OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED:
		{
			EvaluableNode::OrderedType a1(tree1->GetOrderedChildNodesReference());
			EvaluableNode::OrderedType a2(tree2->GetOrderedChildNodesReference());

			if(iocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED)
			{
				auto smallest_list_size = std::min(a1.size(), a2.size());
				if(smallest_list_size >= 1)
				{
					commonality += NumberOfSharedNodes(a1[0], a2[0], mmrp);

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
					auto match_key = NumberOfSharedNodes(a1[0], a2[match_index], mmrp);

					// key match dominates value match
					if(!best_match_found || match_key > best_match_key)
					{
						best_match_found = true;
						best_match_key = match_key;
						best_match_index = match_index;

						//count the value node commonality as long as it exists and is nontrivial
						if(match_key.IsNontrivialMatch() && a1.size() > 1 && a2.size() > match_index + 1)
							best_match_value = NumberOfSharedNodes(a1[1], a2[match_index + 1], mmrp);
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

		case OpcodeDetails::OrderedChildNodeType::POSITION:
		{
			auto &ocn1 = tree1->GetOrderedChildNodesReference();
			auto &ocn2 = tree2->GetOrderedChildNodesReference();
			//use size of smallest list
			auto smallest_list_size = std::min(ocn1.size(), ocn2.size());
			for(size_t i = 0; i < smallest_list_size; i++)
				commonality += NumberOfSharedNodes(ocn1[i], ocn2[i], mmrp);

			break;
		}

		}
	}
	else if(tree1_mapped_nodes_size > 0 && tree2_mapped_nodes_size > 0)
	{
		//use keys from first node
		auto &tree_2_mcn = tree2->GetMappedChildNodesReference();
		for(auto &[node_id, node] : tree1->GetMappedChildNodesReference())
		{
			//skip unless both trees have the key
			auto other_node = tree_2_mcn.find(node_id);
			if(other_node == end(tree_2_mcn))
				continue;

			commonality += NumberOfSharedNodes(node, other_node->second, mmrp);
		}
	}

	if(mmrp.recursiveMatching)
	{
		//if not exact match of nodes and all child nodes, then check all child nodes for better submatches
		if(!commonality.exactMatch)
		{
			if(tree1_ordered_nodes_size > 0)
			{
				for(auto node : tree1->GetOrderedChildNodesReference())
				{
					auto sub_match = NumberOfSharedNodes(node, tree2, mmrp);

					//mark as nonexact match because had to traverse downward,
					// but preserve whether was an exact match for early stopping
					bool exact_match = sub_match.exactMatch;
					sub_match.exactMatch = false;
					if(sub_match > commonality)
					{
						commonality = sub_match;
						mmrp.memoizedNodeMergePairs.insert_or_assign(std::make_pair(node, tree2), commonality);
						if(exact_match)
							break;
					}
				}
			}
			else if(tree1_mapped_nodes_size > 0)
			{
				for(auto &[node_id, node] : tree1->GetMappedChildNodesReference())
				{
					auto sub_match = NumberOfSharedNodes(node, tree2, mmrp);

					//mark as nonexact match because had to traverse downward,
					// but preserve whether was an exact match for early stopping
					bool exact_match = sub_match.exactMatch;
					sub_match.exactMatch = false;
					if(sub_match > commonality)
					{
						commonality = sub_match;
						mmrp.memoizedNodeMergePairs.insert_or_assign(std::make_pair(node, tree2), commonality);
						if(exact_match)
							break;
					}
				}
			}
		}

		//check again for commonality in case exact match was found by iterating via tree1 above
		if(!commonality.exactMatch)
		{
			if(tree2_ordered_nodes_size > 0)
			{
				for(auto node : tree2->GetOrderedChildNodesReference())
				{
					auto sub_match = NumberOfSharedNodes(tree1, node, mmrp);

					//mark as nonexact match because had to traverse downward,
					// but preserve whether was an exact match for early stopping
					bool exact_match = sub_match.exactMatch;
					sub_match.exactMatch = false;
					if(sub_match > commonality)
					{
						commonality = sub_match;
						mmrp.memoizedNodeMergePairs.insert_or_assign(std::make_pair(tree1, node), commonality);
						if(exact_match)
							break;
					}
				}
			}
			else if(tree2_mapped_nodes_size > 0)
			{
				for(auto &[node_id, node] : tree2->GetMappedChildNodesReference())
				{
					auto sub_match = NumberOfSharedNodes(tree1, node, mmrp);

					//mark as nonexact match because had to traverse downward,
					// but preserve whether was an exact match for early stopping
					bool exact_match = sub_match.exactMatch;
					sub_match.exactMatch = false;
					if(sub_match > commonality)
					{
						commonality = sub_match;
						mmrp.memoizedNodeMergePairs.insert_or_assign(std::make_pair(tree1, node), commonality);
						if(exact_match)
							break;
					}
				}
			}
		}
	}

	if(mmrp.checked != nullptr)
	{
		//remove from the checked list so don't block other traversals
		mmrp.checked->erase(tree1);
		mmrp.checked->erase(tree2);
	}

	mmrp.memoizedNodeMergePairs.insert_or_assign(std::make_pair(tree1, tree2), commonality);
	return commonality;
}

MergeMetricResults<EvaluableNode *> EvaluableNodeTreeManipulation::CommonalityBetweenNodes(
	EvaluableNode *n1, EvaluableNode *n2,
	bool types_must_match, bool nominal_numbers, bool nominal_strings)
{
	if(n1 == nullptr && n2 == nullptr)
		return MergeMetricResults(1.0, n1, n2, false, true);

	auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(n1, n2, types_must_match, nominal_numbers, nominal_strings);
	return MergeMetricResults(commonality, n1, n2, types_must_match, commonality == 1.0);
}

std::pair<EvaluableNode *, double> EvaluableNodeTreeManipulation::CommonalityBetweenNodeTypesAndValues(
	EvaluableNode *n1, EvaluableNode *n2,
	bool types_must_match, bool nominal_numbers, bool nominal_strings)
{
	//if either is nullptr, then use an actual EvaluableNode
	if(n1 == nullptr)
		n1 = &nullEvaluableNode;
	if(n2 == nullptr)
		n2 = &nullEvaluableNode;

	auto n1_type = n1->GetType();
	auto n2_type = n2->GetType();

	if(types_must_match && n1_type != n2_type)
		return std::make_pair(n1, 0.0);

	//if types are the same, need special handling for immediates, otherwise return true
	if(n1_type == n2_type)
	{
		if(n1_type == ENT_BOOL)
		{
			bool n1_value = n1->GetBoolValueReference();
			bool n2_value = n2->GetBoolValueReference();
			return std::make_pair(n1, n1_value == n2_value ? 1.0 : (types_must_match ? 0.0 : 0.125));
		}

		if(n1_type == ENT_NUMBER)
		{
			double n1_value = n1->GetNumberValueReference();
			double n2_value = n2->GetNumberValueReference();

			if(nominal_numbers)
			{
				return std::make_pair(n1, n1_value == n2_value ? 1.0 : (types_must_match ? 0.0 : 0.125));
			}
			else
			{
				double commonality = CommonalityBetweenNumbers(n1_value, n2_value);
				double commonality_including_type = std::min(0.125 + 0.875 * commonality, 1.0);
				return std::make_pair(n1, commonality_including_type);
			}
		}

		if(n1_type == ENT_STRING)
		{
			if(nominal_strings)
			{
				auto n1_sid = n1->GetStringIDReference();
				auto n2_sid = n2->GetStringIDReference();
				return std::make_pair(n1, n1_sid == n2_sid ? 1.0 : (types_must_match ? 0.0 : 0.125));
			}
			else
			{
				auto n1sid = n1->GetStringIDReference();
				auto n2sid = n2->GetStringIDReference();
				double commonality = RelativeCommonalityBetweenStrings(n1sid, n2sid);
				double commonality_including_type = std::min(0.125 + 0.875 * commonality, 1.0);
				return std::make_pair(n1, commonality_including_type);
			}
		}

		if(n1_type == ENT_SYMBOL)
		{
			bool match = (n1->GetStringIDReference() == n2->GetStringIDReference());
			return std::make_pair(n1, match ? 1.0 : (types_must_match ? 0.0 : 0.125));
		}
		//same type but not immediate
		return std::make_pair(n1, 1.0);
	}

	//compare similar types that are not the same, or types that have immediate comparisons
	//if the types are the same, it'll be caught below
	switch(n1_type)
	{
	case ENT_SEQUENCE:
		if(n2_type == ENT_UNORDERED_LIST)		return std::make_pair(n1, 0.125);
		if(n2_type == ENT_NULL)					return std::make_pair(n2, 0.125);
		if(n2_type == ENT_LIST)					return std::make_pair(n2, 0.125);
		return std::make_pair(nullptr, 0.0);

	case ENT_LET:
		if(n2_type == ENT_DECLARE)				return std::make_pair(n2, 0.25);
		return std::make_pair(nullptr, 0.0);

	case ENT_DECLARE:
		if(n2_type == ENT_LET)					return std::make_pair(n1, 0.25);
		return std::make_pair(nullptr, 0.0);

	case ENT_REDUCE:
		if(n2_type == ENT_APPLY)				return std::make_pair(n1, 0.125);
		return std::make_pair(nullptr, 0.0);

	case ENT_APPLY:
		if(n2_type == ENT_REDUCE)				return std::make_pair(n2, 0.125);
		return std::make_pair(nullptr, 0.0);

	case ENT_ASSOC:
		if(n2_type == ENT_ASSOCIATE)			return std::make_pair(n1, 0.25);
		return std::make_pair(nullptr, 0.0);

	case ENT_ASSOCIATE:
		if(n2_type == ENT_ASSOC)				return std::make_pair(n2, 0.25);
		return std::make_pair(nullptr, 0.0);

	case ENT_BOOL:
	{
		bool n1_value = n1->GetBoolValueReference();

		if(n2_type == ENT_NULL)
			return std::make_pair(n2, n1_value ? 0.125 : 0.25);

		if(n2_type == ENT_NUMBER)
		{
			double n2_value = n2->GetNumberValueReference();
			bool n2_as_bool = (n2_value != 0.0);
			if(n1_value == n2_as_bool)
				return std::make_pair(n2, 0.25);
		}
		else if(n2_type == ENT_STRING)
		{
			auto &n2_value = n2->GetStringValue();
			bool n2_as_bool = (n2_value != "");
			if(n1_value == n2_as_bool)
				return std::make_pair(n2, 0.25);
		}

		return std::make_pair(nullptr, 0.0);
	}

	case ENT_NULL:
		if(n2_type == ENT_BOOL)
		{
			bool n2_value = n2->GetBoolValueReference();
			if(n2_value)
				return std::make_pair(n1, 0.125);
			return std::make_pair(n1, 0.25);
		}
		if(n2_type == ENT_NUMBER)
		{
			double n2_value = n2->GetNumberValueReference();
			if(n2_value == 0.0)
				return std::make_pair(n2, 0.25);
			return std::make_pair(n2, 0.125);
		}
		if(n2_type == ENT_STRING)
		{
			auto &n2_value = n2->GetStringValue();
			return std::make_pair(n2, n2_value == "" ? 0.25 : 0.125);
		}
		return std::make_pair(nullptr, 0.0);

	case ENT_LIST:
		if(n2_type == ENT_SEQUENCE)			return std::make_pair(n1, 0.125);
		if(n2_type == ENT_UNORDERED_LIST)	return std::make_pair(n1, 0.5);
		return std::make_pair(nullptr, 0.0);

	case ENT_UNORDERED_LIST:
		if(n2_type == ENT_SEQUENCE)				return std::make_pair(n2, 0.125);
		if(n2_type == ENT_LIST)					return std::make_pair(n2, 0.5);
		return std::make_pair(nullptr, 0.0);

	case ENT_NUMBER:
	{
		double n1_value = n1->GetNumberValueReference();

		if(n2_type == ENT_NULL)
		{
			if(n1_value == 0.0)
				return std::make_pair(n1, 0.25);
			return std::make_pair(n1, 0.125);
		}
		else if(n2_type == ENT_BOOL)
		{
			bool n1_as_bool = (n1_value != 0.0);
			bool n2_value = n2->GetBoolValueReference();
			if(n1_as_bool == n2_value)
				return std::make_pair(n2, 0.25);
		}
		else if(n2_type == ENT_RAND)
		{
			return std::make_pair(n1, 0.125);
		}

		return std::make_pair(nullptr, 0.0);
	}

	case ENT_STRING:
	{
		auto &n1_value = n1->GetStringValue();

		if(n2_type == ENT_NULL)
		{
			if(n1_value == "")
				return std::make_pair(n1, 0.25);
			return std::make_pair(n1, 0.125);
		}

		if(n2_type == ENT_BOOL)
		{
			bool n1_as_bool = (n1_value != "");
			bool n2_value = n2->GetBoolValueReference();
			if(n1_as_bool == n2_value)
				return std::make_pair(n2, 0.25);
		}
		else if(n2_type == ENT_NUMBER)
		{
			double n2_value = n2->GetNumberValueReference();
			if(n1_value == "" && n2_value == 0.0)
				return std::make_pair(n2, 0.25);
		}

		return std::make_pair(nullptr, 0.0);
	}

	case ENT_RAND:
		if(n2_type == ENT_NUMBER)
			return std::make_pair(n1, 0.125);
		return std::make_pair(nullptr, 0.0);

	default:
		break;
	}

	//different type, how close?
	if(IsEvaluableNodeTypeQuery(n1_type) && IsEvaluableNodeTypeQuery(n2_type))
		return std::make_pair(n1, 0.25);

	//see if compatible opcode ordering
	if(GetOpcodeOrderedChildNodeType(n1_type) == GetOpcodeOrderedChildNodeType(n2_type))
		return std::make_pair(n1, 0.125);

	return std::make_pair(nullptr, 0.0);
}

static std::string GenerateRandomString(RandomStream &rs)
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

static std::string GenerateRandomStringGivenStringSet(RandomStream &rs, std::vector<std::string> &strings, double novel_chance = 0.08)
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
static void MutateImmediateNode(EvaluableNode *n, EvaluableNodeTreeManipulation::MutationParameters &mp)
{
	auto node_type = n->GetType();
	if(DoesEvaluableNodeTypeUseBoolData(node_type))
	{
		//negate the boolean value to preserve the mutation rate
		n->GetBoolValueReference() = !n->GetBoolValueReference();
	}
	else if(DoesEvaluableNodeTypeUseNumberData(node_type))
	{
		auto &rs = mp.interpreter->randomStream;

		if(mp.immNumberWeights->IsInitialized())
		{
			auto sid = mp.immNumberWeights->WeightedDiscreteRand(mp.interpreter->randomStream);
			if(sid != string_intern_pool.NOT_A_STRING_ID)
			{
				//if found a non-null number, set it, otherwise fall into generating a new number
				double num = Parser::ParseNumberFromKeyStringId(sid);
				if(!FastIsNaN(num))
				{
					n->GetNumberValueReference() = num;
					return;
				}
			}
		}

		double cur_value = n->GetNumberValueReference();

		//50% chance of being negative if negative, 50% of that 50% if positive (minimizing assumptions - a number can be either)
		bool is_negative = (cur_value < 0.0);
		bool new_number_negative = (rs.Rand() < (is_negative ? 0.5 : 0.25));
		double new_value = rs.ExponentialRand(fabs(cur_value));

		//chance to keep it an integer if it is already an integer
		double int_part;
		bool is_integer = (std::modf(cur_value, &int_part) == 0.0);
		if(is_integer && (rs.Rand() < 0.5))
			new_value = std::round(new_value);

		if(rs.Rand() < 0.001)
			new_value = std::numeric_limits<double>::infinity();

		//just in case it ends up being a NaN, set it indirectly to be safe
		n->SetTypeViaNumberValue((new_number_negative ? -1 : 1) * new_value);
	}
	else if(DoesEvaluableNodeTypeUseStringData(node_type))
	{
		if(mp.immStringWeights->IsInitialized())
		{
			auto sid = mp.immStringWeights->WeightedDiscreteRand(mp.interpreter->randomStream);
			if(sid != string_intern_pool.NOT_A_STRING_ID)
			{
				//if found a non-null string, set it, otherwise fall into generating a new string
				n->SetStringID(sid);
				return;
			}
		}
		n->SetStringValue(GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings));
	}
}

static EvaluableNode *AllocateNewRandomNode(EvaluableNodeTreeManipulation::MutationParameters &mp)
{
	//use some heuristics to generate some random immediate value
	EvaluableNode *new_node = mp.enm->AllocNode(mp.randEvaluableNodeType->WeightedDiscreteRand(mp.interpreter->randomStream));

	if(new_node->IsImmediate())
	{
		//give it a respectable default before randomizing
		if(DoesEvaluableNodeTypeUseBoolData(new_node->GetType()))
			new_node->SetTypeViaBoolValue(mp.interpreter->randomStream.RandUInt32() & 1 ? true : false);
		if(DoesEvaluableNodeTypeUseNumberData(new_node->GetType()))
			new_node->SetTypeViaNumberValue(3);
		else if(DoesEvaluableNodeTypeUseStringData(new_node->GetType()))
			new_node->SetStringValue("string");

		if(new_node->GetType() != ENT_NULL)
			MutateImmediateNode(new_node, mp);
	}

	return new_node;
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateNode(EvaluableNode *n, MutationParameters &mp, size_t depth)
{
	if(n == nullptr)
		n = mp.enm->AllocNode(ENT_NULL);
	
	EvaluableNodeBuiltInStringId mutation_type = mp.randMutationType->WeightedDiscreteRand(mp.interpreter->randomStream);

	//if immediate value type, see if can just mutate directly to preserve
	//mutation rate, since most other mutations won't apply
	if(n->IsImmediate() && mutation_type != ENBISI_change_type && mutation_type != ENBISI_insert)
	{
		if(n->GetType() == ENT_NULL)
		{
			mutation_type = ENBISI_change_type;
		}
		else
		{
			MutateImmediateNode(n, mp);
			return n;
		}
	}

	//don't do anything that can change type if less than preserveTypeDepth
	if(depth < mp.preserveTypeDepth &&
		(mutation_type == ENBISI_change_type || mutation_type == ENBISI_insert || mutation_type == ENBISI_remove) )
	{
		//try to find another mutation or give up
		size_t i = 8;
		do
		{
			mutation_type = mp.randMutationType->WeightedDiscreteRand(mp.interpreter->randomStream);
			i--;
		} while(i > 0
			&& mutation_type != ENBISI_change_type && mutation_type != ENBISI_insert && mutation_type != ENBISI_remove);

		//if couldn't find an alternative, just return
		if(mutation_type == ENBISI_change_type || mutation_type == ENBISI_insert || mutation_type == ENBISI_remove)
			return n;
	}

	switch(mutation_type)
	{
	case ENBISI_change_type:
	{
		//change the type of n
		n->SetType(mp.randEvaluableNodeType->WeightedDiscreteRand(mp.interpreter->randomStream), true);
		if(IsEvaluableNodeTypeImmediate(n->GetType()) && n->GetType() != ENT_NULL)
			MutateImmediateNode(n, mp);
		break;
	}

	case ENBISI_insert:
	{
		//insert a new node above n
		EvaluableNode *new_node = AllocateNewRandomNode(mp);
		if(n->IsAssociativeArray())
		{
			// get a random key
			std::string key = GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings);
			new_node->SetMappedChildNode(key, n);
		}
		else
		{
			new_node->AppendOrderedChildNode(n);
		}

		n = new_node;
		break;
	}

	case ENBISI_remove:
		//remove n itself
		if(n->GetOrderedChildNodes().size() > 0)
		{
			auto &ocn = n->GetOrderedChildNodesReference();
			size_t index = mp.interpreter->randomStream.RandSize(ocn.size());
			n = ocn[index];
		}
		else if(n->GetMappedChildNodes().size() > 0)
		{
			auto &mcn = n->GetMappedChildNodesReference();
			double replace_with = mp.interpreter->randomStream.Rand() * mcn.size();
			//iterate over child nodes until find the right index
			for(auto &[_, cn] : mcn)
			{
				if(replace_with < 1.0)
				{
					n = cn;
					break;
				}
				replace_with--;
			}

		}
		else
		{
			n->SetType(ENT_NULL, false);
		}
		break;

	case ENBISI_replace_element_with_copy:
		//copy one element over another
		if(n->GetOrderedChildNodes().size() > 0)
		{
			//get source and destination; note that destination_index is drawn from
			//num_children - 1 to give a different index, but then includes appending to the end / new value
			//so it gets a + 1 added back, and then is incremented if greater or equal to source_index
			size_t num_children = n->GetOrderedChildNodesReference().size();
			size_t source_index = mp.interpreter->randomStream.RandSize(num_children);
			size_t destination_index = mp.interpreter->randomStream.RandSize(num_children);
			if(destination_index >= source_index)
				destination_index++;

			if(destination_index >= num_children)
				n->AppendOrderedChildNode(mp.enm->DeepAllocCopy(n->GetOrderedChildNodes()[source_index]));
			else
				n = n->GetOrderedChildNodes()[destination_index] = mp.enm->DeepAllocCopy(n->GetOrderedChildNodes()[source_index]);
		}
		else if(n->GetMappedChildNodes().size() > 0)
		{
			//get source and destination; note that destination_index is drawn from
			//num_children - 1 to give a different index, but then includes appending to the end / new value
			//so it gets a + 1 added back, and then is incremented if greater or equal to source_index
			auto &mcn = n->GetMappedChildNodes();
			auto num_children = mcn.size();
			size_t source_index = mp.interpreter->randomStream.RandSize(num_children);
			EvaluableNode *source_node = nullptr;
			size_t destination_index = mp.interpreter->randomStream.RandSize(num_children);
			if(destination_index >= source_index)
				destination_index++;

			//iterate over child nodes until find the right source index
			size_t cur_index = 0;
			for(auto &[_, cn] : mcn)
			{
				if(cur_index == source_index)
				{
					source_node = cn;
					break;
				}
				cur_index++;
			}

			//based on destination_index, either add a new copy or replace an existing element
			if(destination_index >= num_children)
			{
				std::string new_key =
					GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings, 0.6);
				n->SetMappedChildNode(new_key, mp.enm->DeepAllocCopy(source_node));
			}
			else
			{
				cur_index = 0;
				for(auto &[_, cn] : mcn)
				{
					if(cur_index == destination_index)
					{
						cn = mp.enm->DeepAllocCopy(source_node);
						break;
					}
					cur_index++;
				}
			}
		}
		break;

	case ENBISI_insert_element:
	{
		//insert element in random location
		EvaluableNode *new_node = AllocateNewRandomNode(mp);
		if(n->IsAssociativeArray())
		{
			// get a random key
			std::string key = GenerateRandomStringGivenStringSet(mp.interpreter->randomStream, *mp.strings);
			n->SetMappedChildNode(key, new_node);
		}
		else if(n->IsOrderedArray())
		{
			auto &ocn = n->GetOrderedChildNodesReference();
			auto ocnt = GetOpcodeOrderedChildNodeType(n->GetType());
			if(ocnt == OpcodeDetails::OrderedChildNodeType::PAIRED)
			{
				size_t location = mp.interpreter->randomStream.RandSize(ocn.size() / 2);
				ocn.insert(ocn.begin() + 2 * location, new_node);
				n->UpdateFlagsBasedOnNewChildNode(new_node);

				//insert second node to keep the pairing
				EvaluableNode *new_node_2 = AllocateNewRandomNode(mp);
				ocn.insert(ocn.begin() + 2 * location + 1, new_node_2);
				n->UpdateFlagsBasedOnNewChildNode(new_node_2);
			}
			else if(ocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED)
			{
				size_t location = 0;
				//guard against empty ocn, and round down to pair start
				if(ocn.size() > 0)
					location = mp.interpreter->randomStream.RandSize((ocn.size() - 1) / 2);

				ocn.insert(ocn.begin() + 1 + 2 * location, new_node);
				n->UpdateFlagsBasedOnNewChildNode(new_node);

				//insert second node to keep the pairing
				EvaluableNode *new_node_2 = AllocateNewRandomNode(mp);
				ocn.insert(ocn.begin() + 1 + 2 * location + 1, new_node_2);
				n->UpdateFlagsBasedOnNewChildNode(new_node_2);
			}
			else
			{
				size_t location = mp.interpreter->randomStream.RandSize(ocn.size() + 1);
				ocn.insert(ocn.begin() + location, new_node);
				n->UpdateFlagsBasedOnNewChildNode(new_node);
			}
		}
		break;
	}

	case ENBISI_remove_element:
		//remove an element from a random location
		if(n->GetMappedChildNodes().size() > 0)
		{
			auto &mcn = n->GetMappedChildNodesReference();
			if(mcn.size() > 0)
			{
				size_t location = mp.interpreter->randomStream.RandSize(mcn.size());
				//iterate over child nodes until find the right index
				for(auto &[key, cn] : mcn)
				{
					if(location < 1)
					{
						n->EraseMappedChildNode(key);
						break;
					}
					location--;
				}
			}
		}
		else if(n->GetOrderedChildNodes().size() > 0)
		{
			auto &ocn = n->GetOrderedChildNodesReference();
			auto ocnt = GetOpcodeOrderedChildNodeType(n->GetType());
			if(ocnt == OpcodeDetails::OrderedChildNodeType::PAIRED && ocn.size() >= 2)
			{
				size_t location = mp.interpreter->randomStream.RandSize(ocn.size() / 2);
				ocn.erase(ocn.begin() + 2 * location);
				//delete second from same spot if room
				if(ocn.size() > 2 * location)
					ocn.erase(ocn.begin() + 2 * location);
			}
			else if(ocnt == OpcodeDetails::OrderedChildNodeType::ONE_POSITION_THEN_PAIRED && ocn.size() >= 3)
			{
				size_t location = mp.interpreter->randomStream.RandSize((ocn.size() - 1) / 2);
				ocn.erase(ocn.begin() + 1 + 2 * location);
				//delete second from same spot if room
				if(ocn.size() > 1 + 2 * location)
					ocn.erase(ocn.begin() + 1 + 2 * location);
			}
			else
			{
				size_t location = mp.interpreter->randomStream.RandSize(ocn.size());
				ocn.erase(ocn.begin() + location);
			}
		}
		else
		{
			n->SetType(ENT_NULL, false);
		}
		break;

	case ENBISI_swap_elements:
		if(n->GetOrderedChildNodes().size() > 1)
		{
			auto &n_ocn = n->GetOrderedChildNodesReference();

			//choose two different indices
			size_t num_child_nodes = n_ocn.size();
			size_t first_index = mp.interpreter->randomStream.RandSize(num_child_nodes);
			size_t second_index = mp.interpreter->randomStream.RandSize(num_child_nodes - 1);
			if(second_index >= first_index)
				second_index++;

			std::swap(n_ocn[first_index], n_ocn[second_index]);
		}
		else if(n->GetMappedChildNodes().size() > 1)
		{
			auto &n_mcn = n->GetMappedChildNodesReference();

			//choose two different indices
			size_t num_child_nodes = n_mcn.size();
			size_t first_index = mp.interpreter->randomStream.RandSize(num_child_nodes);
			size_t second_index = mp.interpreter->randomStream.RandSize(num_child_nodes - 1);
			if(second_index >= first_index)
				second_index++;

			if(first_index != second_index)
			{
				if(first_index > second_index)
					std::swap(first_index, second_index);

				auto first_entry = begin(n_mcn);
				for(size_t i = 0; i < first_index && first_entry != end(n_mcn); i++)
					++first_entry;

				auto second_entry = first_entry;
				++second_entry;
				for(size_t i = first_index + 1; i < second_index && second_entry != end(n_mcn); i++)
					++second_entry;

				std::swap(first_entry->second, second_entry->second);
			}
		}
		break;

	case ENBISI_remove_all_elements:
		n->ClearOrderedChildNodes();
		n->ClearMappedChildNodes();
		break;

	default:
		//error, don't do anything
		break;
	}

	//clear excess nulls (with no child nodes) in lists
	if(n != nullptr)
	{
		auto &n_ocn = n->GetOrderedChildNodes();
		while(!n_ocn.empty()
			&& (n_ocn.back() == nullptr
				|| (n_ocn.back()->GetOrderedChildNodes().size() == 0
					&& n_ocn.back()->GetMappedChildNodes().size() == 0)))
		{
			//either remove this one or stop removing
			if(mp.interpreter->randomStream.Rand() > 0.125)
				n_ocn.pop_back();
			else
				break;
		}
	}

	return n;
}

EvaluableNode *EvaluableNodeTreeManipulation::MutateTree(MutationParameters &mp,
	EvaluableNode *tree, size_t depth)
{
	//if it's nullptr, then move on to making a copy, otherwise see if it's already been copied
	if(tree != nullptr)
	{
		//if this object has already been copied, then just return the reference to the new copy
		auto found_copy = mp.references.find(tree);
		if(found_copy != end(mp.references))
			return found_copy->second;
	}

	EvaluableNode *copy = mp.enm->AllocNode(tree);
	auto node_stack = mp.interpreter->CreateOpcodeStackStateSaver(copy);

	//shouldn't happen, but just to be safe
	if(copy == nullptr)
		return nullptr;

	(mp.references)[tree] = copy;

	//this shouldn't happen - it should be a node of type ENT_NULL, but check just in case
	if(copy == nullptr)
		return nullptr;

	if(copy->IsAssociativeArray())
	{
		//for any mapped children, copy and update
		for(auto &[_, s] : copy->GetMappedChildNodesReference())
		{
			EvaluableNode *n = s;

			//turn into a copy and mutate
			n = MutateTree(mp, n, depth + 1);

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

			//turn into a copy and mutate
			n = MutateTree(mp, n, depth + 1);

			//replace current item in list with copy
			ocn[i] = n;
		}
	}

	//mutate after potentially mutated all child nodes
	if(mp.interpreter->randomStream.Rand() < mp.mutation_rate)
		copy = MutateNode(copy, mp, depth);

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

void EvaluableNodeTreeManipulation::GetStringsFromTree(
	EvaluableNode *tree, std::vector<std::string> &strings, EvaluableNode::ReferenceSetType &checked)
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

#if defined(MULTITHREAD_SUPPORT)
thread_local std::vector<uint32_t> EvaluableNodeTreeManipulation::aCharsBuffer;
thread_local std::vector<uint32_t> EvaluableNodeTreeManipulation::bCharsBuffer;
thread_local FlatMatrix<size_t> EvaluableNodeTreeManipulation::sequenceCommonalityBuffer;
#else
std::vector<uint32_t> EvaluableNodeTreeManipulation::aCharsBuffer;
std::vector<uint32_t> EvaluableNodeTreeManipulation::bCharsBuffer;
FlatMatrix<size_t> EvaluableNodeTreeManipulation::sequenceCommonalityBuffer;
#endif

EvaluableNode EvaluableNodeTreeManipulation::nullEvaluableNode(ENT_NULL);

CompactHashMap<EvaluableNodeBuiltInStringId, double> EvaluableNodeTreeManipulation::mutationOperationTypeProbabilities
{
	{ENBISI_change_type,				0.15   },
	{ENBISI_insert,						0.15   },
	{ENBISI_remove,						0.15   },
	{ENBISI_replace_element_with_copy,	0.0999 },
	{ENBISI_insert_element,				0.15   },
	{ENBISI_remove_element,				0.15   },
	{ENBISI_swap_elements,				0.15   },
	{ENBISI_remove_all_elements,		0.0001 }
};

EvaluableNodeTreeManipulation::MutationParameters::WeightedRandMutationType EvaluableNodeTreeManipulation::mutationOperationTypeRandomStream(mutationOperationTypeProbabilities, true);
