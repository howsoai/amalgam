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

#if defined(MULTITHREAD_SUPPORT)
thread_local
#endif
EntityQueryCaches::QueryCachesBuffers EntityQueryCaches::buffers;

#if defined(MULTITHREAD_SUPPORT)
thread_local
#endif
EntityQueriesDensityProcessor::ConvictionProcessorBuffers EntityQueriesDensityProcessor::buffers;

//accelerating a p of 0 with the current caches would be a large effort, as everything would have to be
// transformed via logarithms and then pValue = 1 applied
//however, because other transforms, like surprisal_to_prob already transform the data to log space,
//accelerating this edge case does not seem worthwhile
static bool CanUseQueryCaches(const std::vector<EntityQuery> &conditions)
{
	for(const auto &query : conditions)
		if(auto distance = std::get_if<EntityQuery::Distance>(&query.payload);
			distance != nullptr && query.queryType != ENT_NULL && distance->distEvaluator.pValue == 0)
			return false;
	return true;
}

#if defined(MULTITHREAD_SUPPORT)
void EntityQueryCaches::EnsureLabelsAreCached(std::vector<StringInternPool::StringID> &labels_to_add, Concurrency::ReadLock &lock)
#else
void EntityQueryCaches::EnsureLabelsAreCached(std::vector<StringInternPool::StringID> &labels_to_add)
#endif
{
	if(labels_to_add.size() == 0)
		return;

#if defined(MULTITHREAD_SUPPORT)
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

#if defined(MULTITHREAD_SUPPORT)
	//release write lock and reacquire read lock
	write_lock.unlock();
	lock.lock();
#endif
}

//caller must hold a cache read lock
template<typename Function>
static double ComputeFromMatchingNumbers(SeparableBoxFilterDataStore &sbfds,
	StringInternPool::StringID value_label, StringInternPool::StringID weight_label,
	BitArrayIntegerSet &matching_entities, bool is_first, Function compute)
{
	size_t column_index = sbfds.GetColumnIndexFromLabelId(value_label);
	if(column_index == std::numeric_limits<size_t>::max())
		return std::numeric_limits<double>::quiet_NaN();

	size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(weight_label);
	bool has_weight = weight_column_index != std::numeric_limits<size_t>::max();
	if(!has_weight)
		weight_column_index = 0;

	auto compute_from = [&](auto &entities) {
		using Iterator = decltype(entities.begin());
		auto get_value = sbfds.GetNumberValueFromEntityIteratorFunction<Iterator>(column_index, false);
		auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<Iterator>(weight_column_index, true);
		return compute(entities.begin(), entities.end(), get_value, has_weight, get_weight);
	};
	if(is_first)
		return compute_from(sbfds.GetEntitiesWithValidNumbers(column_index));
	return compute_from(matching_entities);
}

template<typename Parameters>
#if defined(MULTITHREAD_SUPPORT)
void EntityQueryCaches::EnsureLabelsAreCached(const Parameters &parameters, Concurrency::ReadLock &lock)
#else
void EntityQueryCaches::EnsureLabelsAreCached(const Parameters &parameters)
#endif
{
	std::vector<StringInternPool::StringID> labels_to_add;
	auto add_label = [&](auto label) {
		if(label != StringInternPool::NOT_A_STRING_ID && !DoesHaveLabel(label))
			labels_to_add.push_back(label);
	};
	if constexpr(requires { parameters.positionLabels; })
	{
		for(auto label : parameters.positionLabels)
			add_label(label);
		add_label(parameters.weightLabel);
		add_label(parameters.singleLabel);
		for(auto label : parameters.additionalSortedListLabels)
			add_label(label);
	}
	if constexpr(requires { parameters.valueLabel; })
		add_label(parameters.valueLabel);
	if constexpr(requires { parameters.weightLabel; } && !requires { parameters.positionLabels; })
		add_label(parameters.weightLabel);

#if defined(MULTITHREAD_SUPPORT)
	EnsureLabelsAreCached(labels_to_add, lock);
#else
	EnsureLabelsAreCached(labels_to_add);
#endif
}

double EntityQueryCaches::ComputeNumericAggregate(const EntityQuery &query,
	BitArrayIntegerSet &matching_entities, bool is_first)
{
	auto labels = query.GetAggregateLabels();
	auto [value_label, weight_label] = labels;
#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(labels, lock);
#else
	EnsureLabelsAreCached(labels);
#endif
	return ComputeFromMatchingNumbers(sbfds, value_label, weight_label, matching_entities, is_first,
		[&query](auto first, auto last, auto &get_value, bool has_weight, auto &get_weight) {
			return EntityQueries::ComputeNumericAggregate(query, first, last, get_value, has_weight, get_weight,
				buffers.doubleVector, &buffers.pairDoubleVector);
		});
}

void EntityQueryCaches::GetMatchingEntities(EvaluableNodeType query_type, EntityQuery::Distance *cond, BitArrayIntegerSet &matching_entities,
	std::vector<DistanceReferencePair<size_t>> &compute_results, bool is_first, bool update_matching_entities)
{
#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(*cond, lock);
#else
	EnsureLabelsAreCached(*cond);
#endif

	//get entity (case) weighting if applicable
	bool use_entity_weights = (cond->weightLabel != StringInternPool::NOT_A_STRING_ID);
	size_t weight_column = std::numeric_limits<size_t>::max();
	double min_weight = 1.0;
	if(use_entity_weights)
	{
		weight_column = sbfds.GetColumnIndexFromLabelId(cond->weightLabel);
		min_weight = sbfds.GetMinValueForColumnAsWeight(weight_column);
	}

	auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<size_t, true>(weight_column, true);
	EntityQueriesStatistics::DistanceTransform<size_t> distance_transform(cond->distEvaluator.computeSurprisal,
		cond->distEvaluator.transformSurprisalToProb, cond->distanceWeightExponent,
		cond->minToRetrieve, cond->maxToRetrieve, cond->numToRetrieveMinIncrementalProbability, cond->extraToRetrieve,
		use_entity_weights, min_weight, get_weight);

	if(is_first)
		matching_entities.SetAllIds(sbfds.GetNumInsertedEntities());
	for(auto label : cond->positionLabels)
		sbfds.IntersectEntitiesWithFeature(label, matching_entities, true);
	matching_entities.UpdateNumElements();

	if(matching_entities.size() == 0)
		return;

	sbfds.PopulateGeneralizedDistanceEvaluatorFromColumnData(cond->distEvaluator, cond->positionLabels);
	if(auto neighbors = std::get_if<EntityQuery::Distance::Neighbors>(&cond->target))
	{
		//resolve the exclusion and distance target once, under the cache lock
		size_t exclusion_entity_index = neighbors->entityIdToExclude == StringInternPool::NOT_A_STRING_ID
			? std::numeric_limits<size_t>::max() : container->GetContainedEntityIndex(neighbors->entityIdToExclude);
		cond->distEvaluator.InitializeParametersAndFeatureParams(neighbors->populateOmittedFeatureValues);
		//labels and values must have the same size if they're prepopulated
		if(!cond->distEvaluator.populateOmittedFeatureValues && neighbors->valuesToCompare.size() != cond->positionLabels.size())
		{
			matching_entities.clear();
			return;
		}

		//if no position labels, then the weight must be zero so just randomly choose k
		if(cond->positionLabels.size() == 0)
		{
			BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
			temp = matching_entities;
			temp.erase(exclusion_entity_index);

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
		else if(query_type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE)
		{
			if(cond->distEvaluator.populateOmittedFeatureValues)
			{
				size_t entity_index = container->GetContainedEntityIndex(neighbors->entityIdToExclude);
				sbfds.FindEntitiesNearestToIndexedEntity(cond->distEvaluator, cond->positionLabels, entity_index,
					distance_transform.GetNumToRetrieve(), cond->singleLabel, matching_entities, false, false,
					cond->interpreter, cond->entity, compute_results,
					std::numeric_limits<size_t>::max(), cond->randomStream);
			}
			else
			{
				sbfds.FindEntitiesNearestToPosition(cond->distEvaluator, cond->positionLabels, neighbors->valuesToCompare,
					distance_transform.GetNumToRetrieve(), cond->singleLabel, exclusion_entity_index,
					matching_entities, false, false, cond->interpreter, cond->entity, compute_results,
					cond->randomStream);
			}
		}
		else //ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
		{
			if(cond->distEvaluator.populateOmittedFeatureValues)
			{
				size_t entity_index = container->GetContainedEntityIndex(neighbors->entityIdToExclude);
				sbfds.FindEntitiesWithinDistanceToIndexedEntity(cond->distEvaluator, cond->positionLabels,
					entity_index, neighbors->maxDistance, cond->singleLabel, matching_entities, false,
					cond->interpreter, cond->entity, compute_results);
			}
			else
			{
				sbfds.FindEntitiesWithinDistanceToPosition(cond->distEvaluator, cond->positionLabels,
					neighbors->valuesToCompare, neighbors->maxDistance, cond->singleLabel, matching_entities, false,
					cond->interpreter, cond->entity, compute_results);
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
	else //query_type in ENT_QUERY_DISTANCE_CONTRIBUTIONS, ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS,
		//ENT_QUERY_ENTITY_CONVICTIONS, ENT_QUERY_ENTITY_KL_DIVERGENCES, ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE,
		//ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
	{
		cond->distEvaluator.InitializeParametersAndFeatureParams(false);
		BitArrayIntegerSet *ents_to_compute_ptr = nullptr; //if nullptr, compute is done on all entities in the cache

		//if anything that computes results for entities
		if(query_type == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| query_type == ENT_QUERY_ENTITY_CONVICTIONS
			|| query_type == ENT_QUERY_ENTITY_KL_DIVERGENCES
			|| query_type == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
			|| query_type == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
			|| query_type == ENT_QUERY_ENTITY_CLUSTERS)
		{
			//if subset is specified, set ents_to_compute_ptr to set of ents_to_compute
			auto statistics = std::get_if<EntityQuery::Distance::Statistics>(&cond->target);
			if(statistics != nullptr && !statistics->entityIds.empty())
			{
				ents_to_compute_ptr = &buffers.tempMatchingEntityIndices;
				ents_to_compute_ptr->clear();

				if(query_type == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)
				{
					//determine the base entities by everything not in the list
					*ents_to_compute_ptr = matching_entities;

					for(auto entity_sid : statistics->entityIds)
					{
						size_t entity_index = container->GetContainedEntityIndex(entity_sid);
						ents_to_compute_ptr->erase(entity_index);
					}
				}
				else
				{
					for(auto entity_sid : statistics->entityIds)
					{
						size_t entity_index = container->GetContainedEntityIndex(entity_sid);
						if(entity_index != std::numeric_limits<size_t>::max())
							ents_to_compute_ptr->insert(entity_index);
					}
				}
			}
			else //compute on all
			{
				ents_to_compute_ptr = &matching_entities;
			}
		}

		//rebind the thread-local cache before the processor inspects this query.
		buffers.knnCache.ResetCache(sbfds, matching_entities, cond->distEvaluator, cond->interpreter, cond->entity,
			cond->positionLabels, cond->singleLabel);

	#ifdef MULTITHREAD_SUPPORT
		EntityQueriesDensityProcessor conviction_processor(buffers.knnCache,
			distance_transform, distance_transform.GetNumToRetrieve(), cond->singleLabel, cond->useConcurrency);
	#else
		EntityQueriesDensityProcessor conviction_processor(buffers.knnCache,
			distance_transform, distance_transform.GetNumToRetrieve(), cond->singleLabel);
	#endif

		auto &results_buffer = buffers.doubleVector;
		results_buffer.clear();

		if(query_type == ENT_QUERY_DISTANCE_CONTRIBUTIONS)
		{
			conviction_processor.ComputeDistanceContributionsOnPositions(*std::get<EntityQuery::Distance::Positions>(cond->target).nodes, results_buffer);
		}
		else if(query_type == ENT_QUERY_ENTITY_CONVICTIONS)
		{
			conviction_processor.ComputeCaseKLDivergences(*ents_to_compute_ptr, results_buffer, true, std::get<EntityQuery::Distance::Statistics>(cond->target).convictionOfRemoval);
		}
		else if(query_type == ENT_QUERY_ENTITY_KL_DIVERGENCES)
		{
			conviction_processor.ComputeCaseKLDivergences(*ents_to_compute_ptr, results_buffer, false, std::get<EntityQuery::Distance::Statistics>(cond->target).convictionOfRemoval);
		}
		else if(query_type == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)
		{
			double group_conviction = conviction_processor.ComputeCaseGroupKLDivergence(*ents_to_compute_ptr, std::get<EntityQuery::Distance::Statistics>(cond->target).convictionOfRemoval);

			compute_results.clear();
			compute_results.emplace_back(group_conviction, 0);

			//early exit because don't need to translate distances
			return;
		}
		else if(query_type == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS)
		{
			conviction_processor.ComputeDistanceContributionsWithoutCache(ents_to_compute_ptr, results_buffer);
		}
		else if(query_type == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS)
		{
			conviction_processor.ComputeNeighborWeightsForEntities(ents_to_compute_ptr, compute_results);

			if(cond->returnSortedList)
			{
				std::stable_sort(begin(compute_results), end(compute_results),
					[](auto a, auto b) {return a.distance > b.distance; }
				);
			}

			//early exit because don't need to translate distances
			return;
		}
		else //ENT_QUERY_ENTITY_CLUSTERS
		{
			conviction_processor.ComputeCaseClusters(*ents_to_compute_ptr, results_buffer, std::get<EntityQuery::Distance::Clusters>(cond->target).minWeight);
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
			std::stable_sort(begin(compute_results), end(compute_results),
				[](auto a, auto b) {return a.distance < b.distance; }
			);
		}
	}

}

void EntityQueryCaches::FindMinMax(const EntityQuery::Extremum &parameters, bool largest, BitArrayIntegerSet &matching_entities, bool is_first)
{
#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(parameters, lock);
#else
	EnsureLabelsAreCached(parameters);
#endif

	auto &temp = buffers.tempMatchingEntityIndices;
	if(!is_first)
	{
		temp = matching_entities;
		matching_entities.clear();
	}
	sbfds.FindMinMax(parameters.valueLabel, parameters.valueType, parameters.count,
		largest, is_first ? nullptr : &temp, matching_entities);
}

EvaluableNodeReference EntityQueryCaches::ComputeMode(const EntityQuery &query, BitArrayIntegerSet &matching_entities,
	EvaluableNodeManager *enm, bool is_first)
{
	auto labels = std::get<EntityQuery::Aggregate>(query.payload);
	auto [value_label, weight_label] = labels;
#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(labels, lock);
#else
	EnsureLabelsAreCached(labels);
#endif

	size_t column_index = sbfds.GetColumnIndexFromLabelId(value_label);
	if(column_index == std::numeric_limits<size_t>::max())
		return EvaluableNodeReference::Null();

	size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(weight_label);
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
			return EvaluableNodeReference::Null();
		return Parser::ParseFromKeyString(compute_result, enm);
	}
	else
	{
		auto get_key_string_value = sbfds.GetValueToKeyStringFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(column_index);
		auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(weight_column_index, true);
		auto [found, compute_result] = ModeString(matching_entities.begin(), matching_entities.end(), get_key_string_value, has_weight, get_weight);
		if(!found)
			return EvaluableNodeReference::Null();
		return Parser::ParseFromKeyString(compute_result, enm);
	}
}

OrderedHashMap<StringInternPool::StringID, double> EntityQueryCaches::ComputeValueMasses(const EntityQuery &query,
	BitArrayIntegerSet &matching_entities, bool is_first)
{
	auto labels = std::get<EntityQuery::Aggregate>(query.payload);
	auto [value_label, weight_label] = labels;
#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(labels, lock);
#else
	EnsureLabelsAreCached(labels);
#endif

	size_t column_index = sbfds.GetColumnIndexFromLabelId(value_label);
	if(column_index == std::numeric_limits<size_t>::max())
		return {};

	size_t weight_column_index = sbfds.GetColumnIndexFromLabelId(weight_label);
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
		return EntityQueriesStatistics::ValueMassesStringId<size_t>(0, sbfds.GetNumInsertedEntities(),
			num_unique_values, get_value, has_weight, get_weight);
	}
	else
	{
		auto get_value = sbfds.GetValueToKeyStringIdWithReferenceFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(column_index);
		auto get_weight = sbfds.GetNumberValueFromEntityIteratorFunction<BitArrayIntegerSet::Iterator>(weight_column_index, true);
		return EntityQueriesStatistics::ValueMassesStringId(matching_entities.begin(), matching_entities.end(),
			num_unique_values, get_value, has_weight, get_weight);
	}
}

void EntityQueryCaches::GetMatchingEntitiesViaSamplingWithReplacement(EntityQuery &query, BitArrayIntegerSet &matching_entities, std::vector<size_t> &entity_indices_sampled, bool is_first, bool update_matching_entities)
{
	auto &parameters = std::get<EntityQuery::Sample>(query.payload);

	if(parameters.valueLabel == StringInternPool::NOT_A_STRING_ID)
	{
		size_t num_entities;
		if(is_first)
			num_entities = container->GetNumContainedEntities();
		else
			num_entities = matching_entities.size();

		//if matching_entities is empty, there is nothing to select from
		if(num_entities == 0)
			return;

		size_t num_to_sample = parameters.count;

		if(is_first && !update_matching_entities)
		{
			for(size_t i = 0; i < num_to_sample; i++)
				entity_indices_sampled.push_back(parameters.randomStream.RandSize(num_entities));
			return;
		}

		BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
		if(update_matching_entities)
			temp.clear();

		for(size_t i = 0; i < num_to_sample; i++)
		{
			//get a random id out of all valid ones
			size_t selected_id;
			if(is_first)
				selected_id = parameters.randomStream.RandSize(num_entities);
			else
				selected_id = matching_entities.GetNthElement(parameters.randomStream.RandSize(num_entities));

			//keep track if necessary
			if(update_matching_entities)
				temp.insert(selected_id);
			entity_indices_sampled.push_back(selected_id);
		}

		if(update_matching_entities)
			matching_entities = temp;
		return;
	}

#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
	EnsureLabelsAreCached(parameters, lock);
#else
	EnsureLabelsAreCached(parameters);
#endif

	size_t num_to_sample = parameters.count;

	auto &probabilities = EntityQueryCaches::buffers.doubleVector;
	auto &entity_indices = EntityQueryCaches::buffers.entityIndices;

	if(is_first)
		sbfds.FindAllEntitiesWithValidNumbers(parameters.valueLabel, matching_entities, entity_indices, probabilities);
	else
		sbfds.IntersectEntitiesWithValidNumbers(parameters.valueLabel, matching_entities, entity_indices, probabilities);

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
			size_t selected_entity_index = WeightedDiscreteRandomSample(probabilities, parameters.randomStream);
			auto eid = entity_indices[selected_entity_index];

			if(update_matching_entities)
				matching_entities.insert(eid);

			entity_indices_sampled.push_back(eid);
		}
	}
	else //sampling a bunch, better to precompute and use faster method
	{
		//a table for quickly generating entity indices based on weights
		WeightedDiscreteRandomStreamTransform<size_t> ewt(entity_indices, probabilities, false);

		//sample the entities
		for(size_t i = 0; i < num_to_sample; i++)
		{
			auto eid = ewt.WeightedDiscreteRand(parameters.randomStream);

			if(update_matching_entities)
				matching_entities.insert(eid);

			entity_indices_sampled.push_back(eid);
		}
	}
}

void EntityQueryCaches::SelectEntities(EntityQuery &query, BitArrayIntegerSet &matching_entities, bool is_first)
{
	auto &parameters = std::get<EntityQuery::Select>(query.payload);

	size_t num_to_select = parameters.count;
	size_t offset = parameters.hasStartOffset ? parameters.startOffset : 0; //offset to start selecting from, maintains the order given a random seed

	size_t num_entities;
	if(is_first)
		num_entities = container->GetNumContainedEntities();
	else
		num_entities = matching_entities.size();

	if(num_entities == 0)
		return;

	if(is_first && !parameters.hasRandomStream)
	{
		for(size_t i = offset; i < num_to_select + offset && i < num_entities; i++)
			matching_entities.insert(i);
	}
	else
	{
		BitArrayIntegerSet &temp = buffers.tempMatchingEntityIndices;
		temp.clear();

		if(is_first) //we know hasRandomStream is true from above logic
			temp.SetAllIds(num_entities);
		else
		{
			temp = matching_entities;
			matching_entities.clear();
		}

		if(parameters.hasRandomStream)
		{
			for(size_t i = 0; i < num_to_select + offset; i++)
			{
				if(temp.size() == 0)
					break;

				//find random
				size_t selected_index = parameters.randomStream.RandSize(temp.size());

				selected_index = temp.GetNthElement(selected_index);
				temp.erase(selected_index);

				//if before offset, need to burn through random numbers to get consistent results
				if(i < offset)
					continue;

				//add to results
				matching_entities.insert(selected_index);
			}
		}
		else //no random stream, just go in order
		{
			size_t max_index = std::min(num_to_select + offset, temp.size());
			for(size_t i = offset; i < max_index; i++)
			{
				size_t selected_index = temp.GetNthElement(i);
				matching_entities.insert(selected_index);
			}
		}
	}
}

EvaluableNodeReference EntityQueryCaches::GetMatchingEntitiesFromQueryCaches(Entity *container,
	std::vector<EntityQuery> &conditions, EvaluableNodeManager *enm,
	bool return_query_value, EvaluableNodeRequestedValueTypes immediate_result)
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
		auto &query = conditions[cond_index];
		bool is_first = (cond_index == 0);
		bool is_last = (cond_index == (conditions.size() - 1));

		//start each condition with cleared compute results as to not reuse the results from a previous computation
		compute_results.clear();

		//if query_none, return results as empty list
		if(query.queryType == ENT_NULL)
		{
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
				return EvaluableNodeReference(0.0);
			return EvaluableNodeReference(enm->AllocNode(ENT_LIST), true);
		}

		if(auto distance = std::get_if<EntityQuery::Distance>(&query.payload))
		{
			distance->entity = container;
			entity_caches->GetMatchingEntities(query.queryType, distance, matching_ents, compute_results, is_first, !is_last || !return_query_value);
			if(query.queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE)
			{
				if(compute_results.size() > 0)
					return enm->AllocIfNotImmediate(static_cast<double>(compute_results[0].distance), immediate_result);
				return EvaluableNodeReference::Null();
			}
			continue;
		}

		if(auto extremum = std::get_if<EntityQuery::Extremum>(&query.payload))
		{
			entity_caches->FindMinMax(*extremum, query.queryType == ENT_QUERY_MAX, matching_ents, is_first);
			continue;
		}

		//aggregates return even in non-final position on the cached path.
		switch(query.queryType)
		{
		case ENT_QUERY_SELECT:
			entity_caches->SelectEntities(query, matching_ents, is_first);
			continue;
		case ENT_QUERY_SAMPLE:
			entity_caches->GetMatchingEntitiesViaSamplingWithReplacement(query, matching_ents, indices_with_duplicates, is_first, !is_last || !return_query_value);
			continue;
		case ENT_QUERY_MODE:
			return entity_caches->ComputeMode(query, matching_ents, enm, is_first);
		case ENT_QUERY_VALUE_MASSES:
		{
			auto value_weights = entity_caches->ComputeValueMasses(query, matching_ents, is_first);
			return EntityQueries::ValueMassesToResult(value_weights, enm, immediate_result);
		}
		case ENT_QUERY_SUM:
		case ENT_QUERY_QUANTILE:
		case ENT_QUERY_GENERALIZED_MEAN:
		case ENT_QUERY_MIN_DIFFERENCE:
		case ENT_QUERY_MAX_DIFFERENCE:
			return enm->AllocIfNotImmediate(
				entity_caches->ComputeNumericAggregate(query, matching_ents, is_first), immediate_result);
		default:
			break;
		}

		if(EntityQueries::GetFilterNode(query) != nullptr
			|| std::holds_alternative<EntityQuery::EntityIds>(query.payload))
		{
			//keep the cache lock across filters that previously shared a prepared condition.
			auto group_type = [](EvaluableNodeType type) {
				switch(type)
				{
				case ENT_QUERY_LESS_OR_EQUAL_TO:
				case ENT_QUERY_GREATER_OR_EQUAL_TO:
					return ENT_QUERY_BETWEEN;
				case ENT_QUERY_EXISTS:
				case ENT_QUERY_NOT_EXISTS:
				case ENT_QUERY_EQUALS:
				case ENT_QUERY_NOT_EQUALS:
				case ENT_QUERY_BETWEEN:
				case ENT_QUERY_NOT_BETWEEN:
					return type;
				default:
					return ENT_NULL;
				}
			};
			auto type = group_type(query.queryType);
			size_t group_end = cond_index + 1;
			if(type != ENT_NULL)
				while(group_end < conditions.size()
					&& EntityQueries::GetFilterNode(conditions[group_end]) != nullptr
					&& group_type(conditions[group_end].queryType) == type)
					group_end++;
			entity_caches->GetMatchingEntities(
				std::span<const EntityQuery>(conditions.data() + cond_index, group_end - cond_index), matching_ents, is_first);
			cond_index = group_end - 1;
			continue;
		}
	}

	//---Return Query Results---//
	EntityQuery::Distance *last_query = nullptr;
	EvaluableNodeType last_query_type = ENT_NULL;
	if(conditions.size() > 0)
	{
		last_query = std::get_if<EntityQuery::Distance>(&conditions.back().payload);
		last_query_type = conditions.back().queryType;
	}

	//function to transform entity indices to entity ids
	const auto entity_index_to_id = [container](size_t entity_index) { return container->GetContainedEntityIdFromIndex(entity_index); };

	//if last query condition is query sample, return each sampled entity id which may include duplicates
	if(last_query_type == ENT_QUERY_SAMPLE)
	{
		if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
			return EvaluableNodeReference(static_cast<double>(indices_with_duplicates.size()));

		return CreateListOfStringsIdsFromIteratorAndFunction(indices_with_duplicates, enm, entity_index_to_id);
	}

	//return data as appropriate
	if(return_query_value && !conditions.empty())
	{
		auto &contained_entities = container->GetContainedEntities();

		if(last_query_type == ENT_QUERY_DISTANCE_CONTRIBUTIONS)
		{
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
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
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
				return EvaluableNodeReference(static_cast<double>(compute_results.size()));

			return EntityManipulation::ConvertResultsToEvaluableNodes<size_t>(compute_results,
				enm, last_query->returnSortedList, last_query->additionalSortedListLabels,
				[&contained_entities](auto entity_index) { return contained_entities[entity_index]; });
		}
		else //if there are no compute results, return an assoc of the requested labels for each entity
		{
			if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
				return EvaluableNodeReference(static_cast<double>(matching_ents.size()));

			//return assoc of distances if requested
			EvaluableNode *query_return = enm->AllocNode(ENT_ASSOC);
			query_return->ReserveMappedChildNodes(matching_ents.size());

			size_t num_labels = 0;
			EntityQueries::ForEachQueryLabel(conditions, conditions.size() - 1, [&](auto) { ++num_labels; });
			for(const auto &entity_index : matching_ents)
			{
				EvaluableNode *entity_values = nullptr;
				if(num_labels > 0)
				{
					entity_values = enm->AllocNode(ENT_ASSOC);
					entity_values->ReserveMappedChildNodes(num_labels);
					EntityQueries::ForEachQueryLabel(conditions, conditions.size() - 1,
						[&](auto label_sid) {
							entity_values->SetMappedChildNode(label_sid, contained_entities[entity_index]->GetValueAtLabel(label_sid, enm).first);
						});
				}
				query_return->SetMappedChildNode(contained_entities[entity_index]->GetIdStringId(), entity_values);
			}

			return EvaluableNodeReference(query_return, true);
		}
	}

	if(immediate_result.Allows(EvaluableNodeRequestedValueTypes::Type::SIZE_AS_NUMBER))
		return EvaluableNodeReference(static_cast<double>(matching_ents.size()));
	return CreateListOfStringsIdsFromIteratorAndFunction(matching_ents, enm, entity_index_to_id);
}


EvaluableNodeReference EntityQueryCaches::GetEntitiesMatchingQuery(EntityReadReference &container,
	std::vector<EntityQuery> &conditions, EvaluableNodeManager *enm, bool return_query_value, EvaluableNodeRequestedValueTypes immediate_result)
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

		auto &query = conditions[cond_index];
		auto node = EntityQueries::GetFilterNode(query);
		if(node != nullptr || std::holds_alternative<EntityQuery::EntityIds>(query.payload) || query.queryType == ENT_NULL)
		{
			EntityQueries::FilterEntities(query, container, matching_entities, first_condition);
			if(last_condition && return_query_value && node != nullptr && node->GetType() == ENT_QUERY_EXISTS)
			{
				auto result = enm->AllocNode(ENT_ASSOC);
				result->ReserveMappedChildNodes(matching_entities.size());
				for(auto entity : matching_entities)
				{
					auto values = enm->AllocNode(ENT_ASSOC);
					EntityQueries::ForEachQueryLabel(conditions, cond_index,
						[&](auto label) { values->SetMappedChildNode(label, entity->GetValueAtLabel(label, enm).first); });
					result->SetMappedChildNode(entity->GetIdStringId(), values);
				}
				query_return_value = EvaluableNodeReference(result, true);
			}
			continue;
		}

		auto distance = std::get_if<EntityQuery::Distance>(&query.payload);
		if(distance == nullptr)
		{
			if(first_condition && query.queryType != ENT_QUERY_SAMPLE)
				matching_entities = container->GetContainedEntities();

			if(query.queryType == ENT_QUERY_MIN || query.queryType == ENT_QUERY_MAX)
			{
				EntityQueries::FindMinMax(query, matching_entities);
			}
			else if(query.queryType == ENT_QUERY_SELECT)
			{
				EntityQueries::SelectEntities(query, matching_entities);
			}
			else if(query.queryType == ENT_QUERY_SAMPLE)
			{
				EntityQueries::SampleEntities(query, first_condition ? container->GetContainedEntities() : matching_entities, matching_entities);
			}
			else if(last_condition && return_query_value)
			{
				if(query.queryType == ENT_QUERY_MODE)
				{
					query_return_value = EntityQueries::ComputeMode(query, matching_entities, enm);
				}
				else if(query.queryType == ENT_QUERY_VALUE_MASSES)
				{
					auto value_weights = EntityQueries::ComputeValueMasses(query, matching_entities);
					//fallback always constructs the result, including size-only requests.
					query_return_value = EntityQueries::ValueMassesToResult(value_weights, enm, {});
				}
				else
					query_return_value = EvaluableNodeReference(enm->AllocNode(
						EntityQueries::ComputeNumericAggregate(query, matching_entities)), true);
			}

			continue;
		}

		auto &condition = *distance;
		//populate here because now know container entity
		condition.entity = container;

		//check for any unsupported operations by brute force; if possible, use query caches, otherwise return null
		if(query.queryType == ENT_QUERY_DISTANCE_CONTRIBUTIONS
			|| query.queryType == ENT_QUERY_ENTITY_CONVICTIONS
			|| query.queryType == ENT_QUERY_ENTITY_KL_DIVERGENCES
			|| query.queryType == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
			|| query.queryType == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| query.queryType == ENT_QUERY_ENTITY_CUMULATIVE_NEAREST_ENTITY_WEIGHTS
			|| query.queryType == ENT_QUERY_ENTITY_CLUSTERS)
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

		query_return_value = condition.GetMatchingEntities(query.queryType, container, matching_entities, first_condition, (return_query_value && last_condition) ? enm : nullptr);
	}

	//if need to return something specific, then do so, otherwise return list of matching entities
	if(query_return_value != nullptr)
		return query_return_value;

	EntityManipulation::SortEntitiesByID(matching_entities);
	return CreateListOfStringsIdsFromIteratorAndFunction(matching_entities, enm, [](Entity *e) { return e->GetIdStringId(); });
}

void EntityQueryCaches::GetMatchingEntities(std::span<const EntityQuery> queries,
	BitArrayIntegerSet &matching_entities, bool is_first)
{
	auto type = queries.front().queryType;
	if(type == ENT_QUERY_IN_ENTITY_LIST || type == ENT_QUERY_NOT_IN_ENTITY_LIST)
	{
		auto &temp = buffers.tempMatchingEntityIndices;
		temp.clear();
		auto &included_entities = is_first ? matching_entities : temp;
		if(is_first && type == ENT_QUERY_NOT_IN_ENTITY_LIST)
			matching_entities.SetAllIds(container->GetNumContainedEntities());

		EntityQueries::WithEntityIds(queries.front(), [&](const auto &ids, auto get_id) {
			for(auto item : ids)
			{
				size_t index = container->GetContainedEntityIndex(get_id(item));
				if(type == ENT_QUERY_NOT_IN_ENTITY_LIST)
					matching_entities.erase(index);
				else if(index != std::numeric_limits<size_t>::max())
				{
					//converted lists only grow the scratch set for matching IDs
					if constexpr(std::is_same_v<std::decay_t<decltype(ids)>, EntityQuery::EntityIds>)
						if(!is_first && !matching_entities.contains(index))
							continue;
					included_entities.insert(index);
				}
			}
		});

		if(type == ENT_QUERY_IN_ENTITY_LIST && !is_first)
			matching_entities.Intersect(temp);
		return;
	}

#if defined(MULTITHREAD_SUPPORT)
	Concurrency::ReadLock lock(mutex);
#endif
	std::vector<StringInternPool::StringID> labels_to_add;
	for(const auto &condition : queries)
	{
		auto label = EntityQueries::GetFilterLabel(condition);
		if(!DoesHaveLabel(label))
			labels_to_add.push_back(label);
	}
#if defined(MULTITHREAD_SUPPORT)
	EnsureLabelsAreCached(labels_to_add, lock);
#else
	EnsureLabelsAreCached(labels_to_add);
#endif

	for(const auto &condition : queries)
	{
		auto query = EntityQueries::GetFilterNode(condition);
		auto type = query->GetType();
		auto &ocn = query->GetOrderedChildNodes();
		auto label = EntityQueries::GetFilterLabel(condition);
		auto &temp = buffers.tempMatchingEntityIndices;
		switch(type)
		{
		case ENT_QUERY_EXISTS:
			if(is_first)
				sbfds.FindAllEntitiesWithFeature(label, matching_entities);
			else
				sbfds.IntersectEntitiesWithFeature(label, matching_entities, true);

			matching_entities.UpdateNumElements();
			break;

		case ENT_QUERY_NOT_EXISTS:
			if(is_first)
				sbfds.FindAllEntitiesWithoutFeature(label, matching_entities);
			else
				sbfds.IntersectEntitiesWithoutFeature(label, matching_entities, true);

			matching_entities.UpdateNumElements();
			break;

		case ENT_QUERY_EQUALS:
		case ENT_QUERY_NOT_EQUALS:
		{
			auto value = EvaluableNodeImmediateValueWithType::CreateValueFromEvaluableNode(ocn[1]);
			if(is_first && type == ENT_QUERY_EQUALS)
			{
				sbfds.UnionAllEntitiesWithValue(label, value, matching_entities);
			}
			else
			{
				if(is_first)
					sbfds.FindAllEntitiesWithFeature(label, matching_entities);

				temp.clear();
				sbfds.UnionAllEntitiesWithValue(label, value, temp);

				if(type == ENT_QUERY_EQUALS)
					matching_entities.Intersect(temp);
				else
					matching_entities.erase(temp);
			}
			break;
		}

		case ENT_QUERY_BETWEEN:
		case ENT_QUERY_NOT_BETWEEN:
		case ENT_QUERY_LESS_OR_EQUAL_TO:
		case ENT_QUERY_GREATER_OR_EQUAL_TO:
		{
			auto [value_type, low, high] = EntityQueries::GetQueryRange(query);
			if(is_first)
				sbfds.FindAllEntitiesWithinRange(label, value_type, low, high, matching_entities, type != ENT_QUERY_NOT_BETWEEN);
			else
			{
				temp.clear();
				sbfds.FindAllEntitiesWithinRange(label, value_type, low, high, temp, type != ENT_QUERY_NOT_BETWEEN);
				matching_entities.Intersect(temp);
			}
			break;
		}

		case ENT_QUERY_AMONG:
		case ENT_QUERY_NOT_AMONG:
		{
			if(type == ENT_QUERY_NOT_AMONG)
			{
				if(is_first)
					sbfds.FindAllEntitiesWithFeature(label, matching_entities);
				else
					sbfds.IntersectEntitiesWithFeature(label, matching_entities, false);
			}
			temp.clear();
			auto &matches = is_first && type == ENT_QUERY_AMONG ? matching_entities : temp;
			if(ocn.size() > 1 && EvaluableNode::IsOrderedArray(ocn[1]))
				for(auto node : ocn[1]->GetOrderedChildNodesReference())
					sbfds.UnionAllEntitiesWithValue(label, EvaluableNodeImmediateValueWithType::CreateValueFromEvaluableNode(node), matches);
			if(type == ENT_QUERY_NOT_AMONG)
				matching_entities.erase(temp);
			else if(!is_first)
				matching_entities.Intersect(temp);
			break;
		}

		default:;
		}
		is_first = false;
	}
}
