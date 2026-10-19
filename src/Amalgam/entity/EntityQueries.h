#pragma once

//project headers:
#include "Entity.h"
#include "EvaluableNode.h"
#include "GeneralizedDistance.h"

//system headers:
#include <limits>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

//forward declarations:
class Entity;

//if set to false, will not allow use of the SBF datastore
extern bool _enable_SBF_datastore;

class EntityQueryCondition
{
public:
	EntityQueryCondition() = default;
	EntityQueryCondition(EntityQueryCondition &&) noexcept = default;
	EntityQueryCondition &operator=(EntityQueryCondition &&) noexcept = default;
	EntityQueryCondition(const EntityQueryCondition &) = delete;
	EntityQueryCondition &operator=(const EntityQueryCondition &) = delete;

	//returns true if the entity matches the condition
	bool DoesEntityMatchCondition(Entity *e);

	//computes the distance measure of the condition
	// returns NaN if invalid
	double GetConditionDistanceMeasure(Entity *e, bool high_accuracy);

	EvaluableNodeReference GetMatchingEntities(Entity *container, std::vector<Entity *> &matching_entities,
		bool from_all_entities, EvaluableNodeManager *enm);

	EvaluableNodeType queryType = ENT_NULL;

	//Only non-string filter labels need preparation: retain their conversion in singleLabel.
	//The node itself is still borrowed from the interpreter's rooted condition nodes.
	EvaluableNode *filterNode = nullptr;

	//the labels that comprise each dimension of the position
	std::vector<StringInternPool::StringID> positionLabels;

	//when initialized, points to an OrderedChildNodes vector of the values to compare
	EvaluableNode::OrderedType *positionsToCompare = nullptr;

	GeneralizedDistanceEvaluator distEvaluator{};

	//an entity to exclude, and to use its label values if relevant
	StringInternPool::StringID entityIdToExclude = StringInternPool::NOT_A_STRING_ID;

	//index of an entity to exclude
	size_t exclusionEntityIndex = std::numeric_limits<size_t>::max();

	//minimum number of entities to attempt to retrieve
	size_t minToRetrieve = std::numeric_limits<size_t>::max();

	//incremental probability where, if the next entity is below this threshold, don't retrieve more
	double numToRetrieveMinIncrementalProbability = 0.0;

	//number of entities to attempt to retrieve after any constraints
	size_t extraToRetrieve = 0;

	//distance weight exponent for distance queries (takes distance and raises it to the respective exponent) when returning distances
	//only applicable when computeSurprisal is false
	double distanceWeightExponent = 1.0;

	//if true, will populate any omitted feature values
	bool populateOmittedFeatureValues = false;

	//min cluster for ENT_QUERY_ENTITY_CLUSTERS
	double minClusterWeight = 0.0;

	//indicates whether a compute result should be returned as a sorted list
	bool returnSortedList = false;

	//for ENT_QUERY_NEAREST_GENERALIZED_DISTANCE and ENT_QUERY_WITHIN_GENERALIZED_DISTANCE, if returnSortedList is true,
	// additionally return these labels if valid
	std::vector<StringInternPool::StringID> additionalSortedListLabels;

	//interpreter calling the query
	Interpreter *interpreter = nullptr;

	//containing entity
	Entity *entity = nullptr;

	//if conviction_of_removal is true, then it will compute the conviction as if the entities were removed, if false,
	// will compute added or included
	bool convictionOfRemoval = false;

	//if true, use concurrency if applicable
	bool useConcurrency = false;

	//entity IDs for coerced ID filters or distance statistics
	std::vector<StringInternPool::StringID> existLabels;

	//the position for a distance query
	std::vector<EvaluableNodeImmediateValueWithType> valuesToCompare;

	//a single standalone label in the query
	StringInternPool::StringID singleLabel = StringInternPool::NOT_A_STRING_ID;

	//when requesting a single type
	EvaluableNodeImmediateValueType singleLabelType = ENIVT_NOT_EXIST;

	//a label representing a weight label
	StringInternPool::StringID weightLabel = StringInternPool::NOT_A_STRING_ID;

	//maximum distance, or the requested difference for min/max_difference
	double maxDistance = std::numeric_limits<double>::quiet_NaN();

	//maximum number of entities to attempt to retrieve (based on queryType)
	size_t maxToRetrieve = 0;

	//if ENT_QUERY_SELECT has a start offset
	bool hasStartOffset = false;

	//ENT_QUERY_SELECT's value of the start offset
	size_t startOffset = 0;

	//whether ENT_QUERY_SELECT or ENT_QUERY_SAMPLE has a random stream; if not, it will use consistent order
	bool hasRandomStream = false;

	//the random stream for queries that use it
	RandomStream randomStream;

	//includes zero as a valid difference for ENT_QUERY_MIN_DIFFERENCE
	bool includeZeroDifferences = false;

	//quantile percentage, for ENT_QUERY_QUANTILE
	double qPercentage = 0.5;

	//for ENT_QUERY_GENERALIZED_MEAN
	double pValue = 1.0;
	double center = 0.0;
	bool calculateMoment = false;
	bool absoluteValue = false;
};

namespace EntityQueries
{
	inline EvaluableNodeType GetQueryType(const EntityQuery &query)
	{
		if(auto node = std::get_if<EvaluableNode *>(&query))
			return *node == nullptr ? ENT_NULL : (*node)->GetType();
		if(std::holds_alternative<EntityQuerySum>(query))
			return ENT_QUERY_SUM;
		return std::get<EntityQueryCondition>(query).queryType;
	}

	inline EvaluableNode *GetFilterNode(const EntityQuery &query)
	{
		if(auto node = std::get_if<EvaluableNode *>(&query))
			return *node;
		if(auto prepared = std::get_if<EntityQueryCondition>(&query))
			return prepared->filterNode;
		return nullptr;
	}

	inline StringInternPool::StringID GetFilterLabel(const EntityQuery &query)
	{
		if(auto prepared = std::get_if<EntityQueryCondition>(&query))
			return prepared->singleLabel;
		auto node = std::get<EvaluableNode *>(query);
		if(node == nullptr || node->GetOrderedChildNodes().empty())
			return StringInternPool::NOT_A_STRING_ID;
		auto label = node->GetOrderedChildNodes()[0];
		return label != nullptr && label->GetType() == ENT_STRING ? label->GetStringIDReference() : StringInternPool::NOT_A_STRING_ID;
	}

	//Converts range endpoints once per filter, outside the entity loop.
	std::tuple<EvaluableNodeImmediateValueType, EvaluableNodeImmediateValue, EvaluableNodeImmediateValue>
		GetQueryRange(EvaluableNode *query);

	void FilterEntities(EvaluableNode *query, StringInternPool::StringID label, Entity *container,
		std::vector<Entity *> &matching_entities, bool is_first);

	double ComputeSum(StringInternPool::StringID value_label, StringInternPool::StringID weight_label,
		const std::vector<Entity *> &matching_entities);

	//Existence filters were historically grouped; preserve their combined return labels.
	template<typename Function>
	void ForEachQueryLabel(const std::vector<EntityQuery> &queries, size_t query_index, Function function)
	{
		auto type = GetQueryType(queries[query_index]);
		if(type == ENT_QUERY_EXISTS || type == ENT_QUERY_NOT_EXISTS)
		{
			size_t first = query_index;
			while(first > 0 && GetQueryType(queries[first - 1]) == type)
				--first;
			for(size_t i = first; i <= query_index; i++)
				function(GetFilterLabel(queries[i]));
		}
		else if((type == ENT_QUERY_IN_ENTITY_LIST || type == ENT_QUERY_NOT_IN_ENTITY_LIST)
			&& std::holds_alternative<EvaluableNode *>(queries[query_index]))
		{
			auto &ocn = std::get<EvaluableNode *>(queries[query_index])->GetOrderedChildNodes();
			if(ocn.size() > 0 && EvaluableNode::IsOrderedArray(ocn[0]))
				for(auto node : ocn[0]->GetOrderedChildNodesReference())
				{
					auto [valid, id] = EvaluableNode::ToStringIDIfExists(node);
					if(valid)
						function(id);
				}
		}
		else if(auto prepared = std::get_if<EntityQueryCondition>(&queries[query_index]))
		{
			for(auto id : prepared->existLabels)
				function(id);
		}
	}
}
