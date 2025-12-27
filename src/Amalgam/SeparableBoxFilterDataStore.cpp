//project headers:
#include "Entity.h"
#include "SeparableBoxFilterDataStore.h"

//system headers
#include <limits>

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
SeparableBoxFilterDataStore::SBFDSParametersAndBuffers SeparableBoxFilterDataStore::parametersAndBuffers;

void SeparableBoxFilterDataStore::BuildLabel(size_t column_index, const std::vector<Entity *> &entities)
{
	auto &column_data = columnData[column_index];
	auto label_id = column_data->stringId;

	//if the label is accessible, then don't need to check every label for being private,
	//can just inform entity to get on self for performance
	bool is_label_accessible = !Entity::IsLabelPrivate(label_id);

	//clear value interning if applied
	column_data->ConvertNumberInternsToValues();

	column_data->valueEntries.resize(entities.size());

	//populate data
	// maintaining the order of insertion of the entities from smallest to largest allows for better performance of the insertions
	// and every function called here assumes that entities are inserted in increasing order
	for(size_t entity_index = 0; entity_index < entities.size(); entity_index++)
	{
		auto [value, found] = entities[entity_index]->GetValueAtLabelAsImmediateValue(label_id, is_label_accessible);
		column_data->InsertNextIndexValueExceptNumbers(value.nodeType, value.nodeValue, entity_index);
	}

	OptimizeColumn(column_index);

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForColumn(column_index);
#endif
}

void SeparableBoxFilterDataStore::AddLabels(std::vector<StringInternPool::StringID> &label_sids,
	const std::vector<Entity *> &entities)
{
	//make sure have data to add
	if(label_sids.size() == 0 || entities.size() == 0)
		return;

	numEntities = std::max(numEntities, entities.size());

	//resize the column data storage and populate column and label_id lookups
	size_t num_columns_added = AddLabelsAsEmptyColumns(label_sids);

	size_t num_columns = columnData.size();
	size_t num_previous_columns = columnData.size() - num_columns_added;

#ifdef MULTITHREAD_SUPPORT
	//if big enough (enough entities and/or enough columns), try to use multithreading
	if(num_columns_added > 1 && (numEntities > 10000 || (numEntities > 200 && num_columns_added > 10)))
	{
		auto task_set = Concurrency::urgentThreadPool.CreateCountableTaskSet(num_columns_added);

		auto enqueue_task_lock = Concurrency::urgentThreadPool.AcquireTaskLock();
		for(size_t i = num_previous_columns; i < num_columns; i++)
		{
			Concurrency::urgentThreadPool.BatchEnqueueTask([this, &entities, i, &task_set]()
			{
				BuildLabel(i, entities);
				task_set.MarkTaskCompleted();
			}
			);
		}

		task_set.WaitForTasks(&enqueue_task_lock);
		return;
	}
	//not running concurrently
#endif

	for(size_t i = num_previous_columns; i < num_columns; i++)
		BuildLabel(i, entities);
}

void SeparableBoxFilterDataStore::RemoveColumnIndex(size_t column_index_to_remove)
{
#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif

	//will replace the values at index_to_remove with the values at index_to_move
	size_t column_index_to_move = columnData.size() - 1;
	StringInternPool::StringID label_id = columnData[column_index_to_remove]->stringId;

	//move data from the last column to the removed column if removing the label_id isn't the last column
	if(column_index_to_remove != column_index_to_move)
	{
		//update column lookup
		StringInternPool::StringID label_id_to_move = columnData[column_index_to_move]->stringId;
		labelIdToColumnIndex[label_id_to_move] = column_index_to_remove;

		//rearrange columns
		std::swap(columnData[column_index_to_remove], columnData[column_index_to_move]);
	}

	//remove the columnId lookup, reference, and column
	labelIdToColumnIndex.erase(label_id);
	columnData.pop_back();

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif
}

void SeparableBoxFilterDataStore::AddEntity(Entity *entity, size_t entity_index)
{
#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif

	for(auto &column_data : columnData)
	{
		auto [value, found] = entity->GetValueAtLabelAsImmediateValue(column_data->stringId);
		column_data->InsertIndexValue(value.nodeType, value.nodeValue, entity_index);
	}

	//count this entity
	if(entity_index >= numEntities)
		numEntities = entity_index + 1;

	OptimizeAllColumns();

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif
}

void SeparableBoxFilterDataStore::RemoveEntity(Entity *entity, size_t entity_index, size_t entity_index_to_reassign)
{
	if(entity_index >= numEntities || columnData.size() == 0)
		return;

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif

	//if was the last entity and reassigning the last one or one out of bounds,
	// simply delete from column data, delete last row, and return
	if(entity_index + 1 == GetNumInsertedEntities() && entity_index_to_reassign >= entity_index)
	{
		DeleteEntityIndexFromColumns(entity_index, true);

	#ifdef SBFDS_VERIFICATION
		VerifyAllEntitiesForAllColumns();
	#endif

		return;
	}

	//make sure it's a valid reassignment
	if(entity_index_to_reassign >= numEntities)
	{
	#ifdef SBFDS_VERIFICATION
		VerifyAllEntitiesForAllColumns();
	#endif
		return;
	}

	//if deleting a row and not replacing it, just fill as if it has no data
	if(entity_index == entity_index_to_reassign)
	{
		DeleteEntityIndexFromColumns(entity_index);

	#ifdef SBFDS_VERIFICATION
		VerifyAllEntitiesForAllColumns();
	#endif
		return;
	}

	//truncate cache if removing the last entry, either by moving the last entity or by directly removing the last
	bool remove_last_entity = (entity_index_to_reassign + 1 == numEntities
		|| (entity_index_to_reassign + 1 >= numEntities && entity_index + 1 == numEntities));

	//reassign index for each column
	for(size_t column_index = 0; column_index < columnData.size(); column_index++)
	{
		auto &column_data = columnData[column_index];

		auto &value_to_reassign = GetValue(entity_index_to_reassign, column_index);
		auto value_type_to_reassign = column_data->GetIndexValueType(entity_index_to_reassign);

		//change the destination to the value
		columnData[column_index]->ChangeIndexValue(value_type_to_reassign, value_to_reassign, entity_index);

		//remove the value where it is
		columnData[column_index]->DeleteIndexValue(value_type_to_reassign, value_to_reassign,
			entity_index_to_reassign, remove_last_entity);
	}

	if(remove_last_entity)
		numEntities--;
	
	//clean up any labels that aren't relevant
	RemoveAnyUnusedLabels();

	OptimizeAllColumns();

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif
}

void SeparableBoxFilterDataStore::UpdateAllEntityLabels(Entity *entity, size_t entity_index)
{
	if(entity_index >= numEntities)
		return;

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif

	for(auto &column_data : columnData)
	{
		auto [value, found] = entity->GetValueAtLabelAsImmediateValue(column_data->stringId);
		column_data->ChangeIndexValue(value.nodeType, value.nodeValue, entity_index);
	}

	//clean up any labels that aren't relevant
	RemoveAnyUnusedLabels();

	OptimizeAllColumns();

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForAllColumns();
#endif
}

void SeparableBoxFilterDataStore::UpdateEntityLabel(Entity *entity, size_t entity_index, StringInternPool::StringID label_updated)
{
	if(entity_index >= numEntities)
		return;

	//find the column
	auto column = labelIdToColumnIndex.find(label_updated);
	if(column == end(labelIdToColumnIndex))
		return;
	size_t column_index = column->second;
	auto column_data = columnData[column_index].get();

#ifdef SBFDS_VERIFICATION
	VerifyAllEntitiesForColumn(column_index);
#endif

	//get the new value
	auto [value, found] = entity->GetValueAtLabelAsImmediateValue(column_data->stringId);

	column_data->ChangeIndexValue(value.nodeType, value.nodeValue, entity_index);

	//remove the label if no longer relevant
	if(IsColumnIndexRemovable(column_index))
		RemoveColumnIndex(column_index);
	else
		OptimizeColumn(column_index);

}

//populates distances_out with all entities and their distances that have a distance to target less than max_dist
// and sets distances_out to the found entities.  Infinity is allowed to compute all distances.
//if enabled_indices is not nullptr, it will only find distances to those entities, and it will modify enabled_indices in-place
// removing entities that do not have the corresponding labels
void SeparableBoxFilterDataStore::FindEntitiesWithinDistance(GeneralizedDistanceEvaluator &dist_eval,
	double max_dist, StringInternPool::StringID radius_label,
	BitArrayIntegerSet &enabled_indices, std::vector<DistanceReferencePair<size_t>> &distances_out)
{
	if(GetNumInsertedEntities() == 0 || dist_eval.featureAttribs.size() == 0)
		return;

	auto &r_dist_eval = parametersAndBuffers.rDistEvaluator;
	r_dist_eval.distEvaluator = &dist_eval;
	
	bool high_accuracy = dist_eval.highAccuracyDistances;
	double max_dist_exponentiated = dist_eval.ExponentiateDifferenceTerm(max_dist, high_accuracy);
	
	//initialize all distances to 0
	auto &distances = parametersAndBuffers.entityDistances;
	distances.clear();
	distances.resize(GetNumInsertedEntities(), 0.0);

	//if there is a radius, then change the flow such that every distance starts out with the negative of the maximum
	//distance, such that if the distance is greater than zero, it is too far away
	//this requires populating every initial distance with either the exponentiated maximum distance, or the
	//exponentiated maximum distance plus the radius
	size_t radius_column_index = GetColumnIndexFromLabelId(radius_label);
	if(radius_column_index < columnData.size())
	{
		auto &radius_column_data = columnData[radius_column_index];
		for(auto entity_index : enabled_indices)
		{
			double radius = 0.0;
			auto [radius_value_type, radius_value] = radius_column_data->GetResolvedIndexValueTypeAndValue(entity_index);
			if(radius_value_type == ENIVT_NUMBER)
				radius = radius_value.number;

			if(radius == 0)
				distances[entity_index] = -max_dist_exponentiated;
			else
				distances[entity_index] = -dist_eval.ExponentiateDifferenceTerm(max_dist + radius, high_accuracy);
		}

		max_dist_exponentiated = 0.0;
	}

	//for each desired feature, compute and add distance terms of possible window query candidate entities
	for(size_t query_feature_index = 0; query_feature_index < dist_eval.featureAttribs.size(); query_feature_index++)
	{
		size_t absolute_feature_index = dist_eval.featureAttribs[query_feature_index].featureIndex;
		auto &column_data = columnData[absolute_feature_index];
		auto &target_value = r_dist_eval.featureData[query_feature_index].targetValue;

		if(target_value.IsNull())
		{
			//add the appropriate unknown distance to each element
			double unknown_unknown_term = dist_eval.ComputeDistanceTermUnknownToUnknown(query_feature_index);
			double known_unknown_term = dist_eval.ComputeDistanceTermKnownToUnknown(query_feature_index);

			auto &null_indices = column_data->nullIndices;
			for(auto entity_index : enabled_indices)
			{
				if(null_indices.contains(entity_index))
					distances[entity_index] += unknown_unknown_term;
				else
					distances[entity_index] += known_unknown_term;

				//remove entity if its distance is already greater than the max_dist
				if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
					enabled_indices.erase(entity_index, false);
			}

			continue;
		}

		if(target_value.nodeType == ENIVT_NUMBER)
		{
			//below we branch to optimize the number of distance terms that need to be computed to solve minimum distance problem
			//if there are fewer enabled_indices than the number of unique values for this feature, plus one for unknown values
			// it is usually faster (less distances to compute) to just compute distance for each unique value and add to associated sums
			// unless it happens to be that enabled_indices is very skewed
			if(column_data->sortedNumberValueEntries.size() < enabled_indices.size())
			{
				for(auto &value_entry : column_data->sortedNumberValueEntries)
				{
					//get distance term that is applicable to each entity in this bucket
					double distance_term = r_dist_eval.ComputeDistanceTerm(
						value_entry.first, ENIVT_NUMBER, query_feature_index, high_accuracy);

					//for each bucket, add term to their sums
					for(auto entity_index : value_entry.second.indicesWithValue)
					{
						if(!enabled_indices.contains(entity_index))
							continue;

						distances[entity_index] += distance_term;

						//remove entity if its distance is already greater than the max_dist, won't ever become NaN here (would already have been removed from indices)
						if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
							enabled_indices.erase(entity_index, false);
					}
				}

				//populate all non-number distances
				double unknown_dist = dist_eval.ComputeDistanceTermKnownToUnknown(query_feature_index);
				for(auto entity_index : enabled_indices)
				{
					//skip over number values
					if(column_data->numberIndices.contains(entity_index))
						continue;

					distances[entity_index] += unknown_dist;

					//remove entity if its distance is already greater than the max_dist
					if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
						enabled_indices.erase(entity_index, false);
				}

				continue;
			}
		}
		
		//if target_value_type == ENIVT_CODE or ENIVT_STRING_ID or ENIVT_BOOL, just compute all
		// won't save much for code until cache equal values
		// won't save much for string ids because it's just a lookup (though could make it a little faster by streamlining a specialized string loop)
		//else, there are less indices to consider than possible unique values, so save computation by just considering entities that are still valid
		for(auto entity_index : enabled_indices)
		{
			auto [value_type, value] = column_data->GetResolvedIndexValueTypeAndValue(entity_index);
			distances[entity_index] += r_dist_eval.ComputeDistanceTerm(
											value, value_type, query_feature_index, high_accuracy);

			//remove entity if its distance is already greater than the max_dist
			if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
				enabled_indices.erase(entity_index, false);
		}
	}

	//populate distances_out vector
	distances_out.reserve(enabled_indices.size());
	//need to recompute distances in several circumstances, including if radius is computed,
	// as the intermediate result may be negative and yield an incorrect result otherwise
	bool need_recompute_distances = ((dist_eval.recomputeAccurateDistances && !dist_eval.highAccuracyDistances)
		|| radius_column_index < columnData.size());
	high_accuracy = (dist_eval.recomputeAccurateDistances || dist_eval.highAccuracyDistances);

	if(!need_recompute_distances)
	{
		for(auto index : enabled_indices)
			distances_out.emplace_back(dist_eval.InverseExponentiateDistance(distances[index], high_accuracy), index);
	}
	else
	{
		for(auto index : enabled_indices)
			distances_out.emplace_back(GetDistanceBetween(r_dist_eval, radius_column_index, index, true), index);
	}
}

template<bool expand_to_first_nonzero_distance, bool compute_surprisal>
void SeparableBoxFilterDataStore::FindNearestEntities(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	std::vector<StringInternPool::StringID> &position_label_sids,
	size_t top_k, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
	std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream)
{
	auto &dist_eval = *r_dist_eval.distEvaluator;
	if(top_k == 0 || GetNumInsertedEntities() == 0 || dist_eval.featureAttribs.size() == 0)
		return;

	size_t num_enabled_features = dist_eval.featureAttribs.size();

	enabled_indices.erase(ignore_index);

	size_t radius_column_index = GetColumnIndexFromLabelId(radius_label);

	//if num enabled indices < top_k, return sorted distances
	if(enabled_indices.size() <= top_k)
		return FindAllValidElementDistances<compute_surprisal>(r_dist_eval, radius_column_index, enabled_indices, distances_out, rand_stream);
	
	size_t end_index = enabled_indices.GetEndInteger();
	bool high_accuracy = dist_eval.highAccuracyDistances;

	//reuse the appropriate partial_sums_buffer buffer
	auto &partial_sums = parametersAndBuffers.partialSums;
	partial_sums.ResizeAndClear(num_enabled_features, end_index);

	//calculate the partial sums for the cases that best match for each feature
	// and populate the vectors of smallest possible distances that haven't been computed yet
	auto &min_unpopulated_distances = parametersAndBuffers.minUnpopulatedDistances;
	auto &min_distance_by_unpopulated_count = parametersAndBuffers.minDistanceByUnpopulatedCount;
	PopulateInitialPartialSums(r_dist_eval, top_k, radius_column_index, high_accuracy,
		enabled_indices, min_unpopulated_distances, min_distance_by_unpopulated_count);
	
	auto &potential_good_matches = parametersAndBuffers.potentialGoodMatches;
	PopulatePotentialGoodMatches(potential_good_matches, enabled_indices, partial_sums, top_k);

	//reuse, clear, and set up sorted_results
	auto &sorted_results = parametersAndBuffers.sortedResults;
	//assume there's an error in each addition and subtraction
	sorted_results.Reset(rand_stream.CreateOtherStreamViaRand(), top_k, std::numeric_limits<double>::epsilon());

	//parse the sparse inline hash of good match nodes directly into the compacted vector of good matches
	while(potential_good_matches.size() > 0)
	{
		size_t entity_index = potential_good_matches.top().reference;
		potential_good_matches.pop();

		//skip this entity in the next loops
		enabled_indices.erase(entity_index);

		//insert random selection into results heap
		double distance = ResolveDistanceToNonMatchTargetValues<compute_surprisal>(r_dist_eval,
			partial_sums, entity_index, num_enabled_features, high_accuracy);
		sorted_results.Push(DistanceReferencePair(distance, entity_index));
	}

	//if we did not find K results (search failed), we must populate the remaining K cases/results to search from another way
	//we will randomly select additional nodes to fill K results.  random to prevent bias/patterns
	while(sorted_results.Size() < top_k)
	{
		//get a random index that is still potentially in the knn (neither rejected nor already in the results)
		size_t random_index = enabled_indices.GetRandomElement(rand_stream);

		//skip this entity in the next loops
		enabled_indices.erase(random_index);

		double distance = ResolveDistanceToNonMatchTargetValues<compute_surprisal>(r_dist_eval,
			partial_sums, random_index, num_enabled_features, high_accuracy);
		sorted_results.Push(DistanceReferencePair(distance, random_index));
	}

	//have already gone through all records looking for top_k, if don't have top_k, then have exhausted search
	if(sorted_results.Size() == top_k)
	{
		double worst_candidate_distance = sorted_results.Top().distance;
		if(expand_to_first_nonzero_distance && !sorted_results.TopMeetsThreshold())
		{
			for(size_t entity_index = 0; entity_index < end_index; entity_index++)
			{
				//don't need to check maximum index, because already checked in loop
				if(!enabled_indices.ContainsWithoutMaximumIndexCheck(entity_index))
					continue;

				enabled_indices.erase(entity_index);

				double distance = ResolveDistanceToNonMatchTargetValues<compute_surprisal>(r_dist_eval,
						partial_sums, entity_index, num_enabled_features, high_accuracy);
				sorted_results.Push(DistanceReferencePair(distance, entity_index));

				if(sorted_results.TopMeetsThreshold())
				{
					worst_candidate_distance = sorted_results.Top().distance;
					break;
				}
			}
		}

		if(num_enabled_features > 1)
		{
			auto &previous_nn_cache = parametersAndBuffers.previousQueryNearestNeighbors;
			for(size_t entity_index : previous_nn_cache)
			{
				//only get its distance if it is enabled,
				//but erase to skip this entity in the next loop
				if(!enabled_indices.EraseAndRetrieve(entity_index))
					continue;

				auto [accept, distance] = ResolveDistanceToNonMatchTargetValuesUnlessRejected<compute_surprisal>(r_dist_eval, partial_sums,
					entity_index, min_distance_by_unpopulated_count, num_enabled_features,
					worst_candidate_distance, min_unpopulated_distances, high_accuracy);

				if(accept)
					worst_candidate_distance = sorted_results.PushAndPop<expand_to_first_nonzero_distance>(
						DistanceReferencePair(distance, entity_index)).distance;
			}
		}

		//check to see if any features can have nulls quickly removed because it would push it past worst_candidate_distance
		bool need_enabled_indices_recount = false;
		for(size_t i = 0; i < num_enabled_features; i++)
		{
			//if the target_value is a null, unknown-unknown differences have already been accounted for
			//since they are partial matches
			auto &feature_data = r_dist_eval.featureData[i];
			if(feature_data.targetValue.IsNull())
				continue;

			if(dist_eval.ComputeDistanceTermKnownToUnknown(i) > worst_candidate_distance)
			{
				size_t column_index = dist_eval.featureAttribs[i].featureIndex;
				auto &column = columnData[column_index];
				auto &null_indices = column->nullIndices;
				//make sure there's enough nulls to justify running through all of enabled_indices
				if(null_indices.size() > 20)
				{
					null_indices.EraseInBatchFrom(enabled_indices);
					need_enabled_indices_recount = true;
				}
			}
		}

		if(need_enabled_indices_recount)
			enabled_indices.UpdateNumElements();

		//re-fetch last index just in case enabled_indices was modified anywhere above
		end_index = enabled_indices.GetEndInteger();

		//pick up where left off, already have top_k in sorted_results or are out of entities
		#pragma omp parallel shared(worst_candidate_distance) if(end_index > 200)
		{
			//iterate over all indices
			#pragma omp for schedule(static)
			for(int64_t entity_index = 0; entity_index < static_cast<int64_t>(end_index); entity_index++)
			{
				//don't need to check maximum index, because already checked in loop
				if(!enabled_indices.ContainsWithoutMaximumIndexCheck(entity_index))
					continue;

				auto [accept, distance] = ResolveDistanceToNonMatchTargetValuesUnlessRejected<compute_surprisal>(r_dist_eval,
					partial_sums, entity_index, min_distance_by_unpopulated_count, num_enabled_features,
					worst_candidate_distance, min_unpopulated_distances, high_accuracy);

				if(!accept)
					continue;

			#ifdef _OPENMP
			#pragma omp critical
				{
					//need to check again after going into critical section
					if(distance <= worst_candidate_distance)
					{
					#endif
						//computed the actual distance here, attempt to insert into final sorted results
						worst_candidate_distance = sorted_results.PushAndPop<expand_to_first_nonzero_distance>(
							DistanceReferencePair<size_t>(distance, entity_index)).distance;

					#ifdef _OPENMP
					}
				}
			#endif

			} //for partialSums instances
		}  //#pragma omp parallel

	} // sorted_results.Size() == top_k

	ConvertSortedDistanceSumsToDistancesAndCacheResults(sorted_results, r_dist_eval, radius_column_index, distances_out);
}

template void SeparableBoxFilterDataStore::FindNearestEntities<true, true>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	std::vector<StringInternPool::StringID> &position_label_sids,
	size_t top_k, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
	std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream);

template void SeparableBoxFilterDataStore::FindNearestEntities<true, false>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	std::vector<StringInternPool::StringID> &position_label_sids,
	size_t top_k, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
	std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream);

template void SeparableBoxFilterDataStore::FindNearestEntities<false, true>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	std::vector<StringInternPool::StringID> &position_label_sids,
	size_t top_k, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
	std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream);

template void SeparableBoxFilterDataStore::FindNearestEntities<false, false>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	std::vector<StringInternPool::StringID> &position_label_sids,
	size_t top_k, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
	std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream);

void SeparableBoxFilterDataStore::DeleteEntityIndexFromColumns(size_t entity_index, bool remove_last_entity)
{
	for(size_t i = 0; i < columnData.size(); i++)
	{
		auto &column_data = columnData[i];
		auto &feature_value = GetValue(entity_index, i);
		auto feature_type = column_data->GetIndexValueType(entity_index);
		column_data->DeleteIndexValue(feature_type, feature_value, entity_index, remove_last_entity);
	}

	if(remove_last_entity)
		numEntities--;
}

size_t SeparableBoxFilterDataStore::AddLabelsAsEmptyColumns(std::vector<StringInternPool::StringID> &label_sids)
{
	size_t num_inserted_columns = 0;

	//create columns for the labels, don't count any that already exist
	for(auto label_id : label_sids)
	{
		auto [_, inserted] = labelIdToColumnIndex.emplace(label_id, columnData.size());
		if(inserted)
		{
			columnData.emplace_back(std::make_unique<SBFDSColumnData>(label_id));
			columnData.back()->valueEntries.resize(numEntities);
			num_inserted_columns++;
		}
	}

	return num_inserted_columns;
}

template<bool compute_surprisal>
double SeparableBoxFilterDataStore::PopulatePartialSumsWithSimilarFeatureValue(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t num_entities_to_populate, bool expand_search_if_optimal, bool high_accuracy,
	size_t query_feature_index, BitArrayIntegerSet &enabled_indices)
{
	auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
	auto &feature_data = r_dist_eval.featureData[query_feature_index];
	size_t absolute_feature_index = feature_attribs.featureIndex;
	auto &column = columnData[absolute_feature_index];
	auto feature_type = feature_attribs.featureType;
	auto &value = feature_data.targetValue;

	//need to accumulate values for nulls if the value is a null
	if(value.IsNull())
	{
		double unknown_unknown_term = r_dist_eval.distEvaluator->ComputeDistanceTermUnknownToUnknown(query_feature_index);

		//if it's either a symmetric nominal or continuous, or if sparse deviation matrix but no null value,
		// then there are only two values, unknown to known or known
		if(feature_attribs.IsFeatureSymmetricNominal()
			|| feature_attribs.IsFeatureContinuous()
			|| (feature_attribs.IsFeatureNominal() &&
				!r_dist_eval.HasNominalSpecificKnownToUnknownDistanceTerm(query_feature_index)))
		{
			double known_unknown_term = r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(query_feature_index);

			//if all cases are equidistant and nonzero, then don't compute anything
			if(unknown_unknown_term == known_unknown_term && unknown_unknown_term > 0)
				return unknown_unknown_term;

			if(unknown_unknown_term < known_unknown_term || known_unknown_term == 0.0)
				AccumulatePartialSums(enabled_indices, column->nullIndices, query_feature_index, unknown_unknown_term);

			if(known_unknown_term < unknown_unknown_term || unknown_unknown_term == 0.0)
			{
				BitArrayIntegerSet &known_unknown_indices = parametersAndBuffers.potentialMatchesSet;
				known_unknown_indices = enabled_indices;
				column->nullIndices.EraseTo(known_unknown_indices);
				AccumulatePartialSums(enabled_indices, known_unknown_indices, query_feature_index, known_unknown_term);
			}

			double largest_term_not_computed = std::max(known_unknown_term, unknown_unknown_term);
			//if the largest term not computed is zero, then have computed everything,
			// so set the remaining value to infinity to push this term off sorting of uncomputed distances
			// and make search more efficient
			if(largest_term_not_computed == 0.0)
				largest_term_not_computed = std::numeric_limits<double>::infinity();

			//make computing the rest more efficient
			feature_data.SetPrecomputedRemainingIdenticalDistanceTerm(largest_term_not_computed);
			return largest_term_not_computed;
		}
		else //nonsymmetric nominal -- need to compute
		{
			AccumulatePartialSums(enabled_indices, column->nullIndices, query_feature_index, unknown_unknown_term);

			double nonmatch_dist_term = r_dist_eval.ComputeDistanceTermNominalNonNullSmallestNonmatch<compute_surprisal>(query_feature_index);
			//if the next closest match is larger, no need to compute any more values
			if(nonmatch_dist_term > unknown_unknown_term)
				return nonmatch_dist_term;

			//if there are terms smaller than unknown_unknown_term, then need to compute any other nominal values
			r_dist_eval.IterateOverNominalValuesWithLessOrEqualDistanceTerms(
				feature_data.nominalNumberDistanceTerms, unknown_unknown_term,
				[this, &r_dist_eval, &enabled_indices, &column, query_feature_index](double number_value)
				{
					AccumulatePartialSumsForNominalNumberValue(r_dist_eval, enabled_indices, number_value, query_feature_index, *column);
				});

			r_dist_eval.IterateOverNominalValuesWithLessOrEqualDistanceTerms(
				feature_data.nominalStringDistanceTerms, unknown_unknown_term,
				[this, &r_dist_eval, &enabled_indices, &column, query_feature_index](StringInternPool::StringID sid)
				{
					AccumulatePartialSumsForNominalStringIdValue(r_dist_eval, enabled_indices, sid, query_feature_index, *column);
				});

			return r_dist_eval.ComputeDistanceTermNonNullNominalNextSmallest<compute_surprisal>(unknown_unknown_term, query_feature_index);
		}
	}

	//if symmetric nominal, only need to compute the exact match
	if(feature_attribs.IsFeatureSymmetricNominal())
	{
		if(value.nodeType == ENIVT_NUMBER)
		{
			AccumulatePartialSumsForNominalNumberValue(r_dist_eval, enabled_indices, value.nodeValue.number, query_feature_index, *column);
		}
		else if(value.nodeType == ENIVT_STRING_ID)
		{
			AccumulatePartialSumsForNominalStringIdValue(r_dist_eval, enabled_indices, value.nodeValue.stringID, query_feature_index, *column);
		}
		else if(value.nodeType == ENIVT_BOOL)
		{
			AccumulatePartialSumsForBoolValue(r_dist_eval, enabled_indices, value.nodeValue.boolValue, query_feature_index, *column);
		}
		else if(value.nodeType == ENIVT_CODE)
		{
			//compute partial sums for all code of matching size
			size_t code_size = EvaluableNode::GetDeepSize(value.nodeValue.code);

			auto value_found = column->valueCodeSizeToIndices.find(code_size);
			if(value_found != end(column->valueCodeSizeToIndices))
			{
				auto &entity_indices = *(value_found->second);
				ComputeAndAccumulatePartialSums<compute_surprisal>(r_dist_eval, enabled_indices, entity_indices,
					query_feature_index, absolute_feature_index, high_accuracy);
			}
		}
		//else value_type == ENIVT_NULL and already covered above

		//return the value that the remainder of the entities have
		double nonmatch_dist_term = feature_attribs.nominalSymmetricNonMatchDistanceTerm;
		feature_data.SetPrecomputedRemainingIdenticalDistanceTerm(nonmatch_dist_term);
		return nonmatch_dist_term;
	}
	else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_BOOL)
	{
		bool comparison_value = value.nodeValue.boolValue;
		if(value.nodeType == ENIVT_BOOL)
			AccumulatePartialSumsForBoolValue(
				r_dist_eval, enabled_indices, comparison_value, query_feature_index, *column);

		size_t num_true = column->trueBoolIndices.size();
		size_t num_false = column->trueBoolIndices.size();
		size_t num_non_bool = (numEntities - column->invalidIndices.size()) - (num_true + num_false);
		double nonmatch_dist_term = r_dist_eval.ComputeDistanceTermNominal(!comparison_value, ENIVT_BOOL, query_feature_index);

		if(num_non_bool == 0)
		{
			feature_data.SetPrecomputedRemainingIdenticalDistanceTerm(nonmatch_dist_term);
			return nonmatch_dist_term;
		}

		double known_unknown_term = r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(query_feature_index);

		size_t num_opposite = (comparison_value ? num_false : num_true);
		if(num_opposite < 2000 || known_unknown_term < nonmatch_dist_term)
		{
			AccumulatePartialSumsForBoolValue(r_dist_eval, enabled_indices, !comparison_value, query_feature_index, *column);

			feature_data.SetPrecomputedRemainingIdenticalDistanceTerm(known_unknown_term);
			return known_unknown_term;
		}

		return std::min(nonmatch_dist_term, known_unknown_term);
	}
	else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING)
	{
		//initialize to zero, because if don't find an exact match, but there are distance terms of
		//0, then need to accumulate those later
		double accumulated_term = 0.0;
		if(value.nodeType == ENIVT_STRING_ID)
			accumulated_term = AccumulatePartialSumsForNominalStringIdValue(
				r_dist_eval, enabled_indices, value.nodeValue.stringID, query_feature_index, *column);

		double nonmatch_dist_term = r_dist_eval.ComputeDistanceTermNominalNonNullSmallestNonmatch<compute_surprisal>(query_feature_index);
		//if the next closest match is larger, no need to compute any more values
		if(nonmatch_dist_term > accumulated_term)
			return nonmatch_dist_term;

		//need to iterate over everything with the same distance term
		r_dist_eval.IterateOverNominalValuesWithLessOrEqualDistanceTerms(
			feature_data.nominalStringDistanceTerms, accumulated_term,
			[this, &value, &r_dist_eval, &enabled_indices, &column, query_feature_index](StringInternPool::StringID sid)
			{
				//don't want to double-accumulate the exact match
				if(sid != value.nodeValue.stringID)
					AccumulatePartialSumsForNominalStringIdValue(
						r_dist_eval, enabled_indices, value.nodeValue.stringID, query_feature_index, *column);
			});

		return r_dist_eval.ComputeDistanceTermNonNullNominalNextSmallest<compute_surprisal>(nonmatch_dist_term, query_feature_index);
	}
	else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER)
	{
		//initialize to zero, because if don't find an exact match, but there are distance terms of
		//0, then need to accumulate those later
		double accumulated_term = 0.0;
		if(value.nodeType == ENIVT_NUMBER)
			accumulated_term = AccumulatePartialSumsForNominalNumberValue(
				r_dist_eval, enabled_indices, value.nodeValue.number, query_feature_index, *column);

		double nonmatch_dist_term = r_dist_eval.ComputeDistanceTermNominalNonNullSmallestNonmatch<compute_surprisal>(query_feature_index);
		//if the next closest match is larger, no need to compute any more values
		if(nonmatch_dist_term > accumulated_term)
			return nonmatch_dist_term;

		//need to iterate over everything with the same distance term
		r_dist_eval.IterateOverNominalValuesWithLessOrEqualDistanceTerms(
			feature_data.nominalNumberDistanceTerms, accumulated_term,
			[this, &value, &r_dist_eval, &enabled_indices, &column, query_feature_index](double number_value)
			{
				//don't want to double-accumulate the exact match
				if(!EqualIncludingNaN(number_value, value.nodeValue.number))
					AccumulatePartialSumsForNominalNumberValue(
						r_dist_eval, enabled_indices, value.nodeValue.number, query_feature_index, *column);
			});

		return r_dist_eval.ComputeDistanceTermNonNullNominalNextSmallest<compute_surprisal>(nonmatch_dist_term, query_feature_index);
	}
	else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE
		|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE_NO_RECURSIVE_MATCHING
		|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE)
	{
		//compute partial sums for all code of matching size
		size_t code_size = 1;
		if(value.nodeType == ENIVT_CODE)
			code_size = EvaluableNode::GetDeepSize(value.nodeValue.code);

		auto value_found = column->valueCodeSizeToIndices.find(code_size);
		if(value_found != end(column->valueCodeSizeToIndices))
		{
			auto &entity_indices = *(value_found->second);
			ComputeAndAccumulatePartialSums<compute_surprisal>(r_dist_eval, enabled_indices, entity_indices,
				query_feature_index, absolute_feature_index, high_accuracy);
		}

		if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE)
		{
			double nonmatch_dist_term = r_dist_eval.ComputeDistanceTermNominalNonNullSmallestNonmatch<compute_surprisal>(query_feature_index);
			return nonmatch_dist_term;
		}
		else //GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE_NO_RECURSIVE_MATCHING or GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE
		{
			//next most similar code must be at least a distance of 1 edit away
			return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousNonCyclicNonNullRegular<compute_surprisal>(1.0, query_feature_index, high_accuracy);
		}
	}
	else if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING)
	{
		if(value.nodeType == ENIVT_STRING_ID)
		{
			auto value_found = column->stringIdValueEntries.find(value.nodeValue.stringID);
			if(value_found != end(column->stringIdValueEntries))
			{
				double term = r_dist_eval.distEvaluator->ComputeDistanceTermContinuousExactMatch<compute_surprisal>(query_feature_index, high_accuracy);
				AccumulatePartialSums(enabled_indices, value_found->second->indicesWithValue, query_feature_index, term);
			}
		}

		//the next closest string will have an edit distance of 1
		return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousNonCyclicNonNullRegular<compute_surprisal>(1.0, query_feature_index, high_accuracy);
	}
	//else feature_type == FDT_CONTINUOUS_NUMBER or FDT_CONTINUOUS_NUMBER_CYCLIC

	//if not a number or no numbers available, then no size
	if(value.nodeType != ENIVT_NUMBER || column->sortedNumberValueEntries.size() == 0)
		return GetMaxDistanceTermForContinuousFeature<compute_surprisal>(r_dist_eval, query_feature_index, absolute_feature_index, high_accuracy);

	bool cyclic_feature = r_dist_eval.distEvaluator->IsFeatureCyclic(query_feature_index);
	double cycle_length = std::numeric_limits<double>::infinity();
	if(cyclic_feature)
		cycle_length = feature_attribs.typeAttributes.maxCyclicDifference;

	auto value_entry_iter = column->FindClosestValueEntryForNumberValue(value.nodeValue.number, cycle_length);

	double term = 0.0;
	if(value_entry_iter->first == value.nodeValue.number)
		term = ComputeDistanceTermContinuousExactMatch<compute_surprisal>(r_dist_eval, value_entry_iter->second, query_feature_index, high_accuracy);
	else
		term = ComputeDistanceTermContinuousNonNullRegular<compute_surprisal>(r_dist_eval,
			value.nodeValue.number, value_entry_iter->second, query_feature_index, high_accuracy);

	size_t num_entities_computed = AccumulatePartialSums(enabled_indices, value_entry_iter->second.indicesWithValue, query_feature_index, term);

	//the logic below assumes there are at least two entries
	size_t num_unique_number_values = column->sortedNumberValueEntries.size();
	if(num_unique_number_values <= 1)
		return term;

	//if we haven't filled max_count results, or searched num_buckets, keep expanding search to neighboring buckets
	auto lower_value_iter = value_entry_iter;
	auto upper_value_iter = value_entry_iter;
	auto first_value_entry_iter = begin(column->sortedNumberValueEntries);
	auto last_value_entry_iter = --end(column->sortedNumberValueEntries);

	//largest term encountered so far
	double largest_term = term;

	//used for calculating the gaps between values
	double last_diff = 0.0;
	double largest_diff_delta = 0.0;

	//put a max limit to the number of cases
	size_t max_cases_relative_to_total = std::min(static_cast<size_t>(2000), static_cast<size_t>(parametersAndBuffers.partialSums.numInstances / 8));
	size_t max_num_to_find = std::max(num_entities_to_populate, max_cases_relative_to_total);

	//if one dimension or don't want to expand search, then cut off early
	if(!expand_search_if_optimal)
		max_num_to_find = num_entities_to_populate;

	//compute along the feature
	while(num_entities_computed < max_num_to_find)
	{
		//see if can compute one bucket lower
		bool compute_lower = false;
		double lower_diff = 0.0;
		decltype(value_entry_iter) next_lower_iter;
		if(!cyclic_feature)
		{
			if(lower_value_iter != first_value_entry_iter)
			{
				next_lower_iter = lower_value_iter;
				--next_lower_iter;

				lower_diff = std::abs(value.nodeValue.number - next_lower_iter->first);
				compute_lower = true;
			}
		}
		else //cyclic_feature
		{
			decltype(value_entry_iter) next_iter;
			if(lower_value_iter != first_value_entry_iter)
			{
				next_iter = lower_value_iter;
				--next_iter;
			}
			else
			{
				next_iter = last_value_entry_iter;
			}

			//done if wrapped completely around
			if(next_iter == value_entry_iter)
				break;

			next_lower_iter = next_iter;
			lower_diff = GeneralizedDistanceEvaluator::ConstrainDifferenceToCyclicDifference(
				std::abs(value.nodeValue.number - next_lower_iter->first),
				cycle_length);
			compute_lower = true;
		}

		//see if can compute one bucket upper
		bool compute_upper = false;
		double upper_diff = 0.0;
		decltype(value_entry_iter) next_upper_iter;
		if(!cyclic_feature)
		{
			if(upper_value_iter != last_value_entry_iter)
			{
				next_upper_iter = upper_value_iter;
				++next_upper_iter;

				upper_diff = std::abs(value.nodeValue.number - next_upper_iter->first);
				compute_upper = true;
			}
		}
		else //cyclic_feature
		{
			decltype(value_entry_iter) next_iter;
			if(upper_value_iter != last_value_entry_iter)
			{
				next_iter = upper_value_iter;
				++next_iter;
			}
			else
			{
				next_iter = first_value_entry_iter;
			}

			//done if wrapped completely around
			if(next_iter == value_entry_iter)
				break;

			next_upper_iter = next_iter;
			upper_diff = GeneralizedDistanceEvaluator::ConstrainDifferenceToCyclicDifference(
				std::abs(value.nodeValue.number - next_upper_iter->first), cycle_length);
			compute_upper = true;
		}

		//determine the next closest point and its difference
		double next_closest_diff;
		decltype(value_entry_iter) next_closest_iter;

		//if can only compute lower or lower is closer, then compute lower
		if((compute_lower && !compute_upper)
			|| (compute_lower && compute_upper && lower_diff < upper_diff))
		{
			next_closest_diff = lower_diff;
			next_closest_iter = next_lower_iter;
			lower_value_iter = next_lower_iter;
		}
		else if(compute_upper)
		{
			next_closest_diff = upper_diff;
			next_closest_iter = next_upper_iter;
			upper_value_iter = next_upper_iter;
		}
		else //nothing left, end
		{
			break;
		}

		//if running into the extra_iterations
		if(num_entities_computed >= num_entities_to_populate)
		{
			//use heuristic to decide whether to continue populating based on whether this diff will help the overall distance cutoffs
			// look at the rate of change of the difference compared to before, and how many new entities will be populated
			// if it is too small and doesn't fill enough (or fills too many), then stop expanding
			size_t potential_entities = next_closest_iter->second.indicesWithValue.size();
			if(num_entities_computed + potential_entities > max_num_to_find)
				break;

			//determine if it should continue based on how much this difference will contribute to the total; either a big jump or enough entities
			bool should_continue = false;
			double diff_delta = next_closest_diff - last_diff;

			if(diff_delta >= largest_diff_delta)
				should_continue = true;

			if(diff_delta >= largest_diff_delta / 2 && potential_entities >= 2)
				should_continue = true;

			//going out n deviations is likely to only miss 0.5^GeneralizedDistanceEvaluator::s_deviation_expansion
			// of the likely values of nearest neighbors
			if(r_dist_eval.distEvaluator->DoesFeatureHaveDeviation(query_feature_index)
				&& next_closest_diff < GeneralizedDistanceEvaluator::s_deviation_expansion * feature_attribs.deviation)
			{
				should_continue = true;
			}
			else //exceeded the deviation expansion, so can enable fast surprisal computation
			{	
				if(r_dist_eval.distEvaluator->computeSurprisal)
					r_dist_eval.distEvaluator->featureAttribs[query_feature_index].fastApproxDeviation = true;
			}

			if(!should_continue)
				break;
		}

		term = ComputeDistanceTermContinuousNonNullRegular<compute_surprisal>(r_dist_eval,
			value.nodeValue.number, next_closest_iter->second, query_feature_index, high_accuracy);
		num_entities_computed += AccumulatePartialSums(enabled_indices, next_closest_iter->second.indicesWithValue, query_feature_index, term);

		//track the rate of change of difference
		if(next_closest_diff - last_diff > largest_diff_delta)
			largest_diff_delta = next_closest_diff - last_diff;
		last_diff = next_closest_diff;

		//keep track of the largest seen so far
		if(term > largest_term)
			largest_term = term;

		//if cyclic and have wrapped around or computed every value, then exit
		if(lower_value_iter->first >= upper_value_iter->first
				|| (lower_value_iter == first_value_entry_iter && upper_value_iter == last_value_entry_iter))
			break;
	}

	//return the largest computed so far
	return largest_term;
}

template double SeparableBoxFilterDataStore::PopulatePartialSumsWithSimilarFeatureValue<true>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t num_entities_to_populate, bool expand_search_if_optimal, bool high_accuracy,
	size_t query_feature_index, BitArrayIntegerSet &enabled_indices);

template double SeparableBoxFilterDataStore::PopulatePartialSumsWithSimilarFeatureValue<false>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t num_entities_to_populate, bool expand_search_if_optimal, bool high_accuracy,
	size_t query_feature_index, BitArrayIntegerSet &enabled_indices);

template<bool compute_surprisal>
void SeparableBoxFilterDataStore::PopulateInitialPartialSums(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t top_k, size_t radius_column_index, bool high_accuracy,
	BitArrayIntegerSet &enabled_indices, std::vector<double> &min_unpopulated_distances, std::vector<double> &min_distance_by_unpopulated_count)
{
	if(radius_column_index < columnData.size())
	{
		auto &partial_sums = parametersAndBuffers.partialSums;
		auto &radius_column_data = columnData[radius_column_index];
		for(auto &number_value_entry : radius_column_data->sortedNumberValueEntries)
		{
			//transform the radius to a negative value with an inverse exponent
			//note that this will correctly order the cases by distance (monotonic),
			// but will yield incorrect distance values with the radius, so the distances will need to be recomputed
			double value = -r_dist_eval.distEvaluator->ExponentiateDifferenceTerm<compute_surprisal>(number_value_entry.first, high_accuracy);
			for(auto entity_index : number_value_entry.second.indicesWithValue)
				partial_sums.SetSum(entity_index, value);
		}
	}

	size_t num_enabled_features = r_dist_eval.featureData.size();
	size_t num_entities_to_populate = top_k;
	//populate sqrt(2)^p * top_k, which will yield 2 for p=2, 1 for p=0, and about 1.2 for p=0.5
	if(num_enabled_features > 1)
		num_entities_to_populate = static_cast<size_t>(std::lround(FastPow(GeneralizedDistanceEvaluator::s_sqrt_2, r_dist_eval.distEvaluator->pValue) * top_k)) + 1;

	min_unpopulated_distances.resize(num_enabled_features);
	for(size_t i = 0; i < num_enabled_features; i++)
	{
		double next_closest_distance = PopulatePartialSumsWithSimilarFeatureValue<compute_surprisal>(r_dist_eval,
			num_entities_to_populate,
			//expand search if using more than one dimension
			num_enabled_features > 1, high_accuracy,
			i, enabled_indices);

		//if value isn't null, may need to populate non-null values
		if(!r_dist_eval.featureData[i].targetValue.IsNull())
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[i];
			//if the value is not a null, need to accumulate null distance terms if it's a symmetric nominal feature,
			// because then there's only one value left, or if the nulls are closer than what has already been considered
			if(feature_attribs.IsFeatureSymmetricNominal()
				|| feature_attribs.knownToUnknownDistanceTerm.deviation <= next_closest_distance)
			{
				double known_unknown_term = r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(i);
				AccumulatePartialSums(enabled_indices, columnData[feature_attribs.featureIndex]->nullIndices, i, known_unknown_term);
			}
		}

		min_unpopulated_distances[i] = next_closest_distance;
	}
	std::sort(begin(min_unpopulated_distances), end(min_unpopulated_distances));

	//compute min distance based on the number of features that are unpopulated
	min_distance_by_unpopulated_count.clear();
	//need to add a 0 for when all distances are computed
	min_distance_by_unpopulated_count.push_back(0.0);
	//append all of the sorted distances so they can be accumulated and assigned
	min_distance_by_unpopulated_count.insert(end(min_distance_by_unpopulated_count), begin(min_unpopulated_distances), end(min_unpopulated_distances));
	for(size_t i = 1; i < min_distance_by_unpopulated_count.size(); i++)
		min_distance_by_unpopulated_count[i] += min_distance_by_unpopulated_count[i - 1];
}

template void SeparableBoxFilterDataStore::PopulateInitialPartialSums<true>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t top_k, size_t radius_column_index, bool high_accuracy,
	BitArrayIntegerSet &enabled_indices, std::vector<double> &min_unpopulated_distances, std::vector<double> &min_distance_by_unpopulated_count);

template void SeparableBoxFilterDataStore::PopulateInitialPartialSums<false>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t top_k, size_t radius_column_index, bool high_accuracy,
	BitArrayIntegerSet &enabled_indices, std::vector<double> &min_unpopulated_distances, std::vector<double> &min_distance_by_unpopulated_count);

void SeparableBoxFilterDataStore::PopulatePotentialGoodMatches(FlexiblePriorityQueue<CountDistanceReferencePair<size_t>> &potential_good_matches,
	BitArrayIntegerSet &enabled_indices, PartialSumCollection &partial_sums, size_t top_k)
{
	potential_good_matches.clear();
	potential_good_matches.Reserve(top_k);

	//first, build up top_k that have at least one feature
	size_t entity_index = 0;
	size_t indices_considered = 0;
	size_t end_index = enabled_indices.GetEndInteger();
	for(; entity_index < end_index; entity_index++)
	{
		//don't need to check maximum index, because already checked in loop
		if(!enabled_indices.ContainsWithoutMaximumIndexCheck(entity_index))
			continue;

		indices_considered++;

		auto [num_calculated_feature_deltas, cur_sum] = partial_sums.GetNumFilledAndSum(entity_index);
		if(num_calculated_feature_deltas == 0)
			continue;

		potential_good_matches.emplace(num_calculated_feature_deltas, cur_sum, entity_index);
		if(potential_good_matches.size() == top_k)
		{
			entity_index++;
			break;
		}
	}

	//heuristically attempt to find some cases with the most number of features calculated (by the closest matches) and the lowest distances
	//iterate until at least index_end / e cases are seen, but cap at a maximum number
	size_t total_indices = enabled_indices.size();
	size_t num_indices_to_consider = static_cast<size_t>(std::floor(total_indices * 0.3678794411714));
	num_indices_to_consider = std::min(static_cast<size_t>(1000), num_indices_to_consider);

	//find a good number of features based on the discrete logarithm of the number of features
	size_t good_number_of_features = 0;
	size_t num_features = partial_sums.numTerms;
	while(num_features >>= 1)
		good_number_of_features++;

	//start with requiring at least one feature matching to be considered a good match
	size_t good_match_threshold_count = 1;
	double good_match_threshold_value = std::numeric_limits<double>::infinity();
	if(potential_good_matches.size() > 0)
	{
		const auto &top = potential_good_matches.top();
		good_match_threshold_count = top.count;
		good_match_threshold_value = top.distance;
	}

	//continue on starting at the next unexamined index until have seen at least max_considerable_good_index
	// or k filled with entities having good_number_of_features calculated
	for(; indices_considered < num_indices_to_consider && entity_index < end_index; entity_index++)
	{
		//don't need to check maximum index, because already checked in loop
		if(!enabled_indices.ContainsWithoutMaximumIndexCheck(entity_index))
			continue;

		indices_considered++;

		auto [num_calculated_feature_deltas, cur_sum] = partial_sums.GetNumFilledAndSum(entity_index);
		//skip if not good enough
		if(num_calculated_feature_deltas < good_match_threshold_count)
			continue;

		//either needs to exceed the calculated features or have smaller distance
		if(num_calculated_feature_deltas > good_match_threshold_count
			|| cur_sum < good_match_threshold_value)
		{
			//have top_k, but this one is better
			potential_good_matches.emplace(num_calculated_feature_deltas, cur_sum, entity_index);
			potential_good_matches.pop();

			const auto &top = potential_good_matches.top();
			good_match_threshold_count = top.count;
			good_match_threshold_value = top.distance;

			//if have found enough features, stop searching
			if(good_match_threshold_count >= good_number_of_features)
				break;
		}
	}
}

template<bool compute_surprisal>
void SeparableBoxFilterDataStore::PopulateTargetValueAndLabelIndex(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t query_feature_index, EvaluableNodeImmediateValue position_value,
	EvaluableNodeImmediateValueType position_value_type)
{
	auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
	auto &feature_type = feature_attribs.featureType;
	auto &feature_data = r_dist_eval.featureData[query_feature_index];
	auto &effective_feature_type = r_dist_eval.featureData[query_feature_index].effectiveFeatureType;
	auto &column_data = columnData[feature_attribs.featureIndex];

	feature_data.Clear();
	feature_data.targetValue = EvaluableNodeImmediateValueWithType(position_value, position_value_type);

	if(feature_attribs.IsFeatureNominal())
		r_dist_eval.ComputeAndStoreNominalDistanceTerms<compute_surprisal>(query_feature_index);

	bool complex_comparison = (feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE
		|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING
		|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE_NO_RECURSIVE_MATCHING
		|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE);

	//consider computing interned values if appropriate
	//however, symmetric nominals are fast, so don't compute interned values for them
	if(!feature_attribs.IsFeatureSymmetricNominal() && !complex_comparison)
	{
		if(position_value_type == ENIVT_NUMBER && column_data->internedNumberValues.valueInterningEnabled)
		{
			size_t num_values_stored_as_numbers = column_data->numberIndices.size() + column_data->invalidIndices.size() + column_data->nullIndices.size();

			if(GetNumInsertedEntities() == num_values_stored_as_numbers)
				effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_UNIVERSALLY_INTERNED_PRECOMPUTED;
			else
				effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_NUMERIC_INTERNED_PRECOMPUTED;

			r_dist_eval.ComputeAndStoreInternedDistanceTerms<compute_surprisal>(
				query_feature_index, column_data->internedNumberValues.internedIndexToValue);
			return;
		}
		else if(position_value_type == ENIVT_STRING_ID && column_data->internedStringIdValues.valueInterningEnabled)
		{
			size_t num_values_stored_as_string_ids = column_data->stringIdIndices.size() + column_data->invalidIndices.size() + column_data->nullIndices.size();

			if(GetNumInsertedEntities() == num_values_stored_as_string_ids)
				effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_UNIVERSALLY_INTERNED_PRECOMPUTED;
			else
				effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_STRING_INTERNED_PRECOMPUTED;

			r_dist_eval.ComputeAndStoreInternedDistanceTerms<compute_surprisal>(
				query_feature_index, column_data->internedStringIdValues.internedIndexToValue);
			return;
		}
		else if(position_value_type == ENIVT_BOOL)
		{
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_BOOL_PRECOMPUTED;

			r_dist_eval.ComputeAndStoreInternedDistanceTermsForBool<compute_surprisal>(query_feature_index);
			return;
		}
	}

	if(feature_attribs.IsFeatureNominal() || complex_comparison)
	{
		if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_BOOL)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_BOOL;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_NUMERIC;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_STRING;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_CODE;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_STRING;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE_NO_RECURSIVE_MATCHING)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_CODE_NO_RECURSIVE_MATCHING;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_CODE;
	}
	else // feature_type is some form of continuous numeric
	{
		size_t num_values_stored_as_numbers = column_data->numberIndices.size() + column_data->invalidIndices.size();
		if(GetNumInsertedEntities() == num_values_stored_as_numbers
				&& feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER
				&& !column_data->internedNumberValues.valueInterningEnabled)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_UNIVERSALLY_NUMERIC;
		else if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER_CYCLIC)
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_NUMERIC_CYCLIC;
		else
			effective_feature_type = RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_NUMERIC;
	}
}

template void SeparableBoxFilterDataStore::PopulateTargetValueAndLabelIndex<true>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t query_feature_index, EvaluableNodeImmediateValue position_value,
	EvaluableNodeImmediateValueType position_value_type);

template void SeparableBoxFilterDataStore::PopulateTargetValueAndLabelIndex<false>(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
	size_t query_feature_index, EvaluableNodeImmediateValue position_value,
	EvaluableNodeImmediateValueType position_value_type);
