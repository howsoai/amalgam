#pragma once

//project headers:
#include "Entity.h"
#include "EntityQueriesStatistics.h"
#include "EntityQuery.h"
#include "EvaluableNode.h"
#include "GeneralizedDistance.h"

//system headers:
#include <limits>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

//forward declarations:
class Entity;

//if set to false, will not allow use of the SBF datastore
extern bool _enable_SBF_datastore;

namespace EntityQueries
{
	inline EvaluableNode *GetValueFilterNode(const EntityQuery &query)
	{
		if(auto filter = std::get_if<EntityQuery::ValueFilter>(&query.payload))
			return filter->node;
		return nullptr;
	}

	inline StringInternPool::StringID GetValueFilterLabel(const EntityQuery &query)
	{
		return std::get<EntityQuery::ValueFilter>(query.payload).label;
	}

	//converts range endpoints once per filter, outside the entity loop
	std::tuple<EvaluableNodeImmediateValueType, EvaluableNodeImmediateValue, EvaluableNodeImmediateValue>
		GetQueryRange(EvaluableNode *query);

	void FilterEntities(const EntityQuery &query, Entity *container,
		std::vector<Entity *> &matching_entities, bool is_first);

	void FindMinMax(const EntityQuery &query, std::vector<Entity *> &matching_entities);

	void SelectEntities(EntityQuery &query, std::vector<Entity *> &matching_entities);
	void SampleEntities(EntityQuery &query, std::vector<Entity *> &source_entities, std::vector<Entity *> &matching_entities);

	//computes aggregation across iterators
	template<typename Iterator, typename ValueFunction, typename WeightFunction>
	double ComputeNumericAggregate(const EntityQuery &query, Iterator first, Iterator last,
		const ValueFunction &get_value, bool has_weight, const WeightFunction &get_weight,
		std::vector<double> &values_buffer, std::vector<std::pair<double, double>> *quantile_buffer)
	{
		switch(query.queryType)
		{
		case ENT_QUERY_SUM:
			return EntityQueriesStatistics::Sum(first, last, get_value, has_weight, get_weight);
		case ENT_QUERY_QUANTILE:
			return Quantile(first, last, get_value, has_weight, get_weight, std::get<EntityQuery::Quantile>(query.payload).percentage, quantile_buffer);
		case ENT_QUERY_GENERALIZED_MEAN:
		{
			auto &mean = std::get<EntityQuery::Mean>(query.payload);
			return GeneralizedMean(first, last, get_value, has_weight, get_weight,
				mean.power, mean.center, mean.calculateMoment, mean.absoluteValue);
		}
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		{
			auto &difference = std::get<EntityQuery::Difference>(query.payload);
			return EntityQueriesStatistics::ExtremeDifference(first, last, get_value,
				query.queryType == ENT_QUERY_MIN_DIFFERENCE, difference.limit, difference.includeZero, values_buffer);
		}
		default:
			return std::numeric_limits<double>::quiet_NaN();
		}
	}

	double ComputeNumericAggregate(const EntityQuery &query, const std::vector<Entity *> &matching_entities);

	EvaluableNodeReference ComputeMode(const EntityQuery &query,
		const std::vector<Entity *> &matching_entities, EvaluableNodeManager *enm);

	//the returned map owns the StringID references for the keys
	OrderedHashMap<StringInternPool::StringID, double> ComputeValueMasses(const EntityQuery &query,
		const std::vector<Entity *> &matching_entities);

	EvaluableNodeReference ValueMassesToResult(OrderedHashMap<StringInternPool::StringID, double> &value_weights,
		EvaluableNodeManager *enm, EvaluableNodeRequestedValueTypes immediate_result);
}
