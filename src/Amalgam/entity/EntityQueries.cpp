//project headers:
#include "EntityQueries.h"
#include "Concurrency.h"
#include "EntityQueryManager.h"
#include "EntityQueryCaches.h"
#include "EvaluableNodeTreeFunctions.h"

bool _enable_SBF_datastore = true;

#ifdef MULTITHREAD_SUPPORT
Concurrency::ReadWriteMutex EntityQueryManager::queryCacheMutex;
#endif

FastHashMap<Entity *, std::unique_ptr<EntityQueryCaches>> EntityQueryManager::queryCaches;

size_t EntityQueryManager::maxEntitiesBruteForceSearch = 10;

bool EntityQueryCondition::DoesEntityMatchCondition(Entity *e)
{
	if(e == nullptr)
		return false;

	switch(queryType)
	{
		case ENT_NULL:
			return false;

		case ENT_QUERY_SELECT:
		case ENT_QUERY_SAMPLE:
		case ENT_QUERY_WEIGHTED_SAMPLE:
		case ENT_QUERY_COUNT:
			//it does not fail the condition here - needs to be checked elsewhere
			return true;

		case ENT_QUERY_IN_ENTITY_LIST:
			return std::find(begin(existLabels), end(existLabels), e->GetIdStringId()) != end(existLabels);

		case ENT_QUERY_NOT_IN_ENTITY_LIST:
			return std::find(begin(existLabels), end(existLabels), e->GetIdStringId()) == end(existLabels);

		case ENT_QUERY_EXISTS:
			for(auto &label : existLabels)
			{
				if(!e->DoesLabelExist(label))
					return false;
			}
			return true;
	
		case ENT_QUERY_NOT_EXISTS:
			for(auto &label : existLabels)
			{
				if(e->DoesLabelExist(label))
					return false;
			}
			return true;
	
		case ENT_QUERY_EQUALS:
			for(size_t i = 0; i < singleLabels.size(); i++)
			{
				auto &[label_id, compare_value] = singleLabels[i];
				auto compare_type = valueTypes[i];

				EvaluableNodeImmediateValue value;
				auto value_type = e->GetValueAtLabelAsImmediateValue(label_id, value);

				//needs to exist
				if(value_type == ENIVT_NOT_EXIST)
					return false;

				if(!EvaluableNodeImmediateValue::AreEqual(compare_type, compare_value, value_type, value))
					return false;
			}
			return true;

		case ENT_QUERY_NOT_EQUALS:
			for(size_t i = 0; i < singleLabels.size(); i++)
			{
				auto &[label_id, compare_value] = singleLabels[i];
				auto compare_type = valueTypes[i];

				EvaluableNodeImmediateValue value;
				auto value_type = e->GetValueAtLabelAsImmediateValue(label_id, value);

				//needs to exist
				if(value_type == ENIVT_NOT_EXIST)
					return false;

				if(EvaluableNodeImmediateValue::AreEqual(compare_type, compare_value, value_type, value))
					return false;
			}
			return true;

		case ENT_QUERY_BETWEEN:
			for(size_t i = 0; i < pairedLabels.size(); i++)
			{
				auto &[label_id, range] = pairedLabels[i];

				if(valueTypes[i] == ENIVT_NUMBER)
				{
					double value;
					if(!e->GetValueAtLabelAsNumber(label_id, value))
						return false;

					if(value < range.first.number || range.second.number < value)
						return false;
				}
				else if(valueTypes[i] == ENIVT_STRING_ID)
				{
					StringInternPool::StringID value;
					if(!e->GetValueAtLabelAsStringId(label_id, value))
						return false;

					if(StringNaturalCompare(value, range.first.stringID) <= 0 || StringNaturalCompare(range.second.stringID, value) <= 0)
						return false;
				}
			}
			return true;

		case ENT_QUERY_NOT_BETWEEN:
			for(size_t i = 0; i < pairedLabels.size(); i++)
			{
				auto &[label_id, range] = pairedLabels[i];

				if(valueTypes[i] == ENIVT_NUMBER)
				{
					double value;
					if(!e->GetValueAtLabelAsNumber(label_id, value))
						return false;

					if(value >= range.first.number && range.second.number >= value)
						return false;
				}
				else if(valueTypes[i] == ENIVT_STRING_ID)
				{
					StringInternPool::StringID value;
					if(!e->GetValueAtLabelAsStringId(label_id, value))
						return false;

					if(StringNaturalCompare(value, range.first.stringID) > 0 && StringNaturalCompare(range.second.stringID, value) > 0)
						return false;
				}
			}
			return true;

		case ENT_QUERY_AMONG:
		{
			EvaluableNodeImmediateValue value;
			auto value_type = e->GetValueAtLabelAsImmediateValue(singleLabel, value);

			if(value_type == ENIVT_NOT_EXIST)
				return false;
			
			for(size_t i = 0; i < valueToCompare.size(); i++)
			{
				//make sure same type
				if(value_type != valueTypes[i])
					return false;

				if(EvaluableNodeImmediateValue::AreEqual(value_type, value, valueTypes[i], valueToCompare[i]))
					return true;
			}

			return false;
		}

		case ENT_QUERY_NOT_AMONG:
		{
			EvaluableNodeImmediateValue value;
			auto value_type = e->GetValueAtLabelAsImmediateValue(singleLabel, value);

			if(value_type == ENIVT_NOT_EXIST)
				return false;

			for(size_t i = 0; i < valueToCompare.size(); i++)
			{
				//make sure same type
				if(value_type != valueTypes[i])
					return false;

				if(EvaluableNodeImmediateValue::AreEqual(value_type, value, valueTypes[i], valueToCompare[i]))
					return false;
			}

			return true;
		}

		case ENT_QUERY_MAX:
		case ENT_QUERY_MIN:
		case ENT_QUERY_SUM:
		case ENT_QUERY_MODE:
		case ENT_QUERY_QUANTILE:
		case ENT_QUERY_GENERALIZED_MEAN:
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		case ENT_QUERY_VALUE_MASSES:
			//it does not fail the condition here - needs to be checked elsewhere
			return true;

		case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
		{
			std::vector<EvaluableNodeImmediateValue> position(positionLabels.size());
			std::vector<EvaluableNodeImmediateValueType> position_types(positionLabels.size());
			for(size_t i = 0; i < positionLabels.size(); i++)
			{
				position_types[i] = e->GetValueAtLabelAsImmediateValue(positionLabels[i], position[i]);
				if(position_types[i] == ENIVT_NOT_EXIST)
					return false;
			}

			double radius = 0.0;
			if(singleLabel != StringInternPool::NOT_A_STRING_ID)
			{
				double value;
				if(e->GetValueAtLabelAsNumber(singleLabel, value))
					radius = value;
			}

			double distance = distParams.ComputeMinkowskiDistance(position, position_types, valueToCompare, valueTypes);
			if(distance - radius > maxDistance)
				return false;

			return true;
		}

		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
			//it does not fail the condition here - needs to be checked elsewhere
			return true;

		case ENT_COMPUTE_ENTITY_CONVICTIONS:
		case ENT_COMPUTE_ENTITY_KL_DIVERGENCES:
		case ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE:
		case ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS:
			return false;

		default:
			// eliminates compiler warnings on clang.
			break;
	}

	return false;
}

double EntityQueryCondition::GetConditionDistanceMeasure(Entity *e)
{
	if(e == nullptr)
		return std::numeric_limits<double>::quiet_NaN();

	//make sure not excluding this entity
	if(e->GetIdStringId() == exclusionLabel)
		return std::numeric_limits<double>::quiet_NaN();

	std::vector<EvaluableNodeImmediateValue> position(positionLabels.size());
	std::vector<EvaluableNodeImmediateValueType> position_types(positionLabels.size());
	for(size_t i = 0; i < positionLabels.size(); i++)
	{
		position_types[i] = e->GetValueAtLabelAsImmediateValue(positionLabels[i], position[i]);
		if(position_types[i] == ENIVT_NOT_EXIST)
			return std::numeric_limits<double>::quiet_NaN();
	}

	double radius = 0.0;
	if(singleLabel != StringInternPool::NOT_A_STRING_ID)
	{
		double value;
		if(e->GetValueAtLabelAsNumber(singleLabel, value))
			radius = value;
	}
		
	double distance = distParams.ComputeMinkowskiDistance(position, position_types, valueToCompare, valueTypes);
	return distance - radius;
}

EvaluableNodeReference EntityQueryCondition::GetMatchingEntities(Entity *container,
	std::vector<Entity *> &matching_entities, bool from_all_entities, EvaluableNodeManager *enm)
{
	if(from_all_entities)
	{
		//if the specific entities are specified, then just use those
		if(queryType == ENT_QUERY_IN_ENTITY_LIST)
		{
			//only need to select those from within the list
			matching_entities.reserve(existLabels.size());
			for(auto &entity_sid : existLabels)
			{
				auto entity = container->GetContainedEntity(entity_sid);
				if(entity != nullptr)
					matching_entities.push_back(entity);
			}
			return EvaluableNodeReference::Null();
		}

		//else, start with all entities
		matching_entities.reserve(container->GetContainedEntities().size());
		for(auto entity : container->GetContainedEntities())
			matching_entities.push_back(entity);
	}

	switch(queryType)
	{
	case ENT_QUERY_SELECT:
	{
		//regardless of options, need to sort entities by entity id
		EntityQueryManager::SortEntitiesByID(matching_entities);

		size_t start_offset = std::min(matching_entities.size(), startOffset);
		size_t num_to_select = std::min(matching_entities.size() - start_offset, static_cast<size_t>(maxToRetrieve));

		if(num_to_select == 0)
		{
			matching_entities.clear();
			return EvaluableNodeReference::Null();
		}

		if(hasRandomStream)
		{
			size_t num_entities = matching_entities.size();
			if(hasStartOffset)
			{
				//shuffle all because we don't know what the starting offset will be and some values may be swapped with others
				for(size_t i = 0; i < num_entities; i++)
				{
					size_t index_to_swap = randomStream.RandSize(num_entities);
					std::swap(matching_entities[i], matching_entities[index_to_swap]);
				}
			}
			else //no start offset, only need to shuffle the number to be returned; don't worry about the rest because this sequence won't be resumed
			{
				for(size_t i = 0; i < num_to_select; i++)
				{
					size_t index_to_swap = randomStream.RandSize(num_entities);
					std::swap(matching_entities[i], matching_entities[index_to_swap]);
				}
			}
		}

		//remove any off the front based on start offset
		if(hasStartOffset)
			matching_entities.erase(begin(matching_entities), begin(matching_entities) + start_offset);

		//cut off everything but the number requested
		matching_entities.resize(num_to_select);
		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_SAMPLE:
	{
		size_t num_entities = matching_entities.size();
		size_t num_to_sample = static_cast<size_t>(maxToRetrieve);

		if(num_entities == 0 || num_to_sample == 0)
		{
			matching_entities.clear();
			return EvaluableNodeReference::Null();
		}

		std::vector<Entity *> samples;
		samples.reserve(num_to_sample);

		//obtain random stream either from the condition or use a default one
		RandomStream random_stream;
		if(hasRandomStream)
			random_stream = randomStream.CreateOtherStreamViaRand();
		else //just use a random seed
			random_stream.SetState("12345");

		//select num_to_select entities and save them in the sample vector
		for(size_t i = 0; i < num_to_sample; i++)
		{
			size_t index_to_swap = randomStream.RandSize(num_entities);
			Entity *selected = matching_entities[index_to_swap];
			samples.emplace_back(selected);
		}

		//swap samples vector with the matching_entities 
		std::swap(matching_entities, samples);
		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_WEIGHTED_SAMPLE:
	{
		size_t num_entities = matching_entities.size();
		size_t num_to_sample = static_cast<size_t>(maxToRetrieve);
		auto weight_label_id = singleLabel;

		if(num_entities == 0 || num_to_sample == 0)
		{
			matching_entities.clear();
			return EvaluableNodeReference::Null();
		}

		//retrieve weights
		std::vector<double> entity_weights;
		entity_weights.reserve(num_to_sample);

		//retrieve and accumulate weights
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			double value;
			Entity *e = matching_entities[i];
			if(e != nullptr && e->GetValueAtLabelAsNumber(weight_label_id, value))
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

		//obtain random stream either from the condition or use a default one
		RandomStream random_stream;
		if(hasRandomStream)
			random_stream = randomStream.CreateOtherStreamViaRand();
		else //just use a random seed
			random_stream.SetState("12345");

		std::vector<Entity *> samples;
		samples.reserve(num_to_sample);

		//if just one sample, brute-force it
		if(num_to_sample == 1)
		{
			size_t selected_index = WeightedDiscreteRandomSample(entity_weights, random_stream, true);
			Entity *selected = matching_entities[selected_index];
			samples.emplace_back(selected);
		}
		else //build temporary cache and query
		{
			WeightedDiscreteRandomStreamTransform<Entity *> wdrst(matching_entities, entity_weights, true);
			for(size_t i = 0; i < num_to_sample; i++)
				samples.emplace_back(wdrst.WeightedDiscreteRand(random_stream));
		}

		//swap samples vector with the matching_entities 
		std::swap(matching_entities, samples);
		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_COUNT:
	{
		//not useful unless computing
		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		return EvaluableNodeReference(enm->AllocNode(static_cast<double>(matching_entities.size())), true);
	}

	case ENT_QUERY_EXISTS:
	{
		//find those that match
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			//if it doesn't match the condition, then remove it
			if(!DoesEntityMatchCondition(matching_entities[i]))
			{
				matching_entities.erase(begin(matching_entities) + i);
				i--;
			}
		}

		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		//get values for each entity
		EvaluableNode *query_return = enm->AllocNode(ENT_ASSOC);
		query_return->ReserveMappedChildNodes(matching_entities.size());
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			if(matching_entities[i] == nullptr)
				continue;

			//create assoc for values for each entity
			StringInternPool::StringID entity_sid = matching_entities[i]->GetIdStringId();
			EvaluableNode *entity_values = enm->AllocNode(ENT_ASSOC);
			entity_values->ReserveMappedChildNodes(existLabels.size());
			query_return->SetMappedChildNode(entity_sid, entity_values);

			//get values
			auto &exist_labels = existLabels;
			string_intern_pool.CreateStringReferences(exist_labels);
			for(auto label_sid : exist_labels)
				entity_values->SetMappedChildNodeWithReferenceHandoff(label_sid, matching_entities[i]->GetValueAtLabel(label_sid, enm, false));
		}

		return EvaluableNodeReference(query_return, true);
	}

	case ENT_QUERY_MAX:
	case ENT_QUERY_MIN:
	{
		//get values for each entity
		std::vector<std::pair<Entity *, EvaluableNodeImmediateValue>> entity_values;
		entity_values.reserve(matching_entities.size());
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			if(matching_entities[i] == nullptr)
				continue;

			EvaluableNodeImmediateValue value;
			auto value_type = matching_entities[i]->GetValueAtLabelAsImmediateValue(singleLabel, value);

			if(value_type == singleLabelType)
				entity_values.push_back(std::make_pair(matching_entities[i], value));
		}

		//sort entites by value
		if(queryType == ENT_QUERY_MIN)
		{
			if(singleLabelType == ENIVT_NUMBER)
			{
				std::sort(begin(entity_values), end(entity_values),
					[](std::pair<Entity *, EvaluableNodeImmediateValue> a, std::pair<Entity *, EvaluableNodeImmediateValue> b) -> bool
					{	return a.second.number < b.second.number;	});
			}
			else if(singleLabelType == ENIVT_STRING_ID)
			{
				std::sort(begin(entity_values), end(entity_values),
					[](std::pair<Entity *, EvaluableNodeImmediateValue> a, std::pair<Entity *, EvaluableNodeImmediateValue> b) -> bool
					{	return StringIDNaturalCompareSort(a.second.stringID, b.second.stringID);	});
			}
		}
		else //ENT_QUERY_MAX
		{
			if(singleLabelType == ENIVT_NUMBER)
			{
				std::sort(begin(entity_values), end(entity_values),
					[](std::pair<Entity *, EvaluableNodeImmediateValue> a, std::pair<Entity *, EvaluableNodeImmediateValue> b) -> bool
					{	return a.second.number > b.second.number;	});
			}
			else if(singleLabelType == ENIVT_STRING_ID)
			{
				std::sort(begin(entity_values), end(entity_values),
					[](std::pair<Entity *, EvaluableNodeImmediateValue> a, std::pair<Entity *, EvaluableNodeImmediateValue> b) -> bool
					{	return StringIDNaturalCompareSortReverse(a.second.stringID, b.second.stringID);	});
			}
		}

		//delete elements beyond the number to keep
		size_t num_to_keep = std::min(static_cast<size_t>(maxToRetrieve), entity_values.size());
		entity_values.erase(begin(entity_values) + num_to_keep, end(entity_values));

		//only copy over entities to keep
		matching_entities.resize(entity_values.size());
		for(size_t i = 0; i < entity_values.size(); i++)
			matching_entities[i] = entity_values[i].first;

		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_SUM:
	case ENT_QUERY_MODE:
	case ENT_QUERY_QUANTILE:
	case ENT_QUERY_GENERALIZED_MEAN:
	case ENT_QUERY_MIN_DIFFERENCE:
	case ENT_QUERY_MAX_DIFFERENCE:
	{
		//not useful unless computing
		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		auto get_value = [matching_entities, this]
		(size_t i, double &value)
		{
			return matching_entities[i]->GetValueAtLabelAsNumber(singleLabel, value);
		};

		auto get_weight = [matching_entities, this]
		(size_t i, double &weight_value)
		{
			return matching_entities[i]->GetValueAtLabelAsNumber(weightLabel, weight_value);
		};

		switch(queryType)
		{
		case ENT_QUERY_SUM:
		{
			double sum = EntityQueriesStatistics::Sum<size_t>(0, matching_entities.size(), get_value,
				weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight);
			return EvaluableNodeReference(enm->AllocNode(sum), true);
		}
		case ENT_QUERY_MODE:
		{
			if(singleLabelType == ENIVT_NUMBER)
			{
				double mode = EntityQueriesStatistics::ModeNumber<size_t>(0, matching_entities.size(), get_value,
					weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight);
				return EvaluableNodeReference(enm->AllocNode(mode), true);
			}
			else if(singleLabelType == ENIVT_STRING_ID)
			{
				auto get_string_value = [matching_entities, this]
				(size_t i, StringInternPool::StringID &value)
				{
					return matching_entities[i]->GetValueAtLabelAsStringId(singleLabel, value);
				};

				auto [found, mode_id] = EntityQueriesStatistics::ModeStringId<size_t>(
					0, matching_entities.size(), get_string_value, true, get_weight);

				if(found)
					return EvaluableNodeReference(enm->AllocNode(ENT_STRING, mode_id), true);
				else
					return EvaluableNodeReference::Null();
			}
			break;
		}
		case ENT_QUERY_QUANTILE:
		{
			std::vector<std::pair<double,double>> values_buffer;
			double quantile = EntityQueriesStatistics::Quantile<size_t>(0, matching_entities.size(), get_value,
				weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight, qPercentage, values_buffer);
			return EvaluableNodeReference(enm->AllocNode(quantile), true);
		}
		case ENT_QUERY_GENERALIZED_MEAN:
		{
			double generalized_mean = EntityQueriesStatistics::GeneralizedMean<size_t>(0, matching_entities.size(), get_value,
				weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight, distParams.pValue, center, calculateMoment, absoluteValue);
			return EvaluableNodeReference(enm->AllocNode(generalized_mean), true);
		}
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		{
			std::vector<double> values_buffer;
			double extreme_value = EntityQueriesStatistics::ExtremeDifference<size_t>(0, matching_entities.size(), get_value,
				queryType == ENT_QUERY_MIN_DIFFERENCE, maxDistance, includeZeroDifferences, values_buffer);
			return EvaluableNodeReference(enm->AllocNode(extreme_value), true);
		}
		default:
			break;
		}

		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_VALUE_MASSES:
	{
		//not useful unless computing
		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		if(singleLabelType == ENIVT_NUMBER)
		{
			auto get_value = [matching_entities, this]
			(size_t i, double &value)
			{
				return matching_entities[i]->GetValueAtLabelAsNumber(singleLabel, value);
			};

			auto get_weight = [matching_entities, this]
			(size_t i, double &weight_value)
			{
				return matching_entities[i]->GetValueAtLabelAsNumber(weightLabel, weight_value);
			};

			auto value_weights = EntityQueriesStatistics::ValueMassesNumber<size_t>(0, matching_entities.size(), matching_entities.size(),
				get_value, weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight);

			EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
			assoc->ReserveMappedChildNodes(value_weights.size());

			std::string string_value;
			for(auto &[value, weight] : value_weights)
			{
				string_value = EvaluableNode::NumberToString(value);
				assoc->SetMappedChildNode(string_value, enm->AllocNode(weight));
			}

			return EvaluableNodeReference(assoc, true);
		}
		else if(singleLabelType == ENIVT_STRING_ID)
		{
			auto get_value = [matching_entities, this]
			(size_t i, StringInternPool::StringID &value)
			{
				return matching_entities[i]->GetValueAtLabelAsStringId(singleLabel, value);
			};

			auto get_weight = [matching_entities, this]
			(size_t i, double &weight_value)
			{
				return matching_entities[i]->GetValueAtLabelAsNumber(weightLabel, weight_value);
			};

			auto value_weights = EntityQueriesStatistics::ValueMassesStringId<size_t>(0, matching_entities.size(), matching_entities.size(), get_value,
				weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight);

			EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
			assoc->ReserveMappedChildNodes(value_weights.size());

			for(auto &[value, weight] : value_weights)
				assoc->SetMappedChildNode(value, enm->AllocNode(weight));

			return EvaluableNodeReference(assoc, true);
		}

		return EvaluableNodeReference::Null();
	}

	case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
	{
		size_t num_to_keep = std::min(static_cast<size_t>(maxToRetrieve), matching_entities.size());

		//get values for each entity
		StochasticTieBreakingPriorityQueue<DistanceReferencePair<Entity *>> nearest_entities(randomStream.CreateOtherStreamViaRand());
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			double value = GetConditionDistanceMeasure(matching_entities[i]);
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
			entity_values.push_back(DistanceReferencePair<Entity *>(dist_ent.distance, dist_ent.reference));

			nearest_entities.Pop();
		}

		//reduce matching_entities to only those needed
		matching_entities.resize(entity_values.size());
		for(size_t i = 0; i < entity_values.size(); i++)
			matching_entities[i] = entity_values[i].reference;

		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		if(distParams.recomputeAccurateDistances)
		{
			//store state for reversion and overwrite with compute accurate distances
			bool old_recalculate_distances_accurately_state = distParams.highAccuracy;
			distParams.SetHighAccuracy(true);

			//recompute distance accurately for each found entity result
			for(auto &it : entity_values)
				it.distance = GetConditionDistanceMeasure(it.reference);

			//revert to original state
			distParams.SetHighAccuracy(old_recalculate_distances_accurately_state);
		}

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(transformSuprisalToProb,
			distanceWeightExponent, weightLabel != StringInternPool::NOT_A_STRING_ID,
			[this](Entity *e, double &weight_value) { return e->GetValueAtLabelAsNumber(weightLabel, weight_value); });

		distance_transform.TransformDistances(entity_values, returnSortedList);

		return EntityQueryManager::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabel, [](auto entity) { return entity;  });
	}

	case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
	{
		//find those that match
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			//if it doesn't match the condition, then remove it
			if(!DoesEntityMatchCondition(matching_entities[i]))
			{
				matching_entities.erase(begin(matching_entities) + i);
				i--;
			}
		}

		if(enm == nullptr)
			return EvaluableNodeReference::Null();

		//compute distances
		//Note that this recalculates the distance.  Since this is a small number of cases, it shouldn't be a big performance impact -- for larger queries, it will use faster methods
		// if this becomes a performance issue, then DoesEntityMatchCondition can be refactored to optionally return the values it computed
		std::vector<DistanceReferencePair<Entity *>> entity_values;
		entity_values.reserve(matching_entities.size());
		for(size_t i = 0; i < matching_entities.size(); i++)
			entity_values.push_back(DistanceReferencePair<Entity *>(GetConditionDistanceMeasure(matching_entities[i]), matching_entities[i]));

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(transformSuprisalToProb,
			distanceWeightExponent, weightLabel != StringInternPool::NOT_A_STRING_ID,
			[this](Entity *e, double &weight_value) { return e->GetValueAtLabelAsNumber(weightLabel, weight_value); });

		distance_transform.TransformDistances(entity_values, returnSortedList);

		return EntityQueryManager::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabel, [](auto entity) { return entity;  });
	}

	default:
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			//if it doesn't match the condition, then remove it
			if(!DoesEntityMatchCondition(matching_entities[i]))
			{
				matching_entities.erase(begin(matching_entities) + i);
				i--;
			}
		}

		return EvaluableNodeReference::Null();
	}
}

//returns true if the chain of query conditions can be used in the query caches path (faster queries)
static bool CanUseQueryCaches(std::vector<EntityQueryCondition> &conditions)
{
	for(size_t i = 0; i < conditions.size(); i++)
	{
		if(!EntityQueryCaches::DoesCachedConditionMatch(&conditions[i], i + 1 == conditions.size()))
			return false;
	}

	return true;
}

EntityQueryCaches *EntityQueryManager::GetQueryCachesForContainer(Entity *container)
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadLock lock(queryCacheMutex);
#endif

	auto found_cache = queryCaches.find(container);
	if(found_cache != end(queryCaches))
		return &(*found_cache->second);

#ifdef MULTITHREAD_SUPPORT
	//not found, so need to insert a new one
	lock.unlock();

	Concurrency::WriteLock write_lock(queryCacheMutex);

	//need to double-check to make sure no other thread inserted the cache between the locks
	found_cache = queryCaches.find(container);
	if(found_cache != end(queryCaches))
		return &(*found_cache->second);
#endif

	//doesn't exist, so insert it and return; it is a double lookup compared to above,
	//but it is rare, and it needs a write lock
	queryCaches.emplace(container, std::make_unique<EntityQueryCaches>(container));
	return &(*queryCaches[container]);
}

EvaluableNodeReference EntityQueryManager::GetMatchingEntitiesFromQueryCaches(Entity *container,
	std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm, bool return_query_value)
{
	//get the label existance cache associated with this container
	// use the first condition as an heuristic for building it if it doesn't exist
	EntityQueryCaches *entity_caches = GetQueryCachesForContainer(container);

	//starting collection of matching entities, initialized to all entities with the requested labels
	// reuse existing buffer
	BitArrayIntegerSet &matching_ents = entity_caches->buffers.currentMatchingEntities;
	matching_ents.clear();

	//this will be cleared each iteration
	auto &compute_results = entity_caches->buffers.computeResultsIdToValue;

	auto &indices_with_duplicates = entity_caches->buffers.entityIndicesWithDuplicates;
	indices_with_duplicates.clear();

	//execute each query
	// for the first condition, matching_ents is empty and must be populated
	// for each subsequent loop, matching_ents will have the currently selected entities to query from
	for(size_t cond_index = 0; cond_index < conditions.size(); cond_index++)
	{
		auto &cond = conditions[cond_index];
		bool is_first = (cond_index == 0);
		bool is_last = (cond_index == (conditions.size() - 1));

		//start each condition with cleared compute results as to not reuse the results from a previous computation
		compute_results.clear();

		//if query_none, return results as empty list
		if(cond.queryType == ENT_NULL)
			return EvaluableNodeReference(enm->AllocNode(ENT_LIST), true);

		switch(cond.queryType)
		{
			case ENT_QUERY_COUNT:
				if(is_first)
					return EvaluableNodeReference(enm->AllocNode(static_cast<double>(container->GetNumContainedEntities())), true);
				else
					return EvaluableNodeReference(enm->AllocNode(static_cast<double>(matching_ents.size())), true);

			case ENT_QUERY_IN_ENTITY_LIST:
			{
				if(is_first)
				{
					for(const auto &id : cond.existLabels)
					{
						size_t entity_index = container->GetContainedEntityIndex(id);
						if(entity_index != std::numeric_limits<size_t>::max())
							matching_ents.insert(entity_index);
					}
				}
				else
				{
					BitArrayIntegerSet &temp = entity_caches->buffers.tempMatchingEntityIndices;
					temp.clear();

					for(const auto &id : cond.existLabels)
					{
						size_t entity_index = container->GetContainedEntityIndex(id);
						if(matching_ents.contains(entity_index))
							temp.insert(entity_index);
					}

					matching_ents.Intersect(temp);
				}

				break;
			}
			
			case ENT_QUERY_NOT_IN_ENTITY_LIST:
			{
				//if first, need to start with all entities
				if(is_first)
					matching_ents.SetAllIds(container->GetNumContainedEntities());

				for(const auto &id : cond.existLabels)
				{
					size_t entity_index = container->GetContainedEntityIndex(id);
					matching_ents.erase(entity_index); //note, does nothing if id is already not in matching_ents
				}

				break;
			}

			case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
			{
				//if excluding an entity, translate it into the index
				if(cond.exclusionLabel == string_intern_pool.NOT_A_STRING_ID)
					cond.exclusionLabel = std::numeric_limits<size_t>::max();
				else
					cond.exclusionLabel = container->GetContainedEntityIndex(cond.exclusionLabel);
				//fall through to cases below
			}

			case ENT_QUERY_EXISTS:
			case ENT_QUERY_NOT_EXISTS:
			case ENT_QUERY_EQUALS:
			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_BETWEEN:
			case ENT_QUERY_NOT_BETWEEN:
			case ENT_QUERY_AMONG:
			case ENT_QUERY_NOT_AMONG:
			case ENT_QUERY_MAX:
			case ENT_QUERY_MIN:
			case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
			case ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS:
			case ENT_COMPUTE_ENTITY_CONVICTIONS:
			case ENT_COMPUTE_ENTITY_KL_DIVERGENCES:
			{
				entity_caches->GetMatchingEntities(&cond, matching_ents, compute_results, is_first, !is_last || !return_query_value);
				break;
			}

			case ENT_QUERY_SUM:
			case ENT_QUERY_QUANTILE:
			case ENT_QUERY_GENERALIZED_MEAN:
			case ENT_QUERY_MIN_DIFFERENCE:
			case ENT_QUERY_MAX_DIFFERENCE:
			case ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE:
			{
				entity_caches->GetMatchingEntities(&cond, matching_ents, compute_results, is_first, !is_last || !return_query_value);

				if(compute_results.size() > 0)
					return EvaluableNodeReference(enm->AllocNode(static_cast<double>(compute_results[0].distance)), true);
				else
					return EvaluableNodeReference(enm->AllocNode(std::numeric_limits<double>::quiet_NaN()), true);
			}

			case ENT_QUERY_MODE:
			{
				if(cond.singleLabelType == ENIVT_NUMBER)
				{
					entity_caches->GetMatchingEntities(&cond, matching_ents, compute_results, is_first, !is_last || !return_query_value);

					if(compute_results.size() > 0)
						return EvaluableNodeReference(enm->AllocNode(static_cast<double>(compute_results[0].distance)), true);
					else
						return EvaluableNodeReference(enm->AllocNode(std::numeric_limits<double>::quiet_NaN()), true);
				}
				else if(cond.singleLabelType == ENIVT_STRING_ID)
				{
					StringInternPool::StringID mode = string_intern_pool.NOT_A_STRING_ID;

					if(entity_caches->ComputeValueFromMatchingEntities(&cond, matching_ents, mode, is_first))
						return EvaluableNodeReference(enm->AllocNode(ENT_STRING, mode), true);
					else
						return EvaluableNodeReference::Null();
				}
				break;
			}
			
			case ENT_QUERY_VALUE_MASSES:
			{
				if(cond.singleLabelType == ENIVT_NUMBER)
				{
					FastHashMap<double, double, std::hash<double>, DoubleNanHashComparator> value_weights;
					entity_caches->ComputeValuesFromMatchingEntities(&cond, matching_ents, value_weights, is_first);

					EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
					assoc->ReserveMappedChildNodes(value_weights.size());

					std::string string_value;
					for(auto &[value, weight] : value_weights)
					{
						string_value = EvaluableNode::NumberToString(value);
						assoc->SetMappedChildNode(string_value, enm->AllocNode(weight));
					}

					return EvaluableNodeReference(assoc, true);
				}
				else if(cond.singleLabelType == ENIVT_STRING_ID)
				{
					FastHashMap<StringInternPool::StringID, double> value_weights;
					entity_caches->ComputeValuesFromMatchingEntities(&cond, matching_ents, value_weights, is_first);

					EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
					assoc->ReserveMappedChildNodes(value_weights.size());

					for(auto &[value, weight] : value_weights)
						assoc->SetMappedChildNode(value, enm->AllocNode(weight));

					return EvaluableNodeReference(assoc, true);
				}

				break;
			}
			
			case ENT_QUERY_SAMPLE:
			{
				size_t num_entities;
				if(is_first)
					num_entities = container->GetNumContainedEntities();
				else
					num_entities = matching_ents.size();

				//if matching_ents is empty, there is nothing to select from
				if(num_entities == 0)
					break;

				size_t num_to_sample = static_cast<size_t>(cond.maxToRetrieve);

				bool update_matching_ents = (!is_last || !return_query_value);

				BitArrayIntegerSet &temp = entity_caches->buffers.tempMatchingEntityIndices;
				if(update_matching_ents)
					temp.clear();

				for(size_t i = 0; i < num_to_sample; i++)
				{
					//get a random id out of all valid ones
					size_t selected_id;
					if(is_first)
						selected_id = cond.randomStream.RandSize(num_entities);
					else
						selected_id = matching_ents.GetNthElement(cond.randomStream.RandSize(num_entities));

					//keep track if necessary
					if(!update_matching_ents)
						temp.insert(selected_id);
					indices_with_duplicates.push_back(selected_id);
				}
				
				if(!update_matching_ents)
					matching_ents = temp;
			
				break;
			}

			case ENT_QUERY_WEIGHTED_SAMPLE:
			{
				entity_caches->GetMatchingEntitiesViaSamplingWithReplacement(&cond, matching_ents, indices_with_duplicates, is_first, !is_last);
				break;
			}

			case ENT_QUERY_SELECT:
			{
				size_t num_to_select = static_cast<size_t>(cond.maxToRetrieve);
				size_t offset = cond.hasStartOffset ? static_cast<size_t>(cond.startOffset) : 0; //offset to start selecting from, maintains the order given a random seed

				size_t num_entities;
				if(is_first)
					num_entities = container->GetNumContainedEntities();
				else
					num_entities = matching_ents.size();

				if(num_entities == 0)
					break;

				if(is_first && !cond.hasRandomStream)
				{
					for(size_t i = offset; i < num_to_select + offset && i < num_entities; i++)
						matching_ents.insert(i);
				}
				else
				{
					BitArrayIntegerSet &temp = entity_caches->buffers.tempMatchingEntityIndices;
					temp.clear();

					if(is_first) //we know hasRandomStream is true from above logic
						temp.SetAllIds(num_entities);
					else
					{
						temp = matching_ents;
						matching_ents.clear();
					}

					if(cond.hasRandomStream)
					{
						for(size_t i = 0; i < num_to_select + offset; i++)
						{
							if(temp.size() == 0)
								break;

							//find random 
							size_t selected_index = cond.randomStream.RandSize(temp.size());

							selected_index = temp.GetNthElement(selected_index);
							temp.erase(selected_index);

							//if before offset, need to burn through random numbers to get consistent results
							if(i < offset)
								continue;

							//add to results
							matching_ents.insert(selected_index);
						}
					}
					else //no random stream, just go in order
					{
						size_t max_index = std::min(num_to_select + offset, temp.size());
						for(size_t i = offset; i < max_index; i++)
						{
							size_t selected_index = temp.GetNthElement(i);
							matching_ents.insert(selected_index);
						}
					}
				}

				break;
			}

			default:
				break;
		}
	}

	//---Return Query Results---//
	EntityQueryCondition *last_query = nullptr;
	EvaluableNodeType last_query_type = ENT_NULL;
	if(conditions.size() > 0)
	{
		last_query = &conditions.back();
		last_query_type = last_query->queryType;
	}

	//function to transform entity indices to entity ids
	const auto entity_index_to_id = [container](size_t entity_index) { return container->GetContainedEntityIdFromIndex(entity_index); };

	//if last query condition is query sample, return each sampled entity id which may include duplicates
	if(last_query_type == ENT_QUERY_SAMPLE || last_query_type == ENT_QUERY_WEIGHTED_SAMPLE)
		return CreateListOfStringsIdsFromIteratorAndFunction(indices_with_duplicates, enm, entity_index_to_id);

	//return data as appropriate
	if(return_query_value && last_query != nullptr)
	{
		auto &contained_entities = container->GetContainedEntities();

		//if the query type uses compute results
		if(last_query_type == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
			|| last_query_type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
			|| last_query_type == ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS
			|| last_query_type == ENT_COMPUTE_ENTITY_CONVICTIONS
			|| last_query_type == ENT_COMPUTE_ENTITY_KL_DIVERGENCES)
		{
			return EntityQueryManager::ConvertResultsToEvaluableNodes<size_t>(compute_results,
				enm, last_query->returnSortedList, last_query->additionalSortedListLabel,
				[&contained_entities](auto entity_index) { return contained_entities[entity_index]; });
		}
		else //if there are no compute results, return an assoc of the requested labels for each entity
		{
			//return assoc of distances if requested
			EvaluableNode *query_return = enm->AllocNode(ENT_ASSOC);
			query_return->ReserveMappedChildNodes(matching_ents.size());

			//create a string reference for each entity
			string_intern_pool.CreateStringReferences(matching_ents,
				[&contained_entities](auto entity_index) { return contained_entities[entity_index]->GetIdStringId(); });

			auto &exist_labels = last_query->existLabels;

			if(exist_labels.size() > 0)
			{
				//create string reference for each entity's labels
				string_intern_pool.CreateMultipleStringReferences(exist_labels, matching_ents.size());

				for(const auto &entity_index : matching_ents)
				{
					//create assoc for values for each entity
					EvaluableNode *entity_values = enm->AllocNode(ENT_ASSOC);
					entity_values->ReserveMappedChildNodes(exist_labels.size());
					query_return->SetMappedChildNodeWithReferenceHandoff(contained_entities[entity_index]->GetIdStringId(), entity_values);

					//get values
					for(auto &label_sid : exist_labels)
						entity_values->SetMappedChildNodeWithReferenceHandoff(label_sid, contained_entities[entity_index]->GetValueAtLabel(label_sid, enm, false));
				}
			}
			else //no exist_labels
			{
				//create a null for every entry, since nothing requested
				for(const auto &entity_index : matching_ents)
					query_return->SetMappedChildNodeWithReferenceHandoff(contained_entities[entity_index]->GetIdStringId(), nullptr);
			}

			return EvaluableNodeReference(query_return, true);
		}
	}

	return CreateListOfStringsIdsFromIteratorAndFunction(matching_ents, enm, entity_index_to_id);
}


EvaluableNodeReference EntityQueryManager::GetEntitiesMatchingQuery(Entity *container, std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm, bool return_query_value)
{
	if(_enable_SBF_datastore && CanUseQueryCaches(conditions))
		return GetMatchingEntitiesFromQueryCaches(container, conditions, enm, return_query_value);

	if(container == nullptr)
		return EvaluableNodeReference(enm->AllocNode(ENT_LIST), true);

	//list of the entities to be found, pruned down, and ultimately returned after converting to matching_entity_ids
	std::vector<Entity *> matching_entities;
	EvaluableNodeReference query_return_value;

	//start querying
	for(size_t cond_index = 0; cond_index < conditions.size(); cond_index++)
	{
		bool first_condition = (cond_index == 0);
		bool last_condition = (cond_index + 1 == conditions.size());

		//reset to make sure it doesn't return an outdated list
		query_return_value = EvaluableNodeReference::Null();

		//check for any unsupported operations by brute force; if possible, use query caches, otherwise return null
		if(conditions[cond_index].queryType == ENT_COMPUTE_ENTITY_CONVICTIONS || conditions[cond_index].queryType == ENT_COMPUTE_ENTITY_KL_DIVERGENCES
			|| conditions[cond_index].queryType == ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE || conditions[cond_index].queryType == ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS)
		{
			if(CanUseQueryCaches(conditions))
				return GetMatchingEntitiesFromQueryCaches(container, conditions, enm, return_query_value);
			else
				return EvaluableNodeReference::Null();
		}
		
		query_return_value = conditions[cond_index].GetMatchingEntities(container, matching_entities, first_condition, (return_query_value && last_condition) ? enm : nullptr);
	}

	//if need to return something specific, then do so, otherwise return list of matching entities
	if(query_return_value != nullptr)
		return query_return_value;
	
	SortEntitiesByID(matching_entities);
	return CreateListOfStringsIdsFromIteratorAndFunction(matching_entities, enm, [](Entity *e) { return e->GetIdStringId(); });
}
