#pragma once

//project headers:
#include "Concurrency.h"
#include "SeparableBoxFilterDataStore.h"

//system headers:
#include <vector>

//caches nearest neighbor results for every entity in the provided data structure
class KnnCache
{
public:
	KnnCache()
	{
		sbfDataStore = nullptr;
	}

	//clears all buffers and resizes and resets them based on the datastore of entities and the particular
	// relevant_indices to use from the datastore
	void ResetCache(SeparableBoxFilterDataStore &datastore, BitArrayIntegerSet &relevant_indices,
		GeneralizedDistanceEvaluator &r_dist_eval, std::vector<StringInternPool::StringID> &position_label_ids, StringInternPool::StringID radius_label)
	{
		sbfDataStore = &datastore;
		relevantIndices = &relevant_indices;
		distEvaluator = &r_dist_eval;
		positionLabelIds = &position_label_ids;
		radiusLabelId = radius_label;

		cachedNeighbors.clear();
		cachedNeighbors.resize(sbfDataStore->GetNumInsertedEntities());
	}

	//gets the nearest neighbors to the index and caches them
	//if expand_to_first_nonzero_distance is true, it will expand k so that at least one non-zero distance is returned
	// or return until all entities are included
#ifdef MULTITHREAD_SUPPORT
	void PreCacheAllKnn(size_t top_k, bool expand_to_first_nonzero_distance, bool run_concurrently)
#else
	void PreCacheAllKnn(size_t top_k, bool expand_to_first_nonzero_distance)
#endif
	{

	#ifdef MULTITHREAD_SUPPORT
		if(run_concurrently && relevantIndices->size() > 1)
		{
			auto enqueue_task_lock = Concurrency::threadPool.AcquireTaskLock();
			if(Concurrency::threadPool.AreThreadsAvailable())
			{
				auto task_set = Concurrency::threadPool.CreateCountableTaskSet(relevantIndices->size());
				for(auto index : *relevantIndices)
				{
					//fill in cache entry if it is not sufficient
					if(top_k > cachedNeighbors[index].size())
					{
						Concurrency::threadPool.BatchEnqueueTask(
							[this, index, top_k, expand_to_first_nonzero_distance, &task_set]
							{
								sbfDataStore->FindEntitiesNearestToIndexedEntity(*distEvaluator, *positionLabelIds, index,
									top_k, radiusLabelId, *relevantIndices, expand_to_first_nonzero_distance, cachedNeighbors[index]);
								task_set.MarkTaskCompleted();
							}
						);
					}
				}

				task_set.WaitForTasks(&enqueue_task_lock);
				return;
			}
		}
		//not running concurrently
	#endif

		for(auto index : *relevantIndices)
		{
			//fill in cache entry if it is not sufficient
			if(top_k > cachedNeighbors[index].size())
			{
				cachedNeighbors[index].clear();
				sbfDataStore->FindEntitiesNearestToIndexedEntity(*distEvaluator,
					*positionLabelIds, index, top_k, radiusLabelId, *relevantIndices, expand_to_first_nonzero_distance, cachedNeighbors[index]);
			}
		}
	}

	//returns true if the cached entities nearest to index contain other_index within top_k
	bool DoesCachedKnnContainEntity(size_t index, size_t other_index, size_t top_k)
	{
		for(size_t i = 0; i < top_k && i < cachedNeighbors[index].size(); i++)
		{
			if(cachedNeighbors[index][i].reference == other_index)
				return true;
		}

		return false;
	}

	//gets the top_k nearest neighbor results of entities for the given index, excluding the additional_holdout_index, sets out to the results
	//if expand_to_first_nonzero_distance is true, it will expand k so that at least one non-zero distance is returned
	// or return until all entities are included
	void GetKnn(size_t index, size_t top_k, bool expand_to_first_nonzero_distance,
		std::vector<DistanceReferencePair<size_t>> &out,
		size_t additional_holdout_index = std::numeric_limits<size_t>::max())
	{
		for(auto &neighbor : cachedNeighbors[index])
		{
			if(neighbor.reference == additional_holdout_index)
				continue;

			out.push_back(neighbor);

			//done if have fulfilled top_k and the distance isn't 0
			if(out.size() >= top_k && neighbor.distance != 0.0)
				return;
		}

		//there were not enough results for this search, just do a new search
		out.clear();
		sbfDataStore->FindEntitiesNearestToIndexedEntity(*distEvaluator,
			*positionLabelIds, index, top_k, radiusLabelId, *relevantIndices,
			expand_to_first_nonzero_distance, out, additional_holdout_index);
	}

	//like the other GetKnn, but only considers from_indices
	void GetKnn(size_t index, size_t top_k, bool expand_to_first_nonzero_distance,
		std::vector<DistanceReferencePair<size_t>> &out, BitArrayIntegerSet &from_indices)
	{
		for(auto &neighbor : cachedNeighbors[index])
		{
			if(!from_indices.contains(neighbor.reference))
				continue;

			out.push_back(neighbor);

			if(out.size() >= top_k && neighbor.distance != 0.0)
				return;
		}

		//there were not enough results for this search, just do a new search
		out.clear();
		sbfDataStore->FindEntitiesNearestToIndexedEntity(*distEvaluator,
			*positionLabelIds, index, top_k, radiusLabelId, from_indices, expand_to_first_nonzero_distance, out);
	}

	//returns a pointer to the relevant indices of the cache
	constexpr BitArrayIntegerSet *GetRelevantEntities()
	{
		return relevantIndices;
	}

	//returns the number of relevant indices in the cache
	inline size_t GetNumRelevantEntities()
	{
		return relevantIndices->size();
	}

private:
	//cache of nearest neighbor results.  The index of cache is the entity, and the corresponding vector are its nearest neighbors.
	std::vector<std::vector<DistanceReferencePair<size_t>>> cachedNeighbors;

	//pointer to datastore used to populate cache
	SeparableBoxFilterDataStore *sbfDataStore;

	//distance parameters for the search
	GeneralizedDistanceEvaluator *distEvaluator;

	//position labels
	std::vector<StringInternPool::StringID> *positionLabelIds;

	StringInternPool::StringID radiusLabelId;

	//pointer to the indices of relevant entities used to populate the cache
	BitArrayIntegerSet *relevantIndices;
};
