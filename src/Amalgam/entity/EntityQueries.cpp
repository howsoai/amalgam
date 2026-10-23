//project headers:
#include "Concurrency.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EntityQueriesStatistics.h"
#include "EntityQueryCaches.h"
#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"

bool _enable_SBF_datastore = true;

template<bool WriteMissing, bool PublicLabel = false>
static auto GetEntityNumberAccessor(const std::vector<Entity *> &matching_entities, StringInternPool::StringID label)
{
	return [&matching_entities, label](size_t i, double &value) {
		auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsNumber(label, PublicLabel);
		if(found || WriteMissing)
			value = ret_val;
		return found;
	};
}

double EntityQueries::ComputeNumericAggregate(const EntityQuery &query, const std::vector<Entity *> &matching_entities)
{
	auto [value_label, weight_label] = query.GetAggregateLabels();
	std::vector<double> values_buffer;
	return ComputeNumericAggregate<size_t>(query, 0, matching_entities.size(),
		GetEntityNumberAccessor<false, true>(matching_entities, value_label), weight_label != StringInternPool::NOT_A_STRING_ID,
		GetEntityNumberAccessor<true>(matching_entities, weight_label), values_buffer, nullptr);
}

EvaluableNodeReference EntityQueries::ComputeMode(const EntityQuery &query,
	const std::vector<Entity *> &matching_entities, EvaluableNodeManager *enm)
{
	auto [value_label, weight_label] = std::get<EntityQuery::Aggregate>(query.payload);
	auto get_key_string_value = [&matching_entities, label = value_label](size_t i, std::string &value) {
		auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsString(label, true, true);
		value = std::move(ret_val);
		return found;
	};

	auto [found, mode] = ModeString(static_cast<size_t>(0), matching_entities.size(), get_key_string_value,
		weight_label != StringInternPool::NOT_A_STRING_ID, GetEntityNumberAccessor<true>(matching_entities, weight_label));

	if(!found)
		return EvaluableNodeReference::Null();
	return Parser::ParseFromKeyString(mode, enm);
}

OrderedHashMap<StringInternPool::StringID, double> EntityQueries::ComputeValueMasses(const EntityQuery &query,
	const std::vector<Entity *> &matching_entities)
{
	auto [value_label, weight_label] = std::get<EntityQuery::Aggregate>(query.payload);
	auto get_key_string_value = [&matching_entities, label = value_label](size_t i, StringInternPool::StringID &value) {
		auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsStringIdWithReference(label, true, true);
		value = ret_val;
		return found;
	};

	return EntityQueriesStatistics::ValueMassesStringId<size_t>(0, matching_entities.size(), matching_entities.size(), get_key_string_value,
		weight_label != StringInternPool::NOT_A_STRING_ID, GetEntityNumberAccessor<false>(matching_entities, weight_label));
}

EvaluableNodeReference EntityQueries::ValueMassesToResult(OrderedHashMap<StringInternPool::StringID, double> &value_weights,
	EvaluableNodeManager *enm, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
	{
		double num_results = static_cast<double>(value_weights.size());
		for(auto &[value, weight] : value_weights)
			string_intern_pool.DestroyStringReference(value);
		return EvaluableNodeReference(num_results);
	}

	EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
	assoc->ReserveMappedChildNodes(value_weights.size());
	for(auto &[value, weight] : value_weights)
		assoc->SetMappedChildNodeWithReferenceHandoff(value, enm->AllocNode(weight));
	return EvaluableNodeReference(assoc, true);
}

bool EntityQuery::Distance::DoesEntityMatchCondition(Entity *e, Neighbors &neighbors)
{
	if(e == nullptr || e->GetIdStringId() == neighbors.entityIdToExclude)
		return false;

	std::vector<EvaluableNodeImmediateValueWithType> position(positionLabels.size());
	for(size_t i = 0; i < positionLabels.size(); i++)
	{
		auto [value, found] = e->GetValueAtLabelAsImmediateValue(positionLabels[i]);
		position[i] = value;
		if(position[i].nodeType == ENIVT_NOT_EXIST)
			return false;
	}

	double radius = 0.0;
	if(singleLabel != StringInternPool::NOT_A_STRING_ID)
	{
		auto [value, found] = e->GetValueAtLabelAsNumber(singleLabel);
		if(found)
			radius = value;
	}

	double distance = distEvaluator.ComputeMinkowskiDistance(position, neighbors.valuesToCompare,
		false, distEvaluator.highAccuracyDistances);
	if(distance - radius > neighbors.maxDistance)
		return false;

	return true;
}

double EntityQuery::Distance::GetConditionDistanceMeasure(Entity *e, Neighbors &neighbors, bool high_accuracy)
{
	if(e == nullptr)
		return std::numeric_limits<double>::quiet_NaN();

	//make sure not excluding this entity
	if(e->GetIdStringId() == neighbors.entityIdToExclude)
		return std::numeric_limits<double>::quiet_NaN();

	std::vector<EvaluableNodeImmediateValueWithType> position(positionLabels.size());
	for(size_t i = 0; i < positionLabels.size(); i++)
	{
		auto [value, found] = e->GetValueAtLabelAsImmediateValue(positionLabels[i]);
		position[i] = value;
		if(position[i].nodeType == ENIVT_NOT_EXIST)
			return std::numeric_limits<double>::quiet_NaN();
	}

	double radius = 0.0;
	if(singleLabel != StringInternPool::NOT_A_STRING_ID)
	{
		auto [value, found] = e->GetValueAtLabelAsNumber(singleLabel);
		if(found)
			radius = value;
	}

	double distance = distEvaluator.ComputeMinkowskiDistance(position, neighbors.valuesToCompare, false, high_accuracy);
	return distance - radius;
}

void EntityQueries::SelectEntities(EntityQuery &query, std::vector<Entity *> &matching_entities)
{
	auto &parameters = std::get<EntityQuery::Select>(query.payload);

	//regardless of options, need to sort entities by entity id
	EntityManipulation::SortEntitiesByID(matching_entities);

	size_t start_offset = std::min(matching_entities.size(), parameters.startOffset);
	size_t num_to_select = std::min(matching_entities.size() - start_offset, parameters.count);

	if(num_to_select == 0)
	{
		matching_entities.clear();
		return;
	}

	if(parameters.hasRandomStream)
	{
		size_t num_entities = matching_entities.size();
		if(parameters.hasStartOffset)
		{
			//shuffle all because we don't know what the starting offset will be and some values may be swapped with others
			for(size_t i = 0; i < num_entities; i++)
			{
				size_t index_to_swap = parameters.randomStream.RandSize(num_entities);
				std::swap(matching_entities[i], matching_entities[index_to_swap]);
			}
		}
		else //no start offset, only need to shuffle the number to be returned; don't worry about the rest because this sequence won't be resumed
		{
			for(size_t i = 0; i < num_to_select; i++)
			{
				size_t index_to_swap = parameters.randomStream.RandSize(num_entities);
				std::swap(matching_entities[i], matching_entities[index_to_swap]);
			}
		}
	}

	//remove any off the front based on start offset
	if(parameters.hasStartOffset)
		matching_entities.erase(begin(matching_entities), begin(matching_entities) + start_offset);

	//cut off everything but the number requested
	matching_entities.resize(num_to_select);
}

void EntityQueries::SampleEntities(EntityQuery &query, std::vector<Entity *> &source_entities, std::vector<Entity *> &matching_entities)
{
	auto &parameters = std::get<EntityQuery::Sample>(query.payload);

	size_t num_entities = source_entities.size();
	size_t num_to_sample = parameters.count;

	if(num_entities == 0 || num_to_sample == 0)
	{
		matching_entities.clear();
		return;
	}

	std::vector<Entity *> samples;
	samples.reserve(num_to_sample);

	RandomStream random_stream;
	if(parameters.hasRandomStream)
		random_stream = parameters.randomStream.CreateOtherStreamViaRand();
	else //just use a random seed
		random_stream.SetState("12345");

	if(parameters.valueLabel == StringInternPool::NOT_A_STRING_ID)
	{
		//select num_to_select entities and save them in the sample vector
		for(size_t i = 0; i < num_to_sample; i++)
		{
			size_t index_to_swap = parameters.randomStream.RandSize(num_entities);
			Entity *selected = source_entities[index_to_swap];
			samples.emplace_back(selected);
		}
	}
	else //weighted
	{
		//retrieve weights
		std::vector<double> entity_weights;
		entity_weights.reserve(num_to_sample);

		//retrieve and accumulate weights
		for(Entity *e : source_entities)
		{
			double value;
			double found;

			if(e != nullptr)
				std::tie(value, found) = e->GetValueAtLabelAsNumber(parameters.valueLabel);
			else
				found = false;

			if(found)
			{
				if(FastIsNaN(value))
					value = 0.0;

				entity_weights.push_back(value);
			}
			else
			{
				entity_weights.push_back(0.0);
			}
		}

		//if just one sample, brute-force it
		if(num_to_sample == 1)
		{
			size_t selected_index = WeightedDiscreteRandomSample(entity_weights, random_stream, true);
			Entity *selected = source_entities[selected_index];
			samples.emplace_back(selected);
		}
		else //build temporary cache and query
		{
			WeightedDiscreteRandomStreamTransform<Entity *> wdrst(source_entities, entity_weights, true);
			for(size_t i = 0; i < num_to_sample; i++)
				samples.emplace_back(wdrst.WeightedDiscreteRand(random_stream));
		}
	}

	//swap samples vector with the matching_entities
	std::swap(matching_entities, samples);
}

void EntityQueries::FindMinMax(const EntityQuery &query, std::vector<Entity *> &matching_entities)
{
	auto &parameters = std::get<EntityQuery::Extremum>(query.payload);

	//get values for each entity
	std::vector<std::pair<Entity *, EvaluableNodeImmediateValue>> entity_values;
	entity_values.reserve(matching_entities.size());
	for(size_t i = 0; i < matching_entities.size(); i++)
	{
		if(matching_entities[i] == nullptr)
			continue;

		auto [value, found] = matching_entities[i]->GetValueAtLabelAsImmediateValue(parameters.valueLabel, true);

		if(value.nodeType == parameters.valueType)
			entity_values.emplace_back(matching_entities[i], value.nodeValue);
	}

	auto sort_by = [&entity_values](auto less) {
		std::stable_sort(begin(entity_values), end(entity_values),
			[less](const auto &a, const auto &b) { return less(a.second, b.second); });
	};
	if(query.queryType == ENT_QUERY_MIN)
	{
		if(parameters.valueType == ENIVT_NUMBER)
			sort_by([](auto a, auto b) { return a.number < b.number; });
		else if(parameters.valueType == ENIVT_STRING_ID)
			sort_by([](auto a, auto b) { return StringIDNaturalCompareSort(a.stringID, b.stringID); });
	}
	else //ENT_QUERY_MAX
	{
		if(parameters.valueType == ENIVT_NUMBER)
			sort_by([](auto a, auto b) { return a.number > b.number; });
		else if(parameters.valueType == ENIVT_STRING_ID)
			sort_by([](auto a, auto b) { return StringIDNaturalCompareSortReverse(a.stringID, b.stringID); });
	}

	//delete elements beyond the number to keep
	size_t num_to_keep = std::min(parameters.count, entity_values.size());
	entity_values.erase(begin(entity_values) + num_to_keep, end(entity_values));

	//only copy over entities to keep
	matching_entities.resize(entity_values.size());
	for(size_t i = 0; i < entity_values.size(); i++)
		matching_entities[i] = entity_values[i].first;
}

EvaluableNodeReference EntityQuery::Distance::GetMatchingEntities(EvaluableNodeType query_type, Entity *container,
	std::vector<Entity *> &matching_entities, bool from_all_entities, EvaluableNodeManager *enm)
{
	auto &neighbors = std::get<Neighbors>(target);

	if(from_all_entities)
	{
		//start with all entities
		matching_entities.reserve(container->GetContainedEntities().size());
		for(auto entity : container->GetContainedEntities())
			matching_entities.push_back(entity);
	}

	switch(query_type)
	{
	case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
	{
		if(distEvaluator.populateOmittedFeatureValues)
		{
			//populate values from the comparison entity
			Entity *comparison_entity = container->GetContainedEntity(neighbors.entityIdToExclude);
			neighbors.valuesToCompare.resize(positionLabels.size());
			for(size_t i = 0; i < positionLabels.size(); i++)
			{
				auto [value, found] = comparison_entity->GetValueAtLabelAsImmediateValue(positionLabels[i]);
				neighbors.valuesToCompare[i] = value;
			}
		}

		size_t num_to_keep = std::min(maxToRetrieve, matching_entities.size());

		distEvaluator.InitializeParametersAndFeatureParams();

		//break ties by each entity's index in its container
		struct EntityReferenceToIndex
		{
			inline size_t operator()(const DistanceReferencePair<Entity *> &drp) const
			{
				return drp.reference->GetEntityIndexOfContainer();
			}
		};

		//get values for each entity
		StochasticTieBreakingPriorityQueue<DistanceReferencePair<Entity *>, double, EntityReferenceToIndex> nearest_entities(randomStream);
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			double value = GetConditionDistanceMeasure(matching_entities[i], neighbors, distEvaluator.highAccuracyDistances);
			if(FastIsNaN(value))
				continue;

			nearest_entities.Push(DistanceReferencePair<Entity *>(value, matching_entities[i]));

			if(nearest_entities.Size() > num_to_keep)
				nearest_entities.Pop();
		}

		//retrieve the top k cases into entity_values
		std::vector<DistanceReferencePair<Entity *>> entity_values;
		entity_values.reserve(num_to_keep);
		for(size_t i = 0; i < num_to_keep && nearest_entities.Size() > 0; i++)
		{
			auto &dist_ent = nearest_entities.Top();
			entity_values.emplace_back(dist_ent.distance, dist_ent.reference);

			nearest_entities.Pop();
		}

		//reduce matching_entities to only those needed
		matching_entities.resize(entity_values.size());
		for(size_t i = 0; i < entity_values.size(); i++)
			matching_entities[i] = entity_values[i].reference;

		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		if(!distEvaluator.highAccuracyDistances && distEvaluator.recomputeAccurateDistances)
		{
			//recompute distance accurately for each found entity result
			for(auto &it : entity_values)
				it.distance = GetConditionDistanceMeasure(it.reference, neighbors, true);
		}

		auto weight_function = [this](Entity *e, double &weight)
			{
				bool found = false;
				std::tie(weight, found) = e->GetValueAtLabelAsNumber(weightLabel);
				if(!found)
					weight = 1.0;
				return true;
			};

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(distEvaluator.computeSurprisal,
			distEvaluator.transformSurprisalToProb, distanceWeightExponent,
			minToRetrieve, maxToRetrieve, numToRetrieveMinIncrementalProbability, extraToRetrieve,
			weightLabel != StringInternPool::NOT_A_STRING_ID, 0.0, weight_function);

		distance_transform.TransformDistances(entity_values, returnSortedList);

		return EntityManipulation::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabels, [](auto entity) { return entity;  });
	}

	case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
	{
		if(distEvaluator.populateOmittedFeatureValues)
		{
			//populate values from the comparison entity
			Entity *comparison_entity = container->GetContainedEntity(neighbors.entityIdToExclude);
			neighbors.valuesToCompare.resize(positionLabels.size());
			for(size_t i = 0; i < positionLabels.size(); i++)
			{
				auto [value, found] = comparison_entity->GetValueAtLabelAsImmediateValue(positionLabels[i]);
				neighbors.valuesToCompare[i] = value;
			}
		}

		distEvaluator.InitializeParametersAndFeatureParams();
		//find those that match
		std::erase_if(matching_entities,
			[this, &neighbors](Entity *entity) { return !DoesEntityMatchCondition(entity, neighbors); });

		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		//returned distances may need higher precision than the distances used for filtering
		bool high_accuracy = (distEvaluator.highAccuracyDistances || distEvaluator.recomputeAccurateDistances);
		std::vector<DistanceReferencePair<Entity *>> entity_values;
		entity_values.reserve(matching_entities.size());
		for(size_t i = 0; i < matching_entities.size(); i++)
			entity_values.emplace_back(GetConditionDistanceMeasure(matching_entities[i], neighbors, high_accuracy), matching_entities[i]);


		auto weight_function = [this]
		(Entity *e, double &weight)
			{
				bool found = false;
				std::tie(weight, found) = e->GetValueAtLabelAsNumber(weightLabel);
				if(!found)
					weight = 1.0;
				return true;
			};

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(distEvaluator.computeSurprisal,
			distEvaluator.transformSurprisalToProb, distanceWeightExponent,
			minToRetrieve, maxToRetrieve, numToRetrieveMinIncrementalProbability, extraToRetrieve,
			weightLabel != StringInternPool::NOT_A_STRING_ID, 0.0, weight_function);

		distance_transform.TransformDistances(entity_values, returnSortedList);

		return EntityManipulation::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabels, [](auto entity) { return entity;  });
	}

	default:
		std::erase_if(matching_entities,
			[this, &neighbors](Entity *entity) { return !DoesEntityMatchCondition(entity, neighbors); });

		return EvaluableNodeReference::Null();
	}
}

std::tuple<EvaluableNodeImmediateValueType, EvaluableNodeImmediateValue, EvaluableNodeImmediateValue>
EntityQueries::GetQueryRange(EvaluableNode *query)
{
	auto type = query->GetType();
	auto &ocn = query->GetOrderedChildNodes();
	EvaluableNode *low = ocn[1];
	EvaluableNode *high = ocn.size() > 2 ? ocn[2] : nullptr;
	if(type == ENT_QUERY_LESS_OR_EQUAL_TO || type == ENT_QUERY_GREATER_OR_EQUAL_TO)
	{
		if(EvaluableNode::IsNumericOrNull(low))
		{
			double value = EvaluableNode::ToNumber(low);
			return {ENIVT_NUMBER,
				type == ENT_QUERY_LESS_OR_EQUAL_TO ? -std::numeric_limits<double>::infinity() : value,
				type == ENT_QUERY_LESS_OR_EQUAL_TO ? value : std::numeric_limits<double>::infinity()};
		}
		auto value = EvaluableNode::ToStringIDIfExists(low).second;
		return {ENIVT_STRING_ID,
			type == ENT_QUERY_LESS_OR_EQUAL_TO ? StringInternPool::NOT_A_STRING_ID : value,
			type == ENT_QUERY_LESS_OR_EQUAL_TO ? value : StringInternPool::NOT_A_STRING_ID};
	}
	if(EvaluableNode::IsNumericOrNull(low) || EvaluableNode::IsNumericOrNull(high))
		return {ENIVT_NUMBER, EvaluableNode::ToNumber(low), EvaluableNode::ToNumber(high)};
	return {ENIVT_STRING_ID, EvaluableNode::ToStringIDIfExists(low).second, EvaluableNode::ToStringIDIfExists(high).second};
}

void EntityQueries::FilterEntities(const EntityQuery &query, Entity *container,
	std::vector<Entity *> &matching_entities, bool is_first)
{
	auto type = query.queryType;
	if(type == ENT_NULL)
	{
		matching_entities.clear();
		return;
	}

	if(type == ENT_QUERY_IN_ENTITY_LIST || type == ENT_QUERY_NOT_IN_ENTITY_LIST)
	{
		IterateOverFilteredEntities(query, [&](const auto &ids, auto get_id) {
			if(is_first && type == ENT_QUERY_IN_ENTITY_LIST)
			{
				matching_entities.reserve(ids.size());
				for(auto item : ids)
					if(auto entity = container->GetContainedEntity(get_id(item)))
						matching_entities.push_back(entity);
			}
			else
			{
				if(is_first)
					matching_entities = container->GetContainedEntities();
				std::erase_if(matching_entities, [&](Entity *entity) {
					auto id = entity->GetIdStringId();
					bool found = std::any_of(begin(ids), end(ids), [&](auto item) { return get_id(item) == id; });
					return found == (type == ENT_QUERY_NOT_IN_ENTITY_LIST);
				});
			}
		});
		return;
	}

	auto &filter = std::get<EntityQuery::Filter>(query.payload);
	auto &ocn = filter.node->GetOrderedChildNodes();
	//the builder has already validated this label as public
	auto label = filter.label;

	//on the first filter, copy only matches into the result
	auto filter_entities = [&](auto remove) {
		if(!is_first)
			std::erase_if(matching_entities, remove);
		else
		{
			matching_entities.clear();
			matching_entities.reserve(container->GetNumContainedEntities());
			for(auto entity : container->GetContainedEntities())
				if(!remove(entity))
					matching_entities.push_back(entity);
		}
	};

	switch(type)
	{
	case ENT_QUERY_EXISTS:
	case ENT_QUERY_NOT_EXISTS:
		filter_entities([&](Entity *entity) {
			return entity->DoesLabelExist(label) == (type == ENT_QUERY_NOT_EXISTS);
		});
		break;

	case ENT_QUERY_EQUALS:
	case ENT_QUERY_NOT_EQUALS:
	{
		auto compare_value = EvaluableNodeImmediateValueWithType::CreateValueFromEvaluableNode(ocn[1]);
		filter_entities([&](Entity *entity) {
			auto [value, found] = entity->GetValueAtLabelAsImmediateValue(label, true);
			return value.nodeType == ENIVT_NOT_EXIST
				|| EvaluableNodeImmediateValueWithType::AreEqualGivenImmediateValuesNotCode(compare_value, value)
					== (type == ENT_QUERY_NOT_EQUALS);
		});
		break;
	}

	case ENT_QUERY_BETWEEN:
	case ENT_QUERY_NOT_BETWEEN:
	case ENT_QUERY_LESS_OR_EQUAL_TO:
	case ENT_QUERY_GREATER_OR_EQUAL_TO:
	{
		auto [value_type, low, high] = GetQueryRange(filter.node);
		if(value_type == ENIVT_NUMBER)
			filter_entities([&](Entity *entity) {
				auto [value, found] = entity->GetValueAtLabelAsNumber(label, true);
				if(!found)
					return true;
				if(type == ENT_QUERY_NOT_BETWEEN)
					return value >= low.number && high.number >= value;
				return value < low.number || high.number < value;
			});
		else
			filter_entities([&](Entity *entity) {
				auto [value, found] = entity->GetValueAtLabelAsString(label, true);
				if(!found)
					return true;
				bool within = StringManipulation::StringNaturalCompare(value, string_intern_pool.GetStringViewFromID(low.stringID)) > 0
					&& StringManipulation::StringNaturalCompare(string_intern_pool.GetStringViewFromID(high.stringID), value) > 0;
				return within == (type == ENT_QUERY_NOT_BETWEEN);
			});
		break;
	}

	case ENT_QUERY_AMONG:
	case ENT_QUERY_NOT_AMONG:
		filter_entities([&](Entity *entity) {
			auto [value, found] = entity->GetValueAtLabelAsImmediateValue(label, true);
			if(value.nodeType == ENIVT_NOT_EXIST)
				return true;
			bool matches = false;
			if(ocn.size() > 1 && EvaluableNode::IsOrderedArray(ocn[1]))
				for(auto node : ocn[1]->GetOrderedChildNodesReference())
				{
					EvaluableNodeImmediateValue compare_value;
					auto compare_type = compare_value.CopyValueFromEvaluableNode(node);
					if(EvaluableNodeImmediateValue::AreEqualGivenImmediateValuesNotCode(
						value.nodeType, value.nodeValue, compare_type, compare_value))
					{
						matches = true;
						break;
					}
				}
			return matches == (type == ENT_QUERY_NOT_AMONG);
		});
		break;

	default:;
	}
}
