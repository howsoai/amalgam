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
	inline EvaluableNode *GetFilterNode(const EntityQuery &query)
	{
		if(auto filter = std::get_if<EntityQuery::Filter>(&query.payload))
			return filter->node;
		return nullptr;
	}

	inline StringInternPool::StringID GetFilterLabel(const EntityQuery &query)
	{
		return std::get<EntityQuery::Filter>(query.payload).label;
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

	//calls function once with the borrowed or converted ID list and its accessor, before entity iteration
	template<typename Function>
	__forceinline void IterateOverEntityList(const EntityQuery &query, Function function)
	{
		if(auto filter = std::get_if<EntityQuery::Filter>(&query.payload))
		{
			std::span<EvaluableNode *const> nodes;
			auto &ocn = filter->node->GetOrderedChildNodes();
			if(ocn.size() > 0 && EvaluableNode::IsOrderedArray(ocn[0]))
				nodes = ocn[0]->GetOrderedChildNodesReference();

			function(nodes, [](auto node) {
				return EvaluableNode::IsNull(node) ? StringInternPool::NOT_A_STRING_ID : node->GetStringIDReference();
			});
		}
		else
		{
			function(std::get<EntityQuery::EntityIds>(query.payload), [](auto id) { return id; });
		}
	}

	//iterates over consecutive existence filters of the same type
	template<typename Function>
	void ForEachQueryLabel(const std::vector<EntityQuery> &queries, size_t query_index, Function function)
	{
		auto type = queries[query_index].queryType;
		if(type == ENT_QUERY_EXISTS || type == ENT_QUERY_NOT_EXISTS)
		{
			size_t first = query_index;
			while(first > 0 && queries[first - 1].queryType == type)
				--first;
			for(size_t i = first; i <= query_index; i++)
				function(GetFilterLabel(queries[i]));
		}
		else if(type == ENT_QUERY_IN_ENTITY_LIST || type == ENT_QUERY_NOT_IN_ENTITY_LIST)
		{
			IterateOverEntityList(queries[query_index], [&](const auto &ids, auto get_id) {
				for(auto item : ids)
				{
					if(auto id = get_id(item); id != StringInternPool::NOT_A_STRING_ID)
						function(id);
				}
			});
		}
	}
}
