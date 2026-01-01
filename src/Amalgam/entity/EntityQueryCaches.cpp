//project headers:
#include "Entity.h"
#include "EntityManipulation.h"
#include "EntityQueries.h"
#include "EntityQueriesDensityFunctions.h"
#include "EntityQueryCaches.h"
#include "EvaluableNodeTreeFunctions.h"
#include "HashMaps.h"
#include "IntegerSet.h"
#include "KnnCache.h"
#include "SeparableBoxFilterDataStore.h"
#include "StringInternPool.h"
#include "WeightedDiscreteRandomStream.h"

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
EntityQueryCaches::QueryCachesBuffers EntityQueryCaches::buffers;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
EntityQueriesDensityProcessor::ConvictionProcessorBuffers EntityQueriesDensityProcessor::buffers;

bool EntityQueryCaches::DoesCachedConditionMatch(EntityQueryCondition *cond, bool last_condition)
{
	EvaluableNodeType qt = cond->queryType;

	if(qt == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE || qt == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
		|| qt == ENT_QUERY_DISTANCE_CONTRIBUTIONS || qt == ENT_QUERY_ENTITY_CONVICTIONS
		|| qt == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE || qt == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
		|| qt == ENT_QUERY_ENTITY_KL_DIVERGENCES || qt == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
		|| qt == ENT_QUERY_ENTITY_CLUSTERS)
	{
		//accelerating a p of 0 with the current caches would be a large effort, as everything would have to be
		// transformed via logarithms and then pValue = 1 applied
		//however, because other transforms, like surprisal_to_prob already transform the data to log space,
		//accelerating this edge case does not seem worthwhile
		if(cond->distEvaluator.pValue == 0)
			return false;
	}

	return true;
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

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
void EntityQueryCaches::EnsureLabelsAreCached(EntityQueryCondition *cond, Concurrency::ReadLock &lock)
#else
void EntityQueryCaches::EnsureLabelsAreCached(EntityQueryCondition *cond)
#endif
{
	//if there are any labels that need to be added,
	// this will collected them to be added all at once
	std::vector<StringInternPool::StringID> labels_to_add;

	//add label to cache if missing
	switch(cond->queryType)
	{
		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
		case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
		case ENT_QUERY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_CONVICTIONS:
		case ENT_QUERY_ENTITY_KL_DIVERGENCES:
		case ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE:
		case ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS:
		case ENT_QUERY_ENTITY_CLUSTERS:
		{
			for(auto label : cond->positionLabels)
			{
				if(!DoesHaveLabel(label))
					labels_to_add.push_back(label);
			}

			if(cond->weightLabel != StringInternPool::NOT_A_STRING_ID)
			{
				if(!DoesHaveLabel(cond->weightLabel))
					labels_to_add.push_back(cond->weightLabel);
			}

			//radius
			if(cond->singleLabel != StringInternPool::NOT_A_STRING_ID)
			{
				if(!DoesHaveLabel(cond->singleLabel))
					labels_to_add.push_back(cond->singleLabel);
			}

			for(auto label : cond->additionalSortedListLabels)
			{
				if(!DoesHaveLabel(label))
					labels_to_add.push_back(label);
			}

			break;
		}

		case ENT_QUERY_SAMPLE:
		case ENT_QUERY_AMONG:
		case ENT_QUERY_NOT_AMONG:
		case ENT_QUERY_MIN:
		case ENT_QUERY_MAX:
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		{
			if(cond->singleLabel != StringInternPool::NOT_A_STRING_ID)
			{
				if(!DoesHaveLabel(cond->singleLabel))
					labels_to_add.push_back(cond->singleLabel);
			}

			break;
		}

		case ENT_QUERY_SUM:
		case ENT_QUERY_MODE:
		case ENT_QUERY_QUANTILE:
		case ENT_QUERY_GENERALIZED_MEAN:
		case ENT_QUERY_VALUE_MASSES:
		{
			if(!DoesHaveLabel(cond->singleLabel))
				labels_to_add.push_back(cond->singleLabel);

			if(cond->weightLabel != StringInternPool::NOT_A_STRING_ID)
			{
				if(!DoesHaveLabel(cond->weightLabel))
					labels_to_add.push_back(cond->weightLabel);
			}

			break;
		}

		case ENT_QUERY_EXISTS:
		case ENT_QUERY_NOT_EXISTS:
		{
			for(auto label : cond->existLabels)
			{
				if(!DoesHaveLabel(label))
					labels_to_add.push_back(label);
			}
			break;
		}

		case ENT_QUERY_EQUALS:
		case ENT_QUERY_NOT_EQUALS:
		{
			for(auto &[label_id, _] : cond->singleLabels)
			{
				if(!DoesHaveLabel(label_id))
					labels_to_add.push_back(label_id);
			}
			break;
		}

		default:
		{
			for(auto &[label_id, _] : cond->pairedLabels)
			{
				if(!DoesHaveLabel(label_id))
					labels_to_add.push_back(label_id);
			}
		}
	}


	if(labels_to_add.size() == 0)
		return;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	lock.unlock();
	Concurrency::WriteLock write_lock(mutex);

	//now with write_lock, remove any labels that have already been added by other threads
	labels_to_add.erase(std::remove_if(begin(labels_to_add), end(labels_to_add),
		[this](auto sid) { return DoesHaveLabel(sid); }),
		end(labels_to_add));

	//need to double-check to make sure that another thread didn't already rebuild
	if(labels_to_add.size() > 0)
#endif
		sbfds.AddLabels(labels_to_add, container->GetContainedEntities());

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//release write lock and reacquire read lock
	write_lock.unlock();
	lock.lock();
#endif
}

void EntityQueryCaches::GetMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities,
	std::vector<DistanceReferencePair<size_t>> &compute_results, bool is_first, bool update_matching_entities)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(cond, lock);
#else
	EnsureLabelsAreCached(cond);
#endif

	switch(cond->queryType)
	{
		case ENT_QUERY_EXISTS:
		{
			for(auto label : cond->existLabels)
			{
				if(is_first)
				{
					sbfds.FindAllEntitiesWithFeature(label, matching_entities);
					is_first = false;
				}
				else
					sbfds.IntersectEntitiesWithFeature(label, matching_entities, true);
			}

			if(!is_first || cond->existLabels.size() > 0)
				matching_entities.UpdateNumElements();

			return;
		}

		case ENT_QUERY_NOT_EXISTS:
		{
			for(auto label : cond->existLabels)
			{
				if(is_first)
				{
					sbfds.FindAllEntitiesWithoutFeature(label, matching_entities);
					is_first = false;
				}
				else
					sbfds.IntersectEntitiesWithoutFeature(label, matching_entities, true);
			}

			if(!is_first || cond->existLabels.size() > 0)
				matching_entities.UpdateNumElements();

			return;
		}

		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
		case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
		case ENT_QUERY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_CONVICTIONS:
		case ENT_QUERY_ENTITY_KL_DIVERGENCES:
		case ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE:
		case ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS:
		case ENT_QUERY_ENTITY_CLUSTERS:
		{
			//get entity (case) weighting if applicable
			bool use_entity_weights = (cond->weightLabel != StringInternPool::NOT_A_STRING_ID);
			size_t weight_column = std::numeric_limits<size_t>::max();
			double min_weight = 1.0;
			if(use_entity_weights)
			{
				weight_column = sbfds.GetColumnIndexFromLabelId(cond->weightLabel);
				min_weight = sbfds.GetMinValueForColumnAsWeight(weight_column);
			}

			auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<size_t>(weight_column, true);
			EntityQueriesStatistics::DistanceTransform<size_t> distance_transform(cond->distEvaluator.computeSurprisal,
				cond->distEvaluator.transformSurprisalToProb, cond->distanceWeightExponent,
				cond->minToRetrieve, cond->maxToRetrieve, cond->numToRetrieveMinIncrementalProbability, cond->extraToRetrieve,
				use_entity_weights, min_weight, get_weight);

			//if first, need to populate with all entities
			if(is_first)
			{
				matching_entities.clear();
				matching_entities.SetAllIds(sbfds.GetNumInsertedEntities());
			}

			//only keep entities that have all the correct features,
			//but remove 0 weighted features for better performance
			for(size_t i = 0; i < cond->positionLabels.size(); i++)
			{
				sbfds.IntersectEntitiesWithFeature(cond->positionLabels[i], matching_entities, true);
				if(cond->distEvaluator.featureAttribs[i].weight == 0.0)
				{
					cond->positionLabels.erase(cond->positionLabels.begin() + i);
					cond->distEvaluator.featureAttribs.erase(begin(cond->distEvaluator.featureAttribs) + i);

					if(cond->queryType == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
							|| cond->queryType == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE)
					{
						if(cond->valueToCompare.size() > i)
							cond->valueToCompare.erase(cond->valueToCompare.begin() + i);
						if(cond->valueTypes.size() > i)
							cond->valueTypes.erase(cond->valueTypes.begin() + i);
					}

					//need to process the new value in this feature slot
					i--;
				}
			}
			matching_entities.UpdateNumElements();

			if(matching_entities.size() == 0)
				return;

			sbfds.PopulateGeneralizedDistanceEvaluatorFromColumnData(cond->distEvaluator, cond->positionLabels);
			cond->distEvaluator.InitializeParametersAndFeatureParams(cond->populateOmittedFeatureValues);

			if(cond->queryType == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE || cond->queryType == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE)
			{

				//labels and values must have the same size if they're prepopulated
				if(!cond->distEvaluator.populateOmittedFeatureValues && cond->valueToCompare.size() != cond->positionLabels.size())
				{
					matching_entities.clear();
					return;
				}

				//if no position labels, then the weight must be zero so just randomly choose k
				if(cond->positionLabels.size() == 0)
				{
					BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
					temp = matching_entities;
					matching_entities.clear();

					auto rand_stream = cond->randomStream.CreateOtherStreamViaRand();

					//insert each case and compute to zero distance because the distance because weight was zero to get here
					size_t num_to_retrieve = std::min(cond->maxToRetrieve, temp.size());
					for(size_t i = 0; i < num_to_retrieve; i++)
					{
						size_t rand_index = temp.GetRandomElement(rand_stream);
						temp.erase(rand_index);
						matching_entities.insert(rand_index);
						compute_results.emplace_back(0.0, rand_index);
					}
				}
				else if(cond->queryType == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE)
				{
					if(cond->distEvaluator.populateOmittedFeatureValues)
					{
						size_t entity_index = container->GetContainedEntityIndex(cond->entityIdToExclude);
						sbfds.FindEntitiesNearestToIndexedEntity(cond->distEvaluator, cond->positionLabels, entity_index,
							distance_transform.GetNumToRetrieve(), cond->singleLabel, matching_entities, false, false,
							compute_results, std::numeric_limits<size_t>::max(), cond->randomStream.CreateOtherStreamViaRand());
					}
					else
					{
						sbfds.FindEntitiesNearestToPosition(cond->distEvaluator, cond->positionLabels, cond->valueToCompare, cond->valueTypes,
							distance_transform.GetNumToRetrieve(), cond->singleLabel, cond->exclusionEntityIndex, matching_entities, false, false,
							compute_results, cond->randomStream.CreateOtherStreamViaRand());
					}
				}
				else //ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
				{
					if(cond->distEvaluator.populateOmittedFeatureValues)
					{
						size_t entity_index = container->GetContainedEntityIndex(cond->entityIdToExclude);
						sbfds.FindEntitiesWithinDistanceToIndexedEntity(cond->distEvaluator, cond->positionLabels, entity_index,
							cond->maxDistance, cond->singleLabel, matching_entities, false, compute_results);
					}
					else
					{
						sbfds.FindEntitiesWithinDistanceToPosition(cond->distEvaluator, cond->positionLabels, cond->valueToCompare, cond->valueTypes,
							cond->maxDistance, cond->singleLabel, matching_entities, false, compute_results);
					}
				}

				distance_transform.TransformDistances(compute_results, cond->returnSortedList);

				//populate matching_entities if needed
				if(update_matching_entities)
				{
					matching_entities.clear();
					for(auto &it : compute_results)
						matching_entities.insert(it.reference);
				}
			}
			else //cond->queryType in ENT_QUERY_DISTANCE_CONTRIBUTIONS, ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS,
				//ENT_QUERY_ENTITY_CONVICTIONS, ENT_QUERY_ENTITY_KL_DIVERGENCES, ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE,
				//ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
			{
				BitArrayIntegerSet *ents_to_compute_ptr = nullptr; //if nullptr, compute is done on all entities in the cache

				//if anything that computes results for entities
				if(cond->queryType == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
					|| cond->queryType == ENT_QUERY_ENTITY_CONVICTIONS
					|| cond->queryType == ENT_QUERY_ENTITY_KL_DIVERGENCES
					|| cond->queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
					|| cond->queryType == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
					|| cond->queryType == ENT_QUERY_ENTITY_CLUSTERS)
				{
					//if subset is specified, set ents_to_compute_ptr to set of ents_to_compute
					if(cond->existLabels.size() != 0)
					{
						ents_to_compute_ptr = &buffers.tempMatchingEntityIndices;
						ents_to_compute_ptr->clear();

						if(cond->queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)
						{
							//determine the base entities by everything not in the list
							*ents_to_compute_ptr = matching_entities;

							for(auto entity_sid : cond->existLabels)
							{
								size_t entity_index = container->GetContainedEntityIndex(entity_sid);
								ents_to_compute_ptr->erase(entity_index);
							}
						}
						else
						{
							for(auto entity_sid : cond->existLabels)
							{
								size_t entity_index = container->GetContainedEntityIndex(entity_sid);
								if(entity_index != std::numeric_limits<size_t>::max())
									ents_to_compute_ptr->insert(entity_index);
							}

							//make sure everything asked to be computed is in the base set of entities
							ents_to_compute_ptr->Intersect(matching_entities);
						}
					}
					else //compute on all
					{
						ents_to_compute_ptr = &matching_entities;
					}
				}

			#ifdef MULTITHREAD_SUPPORT
				EntityQueriesDensityProcessor conviction_processor(buffers.knnCache,
					distance_transform, distance_transform.GetNumToRetrieve(), cond->singleLabel, cond->useConcurrency);
			#else
				EntityQueriesDensityProcessor conviction_processor(buffers.knnCache,
					distance_transform, distance_transform.GetNumToRetrieve(), cond->singleLabel);
			#endif
				buffers.knnCache.ResetCache(sbfds, matching_entities, cond->distEvaluator, cond->positionLabels, cond->singleLabel);

				auto &results_buffer = buffers.doubleVector;
				results_buffer.clear();

				if(cond->queryType == ENT_QUERY_DISTANCE_CONTRIBUTIONS)
				{
					conviction_processor.ComputeDistanceContributionsOnPositions(*cond->positionsToCompare, results_buffer);
				}
				else if(cond->queryType == ENT_QUERY_ENTITY_CONVICTIONS)
				{
					conviction_processor.ComputeCaseKLDivergences(*ents_to_compute_ptr, results_buffer, true, cond->convictionOfRemoval);
				}
				else if(cond->queryType == ENT_QUERY_ENTITY_KL_DIVERGENCES)
				{
					conviction_processor.ComputeCaseKLDivergences(*ents_to_compute_ptr, results_buffer, false, cond->convictionOfRemoval);
				}
				else if(cond->queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)
				{
					double group_conviction = conviction_processor.ComputeCaseGroupKLDivergence(*ents_to_compute_ptr, cond->convictionOfRemoval);

					compute_results.clear();
					compute_results.emplace_back(group_conviction, 0);

					//early exit because don't need to translate distances
					return;
				}
				else if(cond->queryType == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS)
				{
					conviction_processor.ComputeDistanceContributionsWithoutCache(ents_to_compute_ptr, results_buffer);
				}
				else if(cond->queryType == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS)
				{
					conviction_processor.ComputeNeighborWeightsForEntities(ents_to_compute_ptr, compute_results);

					if(cond->returnSortedList)
					{
						std::sort(begin(compute_results), end(compute_results),
							[](auto a, auto b) {return a.distance > b.distance; }
						);
					}

					//early exit because don't need to translate distances
					return;
				}
				else //ENT_QUERY_ENTITY_CLUSTERS
				{
					conviction_processor.ComputeCaseClusters(*ents_to_compute_ptr, results_buffer, cond->minClusterWeight);
				}

				//clear compute_results as it may have been used for intermediate results
				compute_results.clear();
				if(ents_to_compute_ptr == nullptr)
				{
					//computed on globals, so convert results to global coordinates paired with their contributions
					compute_results.reserve(results_buffer.size());

					for(size_t i = 0; i < results_buffer.size(); i++)
						compute_results.emplace_back(results_buffer[i], i);
				}
				else //computed on a subset; use ents_to_compute_ptr because don't know what it points to
				{
					compute_results.reserve(ents_to_compute_ptr->size());
					size_t i = 0;
					for(const auto &ent_index : *ents_to_compute_ptr)
						compute_results.emplace_back(results_buffer[i++], ent_index);
				}

				if(cond->returnSortedList)
				{
					std::sort(begin(compute_results), end(compute_results),
						[](auto a, auto b) {return a.distance < b.distance; }
					);
				}
			}

			break;
		}

		case ENT_QUERY_EQUALS:
			{
				bool first_feature = is_first;

				//loop over all features
				for(size_t i = 0; i < cond->singleLabels.size(); i++)
				{
					auto &[label_id, compare_value] = cond->singleLabels[i];
					auto compare_type = cond->valueTypes[i];

					if(first_feature)
					{
						matching_entities.clear();
						sbfds.UnionAllEntitiesWithValue(label_id, compare_type, compare_value, matching_entities);
						first_feature = false;
					}
					else //get corresponding indices and intersect with results
					{
						BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
						temp.clear();
						sbfds.UnionAllEntitiesWithValue(label_id, compare_type, compare_value, temp);
						matching_entities.Intersect(temp);
					}
				}

				break;
			}

		case ENT_QUERY_NOT_EQUALS:
		{
			bool first_feature = is_first;

			//loop over all features
			for(size_t i = 0; i < cond->singleLabels.size(); i++)
			{
				auto &[label_id, compare_value] = cond->singleLabels[i];
				auto compare_type = cond->valueTypes[i];

				if(first_feature)
				{
					matching_entities.clear();
					sbfds.FindAllEntitiesWithFeature(label_id, matching_entities);
					first_feature = false;
				}

				BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
				temp.clear();
				sbfds.UnionAllEntitiesWithValue(label_id, compare_type, compare_value, temp);
				matching_entities.EraseInBatch(temp);
			}
			matching_entities.UpdateNumElements();

			break;
		}

		case ENT_QUERY_BETWEEN:
		case ENT_QUERY_NOT_BETWEEN:
			{
				bool first_feature = is_first;
				BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;

				//loop over all features
				for(size_t i = 0; i < cond->pairedLabels.size(); i++)
				{
					auto label_id = cond->pairedLabels[i].first;
					auto &[low_value, high_value] = cond->pairedLabels[i].second;

					if(first_feature)
					{
						sbfds.FindAllEntitiesWithinRange(label_id, cond->valueTypes[i],
							low_value, high_value, matching_entities, cond->queryType == ENT_QUERY_BETWEEN);
						first_feature = false;
					}
					else //get corresponding indices and intersect with results
					{
						temp.clear();
						sbfds.FindAllEntitiesWithinRange(label_id, cond->valueTypes[i],
							low_value, high_value, temp, cond->queryType == ENT_QUERY_BETWEEN);
						matching_entities.Intersect(temp);
					}
				}

				break;
			}

		case ENT_QUERY_MIN:
		case ENT_QUERY_MAX:
		{
			if(is_first)
			{
				sbfds.FindMinMax(cond->singleLabel, cond->singleLabelType, cond->maxToRetrieve,
									(cond->queryType == ENT_QUERY_MAX), nullptr, matching_entities);
			}
			else
			{
				//move data to temp and compute into matching_entities
				BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
				temp = matching_entities;
				matching_entities.clear();
				sbfds.FindMinMax(cond->singleLabel, cond->singleLabelType, cond->maxToRetrieve,
									(cond->queryType == ENT_QUERY_MAX), &temp, matching_entities);
			}
			break;
		}

		case ENT_QUERY_AMONG:
		{
			if(is_first)
			{
				for(size_t i = 0; i < cond->valueToCompare.size(); i++)
					sbfds.UnionAllEntitiesWithValue(cond->singleLabel, cond->valueTypes[i], cond->valueToCompare[i], matching_entities);
			}
			else
			{
				//get set of entities that are valid
				BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
				temp.clear();
				for(size_t i = 0; i < cond->valueToCompare.size(); i++)
					sbfds.UnionAllEntitiesWithValue(cond->singleLabel, cond->valueTypes[i], cond->valueToCompare[i], temp);

				//only keep those that have a matching value
				matching_entities.Intersect(temp);
			}

			break;
		}

		case ENT_QUERY_NOT_AMONG:
		{
			//ensure that the feature exists
			if(is_first)
				sbfds.FindAllEntitiesWithFeature(cond->singleLabel, matching_entities);
			else
				sbfds.IntersectEntitiesWithFeature(cond->singleLabel, matching_entities, false);

			BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
			temp.clear();
			//get set of entities that are valid
			for(size_t i = 0; i < cond->valueToCompare.size(); i++)
				sbfds.UnionAllEntitiesWithValue(cond->singleLabel, cond->valueTypes[i], cond->valueToCompare[i], temp);

			//only keep those that have a matching value
			matching_entities.erase(temp);

			break;
		}

		case ENT_QUERY_SUM:
		case ENT_QUERY_QUANTILE:
		case ENT_QUERY_GENERALIZED_MEAN:
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		{
			size_t column_index = sbfds.GetColumnIndexFromLabelId(cond->singleLabel);
			if(column_index == std::numeric_limits<size_t>::max())
			{
				compute_results.emplace_back(std::numeric_limits<double>::quiet_NaN(), 0);
				return;
			}

			size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(cond->weightLabel);
			bool has_weight = false;
			if(weight_column_index != std::numeric_limits<size_t>::max())
				has_weight = true;
			else //just use a valid column
				weight_column_index = 0;

			double result = 0.0;

			if(is_first)
			{
				EfficientIntegerSet &entities = sbfds.GetEntitiesWithValidNumbers(column_index);
				auto get_value = sbfds.GetNumberValueFromEntityIteratorFunction<EfficientIntegerSet::Iterator>(column_index, false);
				auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<EfficientIntegerSet::Iterator>(weight_column_index, true);

				switch(cond->queryType)
				{
				case ENT_QUERY_SUM:
					result = EntityQueriesStatistics::Sum(entities.begin(), entities.end(), get_value, has_weight, get_weight);
					break;

				case ENT_QUERY_QUANTILE:
					result = Quantile(entities.begin(), entities.end(), get_value,
						has_weight, get_weight, cond->qPercentage, &EntityQueryCaches::buffers.pairDoubleVector);
					break;

				case ENT_QUERY_GENERALIZED_MEAN:
					result = GeneralizedMean(entities.begin(), entities.end(), get_value,
						has_weight, get_weight, cond->distEvaluator.pValue, cond->center,
						cond->calculateMoment, cond->absoluteValue);
					break;

				case ENT_QUERY_MIN_DIFFERENCE:
					result = EntityQueriesStatistics::ExtremeDifference(entities.begin(), entities.end(), get_value, true,
						cond->maxDistance, cond->includeZeroDifferences, EntityQueryCaches::buffers.doubleVector);
					break;

				case ENT_QUERY_MAX_DIFFERENCE:
					result = EntityQueriesStatistics::ExtremeDifference(entities.begin(), entities.end(), get_value, false,
						cond->maxDistance, cond->includeZeroDifferences, EntityQueryCaches::buffers.doubleVector);
					break;

				default:
					break;
				}
			}
			else
			{
				auto get_value = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(column_index, false);
				auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(weight_column_index, true);

				switch(cond->queryType)
				{
				case ENT_QUERY_SUM:
					result = EntityQueriesStatistics::Sum(matching_entities.begin(), matching_entities.end(), get_value, has_weight, get_weight);
					break;

				case ENT_QUERY_QUANTILE:
					result = Quantile(matching_entities.begin(), matching_entities.end(), get_value,
						has_weight, get_weight, cond->qPercentage, &EntityQueryCaches::buffers.pairDoubleVector);
					break;

				case ENT_QUERY_GENERALIZED_MEAN:
					result = GeneralizedMean(matching_entities.begin(), matching_entities.end(), get_value,
						has_weight, get_weight, cond->distEvaluator.pValue, cond->center, cond->calculateMoment, cond->absoluteValue);
					break;

				case ENT_QUERY_MIN_DIFFERENCE:
					result = EntityQueriesStatistics::ExtremeDifference(matching_entities.begin(), matching_entities.end(), get_value, true,
						cond->maxDistance, cond->includeZeroDifferences, EntityQueryCaches::buffers.doubleVector);
					break;

				case ENT_QUERY_MAX_DIFFERENCE:
					result = EntityQueriesStatistics::ExtremeDifference(matching_entities.begin(), matching_entities.end(), get_value, false,
						cond->maxDistance, cond->includeZeroDifferences, EntityQueryCaches::buffers.doubleVector);
					break;

				default:
					break;
				}
			}

			compute_results.emplace_back(result, 0);
			return;
		}

		default:  // Other Enum value not handled
		{
			break;
		}
	}
}

EvaluableNode *EntityQueryCaches::ComputeValueFromMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities,
	EvaluableNodeManager *enm, bool is_first)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(cond, lock);
#else
	EnsureLabelsAreCached(cond);
#endif

	switch(cond->queryType)
	{
	case ENT_QUERY_MODE:
	{
		size_t column_index = sbfds.GetColumnIndexFromLabelId(cond->singleLabel);
		if(column_index == std::numeric_limits<size_t>::max())
			return nullptr;

		size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(cond->weightLabel);
		bool has_weight = false;
		if(weight_column_index != std::numeric_limits<size_t>::max())
			has_weight = true;
		else //just use a valid column
			weight_column_index = 0;

		if(is_first)
		{
			auto get_key_string_value = sbfds.GetValueToKeyStringFromEntityIteratorFunction<size_t>(column_index);
			auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<size_t>(weight_column_index, true);
			auto [found, compute_result] = ModeString<size_t>(0, sbfds.GetNumInsertedEntities(), get_key_string_value, has_weight, get_weight);
			if(!found)
				return nullptr;
			return Parser::ParseFromKeyString(compute_result, enm);
		}
		else
		{
			auto get_key_string_value = sbfds.GetValueToKeyStringFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(column_index);
			auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(weight_column_index, true);
			auto [found, compute_result] = ModeString(matching_entities.begin(), matching_entities.end(), get_key_string_value, has_weight, get_weight);
			if(!found)
				return nullptr;
			return Parser::ParseFromKeyString(compute_result, enm);
		}
	}
	default:
		break;
	}

	return nullptr;
}

void EntityQueryCaches::ComputeValuesFromMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities,
	FastHashMap<StringInternPool::StringID, double> &compute_results, bool is_first)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(cond, lock);
#else
	EnsureLabelsAreCached(cond);
#endif

	switch(cond->queryType)
	{
	case ENT_QUERY_VALUE_MASSES:
	{
		size_t column_index = sbfds.GetColumnIndexFromLabelId(cond->singleLabel);
		if(column_index == std::numeric_limits<size_t>::max())
			return;
	
		size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(cond->weightLabel);
		bool has_weight = false;
		if(weight_column_index != std::numeric_limits<size_t>::max())
			has_weight = true;
		else //just use a valid column
			weight_column_index = 0;
	
		size_t num_unique_values = sbfds.GetNumUniqueValuesForColumn(column_index, ENIVT_STRING_ID);
	
		if(is_first)
		{
			auto get_value = sbfds.GetValueToKeyStringIdWithReferenceFromEntityIteratorFunction<size_t>(column_index);
			auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<size_t>(weight_column_index, true);
			compute_results = EntityQueriesStatistics::ValueMassesStringId<size_t>(0, sbfds.GetNumInsertedEntities(),
				num_unique_values, get_value, has_weight, get_weight);
		}
		else
		{
			auto get_value = sbfds.GetValueToKeyStringIdWithReferenceFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(column_index);
			auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(weight_column_index, true);
			compute_results = EntityQueriesStatistics::ValueMassesStringId(matching_entities.begin(), matching_entities.end(),
				num_unique_values, get_value, has_weight, get_weight);
		}
	
		return;
	}
	default:
		break;
	}
}

void EntityQueryCaches::GetMatchingEntitiesViaSamplingWithReplacement(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, std::vector<size_t> &entity_indices_sampled, bool is_first, bool update_matching_entities)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(cond, lock);
#else
	EnsureLabelsAreCached(cond);
#endif

	size_t num_to_sample = cond->maxToRetrieve;

	auto &probabilities = EntityQueryCaches::buffers.doubleVector;
	auto &entity_indices = EntityQueryCaches::buffers.entityIndices;

	if(is_first)
		sbfds.FindAllEntitiesWithValidNumbers(cond->singleLabel, matching_entities, entity_indices, probabilities);
	else
		sbfds.IntersectEntitiesWithValidNumbers(cond->singleLabel, matching_entities, entity_indices, probabilities);

	//don't attempt to continue if no elements
	if(matching_entities.size() == 0)
		return;		

	if(update_matching_entities)
		matching_entities.clear();

	NormalizeVector(probabilities, 1.0);

	//if not sampling many, then brute force it
	if(num_to_sample < 10)
	{
		//sample the entities
		for(size_t i = 0; i < num_to_sample; i++)
		{
			size_t selected_entity_index = WeightedDiscreteRandomSample(probabilities, cond->randomStream);
			auto eid = entity_indices[selected_entity_index];

			if(update_matching_entities)
				matching_entities.insert(eid);

			entity_indices_sampled.push_back(eid);
		}
	}
	else //sampling a bunch, better to precompute and use faster method
	{
		//a table for quickly generating entity indices based on weights
		WeightedDiscreteRandomStreamTransform<size_t, CompactHashMap<size_t, double>> ewt(entity_indices, probabilities, false);

		//sample the entities
		for(size_t i = 0; i < num_to_sample; i++)
		{
			auto eid = ewt.WeightedDiscreteRand(cond->randomStream);

			if(update_matching_entities)
				matching_entities.insert(eid);

			entity_indices_sampled.push_back(eid);
		}
	}
}

EvaluableNodeReference EntityQueryCaches::GetMatchingEntitiesFromQueryCaches(Entity *container,
	std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm,
	bool return_query_value, bool immediate_result)
{
	//get the cache associated with this container
	// use the first condition as an heuristic for building it if it doesn't exist
	EntityQueryCaches *entity_caches = container->GetQueryCaches();

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
		{
			if(immediate_result)
				return EvaluableNodeReference(0.0);
			return EvaluableNodeReference(enm->AllocNode(ENT_LIST), true);
		}

		switch(cond.queryType)
		{
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

		case ENT_QUERY_WITHIN_GENERALIZED_DISTANCE:
		case ENT_QUERY_NEAREST_GENERALIZED_DISTANCE:
		{
			//if excluding an entity, translate it into the index
			if(cond.entityIdToExclude == string_intern_pool.NOT_A_STRING_ID)
				cond.exclusionEntityIndex = std::numeric_limits<size_t>::max();
			else
				cond.exclusionEntityIndex = container->GetContainedEntityIndex(cond.entityIdToExclude);
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
		case ENT_QUERY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS:
		case ENT_QUERY_ENTITY_CONVICTIONS:
		case ENT_QUERY_ENTITY_KL_DIVERGENCES:
		case ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS:
		case ENT_QUERY_ENTITY_CLUSTERS:
		{
			entity_caches->GetMatchingEntities(&cond, matching_ents, compute_results, is_first, !is_last || !return_query_value);
			break;
		}

		case ENT_QUERY_SUM:
		case ENT_QUERY_QUANTILE:
		case ENT_QUERY_GENERALIZED_MEAN:
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
		case ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE:
		{
			entity_caches->GetMatchingEntities(&cond, matching_ents, compute_results, is_first, !is_last || !return_query_value);

			if(compute_results.size() > 0)
				return enm->AllocIfNotImmediate(static_cast<double>(compute_results[0].distance), immediate_result);
			else
				return EvaluableNodeReference::Null();
		}

		case ENT_QUERY_MODE:
			return EvaluableNodeReference(entity_caches->ComputeValueFromMatchingEntities(&cond, matching_ents, enm, is_first), true);

		case ENT_QUERY_VALUE_MASSES:
		{
			FastHashMap<StringInternPool::StringID, double> value_weights;
			entity_caches->ComputeValuesFromMatchingEntities(&cond, matching_ents, value_weights, is_first);

			if(immediate_result)
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

		case ENT_QUERY_SAMPLE:
		{
			bool update_matching_ents = (!is_last || !return_query_value);

			if(cond.singleLabel != string_intern_pool.NOT_A_STRING_ID)
			{
				entity_caches->GetMatchingEntitiesViaSamplingWithReplacement(&cond,
					matching_ents, indices_with_duplicates, is_first, update_matching_ents);
			}
			else
			{
				size_t num_entities;
				if(is_first)
					num_entities = container->GetNumContainedEntities();
				else
					num_entities = matching_ents.size();

				//if matching_ents is empty, there is nothing to select from
				if(num_entities == 0)
					break;

				size_t num_to_sample = cond.maxToRetrieve;

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
					if(update_matching_ents)
						temp.insert(selected_id);
					indices_with_duplicates.push_back(selected_id);
				}

				if(update_matching_ents)
					matching_ents = temp;
			}

			break;
		}

		case ENT_QUERY_SELECT:
		{
			size_t num_to_select = cond.maxToRetrieve;
			size_t offset = cond.hasStartOffset ? cond.startOffset : 0; //offset to start selecting from, maintains the order given a random seed

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
	if(last_query_type == ENT_QUERY_SAMPLE)
	{
		if(immediate_result)
			return EvaluableNodeReference(static_cast<double>(indices_with_duplicates.size()));

		return CreateListOfStringsIdsFromIteratorAndFunction(indices_with_duplicates, enm, entity_index_to_id);
	}

	//return data as appropriate
	if(return_query_value && last_query != nullptr)
	{
		auto &contained_entities = container->GetContainedEntities();

		if(last_query_type == ENT_QUERY_DISTANCE_CONTRIBUTIONS)
		{
			if(immediate_result)
				return EvaluableNodeReference(static_cast<double>(compute_results.size()));

			return CreateListOfNumbersFromIteratorAndFunction(compute_results, enm,
				[](auto &result) { return result.distance; });
		}
		else if(last_query_type == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
			|| last_query_type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
			|| last_query_type == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| last_query_type == ENT_QUERY_ENTITY_CONVICTIONS
			|| last_query_type == ENT_QUERY_ENTITY_KL_DIVERGENCES
			|| last_query_type == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
			|| last_query_type == ENT_QUERY_ENTITY_CLUSTERS)
		{
			if(immediate_result)
				return EvaluableNodeReference(static_cast<double>(compute_results.size()));

			return EntityManipulation::ConvertResultsToEvaluableNodes<size_t>(compute_results,
				enm, last_query->returnSortedList, last_query->additionalSortedListLabels,
				[&contained_entities](auto entity_index) { return contained_entities[entity_index]; });
		}
		else //if there are no compute results, return an assoc of the requested labels for each entity
		{
			if(immediate_result)
				return EvaluableNodeReference(static_cast<double>(matching_ents.size()));

			//return assoc of distances if requested
			EvaluableNode *query_return = enm->AllocNode(ENT_ASSOC);
			query_return->ReserveMappedChildNodes(matching_ents.size());

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
					for(auto &label_sid : exist_labels)
						entity_values->SetMappedChildNodeWithReferenceHandoff(label_sid, contained_entities[entity_index]->GetValueAtLabel(label_sid, enm, false).first);

					//append to query_return after building to ensure all flags are propagated
					query_return->SetMappedChildNode(contained_entities[entity_index]->GetIdStringId(), entity_values);
				}
			}
			else //no exist_labels
			{
				//create a null for every entry, since nothing requested
				for(const auto &entity_index : matching_ents)
					query_return->SetMappedChildNode(contained_entities[entity_index]->GetIdStringId(), nullptr);
			}

			return EvaluableNodeReference(query_return, true);
		}
	}

	if(immediate_result)
		return EvaluableNodeReference(static_cast<double>(matching_ents.size()));
	return CreateListOfStringsIdsFromIteratorAndFunction(matching_ents, enm, entity_index_to_id);
}


EvaluableNodeReference EntityQueryCaches::GetEntitiesMatchingQuery(EntityReadReference &container,
	std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm, bool return_query_value, bool immediate_result)
{
	if(_enable_SBF_datastore && CanUseQueryCaches(conditions))
	{
		//if haven't built a cache before, need to build the cache container
		//need to lock the entity to prevent multiple caches from being built concurrently and overwritten
		if(!container->HasQueryCaches())
		{
		#ifdef MULTITHREAD_SUPPORT
			container.lock.unlock();
			EntityWriteReference write_lock(container);
			container->CreateQueryCaches();
			write_lock.lock.unlock();
			container.lock.lock();
		#else
			container->CreateQueryCaches();
		#endif
		}

		return GetMatchingEntitiesFromQueryCaches(container, conditions, enm, return_query_value, immediate_result);
	}

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
		if(conditions[cond_index].queryType == ENT_QUERY_DISTANCE_CONTRIBUTIONS
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_CONVICTIONS
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_KL_DIVERGENCES
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
			|| conditions[cond_index].queryType == ENT_QUERY_ENTITY_CLUSTERS)
		{
			if(!CanUseQueryCaches(conditions))
				return EvaluableNodeReference::Null();

			//if haven't built a cache before, need to build the cache container
			//need to lock the entity to prevent multiple caches from being built concurrently and overwritten
			if(!container->HasQueryCaches())
			{
			#ifdef MULTITHREAD_SUPPORT
				container.lock.unlock();
				EntityWriteReference write_lock(container);
				container->CreateQueryCaches();
				write_lock.lock.unlock();
				container.lock.lock();
			#else
				container->CreateQueryCaches();
			#endif
			}

			return GetMatchingEntitiesFromQueryCaches(container, conditions, enm, return_query_value, immediate_result);	
		}

		query_return_value = conditions[cond_index].GetMatchingEntities(container, matching_entities, first_condition, (return_query_value && last_condition) ? enm : nullptr);
	}

	//if need to return something specific, then do so, otherwise return list of matching entities
	if(query_return_value != nullptr)
		return query_return_value;

	EntityManipulation::SortEntitiesByID(matching_entities);
	return CreateListOfStringsIdsFromIteratorAndFunction(matching_entities, enm, [](Entity *e) { return e->GetIdStringId(); });
}
