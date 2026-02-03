#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeManagement.h"
#include "HashMaps.h"
#include "Merger.h"
#include "WeightedDiscreteRandomStream.h"

//system headers:
#include <cmath>
#include <string>
#include <vector>

//forward declarations:
class Interpreter;

//Functor to transform EvaluableNode into doubles
class EvaluableNodeAsDouble
{
public:
	inline double operator()(EvaluableNode *en)
	{
		return EvaluableNode::ToNumber(en);
	}
};

//hashing for pairs of pointers
template<typename T>
struct std::hash<std::pair<T *, T *>>
{
	inline size_t operator()(std::pair<T *, T *> const &pointer_pair) const
	{
		size_t h1 = std::hash<T *>{}(pointer_pair.first);
		size_t h2 = std::hash<T *>{}(pointer_pair.second);
		return h1 ^ (h2 << 1);
	}
};

//equality for pairs of pointers
template<typename T>
constexpr bool operator==(const std::pair<T *, T *> &a, const std::pair<T *, T *> &b)
{
	return a.first == b.first && a.second == b.second;
}

//for caching pairs of EvaluableNode *'s into MergeMetricResults
struct MergeMetricResultsParams
{
	EvaluableNode::ReferenceSetType *checked;
	CompactHashMap<std::pair<EvaluableNode *, EvaluableNode *>, MergeMetricResults<EvaluableNode *>> memoizedNodeMergePairs;
	bool typesMustMatch;
	bool nominalNumbers;
	bool nominalStrings;
	bool recursiveMatching;
};

//for random streams that are based on an EvaluableNode MappedChildNodes
typedef WeightedDiscreteRandomStreamTransform<EvaluableNodeBuiltInStringId, EvaluableNode::AssocType, EvaluableNodeAsDouble>
			EvaluableNodeMappedWeightedDiscreteRandomStreamTransform;

class EvaluableNodeTreeManipulation
{
public:
	class MutationParameters
	{
	public:
		typedef WeightedDiscreteRandomStreamTransform<EvaluableNodeType,
			CompactHashMap<EvaluableNodeType, double>> WeightedRandEvaluableNodeType;

		typedef WeightedDiscreteRandomStreamTransform<EvaluableNodeBuiltInStringId,
			CompactHashMap<EvaluableNodeBuiltInStringId, double>> WeightedRandMutationType;

		Interpreter *interpreter;
		EvaluableNodeManager *enm;
		double mutation_rate;
		std::vector<std::string> *strings;
		EvaluableNode::ReferenceAssocType references;
		WeightedRandEvaluableNodeType *randEvaluableNodeType;
		WeightedRandMutationType *randMutationType;

		MutationParameters(Interpreter *interpreter,
			EvaluableNodeManager *enm,
			double mutation_rate,
			std::vector<std::string> *strings,
			WeightedRandEvaluableNodeType *rand_operation,
			WeightedRandMutationType *rand_operation_type) :
			interpreter(nullptr),
			enm(nullptr),
			mutation_rate(0),
			strings(nullptr),
			references(EvaluableNode::ReferenceAssocType()),
			randEvaluableNodeType(&evaluableNodeTypeRandomStream),
			randMutationType(&mutationOperationTypeRandomStream)
		{
			this->interpreter = interpreter;
			this->enm = enm;
			this->mutation_rate = mutation_rate;
			this->strings = strings;
			this->randEvaluableNodeType = rand_operation;
			this->randMutationType = rand_operation_type;
		}
	};

	static CompactHashMap<EvaluableNodeBuiltInStringId, double> mutationOperationTypeProbabilities;
	static CompactHashMap<EvaluableNodeType, double> evaluableNodeTypeProbabilities;

	//functionality to merge two nodes
	class NodesMergeMethod : public Merger<EvaluableNode *, nullptr, EvaluableNode::AssocType>
	{
	public:
		NodesMergeMethod(EvaluableNodeManager *_enm, bool keep_all_of_both,
			bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching)
			: enm(_enm), keepAllOfBoth(keep_all_of_both),
			typesMustMatch(types_must_match), nominalNumbers(nominal_numbers), nominalStrings(nominal_strings),
			recursiveMatching(recursive_matching)
		{	}

		virtual MergeMetricResults<EvaluableNode *> MergeMetric(EvaluableNode *a, EvaluableNode *b)
		{
			return NumberOfSharedNodes(a, b, typesMustMatch, nominalNumbers, nominalStrings, recursiveMatching);
		}

		virtual EvaluableNode *MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge = false)
		{	return MergeTrees(this, a, b);	}

		virtual bool KeepAllNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableValue()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableAInsteadOfB()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableA()
		{	return keepAllOfBoth;	}
		virtual bool KeepNonMergeableB()
		{	return keepAllOfBoth;	}

		virtual bool AreMergeable(EvaluableNode *a, EvaluableNode *b)
		{
			auto [_, commonality] = CommonalityBetweenNodeTypesAndValues(a, b, TypesMustMatch(), NominalNumbers(), NominalStrings());
			return (commonality == 1.0);
		}

		virtual EvaluableNode::ReferenceAssocType &GetReferences()
		{	return references;	}

		constexpr bool TypesMustMatch()
		{	return typesMustMatch;		}

		constexpr bool NominalNumbers()
		{	return nominalNumbers;		}

		constexpr bool NominalStrings()
		{	return nominalStrings;		}

		constexpr bool RecursiveMatching()
		{	return recursiveMatching;		}

		//use for allocating
		EvaluableNodeManager *enm;

	protected:
		bool keepAllOfBoth;
		bool typesMustMatch;
		bool nominalNumbers;
		bool nominalStrings;
		bool recursiveMatching;
		EvaluableNode::ReferenceAssocType references;
	};

	//functionality to mix nodes
	class NodesMixMethod : public NodesMergeMethod
	{
	public:
		NodesMixMethod(RandomStream random_stream, EvaluableNodeManager *_enm,
			double fraction_a, double fraction_b, double similar_mix_chance,
			bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching);

		virtual EvaluableNode *MergeValues(EvaluableNode *a, EvaluableNode *b, bool must_merge = false);

		virtual bool KeepAllNonMergeableValues()
		{	return false;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return true;	}

		virtual bool KeepNonMergeableValue()
		{
			return randomStream.Rand() < fractionAOrB;
		}

		virtual bool KeepNonMergeableAInsteadOfB()
		{
			return randomStream.Rand() < fractionAInsteadOfB;
		}

		virtual bool KeepNonMergeableA()
		{
			return randomStream.Rand() < fractionA;
		}
		virtual bool KeepNonMergeableB()
		{
			return randomStream.Rand() < fractionB;
		}

		virtual bool AreMergeable(EvaluableNode *a, EvaluableNode *b);

	protected:

		RandomStream randomStream;

		double fractionA;
		double fractionB;
		double fractionAOrB;
		double fractionAInsteadOfB;
		double similarMixChance;
	};

	//functionality to merge sequences of strings (e.g., for comments)
	class StringSequenceMergeMetric : public Merger<std::string *>
	{
	public:
		constexpr StringSequenceMergeMetric(bool keep_all_of_both)
			: keepAllOfBoth(keep_all_of_both)
		{	}

		virtual MergeMetricResults<std::string *> MergeMetric(std::string *a, std::string *b);

		virtual std::string *MergeValues(std::string *a, std::string *b, bool must_merge = false);

		virtual bool KeepAllNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableValue()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableAInsteadOfB()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableA()
		{	return keepAllOfBoth;	}
		virtual bool KeepNonMergeableB()
		{	return keepAllOfBoth;	}

		virtual bool AreMergeable(std::string *a, std::string *b)
		{
			if(a == b)
				return true;
			return (a != nullptr && b != nullptr && *a == *b);
		}

	protected:
		bool keepAllOfBoth;
	};
	
	//functionality to mix utf-8 strings
	class StringsMixMethodUtf8 : public Merger<uint32_t, 0>
	{
	public:
		StringsMixMethodUtf8(RandomStream random_stream, double fraction_a, double fraction_b);

		virtual MergeMetricResults<uint32_t> MergeMetric(uint32_t a, uint32_t b)
		{
			if(a == b)
				return MergeMetricResults(1.0, a, b);
			else
				return MergeMetricResults(0.0, a, b, false, false);
		}

		virtual uint32_t MergeValues(uint32_t a, uint32_t b, bool must_merge = false)
		{
			if(b == 0)
				return a;
			if(a == 0)
				return b;

			if(KeepNonMergeableAInsteadOfB())
				return a;
			return b;
		}

		virtual bool KeepAllNonMergeableValues()
		{	return false;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return true;	}

		virtual bool KeepNonMergeableValue()
		{
			return randomStream.Rand() < fractionAOrB;
		}

		virtual bool KeepNonMergeableAInsteadOfB()
		{
			return randomStream.Rand() < fractionAInsteadOfB;
		}

		virtual bool KeepNonMergeableA()
		{
			return randomStream.Rand() < fractionA;
		}
		virtual bool KeepNonMergeableB()
		{
			return randomStream.Rand() < fractionB;
		}

		virtual bool AreMergeable(uint32_t a, uint32_t b)
		{	return a == b;	}

	protected:
		RandomStream randomStream;

		double fractionA;
		double fractionB;
		double fractionAOrB;
		double fractionAInsteadOfB;
		double similarMixChance;
	};

	//Tree and string merging functions
	static inline EvaluableNode *IntersectTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2,
		bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching)
	{
		NodesMergeMethod mm(enm, false, types_must_match, nominal_numbers, nominal_strings, recursive_matching);
		return mm.MergeValues(tree1, tree2);
	}

	static inline EvaluableNode *UnionTrees(EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2,
		bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching)
	{
		NodesMergeMethod mm(enm, true, types_must_match, nominal_numbers, nominal_strings, recursive_matching);
		return mm.MergeValues(tree1, tree2);
	}

	static inline EvaluableNode *MixTrees(RandomStream random_stream, EvaluableNodeManager *enm, EvaluableNode *tree1, EvaluableNode *tree2,
		double fraction_a, double fraction_b, double similar_mix_chance,
		bool types_must_match, bool nominal_numbers, bool nominal_strings, bool recursive_matching)
	{
		NodesMixMethod mm(random_stream, enm, fraction_a, fraction_b, similar_mix_chance,
			types_must_match, nominal_numbers, nominal_strings, recursive_matching);
		return mm.MergeValues(tree1, tree2);
	}

	static std::string MixStrings(const std::string &a, const std::string &b,
		RandomStream random_stream, double fraction_a, double fraction_b);

	//returns a number between 0 and 1, where 1 is exactly the same and 0 is maximally different
	static inline double CommonalityBetweenNumbers(double n1, double n2)
	{
		if(n1 == n2)
			return 1.0;

		//if both were the same signed infinity, it would have been caught above
		if(n1 == std::numeric_limits<double>::infinity() || n2 == std::numeric_limits<double>::infinity())
			return 0.0;

		const double a1 = std::fabs(n1);
		const double a2 = std::fabs(n2);

		const double max_abs = std::max(a1, a2);
		const double min_abs = std::min(a1, a2);

		//note that when both numbers are zero, the function will have already handled it
		const double rel_diff = (max_abs == 0.0) ? 0.0 : (max_abs - min_abs) / max_abs;

		//prevent the relative term from blowing up when max_abs is tiny
		const double abs_diff = std::fabs(n1 - n2);
		
		//blend the two differences
		const double blended = 0.875 * rel_diff + 0.125 * std::min(abs_diff, 1.0);

		//smoothly decay
		const double similarity = std::exp(-4 * blended);

		//make sure floating point doesn't push outside of the bounds
		return std::clamp(similarity, 0.0, 1.0);
	}

	//returns the commonality between two strings that are different
	static inline double CommonalityBetweenStrings(StringInternPool::StringID sid1, StringInternPool::StringID sid2)
	{
		if(sid1 == sid2)
			return 1.0;

		if(sid1 == string_intern_pool.NOT_A_STRING_ID || sid2 == string_intern_pool.NOT_A_STRING_ID)
			return 0.0;

		const auto &s1 = string_intern_pool.GetStringFromID(sid1);
		const auto &s2 = string_intern_pool.GetStringFromID(sid2);

		size_t len1 = s1.size();
		size_t len2 = s2.size();

		size_t diff = EditDistance(s1, s2, len1, len2);

		double avg_len = (len1 + len2) * 0.5;
		double length_ratio = std::min(len1, len2) / static_cast<double>(std::max(len1, len2));

		double edit_score = std::exp(-static_cast<double>(diff) / avg_len);
		return 0.75 * edit_score + 0.25 * length_ratio;
	}

	//returns the EditDistance between the sequences a and b using the specified sequence_commonality_buffer
	template<typename ElementType>
	static size_t EditDistance(std::vector<ElementType> &a, std::vector<ElementType> &b,
		FlatMatrix<size_t> &sequence_commonality_buffer)
	{
		//if either string is empty, return the other
		size_t a_size = a.size();
		size_t b_size = b.size();
		if(a_size == 0)
			return b_size;
		if(b_size == 0)
			return a_size;

		ComputeSequenceCommonalityMatrix(sequence_commonality_buffer, a, b,
			[] (ElementType a, ElementType b)
			{
				return (a == b ? 1 : 0);
			});

		//edit distance is the longest sequence's size minus the commonality
		return std::max(a_size, b_size) - sequence_commonality_buffer.At(a_size, b_size);
	}

	//returns the EditDistance between the sequences a and b
	template<typename ElementType>
	inline static size_t EditDistance(std::vector<ElementType> &a, std::vector<ElementType> &b)
	{
		FlatMatrix<size_t> sequence_commonality;
		return EditDistance(a, b, sequence_commonality);
	}

	//computes the edit distance (Levenshtein distance) between the two utf-8 strings
	inline static size_t EditDistance(const std::string &a, const std::string &b)
	{
		StringManipulation::ExplodeUTF8Characters(a, aCharsBuffer);
		StringManipulation::ExplodeUTF8Characters(b, bCharsBuffer);
		return EvaluableNodeTreeManipulation::EditDistance(aCharsBuffer, bCharsBuffer, sequenceCommonalityBuffer);
	}

	//computes the edit distance (Levenshtein distance) between the two utf-8 strings
	//a_size and b_size are set to the length of the strings respectively
	inline static size_t EditDistance(const std::string &a, const std::string &b,
		size_t &a_len, size_t &b_len)
	{
		StringManipulation::ExplodeUTF8Characters(a, aCharsBuffer);
		a_len = aCharsBuffer.size();

		StringManipulation::ExplodeUTF8Characters(b, bCharsBuffer);
		b_len = bCharsBuffer.size();

		return EvaluableNodeTreeManipulation::EditDistance(aCharsBuffer, bCharsBuffer, sequenceCommonalityBuffer);
	}

	//computes the edit distance between the two trees
	//types_must_match, nominal_numbers, nominal_strings govern whether matches are exact based on type
	//if recursive_matching is true, then it will attempt to recursively match any part of the data structure of tree1 to tree2
	//if recursive_matching is false, then it will only attempt to merge the two at the same level, which yield better
	// results if the data structures are common, and additionally will be much faster
	static double EditDistance(EvaluableNode *tree1, EvaluableNode *tree2,
		bool types_must_match = true, bool nominal_numbers = true, bool nominal_strings = true, bool recursive_matching = true)
	{
		auto shared_nodes = NumberOfSharedNodes(tree1, tree2, types_must_match, nominal_numbers, nominal_strings, recursive_matching);
		size_t tree_1_size = EvaluableNode::GetDeepSize(tree1);
		size_t tree_2_size = EvaluableNode::GetDeepSize(tree2);

		//find the distance to edit from tree1 to shared, then from shared to tree_2.  Shared is the smallest, so subtract from each.
		return (tree_1_size - shared_nodes.commonality) + (tree_2_size - shared_nodes.commonality);
	}

	//computes the total number of nodes in both trees that are equal
	//types_must_match, nominal_numbers, nominal_strings govern whether matches are exact based on type
	//if recursive_matching is true, then it will attempt to recursively match any part of the data structure of tree1 to tree2
	//if recursive_matching is false, then it will only attempt to merge the two at the same level, which yield better
	// results if the data structures are common, and additionally will be much faster
	inline static MergeMetricResults<EvaluableNode *> NumberOfSharedNodes(
		EvaluableNode *tree1, EvaluableNode *tree2,
		bool types_must_match = true, bool nominal_numbers = true, bool nominal_strings = true, bool recursive_matching = true)
	{
		MergeMetricResultsParams mmrp;
		mmrp.typesMustMatch = types_must_match;
		mmrp.nominalNumbers = nominal_numbers;
		mmrp.nominalStrings = nominal_strings;
		mmrp.recursiveMatching = recursive_matching;
		if((tree1 != nullptr && tree1->GetNeedCycleCheck()) || (tree2 != nullptr && tree2->GetNeedCycleCheck()))
		{
			EvaluableNode::ReferenceSetType checked;
			mmrp.checked = &checked;
			return NumberOfSharedNodes(tree1, tree2, mmrp);
		}
		else //don't need to check for cycles
		{
			mmrp.checked = nullptr;
			return NumberOfSharedNodes(tree1, tree2, mmrp);
		}
	}

	//Returns the total number of nodes in both trees that are equal, ignoring those in the checked set
	// Assists the public function NumberOfSharedNodes
	static MergeMetricResults<EvaluableNode *> NumberOfSharedNodes(EvaluableNode *tree1, EvaluableNode *tree2,
		MergeMetricResultsParams &mmrp);

	//If the nodes, n1 and n2 can be generalized, then returns a new (allocated) node that is preferable to use (usually the more specific one)
	// If the nodes are not equivalent, then returns null
	// Only extra data (labels, comments, etc.) that is common to both is kept, unless KeepAllNonMergeableValues is true.  Then everything from both is kept. If 
	// KeepSomeNonMergeableValues is set and one node is null, it will return a copy of the non-null node
	static EvaluableNode *CreateGeneralizedNode(NodesMergeMethod *mm, EvaluableNode *n1, EvaluableNode *n2);

	//returns the union of the two sets of labels
	static std::vector<StringInternPool::StringID> UnionStringIDVectors(
		const std::vector<StringInternPool::StringID> &label_list_a, const std::vector<StringInternPool::StringID> &label_list_b);

	//returns the intersection of the two sets of labels
	static std::vector<StringInternPool::StringID> IntersectStringIDVectors(
		const std::vector<StringInternPool::StringID> &label_list_a, const std::vector<StringInternPool::StringID> &label_list_b);

	//Returns a tree that consists of only nodes that are common across all of the trees specified,
	// where all returned values are newly allocated and modifiable
	//Note that MergeTrees does not guarantee that EvaluableNodeFlags will be set appropriately
	static EvaluableNode *MergeTrees(NodesMergeMethod *mm, EvaluableNode *tree1, EvaluableNode *tree2);

	//Returns a tree that is a copy of tree but mutated based on mutation_rate
	// will create the new tree with interpreter's evaluableNodeManager and will use interpreter's RandomStream
	//Note that MutateTree does not guarantee that EvaluableNodeFlags will be set appropriately
	static EvaluableNode *MutateTree(Interpreter *interpreter, EvaluableNodeManager *enm, EvaluableNode *tree, double mutation_rate,
		CompactHashMap<EvaluableNodeBuiltInStringId, double> *mutation_weights, CompactHashMap<EvaluableNodeType, double> *evaluable_node_weights);

	//traverses tree and replaces any string that matches a key of to_replace with the value in to_replace
	static inline void ReplaceStringsInTree(EvaluableNode *tree, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &to_replace)
	{
		EvaluableNode::ReferenceSetType checked;
		ReplaceStringsInTree(tree, to_replace, checked);
	}

	//returns an EvaluableNodeType based on the probabilities specified by evaluableNodeTypeRandomStream
	static EvaluableNodeType GetRandomEvaluableNodeType(RandomStream *rs);

protected:

	//Evaluates commonality metric between the two nodes passed in, including labels.  1.0 if identical, 0.0 if completely different, and some value between if similar
	//appropriate type or value matching parameters apply, then it will only return 1.0 or 0.0
	static MergeMetricResults<EvaluableNode *> CommonalityBetweenNodes(
		EvaluableNode *n1, EvaluableNode *n2,
		bool types_must_match, bool nominal_numbers, bool nominal_strings);

	//Evaluates the functional commonality between the types and immediate values of n1 and n2 (excluding labels, comments, etc.)
	// Returns a pair: the first value is the more general of the two nodes and the second is a commonality value
	// The more general of the two nodes will be the one whose type is more general
	// The commonality metric will return 1.0 if identical, 0.0 if completely different, and some value between if similar
	//appropriate type or value matching parameters apply, then it will only return 1.0 or 0.0
	//The EvaluableNode * returned should not be modified, nor should it be included in any data outside the scope of the caller
	static std::pair<EvaluableNode *, double> CommonalityBetweenNodeTypesAndValues(
							EvaluableNode *n1, EvaluableNode *n2,
							bool types_must_match, bool nominal_numbers, bool nominal_strings);

	//Mutates the current node n, changing its type or value, based on the mutation_rate
	// strings contains a list of strings to likely choose from if mutating to a string value
	// returns the new value, which may be n, a modification of n, or an entirely different node
	static EvaluableNode *MutateNode(EvaluableNode *n, MutationParameters &mp);

	//random stream for EvaluableNodeType, so can obtain a random type from a useful distribution
	static MutationParameters::WeightedRandEvaluableNodeType evaluableNodeTypeRandomStream;

	//Recursively creates a new tree using enm which is a copy of tree, but given a mutation_rate
	// will create the new tree with interpreter's evaluableNodeManager
	// strings is a list of strings to choose from when mutating and adding new strings
	static EvaluableNode *MutateTree(MutationParameters &mp, EvaluableNode *tree);

	//traverses tree and replaces any string that matches a key of to_replace with the value in to_replace
	static void ReplaceStringsInTree(EvaluableNode *tree, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &to_replace, EvaluableNode::ReferenceSetType &checked);

	//returns a set of strings that have appeared at least once in the given tree
	static void GetStringsFromTree(EvaluableNode *tree, std::vector<std::string> &strings, EvaluableNode::ReferenceSetType &checked);

	//random stream for MutationOperationType, so can obtain a random type from a useful distribution
	static MutationParameters::WeightedRandMutationType mutationOperationTypeRandomStream;

	//reusable buffers for string distance and mixing
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local static std::vector<uint32_t> aCharsBuffer;
	thread_local static std::vector<uint32_t> bCharsBuffer;
	thread_local static FlatMatrix<size_t> sequenceCommonalityBuffer;
#else
	static std::vector<uint32_t> aCharsBuffer;
	static std::vector<uint32_t> bCharsBuffer;
	static FlatMatrix<size_t> sequenceCommonalityBuffer;
#endif

	//used by CommonalityBetweenNodeTypesAndValues for returning a null
	static EvaluableNode nullEvaluableNode;
};
