//project headers:
#include "EntityQueries.h"
#include "Concurrency.h"
#include "EntityManipulation.h"
#include "EntityQueryCaches.h"
#include "EvaluableNodeTreeFunctions.h"

bool _enable_SBF_datastore = true;

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
				EvaluableNodeImmediateValueWithType compare(compare_value, compare_type);

				auto [value, found] = e->GetValueAtLabelAsImmediateValue(label_id);

				//needs to exist
				if(value.nodeType == ENIVT_NOT_EXIST)
					return false;

				if(!EvaluableNodeImmediateValueWithType::AreEqual(compare, value))
					return false;
			}
			return true;

		case ENT_QUERY_NOT_EQUALS:
			for(size_t i = 0; i < singleLabels.size(); i++)
			{
				auto &[label_id, compare_value] = singleLabels[i];
				auto compare_type = valueTypes[i];
				EvaluableNodeImmediateValueWithType compare(compare_value, compare_type);

				auto [value, found] = e->GetValueAtLabelAsImmediateValue(label_id);

				//needs to exist
				if(value.nodeType == ENIVT_NOT_EXIST)
					return false;

				if(EvaluableNodeImmediateValueWithType::AreEqual(compare, value))
					return false;
			}
			return true;

		case ENT_QUERY_BETWEEN:
			for(size_t i = 0; i < pairedLabels.size(); i++)
			{
				auto &[label_id, range] = pairedLabels[i];

				if(valueTypes[i] == ENIVT_NUMBER)
				{
					auto [value, found] = e->GetValueAtLabelAsNumber(label_id);
					if(!found)
						return false;

					if(value < range.first.number || range.second.number < value)
						return false;
				}
				else if(valueTypes[i] == ENIVT_STRING_ID)
				{
					auto [value, found] = e->GetValueAtLabelAsStringId(label_id);
					if(!found)
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
					auto [value, found] = e->GetValueAtLabelAsNumber(label_id);
					if(!found)
						return false;

					if(value >= range.first.number && range.second.number >= value)
						return false;
				}
				else if(valueTypes[i] == ENIVT_STRING_ID)
				{
					auto [value, found] = e->GetValueAtLabelAsStringId(label_id);
					if(!found)
						return false;

					if(StringNaturalCompare(value, range.first.stringID) > 0 && StringNaturalCompare(range.second.stringID, value) > 0)
						return false;
				}
			}
			return true;

		case ENT_QUERY_AMONG:
		{
			auto [value,found] = e->GetValueAtLabelAsImmediateValue(singleLabel);

			if(value.nodeType == ENIVT_NOT_EXIST)
				return false;
			
			for(size_t i = 0; i < valueToCompare.size(); i++)
			{
				if(EvaluableNodeImmediateValue::AreEqual(value.nodeType, value.nodeValue, valueTypes[i], valueToCompare[i]))
					return true;
			}

			return false;
		}

		case ENT_QUERY_NOT_AMONG:
		{
			auto [value, found] = e->GetValueAtLabelAsImmediateValue(singleLabel);

			if(value.nodeType == ENIVT_NOT_EXIST)
				return false;

			for(size_t i = 0; i < valueToCompare.size(); i++)
			{
				//make sure same type
				if(value.nodeType != valueTypes[i])
					return false;

				if(EvaluableNodeImmediateValue::AreEqual(value.nodeType, value.nodeValue, valueTypes[i], valueToCompare[i]))
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
				auto [value, found] = e->GetValueAtLabelAsImmediateValue(positionLabels[i]);
				position_types[i] = value.nodeType;
				position[i] = value.nodeValue;
				if(position_types[i] == ENIVT_NOT_EXIST)
					return false;
			}

			double radius = 0.0;
			if(singleLabel != StringInternPool::NOT_A_STRING_ID)
			{
				auto [value, found] = e->GetValueAtLabelAsNumber(singleLabel);
				if(found)
					radius = value;
			}

			double distance = distEvaluator.ComputeMinkowskiDistance(position, position_types, valueToCompare, valueTypes, distEvaluator.highAccuracyDistances);
			if(distance - radius > maxDistance)
				return false;

			return true;
		}

		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
			//it does not fail the condition here - needs to be checked elsewhere
			return true;

		case ENT_QUERY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_CONVICTIONS:
		case ENT_QUERY_ENTITY_KL_DIVERGENCES:
		case ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE:
		case ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS:
			return false;

		default:
			// eliminates compiler warnings on clang.
			break;
	}

	return false;
}

double EntityQueryCondition::GetConditionDistanceMeasure(Entity *e, bool high_accuracy)
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
		auto [value, found] = e->GetValueAtLabelAsImmediateValue(positionLabels[i]);
		position_types[i] = value.nodeType;
		position[i] = value.nodeValue;
		if(position_types[i] == ENIVT_NOT_EXIST)
			return std::numeric_limits<double>::quiet_NaN();
	}

	double radius = 0.0;
	if(singleLabel != StringInternPool::NOT_A_STRING_ID)
	{
		auto [value, found] = e->GetValueAtLabelAsNumber(singleLabel);
		if(found)
			radius = value;
	}
		
	double distance = distEvaluator.ComputeMinkowskiDistance(position, position_types, valueToCompare, valueTypes, high_accuracy);
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
		EntityManipulation::SortEntitiesByID(matching_entities);

		size_t start_offset = std::min(matching_entities.size(), startOffset);
		size_t num_to_select = std::min(matching_entities.size() - start_offset, maxToRetrieve);

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
		size_t num_to_sample = maxToRetrieve;

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

		if(singleLabel == StringInternPool::NOT_A_STRING_ID)
		{
			//select num_to_select entities and save them in the sample vector
			for(size_t i = 0; i < num_to_sample; i++)
			{
				size_t index_to_swap = randomStream.RandSize(num_entities);
				Entity *selected = matching_entities[index_to_swap];
				samples.emplace_back(selected);
			}
		}
		else //weighted
		{
			//retrieve weights
			std::vector<double> entity_weights;
			entity_weights.reserve(num_to_sample);

			//retrieve and accumulate weights
			for(Entity *e : matching_entities)
			{
				double value;
				double found;

				if(e != nullptr)
					std::tie(value, found) = e->GetValueAtLabelAsNumber(singleLabel);
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
				Entity *selected = matching_entities[selected_index];
				samples.emplace_back(selected);
			}
			else //build temporary cache and query
			{
				WeightedDiscreteRandomStreamTransform<Entity *> wdrst(matching_entities, entity_weights, true);
				for(size_t i = 0; i < num_to_sample; i++)
					samples.emplace_back(wdrst.WeightedDiscreteRand(random_stream));
			}
		}

		//swap samples vector with the matching_entities 
		std::swap(matching_entities, samples);
		return EvaluableNodeReference::Null();
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
			for(auto label_sid : exist_labels)
				entity_values->SetMappedChildNode(label_sid, matching_entities[i]->GetValueAtLabel(label_sid, enm, false).first);
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

			auto [value, found] = matching_entities[i]->GetValueAtLabelAsImmediateValue(singleLabel);

			if(value.nodeType == singleLabelType)
				entity_values.push_back(std::make_pair(matching_entities[i], value.nodeValue));
		}

		//sort entities by value
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
		size_t num_to_keep = std::min(maxToRetrieve, entity_values.size());
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
			auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsNumber(singleLabel);
			if(found)
				value = ret_val;
			return found;
		};

		auto get_weight = [matching_entities, this]
		(size_t i, double &weight_value)
		{
			auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsNumber(weightLabel);
			weight_value = ret_val;

			return found;
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
					auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsStringId(singleLabel, value);
					value = ret_val;

					return found;
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
				weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight, distEvaluator.pValue, center, calculateMoment, absoluteValue);
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
				auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsNumber(singleLabel);
				value = ret_val;

				return found;
			};

			auto get_weight = [matching_entities, this]
			(size_t i, double &weight_value)
			{
				auto [ret_val, found] = matching_entities[i]->GetValueAtLabelAsNumber(weightLabel);
				weight_value = ret_val;

				return found;
			};

			auto value_weights = EntityQueriesStatistics::ValueMassesNumber<size_t>(0, matching_entities.size(), matching_entities.size(),
																					get_value, weightLabel != StringInternPool::NOT_A_STRING_ID, get_weight);
			EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
			assoc->ReserveMappedChildNodes(value_weights.size());

			std::string string_value;
			for(auto &[value, weight] : value_weights)
			{
				string_value = EvaluableNode::NumberToString(value, true);
				assoc->SetMappedChildNode(string_value, enm->AllocNode(weight));
			}

			return EvaluableNodeReference(assoc, true);
		}
		else if(singleLabelType == ENIVT_STRING_ID)
		{
			auto get_value = [matching_entities, this]
			(size_t i, StringInternPool::StringID &value)
			{
				auto [ret_value, found] = matching_entities[i]->GetValueAtLabelAsStringId(singleLabel);
				value = ret_value;
				return found;
			};

			auto get_weight = [matching_entities, this]
			(size_t i, double &weight_value)
			{
				auto [ret_value, found] = matching_entities[i]->GetValueAtLabelAsNumber(weightLabel);
				if(found)
					weight_value = ret_value;
				return found;
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
		size_t num_to_keep = std::min(maxToRetrieve, matching_entities.size());

		distEvaluator.InitializeParametersAndFeatureParams();

		//get values for each entity
		StochasticTieBreakingPriorityQueue<DistanceReferencePair<Entity *>, double> nearest_entities(randomStream.CreateOtherStreamViaRand());
		for(size_t i = 0; i < matching_entities.size(); i++)
		{
			double value = GetConditionDistanceMeasure(matching_entities[i], distEvaluator.highAccuracyDistances);
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

		if(!distEvaluator.highAccuracyDistances && distEvaluator.recomputeAccurateDistances)
		{
			//recompute distance accurately for each found entity result
			for(auto &it : entity_values)
				it.distance = GetConditionDistanceMeasure(it.reference, true);
		}

		auto weight_function = [this](Entity *e)
							   {
								 	auto [weight, found] = e->GetValueAtLabelAsNumber(weightLabel);
									if(found)
										return weight;
									return 1.0;
							   }; 

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(distEvaluator.computeSurprisal,
			distEvaluator.transformSurprisalToProb, distanceWeightExponent,
			minToRetrieve, maxToRetrieve, numToRetrieveMinIncrementalProbability, extraToRetrieve,
			weightLabel != StringInternPool::NOT_A_STRING_ID, 0.0, weight_function);

		num_to_keep = distance_transform.TransformDistances(begin(entity_values), end(entity_values), returnSortedList);
		entity_values.resize(num_to_keep);

		return EntityManipulation::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabels, [](auto entity) { return entity;  });
	}

	case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
	{
		distEvaluator.InitializeParametersAndFeatureParams();
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
		bool high_accuracy = (distEvaluator.highAccuracyDistances || distEvaluator.recomputeAccurateDistances);
		std::vector<DistanceReferencePair<Entity *>> entity_values;
		entity_values.reserve(matching_entities.size());
		for(size_t i = 0; i < matching_entities.size(); i++)
			entity_values.push_back(DistanceReferencePair<Entity *>(GetConditionDistanceMeasure(matching_entities[i], high_accuracy), matching_entities[i]));


		auto weight_function = [this]
							   (Entity *e)
							   {
									auto [weight, found] = e->GetValueAtLabelAsNumber(weightLabel);
									if(found)
										return weight;
									return 1.0;
							   };

		//transform distances as appropriate
		EntityQueriesStatistics::DistanceTransform<Entity *> distance_transform(distEvaluator.computeSurprisal,
			distEvaluator.transformSurprisalToProb, distanceWeightExponent,
			minToRetrieve, maxToRetrieve, numToRetrieveMinIncrementalProbability, extraToRetrieve,
			weightLabel != StringInternPool::NOT_A_STRING_ID, 0.0, weight_function);

		size_t num_to_keep = distance_transform.TransformDistances(begin(entity_values), end(entity_values), returnSortedList);
		entity_values.resize(num_to_keep);

		return EntityManipulation::ConvertResultsToEvaluableNodes<Entity *>(entity_values,
			enm, returnSortedList, additionalSortedListLabels, [](auto entity) { return entity;  });
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
