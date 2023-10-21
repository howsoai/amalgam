//project headers:
#include "Entity.h"
#include "SeparableBoxFilterDataStore.h"

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
SeparableBoxFilterDataStore::SBFDSParametersAndBuffers SeparableBoxFilterDataStore::parametersAndBuffers;

void SeparableBoxFilterDataStore::BuildLabel(size_t column_index, const std::vector<Entity *> &entities)
{
	auto &column_data = columnData[column_index];
	auto label_id = column_data->stringId;

	auto &entities_with_number_values = parametersAndBuffers.entitiesWithValues;
	entities_with_number_values.clear();

	//populate matrix and get values
	// maintaining the order of insertion of the entities from smallest to largest allows for better performance of the insertions
	// and every function called here assumes that entities are inserted in increasing order
	for(size_t entity_index = 0; entity_index < entities.size(); entity_index++)
	{
		EvaluableNodeImmediateValueType value_type;
		EvaluableNodeImmediateValue value;
		value_type = entities[entity_index]->GetValueAtLabelAsImmediateValue(label_id, value);
		GetValue(entity_index, column_index) = value;

		column_data->InsertNextIndexValueExceptNumbers(value_type, value, entity_index, entities_with_number_values);
	}

	//sort the number values for efficient insertion, but keep the entities in their order
	std::stable_sort(begin(entities_with_number_values), end(entities_with_number_values));

	column_data->AppendSortedNumberIndicesWithSortedIndices(entities_with_number_values);

	OptimizeColumn(column_index);
}

void SeparableBoxFilterDataStore::OptimizeColumn(size_t column_index)
{
	auto &column_data = columnData[column_index];

	if(column_data->numberValuesInterned)
	{
		if(column_data->AreNumberValuesPreferredToInterns())
		{
			for(auto &value_entry : column_data->sortedNumberValueEntries)
			{
				double value = value_entry->value.number;
				for(auto entity_index : value_entry->indicesWithValue)
					GetValue(entity_index, column_index).number = value;
			}

			for(auto entity_index : column_data->nanIndices)
				GetValue(entity_index, column_index).number = std::numeric_limits<double>::quiet_NaN();

			for(auto entity_index : column_data->nullIndices)
				GetValue(entity_index, column_index).number = std::numeric_limits<double>::quiet_NaN();

			column_data->ConvertNumberInternsToValues();
		}
	}
	else if(column_data->AreNumberInternsPreferredToValues())
	{
		column_data->ConvertNumberValuesToInterns();

		for(auto &value_entry : column_data->sortedNumberValueEntries)
		{
			size_t value_index = value_entry->valueInternIndex;
			for(auto entity_index : value_entry->indicesWithValue)
				GetValue(entity_index, column_index).indirectionIndex = value_index;
		}

		for(auto entity_index : column_data->nanIndices)
			GetValue(entity_index, column_index).number = SBFDSColumnData::ValueEntry::NAN_INDEX;

		for(auto entity_index : column_data->nullIndices)
			GetValue(entity_index, column_index).number = SBFDSColumnData::ValueEntry::NAN_INDEX;
	}
}

void SeparableBoxFilterDataStore::RemoveColumnIndex(size_t column_index_to_remove)
{
	//will replace the values at index_to_remove with the values at index_to_move
	size_t column_index_to_move = columnData.size() - 1;

	size_t label_id = columnData[column_index_to_remove]->stringId;

	size_t num_columns = columnData.size();

	//move data from the last column to the removed column if removing the label_id isn't the last column
	if(column_index_to_remove != column_index_to_move)
	{
		for(size_t i = 0; i < numEntities; i++)
			matrix[i * num_columns + column_index_to_remove] = matrix[i * num_columns + column_index_to_move];

		//update column lookup
		size_t label_id_to_move = columnData[column_index_to_move]->stringId;
		labelIdToColumnIndex[label_id_to_move] = column_index_to_remove;

		//rearrange columns
		std::swap(columnData[column_index_to_remove], columnData[column_index_to_move]);
	}

	//remove the columnId lookup, reference, and column
	labelIdToColumnIndex.erase(label_id);
	columnData.pop_back();

	//create new smaller container to hold the reduced data
	std::vector<EvaluableNodeImmediateValue> old_matrix;
	std::swap(old_matrix, matrix);

	//if no columns left, then done
	if(columnData.size() == 0)
		return;

	//move data over to new reduced copy of matrix
	matrix.resize(columnData.size() * numEntities);
	for(size_t i = 0; i < numEntities; i++)
		memcpy((char *)&matrix[i * columnData.size()], (char *)&old_matrix[i * (columnData.size() + 1)], sizeof(EvaluableNodeImmediateValue) * (columnData.size()));
}

void SeparableBoxFilterDataStore::AddEntity(Entity *entity, size_t entity_index)
{
	size_t starting_cell_index = GetMatrixCellIndex(entity_index);

	//fill with missing values, including any empty indices
	matrix.resize(starting_cell_index + columnData.size());

	//fill in matrix cells from entity
	size_t cell_index = starting_cell_index;
	for(size_t column_index = 0; column_index < columnData.size(); column_index++, cell_index++)
	{
		EvaluableNodeImmediateValueType value_type;
		EvaluableNodeImmediateValue value;
		value_type = entity->GetValueAtLabelAsImmediateValue(columnData[column_index]->stringId, value);
		matrix[cell_index] = columnData[column_index]->InsertIndexValue(value_type, value, entity_index);
	}

	//count this entity
	if(entity_index >= numEntities)
		numEntities = entity_index + 1;

	OptimizeAllColumns();
}

void SeparableBoxFilterDataStore::RemoveEntity(Entity *entity, size_t entity_index, size_t entity_index_to_reassign)
{
	if(entity_index >= numEntities || columnData.size() == 0)
		return;

	//if was the last entity and reassigning the last one or one out of bounds,
	// simply delete from column data, delete last row, and return
	if(entity_index + 1 == GetNumInsertedEntities() && entity_index_to_reassign >= entity_index)
	{
		DeleteEntityIndexFromColumns(entity_index);
		DeleteLastRow();
		return;
	}

	//make sure it's a valid rassignment
	if(entity_index_to_reassign >= numEntities)
		return;

	//if deleting a row and not replacing it, just fill as if it has no data
	if(entity_index == entity_index_to_reassign)
	{
		DeleteEntityIndexFromColumns(entity_index);

		//fill with missing values
		size_t starting_cell_index = GetMatrixCellIndex(entity_index);
		for(size_t column_index = 0; column_index < columnData.size(); column_index++)
			matrix[starting_cell_index + column_index].number = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	//reassign index for each column
	for(size_t column_index = 0; column_index < columnData.size(); column_index++)
	{
		auto &column_data = columnData[column_index];

		auto &val_to_overwrite = GetValue(entity_index, column_index);
		auto type_to_overwrite = column_data->GetIndexValueType(entity_index);

		auto &value_to_reassign = GetValue(entity_index_to_reassign, column_index);
		auto value_type_to_reassign = columnData[column_index]->GetIndexValueType(entity_index_to_reassign);

		//remove the value where it is
		columnData[column_index]->DeleteIndexValue(value_type_to_reassign, value_to_reassign, entity_index_to_reassign);

		//change the destination to the value
		columnData[column_index]->ChangeIndexValue(type_to_overwrite, val_to_overwrite, value_type_to_reassign, value_to_reassign, entity_index);
	}

	//copy data from entity_index_to_reassign to entity_index
	memcpy((char *)&(matrix[entity_index * columnData.size()]), (char *)&(matrix[entity_index_to_reassign * columnData.size()]), sizeof(EvaluableNodeImmediateValue) * columnData.size());

	//truncate matrix cache if removing the last entry, either by moving the last entity or by directly removing the last
	if(entity_index_to_reassign + 1 == numEntities
			|| (entity_index_to_reassign + 1 >= numEntities && entity_index + 1 == numEntities))
		DeleteLastRow();

	//clean up any labels that aren't relevant
	RemoveAnyUnusedLabels();

	OptimizeAllColumns();
}

void SeparableBoxFilterDataStore::UpdateAllEntityLabels(Entity *entity, size_t entity_index)
{
	if(entity_index >= numEntities)
		return;

	size_t matrix_index = GetMatrixCellIndex(entity_index);
	for(size_t column_index = 0; column_index < columnData.size(); column_index++)
	{
		auto &column_data = columnData[column_index];

		EvaluableNodeImmediateValueType value_type;
		EvaluableNodeImmediateValue value;
		value_type = entity->GetValueAtLabelAsImmediateValue(columnData[column_index]->stringId, value);

		//update the value
		auto &matrix_value = matrix[matrix_index];
		auto previous_value_type = column_data->GetIndexValueType(entity_index);
		column_data->ChangeIndexValue(previous_value_type, matrix_value, value_type, value, entity_index);
		matrix_value = value;

		matrix_index++;
	}

	//clean up any labels that aren't relevant
	RemoveAnyUnusedLabels();

	OptimizeAllColumns();
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
	auto &column_data = columnData[column_index];

	//get the new value
	EvaluableNodeImmediateValueType value_type;
	EvaluableNodeImmediateValue value;
	value_type = entity->GetValueAtLabelAsImmediateValue(column_data->stringId, value);

	//update the value
	auto &matrix_value = GetValue(entity_index, column_index);
	auto previous_value_type = column_data->GetIndexValueType(entity_index);
	column_data->ChangeIndexValue(previous_value_type, matrix_value, value_type, value, entity_index);
	matrix_value = value;

	//remove the label if no longer relevant
	if(IsColumnIndexRemovable(column_index))
		RemoveColumnIndex(column_index);

	OptimizeColumn(column_index);
}

//populates distances_out with all entities and their distances that have a distance to target less than max_dist
// and sets distances_out to the found entities.  Infinity is allowed to compute all distances.
//if enabled_indices is not nullptr, it will only find distances to those entities, and it will modify enabled_indices in-place
// removing entities that do not have the corresponding labels
void SeparableBoxFilterDataStore::FindEntitiesWithinDistance(GeneralizedDistance &dist_params, std::vector<size_t> &position_label_ids,
	std::vector<EvaluableNodeImmediateValue> &position_values, std::vector<EvaluableNodeImmediateValueType> &position_value_types, double max_dist,
	BitArrayIntegerSet &enabled_indices, std::vector<DistanceReferencePair<size_t>> &distances_out)
{
	if(GetNumInsertedEntities() == 0)
		return;

	//look up these data structures upfront for performance
	auto &target_column_indices = parametersAndBuffers.targetColumnIndices;
	auto &target_values = parametersAndBuffers.targetValues;
	auto &target_value_types = parametersAndBuffers.targetValueTypes;
	PopulateTargetValuesAndLabelIndices(dist_params, position_label_ids, position_values, position_value_types);
	if(target_values.size() == 0)
		return;

	PopulateUnknownFeatureValueTerms(dist_params);

	//Starting with all entities, narrow down the list by incrementally summing up the minkowski distances
	const double max_dist_exponentiated = std::pow(max_dist, dist_params.pValue); //max_dist ^ p >= MinkowskiDistanceSum

	//initialize all distances to 0
	auto &distances = parametersAndBuffers.entityDistances;
	distances.clear();
	distances.resize(GetNumInsertedEntities(), 0.0);

	//remove any entities that are missing labels
	for(auto absolute_feature_index : target_column_indices)
		columnData[absolute_feature_index]->invalidIndices.EraseInBatchFrom(enabled_indices);
	enabled_indices.UpdateNumElements();

	//for each desired feature, compute and add distance terms of possible window query candidate entities
	for(size_t query_feature_index = 0; query_feature_index < target_column_indices.size(); query_feature_index++)
	{
		size_t absolute_feature_index = target_column_indices[query_feature_index];
		auto target_value = target_values[query_feature_index];
		auto target_value_type = target_value_types[query_feature_index];

		auto &column_data = columnData[absolute_feature_index];

		if(target_value_type == ENIVT_NULL || (target_value_type == ENIVT_NUMBER && FastIsNaN(target_value.number)) )
		{
			//add the appropriate unknown distance to each element
			double unknown_unknown_term = dist_params.ComputeDistanceTermUnknownToUnknown(query_feature_index);
			double known_unknown_term = dist_params.ComputeDistanceTermKnownToUnknown(query_feature_index);

			auto &null_indices = column_data->nullIndices;
			auto &nan_indices = column_data->nanIndices;
			for(auto entity_index : enabled_indices)
			{
				if(null_indices.contains(entity_index) || nan_indices.contains(entity_index))
					distances[entity_index] += unknown_unknown_term;
				else
					distances[entity_index] += known_unknown_term;

				//remove entity if its distance is already greater than the max_dist
				if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
					enabled_indices.erase(entity_index);
			}

			continue;
		}

		if(target_value_type == ENIVT_NUMBER)
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
					double distance_term = dist_params.ComputeDistanceTermRegularOneNonNull(target_value.number - value_entry->value.number, query_feature_index);

					//for each bucket, add term to their sums
					for(auto entity_index : value_entry->indicesWithValue)
					{
						if(!enabled_indices.contains(entity_index))
							continue;

						distances[entity_index] += distance_term;

						//remove entity if its distance is already greater than the max_dist, won't ever become NaN here (would already have been removed from indices)
						if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
							enabled_indices.erase(entity_index);
					}
				}

				//populate all non-number distances
				double unknown_dist = dist_params.ComputeDistanceTermKnownToUnknown(query_feature_index);
				for(auto entity_index : enabled_indices)
				{
					//skip over number values
					if(column_data->numberIndices.contains(entity_index))
						continue;

					distances[entity_index] += unknown_dist;

					//remove entity if its distance is already greater than the max_dist
					if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
						enabled_indices.erase(entity_index);
				}

				continue;
			}
		}
		
		//if target_value_type == ENIVT_CODE or ENIVT_STRING_ID, just compute all
		// won't save much for code until cache equal values
		// won't save much for string ids because it's just a lookup (though could make it a little faster by streamlining a specialized string loop)
		
		//else, there are less indices to consider than possible unique values, so save computation by just considering entities that are still valid
		for(auto entity_index : enabled_indices)
		{
			auto value_type = column_data->GetIndexValueType(entity_index);
			auto value = column_data->GetResolvedValue(value_type, GetValue(entity_index, absolute_feature_index));
			value_type = column_data->GetResolvedValueType(value_type);
			
			distances[entity_index] += dist_params.ComputeDistanceTermRegular(target_value, value, target_value_type, value_type, query_feature_index);

			//remove entity if its distance is already greater than the max_dist
			if(!(distances[entity_index] <= max_dist_exponentiated)) //false for NaN indices as well so they will be removed
				enabled_indices.erase(entity_index);
		}
	}

	//populate distances_out vector
	distances_out.reserve(enabled_indices.size());
	bool need_recompute_distances = (dist_params.recomputeAccurateDistances && !dist_params.highAccuracy);
	if(!need_recompute_distances)
	{
		for(auto index : enabled_indices)
			distances_out.emplace_back(dist_params.InverseExponentiateDistance(distances[index]), index);
	}
	else
	{
		dist_params.SetHighAccuracy(true);
		for(auto index : enabled_indices)
			distances_out.emplace_back(GetDistanceBetween(dist_params, target_values, target_value_types, target_column_indices, index), index);
	}
}

void SeparableBoxFilterDataStore::FindEntitiesNearestToIndexedEntity(GeneralizedDistance *dist_params_ref, std::vector<size_t> &position_label_ids,
	bool constant_dist_params, size_t search_index, size_t top_k, BitArrayIntegerSet &enabled_indices,
	bool expand_to_first_nonzero_distance, std::vector<DistanceReferencePair<size_t>> &distances_out, size_t ignore_index, RandomStream rand_stream)
{
	if(top_k == 0 || GetNumInsertedEntities() == 0)
		return;

	GeneralizedDistance *dist_params = dist_params_ref;
	if(constant_dist_params)
	{
		dist_params = &parametersAndBuffers.distParams;
		*dist_params = *dist_params_ref;
	}
		
	//build target
	auto &target_column_indices = parametersAndBuffers.targetColumnIndices;
	target_column_indices.clear();

	auto &target_values = parametersAndBuffers.targetValues;
	target_values.clear();

	auto &target_value_types = parametersAndBuffers.targetValueTypes;
	target_value_types.clear();

	const size_t matrix_index_base = search_index * columnData.size();
	for(size_t i = 0; i < position_label_ids.size(); i++)
	{
		auto found = labelIdToColumnIndex.find(position_label_ids[i]);
		if(found == end(labelIdToColumnIndex))
			continue;

		if(dist_params->IsFeatureEnabled(i))
		{
			size_t column_index = found->second;
			auto &column_data = columnData[column_index];

			auto value_type = column_data->GetIndexValueType(search_index);
			//overwrite value in case of value interning
			auto value = column_data->GetResolvedValue(value_type, matrix[matrix_index_base + column_index]);
			value_type = column_data->GetResolvedValueType(value_type);

			PopulateNextTargetAttributes(*dist_params, i,
				target_column_indices, target_values, target_value_types,
				column_index, value, value_type);
		}
	}

	PopulateUnknownFeatureValueTerms(*dist_params);

	size_t num_enabled_features = target_values.size();

	//make a copy of the entities so that the list can be modified
	BitArrayIntegerSet &possible_knn_indices = parametersAndBuffers.potentialMatchesSet;
	possible_knn_indices = enabled_indices;

	//remove search_index and ignore_index
	possible_knn_indices.erase(search_index);
	possible_knn_indices.erase(ignore_index);

	//remove invalid cases
	for(size_t absolute_feature_index : target_column_indices)
		columnData[absolute_feature_index]->invalidIndices.EraseInBatchFrom(possible_knn_indices);
	possible_knn_indices.UpdateNumElements();

	//if num enabled indices < top_k, return sorted distances
	if(GetNumInsertedEntities() <= top_k || possible_knn_indices.size() <= top_k)
		return FindAllValidElementDistances(*dist_params, target_column_indices, target_values, target_value_types, possible_knn_indices, distances_out, rand_stream);
	
	size_t end_index = possible_knn_indices.GetEndInteger();

	//reuse the appropriate partial_sums_buffer buffer
	auto &partial_sums = parametersAndBuffers.partialSums;
	partial_sums.ResizeAndClear(num_enabled_features, end_index);

	//calculate the partial sums for the cases that best match for each feature
	// and populate the vectors of smallest possible distances that haven't been computed yet
	auto &min_unpopulated_distances = parametersAndBuffers.minUnpopulatedDistances;
	auto &min_distance_by_unpopulated_count = parametersAndBuffers.minDistanceByUnpopulatedCount;
	PopulateInitialPartialSums(*dist_params, top_k, num_enabled_features, possible_knn_indices, min_unpopulated_distances, min_distance_by_unpopulated_count);
	
	auto &potential_good_matches = parametersAndBuffers.potentialGoodMatches;
	PopulatePotentialGoodMatches(potential_good_matches, possible_knn_indices, partial_sums, top_k);

	//reuse, clear, and set up sorted_results
	auto &sorted_results = parametersAndBuffers.sortedResults;
	sorted_results.clear();
	sorted_results.SetStream(rand_stream);
	sorted_results.Reserve(top_k);

	//parse the sparse inline hash of good match nodes directly into the compacted vector of good matches
	while(potential_good_matches.size() > 0)
	{
		size_t entity_index = potential_good_matches.top().reference;

		//insert random selection into results heap
		double distance = ResolveDistanceToNonMatchTargetValues(*dist_params,
			target_column_indices, target_values, target_value_types, partial_sums, entity_index, num_enabled_features);
		sorted_results.Push(DistanceReferencePair(distance, entity_index));

		//skip this entity in the next loops
		possible_knn_indices.erase(entity_index);
		
		potential_good_matches.pop();
	}

	//if we did not find K results (search failed), we must populate the remaining K cases/results to search from another way
	//we will randomly select additional nodes to fill K results.  random to prevent bias/patterns
	while(sorted_results.Size() < top_k && possible_knn_indices.size() > 0)
	{
		//get a random index that is still potentially in the knn (neither rejected nor already in the results)
		size_t random_index = possible_knn_indices.GetRandomElement(rand_stream);

		double distance = ResolveDistanceToNonMatchTargetValues(*dist_params,
			target_column_indices, target_values, target_value_types, partial_sums, random_index, num_enabled_features);
		sorted_results.Push(DistanceReferencePair(distance, random_index));

		//skip this entity in the next loops
		possible_knn_indices.erase(random_index);
	}

	//cache kth smallest distance to target search node
	double worst_candidate_distance = std::numeric_limits<double>::infinity();
	if(sorted_results.Size() == top_k)
	{
		double top_distance = sorted_results.Top().distance;
		//don't clamp top distance if we're expanding and only have 0 distances
		if(! (expand_to_first_nonzero_distance && top_distance == 0.0) )
			worst_candidate_distance = top_distance;
	}

	//execute window query, with dynamically shrinking bounds
	for(const size_t entity_index : possible_knn_indices)
	{
		//if still accepting new candidates because found only zero distances
		if(worst_candidate_distance == std::numeric_limits<double>::infinity())
		{
			double distance = ResolveDistanceToNonMatchTargetValues(*dist_params,
				target_column_indices, target_values, target_value_types, partial_sums, entity_index, num_enabled_features);
			sorted_results.Push(DistanceReferencePair(distance, entity_index));

			//if full, update worst_candidate_distance
			if(sorted_results.Size() >= top_k)
			{
				double top_distance = sorted_results.Top().distance;
				//don't clamp top distance if we're expanding and only have 0 distances
				if(!(expand_to_first_nonzero_distance && top_distance == 0.0))
					worst_candidate_distance = top_distance;
			}

			continue;
		}

		//already have enough elements, but see if this one is good enough
		auto [accept, distance] = ResolveDistanceToNonMatchTargetValues(*dist_params,
			target_column_indices, target_values, target_value_types, partial_sums,
			entity_index, min_distance_by_unpopulated_count, num_enabled_features, worst_candidate_distance, min_unpopulated_distances);

		if(!accept)
			continue;

		//if not expanding and pushing a zero distance onto the stack, then push and pop a value onto the stack
		if(!(expand_to_first_nonzero_distance && distance == 0.0))
			worst_candidate_distance = sorted_results.PushAndPop(DistanceReferencePair(distance, entity_index)).distance;
		else //adding a zero and need to expand beyond zeros
		{
			//add the zero
			sorted_results.Push(DistanceReferencePair(distance, entity_index));

			//make copy of the top and pop it
			DistanceReferencePair drp = sorted_results.Top();
			sorted_results.Pop();

			//if the next largest size is zero, then need to put the non-zero value back in sorted_results
			if(sorted_results.Size() > 0 && sorted_results.Top().distance == 0)
				sorted_results.Push(drp);
		}
	}

	//return k nearest -- don't need to clear because the values will be clobbered
	distances_out.resize(sorted_results.Size());
	bool need_recompute_distances = (dist_params->recomputeAccurateDistances && !dist_params->highAccuracy);
	if(need_recompute_distances)
		dist_params->SetHighAccuracy(true);

	while(sorted_results.Size() > 0)
	{
		auto &drp = sorted_results.Top();
		double distance;
		if(!need_recompute_distances)
			distance = dist_params->InverseExponentiateDistance(drp.distance);
		else
			distance = GetDistanceBetween(*dist_params, target_values, target_value_types, target_column_indices, drp.reference);

		distances_out[sorted_results.Size() - 1] = DistanceReferencePair(distance, drp.reference);
		sorted_results.Pop();
	}
}

void SeparableBoxFilterDataStore::FindNearestEntities(GeneralizedDistance &dist_params, std::vector<size_t> &position_label_ids,
	std::vector<EvaluableNodeImmediateValue> &position_values, std::vector<EvaluableNodeImmediateValueType> &position_value_types, size_t top_k,
	size_t ignore_entity_index, BitArrayIntegerSet &enabled_indices, std::vector<DistanceReferencePair<size_t>> &distances_out, RandomStream rand_stream)
{
	if(top_k == 0 || GetNumInsertedEntities() == 0)
		return;

	//look up these data structures upfront for performance
	auto &target_column_indices = parametersAndBuffers.targetColumnIndices;
	auto &target_values = parametersAndBuffers.targetValues;
	auto &target_value_types = parametersAndBuffers.targetValueTypes;
	PopulateTargetValuesAndLabelIndices(dist_params, position_label_ids, position_values, position_value_types);

	size_t num_enabled_features = target_values.size();
	if(num_enabled_features == 0)
		return;

	PopulateUnknownFeatureValueTerms(dist_params);

	//ignore cases with missing labels
	for(size_t i = 0; i < num_enabled_features; i++)
		columnData[target_column_indices[i]]->invalidIndices.EraseInBatchFrom(enabled_indices);
	enabled_indices.UpdateNumElements();

	enabled_indices.erase(ignore_entity_index);

	//if num enabled indices < top_k, return sorted distances
	if(enabled_indices.size() <= top_k)
		return FindAllValidElementDistances(dist_params, target_column_indices, target_values, target_value_types, enabled_indices, distances_out, rand_stream);

	//one past the maximum entity index to be considered
	size_t end_index = enabled_indices.GetEndInteger();

	//reuse the appropriate partial_sums_buffer buffer
	auto &partial_sums = parametersAndBuffers.partialSums;
	partial_sums.ResizeAndClear(num_enabled_features, end_index);

	//calculate the partial sums for the cases that best match for each feature
	// and populate the vectors of smallest possible distances that haven't been computed yet
	auto &min_unpopulated_distances = parametersAndBuffers.minUnpopulatedDistances;
	auto &min_distance_by_unpopulated_count = parametersAndBuffers.minDistanceByUnpopulatedCount;
	PopulateInitialPartialSums(dist_params, top_k, num_enabled_features, enabled_indices, min_unpopulated_distances, min_distance_by_unpopulated_count);

	auto &potential_good_matches = parametersAndBuffers.potentialGoodMatches;
	PopulatePotentialGoodMatches(potential_good_matches, enabled_indices, partial_sums, top_k);

	//reuse, clear, and set up sorted_results
	auto &sorted_results = parametersAndBuffers.sortedResults;
	sorted_results.clear();
	sorted_results.SetStream(rand_stream.CreateOtherStreamViaRand());
	sorted_results.Reserve(top_k);

	//parse the sparse inline hash of good match nodes directly into the compacted vector of good matches
	while(potential_good_matches.size() > 0)
	{
		size_t good_match_index = potential_good_matches.top().reference;
		potential_good_matches.pop();

		//skip this entity in the next loops
		enabled_indices.erase(good_match_index);

		double distance = ResolveDistanceToNonMatchTargetValues(dist_params,\
			target_column_indices, target_values, target_value_types, partial_sums, good_match_index, num_enabled_features);
		sorted_results.Push(DistanceReferencePair(distance, good_match_index));
	}

	//if we did not find top_k results (search failed), attempt to randomly fill the top k with random results
	// to remove biases that might slow down performance
	while(sorted_results.Size() < top_k)
	{
		//find a random case index
		size_t random_index = enabled_indices.GetRandomElement(rand_stream);

		//skip this entity in the next loops
		enabled_indices.erase(random_index);
				
		double distance = ResolveDistanceToNonMatchTargetValues(dist_params,
			target_column_indices, target_values, target_value_types, partial_sums, random_index, num_enabled_features);
		sorted_results.Push(DistanceReferencePair(distance, random_index));
	}

	auto &previous_nn_cache = parametersAndBuffers.previousQueryNearestNeighbors;

	//have already gone through all records looking for top_k, if don't have top_k, then have exhausted search
	if(sorted_results.Size() == top_k)
	{
		double worst_candidate_distance = sorted_results.Top().distance;
		if(num_enabled_features > 1)
		{
			for(size_t entity_index : previous_nn_cache)
			{
				//only get its distance if it is enabled,
				//but erase to skip this entity in the next loop
				if(!enabled_indices.EraseAndRetrieve(entity_index))
					continue;

				auto [accept, distance] = ResolveDistanceToNonMatchTargetValues(dist_params,
					target_column_indices, target_values, target_value_types, partial_sums, entity_index,
					min_distance_by_unpopulated_count, num_enabled_features, worst_candidate_distance, min_unpopulated_distances);

				if(accept)
					worst_candidate_distance = sorted_results.PushAndPop(DistanceReferencePair(distance, entity_index)).distance;
			}
		}

		//check to see if any features can have nulls quickly removed because it would push it past worst_candidate_distance
		bool need_enabled_indices_recount = false;
		for(size_t i = 0; i < num_enabled_features; i++)
		{
			//if the target_value is a null/nan, unknown-unknown differences have already been accounted for
			//since they are partial matches
			if(target_value_types[i] == ENIVT_NULL || (target_value_types[i] == ENIVT_NUMBER && FastIsNaN(target_values[i].number)))
				continue;
			
			if(dist_params.ComputeDistanceTermKnownToUnknown(i) > worst_candidate_distance)
			{
				auto &column = columnData[target_column_indices[i]];
				auto &null_indices = column->nullIndices;
				//make sure there's enough nulls to justify running through all of enabled_indices
				if(null_indices.size() > 20)
				{
					null_indices.EraseInBatchFrom(enabled_indices);
					need_enabled_indices_recount = true;
				}

				auto &nan_indices = column->nanIndices;
				//make sure there's enough nulls to justify running through all of enabled_indices
				if(nan_indices.size() > 20)
				{
					nan_indices.EraseInBatchFrom(enabled_indices);
					need_enabled_indices_recount = true;
				}
			}
		}
		if(need_enabled_indices_recount)
			enabled_indices.UpdateNumElements();

		//if have removed some from the end, reduce the range
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

				auto [accept, distance] = ResolveDistanceToNonMatchTargetValues(dist_params,
					target_column_indices, target_values, target_value_types, partial_sums, entity_index,
					min_distance_by_unpopulated_count, num_enabled_features, worst_candidate_distance, min_unpopulated_distances);

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
						worst_candidate_distance = sorted_results.PushAndPop(DistanceReferencePair<size_t>(distance, entity_index)).distance;

			#ifdef _OPENMP
					}
				}
			#endif
		
			} //for partialSums instances
		}  //#pragma omp parallel

	} // sorted_results.Size() == top_k

	//return and cache k nearest -- don't need to clear because the values will be clobbered
	size_t num_results = sorted_results.Size();
	distances_out.resize(num_results);
	previous_nn_cache.resize(num_results);
	bool need_recompute_distances = (dist_params.recomputeAccurateDistances && !dist_params.highAccuracy);
	if(need_recompute_distances)
		dist_params.SetHighAccuracy(true);

	while(sorted_results.Size() > 0)
	{
		auto &drp = sorted_results.Top();
		double distance;
		if(!need_recompute_distances)
			distance = dist_params.InverseExponentiateDistance(drp.distance);
		else
			distance = GetDistanceBetween(dist_params, target_values, target_value_types, target_column_indices, drp.reference);

		size_t output_index = sorted_results.Size() - 1;
		distances_out[output_index] = DistanceReferencePair(distance, drp.reference);
		previous_nn_cache[output_index] = drp.reference;

		sorted_results.Pop();
	}
}

void SeparableBoxFilterDataStore::DeleteEntityIndexFromColumns(size_t entity_index)
{
	for(size_t i = 0; i < columnData.size(); i++)
	{
		auto &column_data = columnData[i];
		auto &feature_value = GetValue(entity_index, i);
		auto feature_type = column_data->GetIndexValueType(entity_index);
		columnData[i]->DeleteIndexValue(feature_type, feature_value, entity_index);
	}
}

size_t SeparableBoxFilterDataStore::AddLabelsAsEmptyColumns(std::vector<size_t> &label_ids, size_t num_entities)
{
	size_t num_existing_columns = columnData.size();
	size_t num_inserted_columns = 0;

	//create columns for the labels, don't count any that already exist
	for(auto label_id : label_ids)
	{
		auto [_, inserted] = labelIdToColumnIndex.insert(std::make_pair(label_id, columnData.size()));
		if(inserted)
		{
			columnData.emplace_back(std::make_unique<SBFDSColumnData>(label_id));
			num_inserted_columns++;
		}
	}

	//if nothing has been populated, then just create an empty matrix
	if(matrix.size() == 0)
	{
		numEntities = num_entities;
		matrix.resize(columnData.size() * numEntities);
		return num_inserted_columns;
	}

	//expand the matrix to add the empty columns
	std::vector<EvaluableNodeImmediateValue> old_matrix;
	std::swap(old_matrix, matrix); //swap data pointers to free old memory
	matrix.resize(columnData.size() * numEntities);

	size_t num_columns_new = columnData.size();

	//copy over existing data in blocks per entity
	for(size_t i = 0; i < num_entities; i++)
		memcpy((char *)&matrix[i * num_columns_new], (char *)&old_matrix[i * num_existing_columns], sizeof(EvaluableNodeImmediateValue) * num_existing_columns);

	//update the number of entities
	numEntities = num_entities;

	return num_inserted_columns;
}

double SeparableBoxFilterDataStore::PopulatePartialSumsWithSimilarFeatureValue(GeneralizedDistance &dist_params,
	EvaluableNodeImmediateValue value, EvaluableNodeImmediateValueType value_type,
	size_t num_entities_to_populate, bool expand_search_if_optimal,
	size_t query_feature_index, size_t absolute_feature_index, BitArrayIntegerSet &enabled_indices)
{
	auto &column = columnData[absolute_feature_index];
	auto feature_type = dist_params.featureParams[query_feature_index].featureType;

	bool value_is_null = (value_type == ENIVT_NULL || (value_type == ENIVT_NUMBER && FastIsNaN(value.number)));
	//need to accumulate values for nulls if the value is a null
	if(value_is_null)
	{
		double unknown_unknown_term = dist_params.ComputeDistanceTermUnknownToUnknown(query_feature_index);
		AccumulatePartialSums(column->nullIndices, query_feature_index, unknown_unknown_term);
		AccumulatePartialSums(column->nanIndices, query_feature_index, unknown_unknown_term);

		//if nominal, need to compute null matches to keep the inner loops fast
		// if a data set is mostly nulls, it'll be slower, but this is acceptable as a more rare situation
		//if the known-unknown term is less than unknown_unknown (this should be rare if nulls have semantic meaning)
		//then need to populate the rest of the cases
		double known_unknown_term = dist_params.ComputeDistanceTermKnownToUnknown(query_feature_index);
		if(feature_type == FDT_NOMINAL || known_unknown_term < unknown_unknown_term)
		{
			BitArrayIntegerSet &known_unknown_indices = parametersAndBuffers.potentialMatchesSet;
			known_unknown_indices = enabled_indices;
			column->nullIndices.EraseTo(known_unknown_indices);
			column->nanIndices.EraseTo(known_unknown_indices);
			AccumulatePartialSums(known_unknown_indices, query_feature_index, known_unknown_term);
		}

		return known_unknown_term;
	}

	//need to accumulate nulls if they're closer than an exact match
	//but if made it here, then the value itself isn't null
	if(dist_params.IsKnownToUnknownDistanceLessThanOrEqualToExactMatch(query_feature_index))
	{
		double known_unknown_term = dist_params.ComputeDistanceTermKnownToUnknown(query_feature_index);
		AccumulatePartialSums(column->nullIndices, query_feature_index, known_unknown_term);
		AccumulatePartialSums(column->nanIndices, query_feature_index, known_unknown_term);
	}

	//if nominal, only need to compute the exact match
	if(feature_type == FDT_NOMINAL)
	{
		if(value_type == ENIVT_NUMBER)
		{
			auto [value_index, exact_index_found] = column->FindExactIndexForValue(value.number);
			if(exact_index_found)
			{
				double term = dist_params.ComputeDistanceTermNominalExactMatch(query_feature_index);
				AccumulatePartialSums(column->sortedNumberValueEntries[value_index]->indicesWithValue, query_feature_index, term);
			}
		}
		else if(value_type == ENIVT_STRING_ID)
		{
			auto value_found = column->stringIdValueToIndices.find(value.stringID);
			if(value_found != end(column->stringIdValueToIndices))
			{
				double term = dist_params.ComputeDistanceTermNominalExactMatch(query_feature_index);
				AccumulatePartialSums(*(value_found->second), query_feature_index, term);
			}
		}
		else if(value_type == ENIVT_CODE)
		{
			//compute partial sums for all code of matching size
			size_t code_size = 1;
			if(value_type == ENIVT_CODE)
				code_size = EvaluableNode::GetDeepSize(value.code);

			auto value_found = column->valueCodeSizeToIndices.find(code_size);
			if(value_found != end(column->valueCodeSizeToIndices))
			{
				auto &entity_indices = *(value_found->second);
				ComputeAndAccumulatePartialSums(dist_params, value, value_type,
					entity_indices, query_feature_index, absolute_feature_index);
			}
		}
		//else value_type == ENIVT_NULL

		//didn't find the value
		return dist_params.ComputeDistanceTermNominalNonMatch(query_feature_index);
	}
	else if(feature_type == FDT_CONTINUOUS_STRING)
	{
		if(value_type == ENIVT_STRING_ID)
		{
			auto value_found = column->stringIdValueToIndices.find(value.stringID);
			if(value_found != end(column->stringIdValueToIndices))
			{
				double term = dist_params.ComputeDistanceTermNonNominalExactMatch(query_feature_index);
				AccumulatePartialSums(*(value_found->second), query_feature_index, term);
			}
		}

		//the next closest string will have an edit distance of 1
		return dist_params.ComputeDistanceTermNonNominalNonCyclicNonNullRegular(1.0, query_feature_index);
	}
	else if(feature_type == FDT_CONTINUOUS_CODE)
	{
		//compute partial sums for all code of matching size
		size_t code_size = 1;
		if(value_type == ENIVT_CODE)
			code_size = EvaluableNode::GetDeepSize(value.code);

		auto value_found = column->valueCodeSizeToIndices.find(code_size);
		if(value_found != end(column->valueCodeSizeToIndices))
		{
			auto &entity_indices = *(value_found->second);
			ComputeAndAccumulatePartialSums(dist_params, value, value_type,
				entity_indices, query_feature_index, absolute_feature_index);
		}

		//next most similar code must be at least a distance of 1 edit away
		return dist_params.ComputeDistanceTermNonNominalNonCyclicNonNullRegular(1.0, query_feature_index);
	}
	//else feature_type == FDT_CONTINUOUS_NUMERIC or FDT_CONTINUOUS_UNIVERSALLY_NUMERIC

	//if not a number or no numbers available, then no size
	if(value_type != ENIVT_NUMBER || column->sortedNumberValueEntries.size() == 0)
		return GetMaxDistanceTermFromValue(dist_params, value, value_type, query_feature_index, absolute_feature_index);

	bool cyclic_feature = dist_params.IsFeatureCyclic(query_feature_index);
	double cycle_length = std::numeric_limits<double>::infinity();
	if(cyclic_feature)
		cycle_length = dist_params.featureParams[query_feature_index].typeAttributes.maxCyclicDifference;

	auto [value_index, exact_index_found] = column->FindClosestValueIndexForValue(value.number, cycle_length);

	double term = 0.0;
	if(exact_index_found)
		term = dist_params.ComputeDistanceTermNonNominalExactMatch(query_feature_index);
	else
		term = dist_params.ComputeDistanceTermNonNominalNonNullRegular(value.number - column->sortedNumberValueEntries[value_index]->value.number, query_feature_index);

	size_t num_entities_computed = AccumulatePartialSums(column->sortedNumberValueEntries[value_index]->indicesWithValue, query_feature_index, term);

	//the logic below assumes there are at least two entries
	size_t num_unique_number_values = column->sortedNumberValueEntries.size();
	if(num_unique_number_values <= 1)
		return term;

	//if we haven't filled max_count results, or searched num_buckets, keep expanding search to neighboring buckets
	size_t lower_value_index = value_index;
	size_t upper_value_index = value_index;
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
		size_t next_lower_index = 0;
		if(!cyclic_feature)
		{
			if(lower_value_index > 1)
			{
				next_lower_index = lower_value_index - 1;
				lower_diff = std::abs(value.number - column->sortedNumberValueEntries[next_lower_index]->value.number);
				compute_lower = true;
			}
		}
		else //cyclic_feature
		{
			size_t next_index;
			//0th index is unknown
			if(lower_value_index > 1)
				next_index = lower_value_index - 1;
			else
				next_index = num_unique_number_values - 1;

			//make sure didn't wrap all the way around for cyclic features
			if(next_index != value_index)
			{
				next_lower_index = next_index;
				lower_diff = GeneralizedDistance::ConstrainDifferenceToCyclicDifference(std::abs(value.number - column->sortedNumberValueEntries[next_lower_index]->value.number), cycle_length);
				compute_lower = true;
			}
		}

		//see if can compute one bucket upper
		bool compute_upper = false;
		double upper_diff = 0.0;
		size_t next_upper_index = 0;
		if(!cyclic_feature)
		{
			if(upper_value_index + 1 < num_unique_number_values)
			{
				next_upper_index = upper_value_index + 1;
				upper_diff = std::abs(value.number - column->sortedNumberValueEntries[next_upper_index]->value.number);
				compute_upper = true;
			}
		}
		else //cyclic_feature
		{
			size_t next_index;
			if(upper_value_index + 1 < num_unique_number_values)
				next_index = upper_value_index + 1;
			else //0th index is unknown, start at 1st
				next_index = 1;

			//make sure didn't wrap all the way around for cyclic features
			//either from the value itself or overlapping with the next_lower_index
			if(next_index != value_index)
			{
				if((!compute_lower || next_index != next_lower_index))
				{
					next_upper_index = next_index;
					upper_diff = GeneralizedDistance::ConstrainDifferenceToCyclicDifference(std::abs(value.number - column->sortedNumberValueEntries[next_upper_index]->value.number), cycle_length);
					compute_upper = true;
				}
				else //upper and lower have overlapped, want to exit the loop
					next_upper_index = next_lower_index;
			}
		}

		//determine the next closest point and its difference
		double next_closest_diff;
		size_t next_closest_index;

		//if can only compute lower or lower is closer, then compute lower
		if((compute_lower && !compute_upper)
			|| (compute_lower && compute_upper && lower_diff < upper_diff))
		{
			next_closest_diff = lower_diff;
			next_closest_index = next_lower_index;
			lower_value_index = next_lower_index;
		}
		else if(compute_upper)
		{
			next_closest_diff = upper_diff;
			next_closest_index = next_upper_index;
			upper_value_index = next_upper_index;
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
			size_t potential_entities = column->sortedNumberValueEntries[next_closest_index]->indicesWithValue.size();
			if(num_entities_computed + potential_entities > max_num_to_find)
				break;

			//determine if it should continue based on how much this difference will contribute to the total; either a big jump or enough entities
			bool should_continue = false;
			double diff_delta = next_closest_diff - last_diff;

			if(diff_delta >= largest_diff_delta)
				should_continue = true;

			if(diff_delta >= largest_diff_delta / 2 && potential_entities >= 2)
				should_continue = true;

			//going out n deviations is likely to only miss 0.5^n of the likely values of nearest neighbors
			// so 0.5^5 should catch ~97% of the values
			if(dist_params.DoesFeatureHaveDeviation(query_feature_index)
				&& next_closest_diff < 5 * dist_params.featureParams[query_feature_index].deviation)
				should_continue = true;

			if(!should_continue)
				break;
		}

		term = dist_params.ComputeDistanceTermNonNominalNonNullRegular(next_closest_diff, query_feature_index);
		num_entities_computed += AccumulatePartialSums(column->sortedNumberValueEntries[next_closest_index]->indicesWithValue, query_feature_index, term);

		//track the rate of change of difference
		if(next_closest_diff - last_diff > largest_diff_delta)
			largest_diff_delta = next_closest_diff - last_diff;
		last_diff = next_closest_diff;

		//keep track of the largest seen so far
		if(term > largest_term)
			largest_term = term;

		//if cyclic and have wrapped around, then exit
		if(lower_value_index >= upper_value_index)
			break;
	}

	//return the largest computed so far
	return largest_term;
}

void SeparableBoxFilterDataStore::PopulateInitialPartialSums(GeneralizedDistance &dist_params, size_t top_k, size_t num_enabled_features,
	BitArrayIntegerSet &enabled_indices, std::vector<double> &min_unpopulated_distances, std::vector<double> &min_distance_by_unpopulated_count)
{
	size_t num_entities_to_populate = top_k;
	//populate sqrt(2)^p * top_k, which will yield 2 for p=2, 1 for p=0, and about 1.2 for p=0.5
	if(num_enabled_features > 1)
		num_entities_to_populate = static_cast<size_t>(std::lround(FastPow(GeneralizedDistance::s_sqrt_2, dist_params.pValue) * top_k)) + 1;

	min_unpopulated_distances.resize(num_enabled_features);
	for(size_t i = 0; i < num_enabled_features; i++)
	{
		double next_closest_distance = PopulatePartialSumsWithSimilarFeatureValue(dist_params,
			parametersAndBuffers.targetValues[i], parametersAndBuffers.targetValueTypes[i],
			num_entities_to_populate,
			//expand search if using more than one dimension
			num_enabled_features > 1,
			i, parametersAndBuffers.targetColumnIndices[i], enabled_indices);

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
	size_t num_features = partial_sums.numDimensions;
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
