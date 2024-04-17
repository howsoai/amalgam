#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------
//Seperable Box-Filter Data Store
//Spatial acceleration database for high-dimensional data with no constraints on metric space (Minkowski, Euclidean, LK, etc).
//The structure can efficiently search for data when using different metric space parameters without being rebuilt.
//-------------------------------------------------------------------------------------------------------------------------------------

//project headers:
#include "Concurrency.h"
#include "FastMath.h"
#include "EntityQueriesStatistics.h"
#include "EvaluableNode.h"
#include "IntegerSet.h"
#include "GeneralizedDistance.h"
#include "PartialSum.h"
#include "SBFDSColumnData.h"

//system headers:
#include <bitset>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

//forward declarations:
class Entity;

//supports cheap modification of:
//p-value, nominals, weights, distance accuracy, feature selections, case sub-selections
//requires minor updates for adding cases and features beyond initial dimensions
class SeparableBoxFilterDataStore
{
public:

	//contains the parameters and buffers to perform find operations on the SBFDS
	// for multithreading, there should be one of these per thread
	struct SBFDSParametersAndBuffers
	{
		//buffers for finding nearest cases
		RepeatedGeneralizedDistanceEvaluator rDistEvaluator;
		PartialSumCollection partialSums;
		std::vector<double> minUnpopulatedDistances;
		std::vector<double> minDistanceByUnpopulatedCount;
		std::vector<double> entityDistances;

		//used when finding a nearest entity to another nearest entity
		BitArrayIntegerSet potentialMatchesSet;

		//used when needing to accum entities with nulls
		BitArrayIntegerSet nullAccumSet;

		std::vector<DistanceReferencePair<size_t>> entitiesWithValues;

		FlexiblePriorityQueue<CountDistanceReferencePair<size_t>> potentialGoodMatches;
		StochasticTieBreakingPriorityQueue<DistanceReferencePair<size_t>> sortedResults;

		//cache of nearest neighbors from previous query
		std::vector<size_t> previousQueryNearestNeighbors;
	};

	SeparableBoxFilterDataStore()
	{
		numEntities = 0;
	}

	//Gets the maximum possible distance term from value assuming the feature is continuous
	// absolute_feature_index is the offset to access the feature relative to the entire data store
	// query_feature_index is relative to feature attributes and data in r_dist_eval
	inline double GetMaxDistanceTermForContinuousFeature(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t query_feature_index, size_t absolute_feature_index, bool high_accuracy)
	{
		double max_diff = columnData[absolute_feature_index]->GetMaxDifferenceTerm(
														r_dist_eval.distEvaluator->featureAttribs[query_feature_index]);
		return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousNonNullRegular(
														max_diff, query_feature_index, high_accuracy);
	}

	//gets the matrix cell index for the specified index
	__forceinline const size_t GetMatrixCellIndex(size_t entity_index)
	{
		return entity_index * columnData.size();
	}

	//returns the the element at index's value for the specified column at column_index, requires valid index
	__forceinline EvaluableNodeImmediateValue &GetValue(size_t index, size_t column_index)
	{
		return matrix[index * columnData.size() + column_index];
	}

	//returns the column index for the label_id, or maximum value if not found
	inline size_t GetColumnIndexFromLabelId(StringInternPool::StringID label_id)
	{
		if(label_id == string_intern_pool.NOT_A_STRING_ID)
			return std::numeric_limits<size_t>::max();

		auto column = labelIdToColumnIndex.find(label_id);
		if(column == end(labelIdToColumnIndex))
			return std::numeric_limits<size_t>::max();
		return column->second;
	}

	//returns true if the structure already has the label
	inline bool DoesHaveLabel(size_t label_id)
	{
		return (labelIdToColumnIndex.count(label_id) > 0);
	}

	//populates the matrix with the label and builds column data
	// assumes column data is empty
	void BuildLabel(size_t column_index, const std::vector<Entity *> &entities);

	//changes column to/from interning as would yield best performance
	void OptimizeColumn(size_t column_index);

	//calls OptimizeColumn on all columns
	inline void OptimizeAllColumns()
	{
		for(size_t column_index = 0; column_index < columnData.size(); column_index++)
			OptimizeColumn(column_index);
	}

	//expand the structure by adding a new column/label/feature and populating with data from entities
	void AddLabels(std::vector<size_t> &label_ids, const std::vector<Entity *> &entities)
	{
		//make sure have data to add
		if(label_ids.size() == 0 || entities.size() == 0)
			return;

		//resize the matrix and populate column and label_id lookups
		size_t num_columns_added = AddLabelsAsEmptyColumns(label_ids, entities.size());

		size_t num_columns = columnData.size();
		size_t num_previous_columns = columnData.size() - num_columns_added;

	#ifdef MULTITHREAD_SUPPORT
		//if big enough (enough entities and/or enough columns), try to use multithreading
		if(num_columns_added > 1 && (numEntities > 10000 || (numEntities > 200 && num_columns_added > 10)))
		{
			std::vector<std::future<void>> columns_completed;
			columns_completed.reserve(num_columns);

			auto enqueue_task_lock = Concurrency::urgentThreadPool.BeginEnqueueBatchTask(false);
			for(size_t i = num_previous_columns; i < num_columns; i++)
			{
				columns_completed.emplace_back(
					Concurrency::urgentThreadPool.BatchEnqueueTask([this, &entities, i]() { BuildLabel(i, entities); })
				);
			}
			enqueue_task_lock.Unlock();

			Concurrency::urgentThreadPool.ChangeCurrentThreadStateFromActiveToWaiting();
			for(auto &future : columns_completed)
				future.wait();
			Concurrency::urgentThreadPool.ChangeCurrentThreadStateFromWaitingToActive();

			return;
		}
		//not running concurrently
	#endif

		for(size_t i = num_previous_columns; i < num_columns; i++)
			BuildLabel(i, entities);
	}

	//returns true only if none of the entities have the label
	inline bool IsColumnIndexRemovable(size_t column_index_to_remove)
	{
		//removable only if have no values; every entity is invalid
		return (columnData[column_index_to_remove]->invalidIndices.size() == GetNumInsertedEntities());
	}

	//removes a column from the database
	void RemoveColumnIndex(size_t column_index_to_remove);

	//finds any columns / labels that are no longer used by any entity and removes them
	inline void RemoveAnyUnusedLabels()
	{
		//column_index is one greater than the actual index to keep it above zero
		// work from high column indices to low for performance and because removal swaps
		// the last column into the current column's place, so don't need to recheck the index or update the indices
		for(size_t column_index = columnData.size(); column_index > 0; column_index--)
		{
			if(IsColumnIndexRemovable(column_index - 1))
				RemoveColumnIndex(column_index - 1);
		}
	}

	//adds an entity to the database
	void AddEntity(Entity *entity, size_t entity_index);

	//removes an entity to the database using an incremental update scheme
	void RemoveEntity(Entity *entity, size_t entity_index, size_t entity_index_to_reassign);

	//updates all of the label values for entity with index entity_index
	void UpdateAllEntityLabels(Entity *entity, size_t entity_index);

	//like UpdateAllEntityLabels, but only updates labels for label_updated
	void UpdateEntityLabel(Entity *entity, size_t entity_index, StringInternPool::StringID label_updated);

	constexpr size_t GetNumInsertedEntities()
	{
		return numEntities;
	}

	//returns a reference to the BitArrayIntegerSet corresponding to the entities with numbers for column_index
	inline EfficientIntegerSet &GetEntitiesWithValidNumbers(size_t column_index)
	{
		return columnData[column_index]->numberIndices;
	}

	//returns a reference to the BitArrayIntegerSet corresponding to the entities with strings ids for column_index
	inline EfficientIntegerSet &GetEntitiesWithValidStringIds(size_t column_index)
	{
		return columnData[column_index]->stringIdIndices;
	}

	//given a feature_id and a range [low, high], fills out with all the entities with values of feature feature_id within specified range
	//Note about Null/NaNs:
	//if the feature value is Nan/Null, it will NOT be present in the search results, ie "x" != 3 will NOT include elements with x is nan/Null, even though nan/null != 3
	inline void FindAllEntitiesWithinRange(size_t feature_id, EvaluableNodeImmediateValueType value_type,
		EvaluableNodeImmediateValue &low, EvaluableNodeImmediateValue &high, BitArrayIntegerSet &out, bool between_values = true)
	{
		if(numEntities == 0)
		{
			out.clear();
			return;
		}

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
		{
			out.clear();
			return;
		}

		columnData[column->second]->FindAllIndicesWithinRange(value_type, low, high, out, between_values);
	}

	//sets out to include only entities that have the given feature
	inline void FindAllEntitiesWithFeature(size_t feature_id, BitArrayIntegerSet &out)
	{
		if(numEntities == 0)
		{
			out.clear();
			return;
		}

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
		{
			out.clear();
			return;
		}

		columnData[column->second]->invalidIndices.NotTo(out, GetNumInsertedEntities());
	}

	//filters out to include only entities that have the given feature
	//if in_batch is true, will update out in batch for performance,
	//meaning its number of elements will need to be updated
	inline void IntersectEntitiesWithFeature(size_t feature_id, BitArrayIntegerSet &out, bool in_batch)
	{
		if(numEntities == 0)
		{
			out.clear();
			return;
		}

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
		{
			out.clear();
			return;
		}

		columnData[column->second]->invalidIndices.EraseTo(out, in_batch);
	}

	//sets out to include only entities that have the given feature and records the values into
	// entities and values respectively.  enabled_entities is used as a buffer
	inline void FindAllEntitiesWithValidNumbers(size_t feature_id, BitArrayIntegerSet &enabled_entities,
		std::vector<size_t> &entities, std::vector<double> &values)
	{
		if(numEntities == 0)
			return;

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
			return;
		size_t column_index = column->second;
		auto &column_data = columnData[column_index];

		column_data->numberIndices.CopyTo(enabled_entities);
		column_data->nanIndices.EraseTo(enabled_entities);

		//resize buffers and place each entity and value into its respective buffer
		entities.resize(enabled_entities.size());
		values.resize(enabled_entities.size());
		size_t index = 0;
		auto value_type = column_data->GetUnresolvedValueType(ENIVT_NUMBER);
		for(auto entity_index : enabled_entities)
		{
			entities[index] = entity_index;
			values[index] = column_data->GetResolvedValue(value_type, GetValue(entity_index, column_index)).number;
			index++;
		}
	}

	//filters enabled_indices to include only entities that have the given feature
	// records the entities into entities and values respectively
	inline void IntersectEntitiesWithValidNumbers(size_t feature_id, BitArrayIntegerSet &enabled_entities,
		std::vector<size_t> &entities, std::vector<double> &values)
	{
		if(numEntities == 0)
			return;

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
			return;
		size_t column_index = column->second;
		auto &column_data = columnData[column_index];

		column_data->numberIndices.IntersectTo(enabled_entities);
		column_data->nanIndices.EraseTo(enabled_entities);

		//resize buffers and place each entity and value into its respective buffer
		entities.resize(enabled_entities.size());
		values.resize(enabled_entities.size());
		size_t index = 0;
		auto value_type = column_data->GetUnresolvedValueType(ENIVT_NUMBER);
		for(auto entity_index : enabled_entities)
		{
			entities[index] = entity_index;
			values[index] = column_data->GetResolvedValue(value_type, GetValue(entity_index, column_index)).number;
			index++;
		}
	}

	//sets out to include only entities that don't have the given feature
	inline void FindAllEntitiesWithoutFeature(size_t feature_id, BitArrayIntegerSet &out)
	{
		if(numEntities == 0)
		{
			out.clear();
			return;
		}

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
		{
			out.clear();
			return;
		}

		columnData[column->second]->invalidIndices.CopyTo(out);
	}

	//filters out to include only entities that don't have the given feature
	//if in_batch is true, will update out in batch for performance,
	//meaning its number of elements will need to be updated
	inline void IntersectEntitiesWithoutFeature(size_t feature_id, BitArrayIntegerSet &out, bool in_batch)
	{
		if(numEntities == 0)
			return;

		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
			return;

		columnData[column->second]->invalidIndices.IntersectTo(out, in_batch);
	}

	//given a feature_id, value_type, and value, inserts into out all the entities that have the value
	inline void UnionAllEntitiesWithValue(size_t feature_id,
		EvaluableNodeImmediateValueType value_type, EvaluableNodeImmediateValue &value, BitArrayIntegerSet &out)
	{
		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
			return;
		size_t column_index = column->second;

		if(value_type != ENIVT_CODE)
		{
			columnData[column_index]->UnionAllIndicesWithValue(value_type, value, out);
		}
		else //compare if code is equal
		{
			for(auto entity_index : columnData[column_index]->codeIndices)
			{
				if(EvaluableNode::AreDeepEqual(value.code, GetValue(entity_index, column_index).code))
					out.insert(entity_index);
			}
		}
	}

	//Finds the Minimum or Maximum (with respect to feature_id feature value) num_to_find entities in the database; if is_max is true, finds max, else finds min
	inline void FindMinMax(size_t feature_id,
		EvaluableNodeImmediateValueType value_type, size_t num_to_find, bool is_max,
		BitArrayIntegerSet *enabled_indices, BitArrayIntegerSet &out)
	{
		auto column = labelIdToColumnIndex.find(feature_id);
		if(column == labelIdToColumnIndex.end())
			return;

		columnData[column->second]->FindMinMax(value_type, num_to_find, is_max, enabled_indices, out);
	}

	//returns the number of unique values for a column for the given value_type
	size_t GetNumUniqueValuesForColumn(size_t column_index, EvaluableNodeImmediateValueType value_type)
	{
		auto &column_data = columnData[column_index];
		if(value_type == ENIVT_NUMBER)
			return column_data->numberIndices.size();
		else if(value_type == ENIVT_STRING_ID)
			return column_data->stringIdIndices.size();
		else //return everything else
			return GetNumInsertedEntities() - column_data->invalidIndices.size();
	}

	//returns a function that will take in an entity index iterator and reference to a double to store the value and return true if the value is found
	// assumes and requires column_index is a valid column (not a feature_id)
	template<typename Iter>
	inline std::function<bool(Iter, double &)> GetNumberValueFromEntityIteratorFunction(size_t column_index)
	{
		auto column_data = columnData[column_index].get();
		auto number_indices_ptr = &column_data->numberIndices;
		auto value_type = column_data->GetUnresolvedValueType(ENIVT_NUMBER);

		return [&, number_indices_ptr, column_index, column_data, value_type]
		(Iter i, double &value)
		{
			size_t entity_index = *i;
			if(!number_indices_ptr->contains(entity_index))
				return false;

			value = column_data->GetResolvedValue(value_type, GetValue(entity_index, column_index)).number;
			return true;
		};
	}

	//returns a function that will take in an entity index and reference to a double to store the value and return true if the value is found
	// assumes and requires column_index is a valid column (not a feature_id)
	inline std::function<bool(size_t, double &)> GetNumberValueFromEntityIndexFunction(size_t column_index)
	{
		//if invalid column_index, then always return false
		if(column_index >= columnData.size())
			return [](size_t i, double &value) { return false; };

		auto column_data = columnData[column_index].get();
		auto number_indices_ptr = &column_data->numberIndices;
		auto value_type = column_data->GetUnresolvedValueType(ENIVT_NUMBER);

		return [&, number_indices_ptr, column_index, column_data, value_type]
			(size_t i, double &value)
			{
				if(!number_indices_ptr->contains(i))
					return false;

				value = column_data->GetResolvedValue(value_type, GetValue(i, column_index)).number;
				return true;
			};
	}

	//returns a function that will take in an entity index iterator and reference to a string id to store the value and return true if the value is found
	// assumes and requires column_index is a valid column (not a feature_id)
	template<typename Iter>
	inline std::function<bool(Iter, StringInternPool::StringID &)> GetStringIdValueFromEntityIteratorFunction(size_t column_index)
	{
		auto string_indices_ptr = &columnData[column_index]->stringIdIndices;

		return [&, string_indices_ptr, column_index]
		(Iter i, StringInternPool::StringID &value)
		{
			size_t entity_index = *i;
			if(!string_indices_ptr->contains(entity_index))
				return false;

			value = GetValue(entity_index, column_index).stringID;
			return true;
		};
	}

	//populates distances_out with all entities and their distances that have a distance to target less than max_dist
	//if enabled_indices is not nullptr, intersects with the enabled_indices set.
	//assumes that enabled_indices only contains indices that have valid values for all the features
	void FindEntitiesWithinDistance(GeneralizedDistanceEvaluator &r_dist_eval, std::vector<size_t> &position_label_ids,
		std::vector<EvaluableNodeImmediateValue> &position_values, std::vector<EvaluableNodeImmediateValueType> &position_value_types,
		double max_dist, StringInternPool::StringID radius_label, BitArrayIntegerSet &enabled_indices,
		std::vector<DistanceReferencePair<size_t>> &distances_out);

	//Finds the top_k nearest neighbors results to the entity at search_index.
	// if expand_to_first_nonzero_distance is set, then it will expand top_k until it it finds the first nonzero distance or until it includes all enabled indices 
	//will not modify enabled_indices, but instead will make a copy for any modifications
	//assumes that enabled_indices only contains indices that have valid values for all the features
	void FindEntitiesNearestToIndexedEntity(GeneralizedDistanceEvaluator &dist_eval, std::vector<size_t> &position_label_ids,
		size_t search_index, size_t top_k, StringInternPool::StringID radius_label,
		BitArrayIntegerSet &enabled_indices, bool expand_to_first_nonzero_distance,
		std::vector<DistanceReferencePair<size_t>> &distances_out,
		size_t ignore_index = std::numeric_limits<size_t>::max(), RandomStream rand_stream = RandomStream());
	
	//Finds the nearest neighbors
	//enabled_indices is the set of entities to find from, and will be modified
	//assumes that enabled_indices only contains indices that have valid values for all the features
	void FindNearestEntities(GeneralizedDistanceEvaluator &dist_eval, std::vector<size_t> &position_label_ids,
		std::vector<EvaluableNodeImmediateValue> &position_values, std::vector<EvaluableNodeImmediateValueType> &position_value_types,
		size_t top_k, StringInternPool::StringID radius_label, size_t ignore_entity_index, BitArrayIntegerSet &enabled_indices,
		std::vector<DistanceReferencePair<size_t>> &distances_out, RandomStream rand_stream = RandomStream());

protected:

	//deletes/pops off the last row in the matrix cache
	inline void DeleteLastRow()
	{
		if(matrix.size() == 0)
			return;

		//truncate matrix cache
		numEntities--;
		matrix.resize(matrix.size() - columnData.size());
	}

	//deletes the index and associated data
	void DeleteEntityIndexFromColumns(size_t entity_index);

	//adds a new labels to the database, populating new cells with -NaN, and updating the number of entities
	// assumes label_ids is not empty and num_entities is nonzero
	//returns the number of new columns inserted
	size_t AddLabelsAsEmptyColumns(std::vector<size_t> &label_ids, size_t num_entities);

	//computes each partial sum and adds the term to the partial sums associated for each id in entity_indices for query_feature_index
	//returns the number of entities indices accumulated
	size_t ComputeAndAccumulatePartialSums(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		SortedIntegerSet &entity_indices, size_t query_feature_index, size_t absolute_feature_index, bool high_accuracy)
	{
		size_t num_entity_indices = entity_indices.size();

		auto &partial_sums = parametersAndBuffers.partialSums;
		const auto accum_location = partial_sums.GetAccumLocation(query_feature_index);

		auto &column_data = columnData[absolute_feature_index];

		//for each found element, accumulate associated partial sums
		for(size_t entity_index : entity_indices)
		{
			//get value
			auto other_value_type = column_data->GetIndexValueType(entity_index);
			auto other_value = column_data->GetResolvedValue(other_value_type, GetValue(entity_index, absolute_feature_index));
			other_value_type = column_data->GetResolvedValueType(other_value_type);

			//compute term
			double term = r_dist_eval.ComputeDistanceTerm(other_value, other_value_type, query_feature_index, high_accuracy);

			//accumulate
			partial_sums.Accum(entity_index, accum_location, term);
		}

		return num_entity_indices;
	}

	//adds term to the partial sums associated for each id in entity_indices for query_feature_index
	//returns the number of entities indices accumulated
	inline size_t AccumulatePartialSums(SortedIntegerSet &entity_indices, size_t query_feature_index, double term)
	{
		size_t num_entity_indices = entity_indices.size();
		size_t max_index = num_entity_indices;

		auto &partial_sums = parametersAndBuffers.partialSums;
		const auto accum_location = partial_sums.GetAccumLocation(query_feature_index);
		size_t max_element = partial_sums.numInstances;

		auto &entity_indices_vector = entity_indices.GetIntegerVector();

		//it's almost always faster to just accumulate an index than to check if it is a valid index
		// and then only accumulate if it is valid
		//however, indices beyond the range of partial_sums will cause an issue
		//therefore, only trim back the end if needed, and trim back to the largest possible element id (max_element - 1)
		if(entity_indices.GetEndInteger() >= max_element)
		{
			max_index = entity_indices.GetFirstIntegerVectorLocationGreaterThan(max_element - 1);
			num_entity_indices = max_index - 1;
		}

		//for each found element, accumulate associated partial sums, or if zero, just mark that it's accumulated
		if(term != 0.0)
		{
			#pragma omp parallel for schedule(static) if(max_index > 300)
			for(int64_t i = 0; i < static_cast<int64_t>(max_index); i++)
			{
				const auto entity_index = entity_indices_vector[i];
				partial_sums.Accum(entity_index, accum_location, term);
			}
		}
		else //term == 0.0
		{
			#pragma omp parallel for schedule(static) if(max_index > 300)
			for(int64_t i = 0; i < static_cast<int64_t>(max_index); i++)
			{
				const auto entity_index = entity_indices_vector[i];
				partial_sums.AccumZero(entity_index, accum_location);
			}
		}

		return num_entity_indices;
	}

	//adds term to the partial sums associated for each id in entity_indices for query_feature_index
	//returns the number of entities indices accumulated
	inline size_t AccumulatePartialSums(BitArrayIntegerSet &entity_indices, size_t query_feature_index, double term)
	{
		size_t num_entity_indices = entity_indices.size();
		if(num_entity_indices == 0)
			return 0;

		auto &partial_sums = parametersAndBuffers.partialSums;
		const auto accum_location = partial_sums.GetAccumLocation(query_feature_index);
		size_t max_element = partial_sums.numInstances;

		if(term != 0.0)
		{
			entity_indices.IterateOver(
				[&partial_sums, &accum_location, term]
				(size_t entity_index)
				{
					partial_sums.Accum(entity_index, accum_location, term);
				},
				max_element);
		}
		else
		{
			entity_indices.IterateOver(
				[&partial_sums, &accum_location]
				(size_t entity_index)
				{
					partial_sums.AccumZero(entity_index, accum_location);
				},
				max_element);
		}

		return entity_indices.size();
	}

	//adds term to the partial sums associated for each id in entity_indices for query_feature_index
	//returns the number of entities indices accumulated
	inline size_t AccumulatePartialSums(EfficientIntegerSet &entity_indices, size_t query_feature_index, double term)
	{
		if(entity_indices.IsSisContainer())
			return AccumulatePartialSums(entity_indices.GetSisContainer(), query_feature_index, term);
		else
			return AccumulatePartialSums(entity_indices.GetBaisContainer(), query_feature_index, term);
	}

	//accumulates the partial sums for the specified value
	// returns the distance term evaluated, or 0.0 if value was not found
	inline double AccumulatePartialSumsForNominalNumberValueIfExists(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		double value, size_t query_feature_index, SBFDSColumnData &column, bool high_accuracy)
	{
		auto [value_index, exact_index_found] = column.FindExactIndexForValue(value);
		if(exact_index_found)
		{
			double term = r_dist_eval.ComputeDistanceTermNominalNumeric(value, true, query_feature_index, high_accuracy);
			AccumulatePartialSums(column.sortedNumberValueEntries[value_index]->indicesWithValue, query_feature_index, term);
			return term;
		}

		return 0.0;
	}

	//accumulates the partial sums for the specified value
	// returns the distance term evaluated, or 0.0 if value was not found
	inline double AccumulatePartialSumsForNominalStringIdValueIfExists(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		StringInternPool::StringID value, size_t query_feature_index, SBFDSColumnData &column, bool high_accuracy)
	{
		auto value_found = column.stringIdValueToIndices.find(value);
		if(value_found != end(column.stringIdValueToIndices))
		{
			double term = r_dist_eval.ComputeDistanceTermNominalString(value, true, query_feature_index, high_accuracy);
			AccumulatePartialSums(*(value_found->second), query_feature_index, term);
			return term;
		}

		return 0.0;
	}

	//search a projection width in terms of bucket count or number of collected entities
	//accumulates partial sums
	//searches until num_entities_to_populate are popluated or other heuristics have been reached
	//will only consider indices in enabled_indiced
	// query_feature_index is the offset to access the feature relative to the particular query data parameters
	//returns the smallest partial sum for any value not yet computed
	double PopulatePartialSumsWithSimilarFeatureValue(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t num_entities_to_populate, bool expand_search_if_optimal, bool high_accuracy,
		size_t query_feature_index, BitArrayIntegerSet &enabled_indices);

	//computes a heuristically derived set of partial sums across all the enabled features from parametersAndBuffers.targetValues[i] and parametersAndBuffers.targetColumnIndices[i]
	// if enabled_indices is not nullptr, then will only use elements in that list
	// uses top_k for heuristics as to how many partial sums to compute
	// if radius_column_index is specified, it will populate the initial partial sums with them
	// will compute and populate min_unpopulated_distances and min_distance_by_unpopulated_count, where the former is the next smallest uncomputed feature distance indexed by the number of features not computed
	// and min_distance_by_unpopulated_count is the total distance of all uncomputed features where the index is the number of uncomputed features
	void PopulateInitialPartialSums(RepeatedGeneralizedDistanceEvaluator &r_dist_eval, size_t top_k, size_t radius_column_index,
		bool high_accuracy, BitArrayIntegerSet &enabled_indices,
		std::vector<double> &min_unpopulated_distances, std::vector<double> &min_distance_by_unpopulated_count);

	void PopulatePotentialGoodMatches(FlexiblePriorityQueue<CountDistanceReferencePair<size_t>> &potential_good_matches,
		BitArrayIntegerSet &enabled_indices, PartialSumCollection &partial_sums, size_t top_k);

	//returns the distance between two nodes while respecting the feature mask
	inline double GetDistanceBetween(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t radius_column_index, size_t other_index, bool high_accuracy)
	{
		const size_t matrix_base_position = other_index * columnData.size();

		double dist_accum = 0.0;
		for(size_t i = 0; i < r_dist_eval.featureData.size(); i++)
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[i];

			size_t column_index = feature_attribs.featureIndex;
			auto &column_data = columnData[column_index];

			auto other_value_type = column_data->GetIndexValueType(other_index);
			auto other_value = column_data->GetResolvedValue(other_value_type, matrix[matrix_base_position + column_index]);
			other_value_type = column_data->GetResolvedValueType(other_value_type);

			dist_accum += r_dist_eval.ComputeDistanceTerm(other_value, other_value_type, i, high_accuracy);
		}

		double dist = r_dist_eval.distEvaluator->InverseExponentiateDistance(dist_accum, high_accuracy);

		if(radius_column_index < columnData.size())
		{
			auto &column_data = columnData[radius_column_index];
			auto radius_value_type = column_data->GetIndexValueType(other_index);
			if(radius_value_type == ENIVT_NUMBER || radius_value_type == ENIVT_NUMBER_INDIRECTION_INDEX)
				dist -= column_data->GetResolvedValue(radius_value_type, matrix[matrix_base_position + radius_column_index]).number;
		}

		return dist;
	}

	//computes the distance term for the entity, query_feature_index, and feature_type,
	// where the value does not match any in the SBFDS
	//assumes that null values have already been taken care of for nominals
	__forceinline double ComputeDistanceTermNonMatch(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t entity_index, size_t query_feature_index, bool high_accuracy)
	{
		auto &feature_data = r_dist_eval.featureData[query_feature_index];
		switch(feature_data.effectiveFeatureType)
		{
		case RepeatedGeneralizedDistanceEvaluator::EFDT_REMAINING_IDENTICAL_PRECOMPUTED:
			return feature_data.precomputedRemainingIdenticalDistanceTerm;

		case RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_UNIVERSALLY_NUMERIC:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousNonCyclicOneNonNullRegular(
				feature_data.targetValue.nodeValue.number - GetValue(entity_index, feature_attribs.featureIndex).number,
				query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_NUMERIC_PRECOMPUTED:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			return r_dist_eval.ComputeDistanceTermNumberInternedPrecomputed(
				GetValue(entity_index, feature_attribs.featureIndex).indirectionIndex, query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_NUMERIC:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			if(column_data->numberIndices.contains(entity_index))
				return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousNonCyclicOneNonNullRegular(
					feature_data.targetValue.nodeValue.number - GetValue(entity_index, feature_attribs.featureIndex).number,
					query_feature_index, high_accuracy);
			else
				return r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_NUMERIC_CYCLIC:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			if(column_data->numberIndices.contains(entity_index))
				return r_dist_eval.distEvaluator->ComputeDistanceTermContinuousOneNonNullRegular(
					feature_data.targetValue.nodeValue.number - GetValue(entity_index, feature_attribs.featureIndex).number,
					query_feature_index, high_accuracy);
			else
				return r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_NUMERIC_PRECOMPUTED:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			if(column_data->numberIndices.contains(entity_index))
				return r_dist_eval.ComputeDistanceTermNumberInternedPrecomputed(
					GetValue(entity_index, feature_attribs.featureIndex).indirectionIndex, query_feature_index, high_accuracy);
			else
				return r_dist_eval.distEvaluator->ComputeDistanceTermKnownToUnknown(query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_STRING:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			if(column_data->stringIdIndices.contains(entity_index))
				return r_dist_eval.ComputeDistanceTermNominalString(
					GetValue(entity_index, feature_attribs.featureIndex).stringID, true,
					query_feature_index, high_accuracy);
			else
				return r_dist_eval.ComputeDistanceTermNominalString(string_intern_pool.EMPTY_STRING_ID, false,
					query_feature_index, high_accuracy);
		}

		case RepeatedGeneralizedDistanceEvaluator::EFDT_NOMINAL_NUMERIC:
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			if(column_data->numberIndices.contains(entity_index))
				return r_dist_eval.ComputeDistanceTermNominalNumeric(
					GetValue(entity_index, feature_attribs.featureIndex).number, true,
					query_feature_index, high_accuracy);
			else
				return r_dist_eval.ComputeDistanceTermNominalNumeric(0.0, false,
					query_feature_index, high_accuracy);
		}

		default:
			//RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_STRING
			//or RepeatedGeneralizedDistanceEvaluator::EFDT_CONTINUOUS_CODE
		{
			auto &feature_attribs = r_dist_eval.distEvaluator->featureAttribs[query_feature_index];
			auto &column_data = columnData[feature_attribs.featureIndex];
			auto other_value_type = column_data->GetIndexValueType(entity_index);
			auto other_value = column_data->GetResolvedValue(other_value_type, GetValue(entity_index, feature_attribs.featureIndex));

			return r_dist_eval.ComputeDistanceTerm(other_value, other_value_type, query_feature_index, high_accuracy);
		}
		}
	}

	//given an estimate of distance that uses best_possible_feature_distance filled in for any features not computed,
	// this function iterates over the partial sums indices, replacing each uncomputed feature with the actual distance for that feature
	//returns the distance
	//assumes that all features that are exact matches have already been computed
	__forceinline double ResolveDistanceToNonMatchTargetValues(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		PartialSumCollection &partial_sums, size_t entity_index, size_t num_target_labels, bool high_accuracy)
	{
		//calculate full non-exponentiated Minkowski distance to the target
		double distance = partial_sums.GetSum(entity_index);

		for(auto it = partial_sums.BeginPartialSumIndex(entity_index); *it < num_target_labels; ++it)
		{
			if(it.IsIndexComputed())
				continue;

			size_t query_feature_index = *it;
			distance += ComputeDistanceTermNonMatch(r_dist_eval, entity_index, query_feature_index, high_accuracy);
		}

		return distance;
	}

	//given an estimate of distance that uses best_possible_feature_distance filled in for any features not computed,
	// this function iterates over the partial sums indices, replacing each uncomputed feature with the actual distance for that feature
	// if the distance ever exceeds reject_distance, then the resolving will stop early
	// if reject_distance is infinite, then it will just complete the distance terms
	//returns a pair of a boolean and the distance.  if the boolean is true, then the distance is less than or equal to the reject distance
	//assumes that all features that are exact matches have already been computed
	__forceinline std::pair<bool, double> ResolveDistanceToNonMatchTargetValues(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		PartialSumCollection &partial_sums, size_t entity_index, std::vector<double> &min_distance_by_unpopulated_count, size_t num_features,
		double reject_distance, std::vector<double> &min_unpopulated_distances, bool high_accuracy)
	{
		auto [num_calculated_features, distance] = partial_sums.GetNumFilledAndSum(entity_index);

		//complete known sums with worst and best possibilities
		//calculate the number of features for which the minkowski distance term has not yet been calculated 
		size_t num_uncalculated_features = (num_features - num_calculated_features);
		//if have already calculated everything, then already have the distance
		if(num_uncalculated_features == 0)
			return std::make_pair(distance <= reject_distance, distance);

		//if too far out, reject immediately
		distance += min_distance_by_unpopulated_count[num_uncalculated_features];
		if(distance > reject_distance)
			return std::make_pair(false, distance);

		//use infinite loop with exit at the end to remove need for extra iterator increment
		for(auto it = partial_sums.BeginPartialSumIndex(entity_index); true; ++it)
		{
			if(it.IsIndexComputed())
				continue;

			//remove distance already added and reduce num_uncalculated_partial_sum_features
			distance -= min_unpopulated_distances[--num_uncalculated_features];

			const size_t query_feature_index = *it;
			distance += ComputeDistanceTermNonMatch(r_dist_eval, entity_index, query_feature_index, high_accuracy);

			//break out of the loop before the iterator is incremented to save a few cycles
			//do this via logic to minimize the number of branches
			bool unacceptable_distance = (distance > reject_distance);
			if(unacceptable_distance || num_uncalculated_features == 0)
				return std::make_pair(!unacceptable_distance, distance);
		}

		//shouldn't make it here
		return std::make_pair(true, distance);
	}

public:

	//populates specified target value given the selected target values for each value in corresponding position* parameters
	void PopulateTargetValueAndLabelIndex(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t query_feature_index, EvaluableNodeImmediateValue position_value,
		EvaluableNodeImmediateValueType position_value_type);

	//populates all target values given the selected target values for each value in corresponding position* parameters
	void PopulateTargetValuesAndLabelIndices(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		std::vector<size_t> &position_label_ids, std::vector<EvaluableNodeImmediateValue> &position_values,
		std::vector<EvaluableNodeImmediateValueType> &position_value_types)
	{
		size_t num_features = position_values.size();
		r_dist_eval.featureData.resize(num_features);
		for(size_t query_feature_index = 0; query_feature_index < num_features; query_feature_index++)
		{
			auto column = labelIdToColumnIndex.find(position_label_ids[query_feature_index]);
			if(column != end(labelIdToColumnIndex))
				PopulateTargetValueAndLabelIndex(r_dist_eval, query_feature_index,
					position_values[query_feature_index], position_value_types[query_feature_index]);
		}
	}

	//recomputes column indices for each feature as well as filling in unknowns
	inline void PopulateColumnIndicesAndUnknownFeatureValueDifferences(
		GeneralizedDistanceEvaluator &dist_eval, std::vector<size_t> &position_label_ids)
	{
		for(size_t query_feature_index = 0; query_feature_index < position_label_ids.size(); query_feature_index++)
		{
			auto column = labelIdToColumnIndex.find(position_label_ids[query_feature_index]);
			if(column == end(labelIdToColumnIndex))
				continue;

			auto &feature_attribs = dist_eval.featureAttribs[query_feature_index];
			feature_attribs.featureIndex = column->second;

			//if either known or unknown to unknown is missing, need to compute difference
			// and store it where it is needed
			double unknown_distance_term = 0.0;
			if(FastIsNaN(feature_attribs.knownToUnknownDistanceTerm.deviation)
				|| FastIsNaN(feature_attribs.unknownToUnknownDistanceTerm.deviation))
			{
				unknown_distance_term = columnData[feature_attribs.featureIndex]->GetMaxDifferenceTerm(
					feature_attribs);

				if(FastIsNaN(feature_attribs.knownToUnknownDistanceTerm.deviation))
					feature_attribs.knownToUnknownDistanceTerm.deviation = unknown_distance_term;
				if(FastIsNaN(feature_attribs.unknownToUnknownDistanceTerm.deviation))
					feature_attribs.unknownToUnknownDistanceTerm.deviation = unknown_distance_term;
			}
		}
	}

	//returns all elements in the database that yield valid distances along with their sorted distances to the values for entity
	// at target_index, optionally limits results count to k
	inline void FindAllValidElementDistances(RepeatedGeneralizedDistanceEvaluator &r_dist_eval,
		size_t radius_column_index, BitArrayIntegerSet &valid_indices,
		std::vector<DistanceReferencePair<size_t>> &distances_out, RandomStream rand_stream)
	{
		auto &sorted_results = parametersAndBuffers.sortedResults;
		sorted_results.clear();
		sorted_results.SetStream(rand_stream);

		bool high_accuracy = (r_dist_eval.distEvaluator->highAccuracyDistances || r_dist_eval.distEvaluator->recomputeAccurateDistances);

		for(auto index : valid_indices)
		{
			double distance = GetDistanceBetween(r_dist_eval, radius_column_index, index, high_accuracy);
			distances_out.emplace_back(distance, index);
		}

		std::sort(begin(distances_out), end(distances_out));
	}

	//contains entity lookups for each of the values for each of the columns
	std::vector<std::unique_ptr<SBFDSColumnData>> columnData;
	
	//for multithreading, there should be one of these per thread
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
	static SBFDSParametersAndBuffers parametersAndBuffers;
	
	//map from label id to column index in the matrix
	FastHashMap<StringInternPool::StringID, size_t> labelIdToColumnIndex;

	//matrix of cases (rows) * features (columns)
	std::vector<EvaluableNodeImmediateValue> matrix;

	//the number of entities in the data store; all indices below this value are populated
	size_t numEntities;
};
