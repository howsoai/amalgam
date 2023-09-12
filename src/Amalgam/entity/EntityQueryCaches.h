#pragma once

//project headers:
#include "Conviction.h"
#include "Entity.h"
#include "HashMaps.h"
#include "IntegerSet.h"
#include "KnnCache.h"
#include "SeparableBoxFilterDataStore.h"
#include "StringInternPool.h"
#include "WeightedDiscreteRandomStream.h"

//system headers:
#include <algorithm>
#include <memory>
#include <vector>

//stores all of the types of caches needed for queries on a particular entity
class EntityQueryCaches
{
public:

	EntityQueryCaches(Entity *_container) : container(_container)
	{	}

	//adds the entity to the cache
	// container should contain entity
	// entity_index is the index that the entity should be stored as
	inline void AddEntity(Entity *e, size_t entity_index, bool batch_add = false)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		//don't lock if batch_call is set
		Concurrency::WriteLock write_lock(mutex, std::defer_lock);
		if(!batch_add)
			write_lock.lock();
	#endif

		sbfds.AddEntity(e, entity_index);
	}

	//like AddEntity, but removes the entity from the cache and reassigns entity_index_to_reassign to use the old
	// entity_index; for example, if entity_index 3 is being removed and 5 is the highest index, if entity_index_to_reassign is 5,
	// then this function will move the entity data that was previously in index 5 to be referenced by index 3 for all caches
	inline void RemoveEntity(Entity *e, size_t entity_index, size_t entity_index_to_reassign, bool batch_remove = false)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		//don't lock if batch_call is set
		Concurrency::WriteLock write_lock(mutex, std::defer_lock);
		if(!batch_remove)
			write_lock.lock();
	#endif

		sbfds.RemoveEntity(e, entity_index, entity_index_to_reassign);
	}

	//updates all of the label values for entity e with index entity_index
	inline void UpdateAllEntityLabels(Entity *entity, size_t entity_index)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::WriteLock write_lock(mutex);
	#endif

		sbfds.UpdateAllEntityLabels(entity, entity_index);
	}

	//like UpdateAllEntityLabels, but only updates labels for the keys of labels_updated
	inline void UpdateEntityLabels(Entity *entity, size_t entity_index, EvaluableNode::AssocType &labels_updated)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::WriteLock write_lock(mutex);
	#endif

		for(auto &[label_id, _] : labels_updated)
			sbfds.UpdateEntityLabel(entity, entity_index, label_id);
	}

	//like UpdateEntityLabels, but only updates labels for the keys of labels_updated that are not in labels_previous
	// or where the value has changed
	inline void UpdateEntityLabelsAddedOrChanged(Entity *entity, size_t entity_index,
		EvaluableNode::AssocType &labels_previous, EvaluableNode::AssocType &labels_updated)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::WriteLock write_lock(mutex);
	#endif

		for(auto &[label_id, label] : labels_updated)
		{
			auto prev_entry = labels_previous.find(label_id);

			//if not found or different, need to update the label
			if(prev_entry == end(labels_previous) || prev_entry->second != label)
				sbfds.UpdateEntityLabel(entity, entity_index, label_id);
		}
	}

	//like UpdateAllEntityLabels, but only updates labels for label_updated
	inline void UpdateEntityLabel(Entity *entity, size_t entity_index, StringInternPool::StringID label_updated)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::WriteLock write_lock(mutex);
	#endif

		sbfds.UpdateEntityLabel(entity, entity_index, label_updated);
	}

	//specifies that this cache can be used for the input condition
	static bool DoesCachedConditionMatch(EntityQueryCondition *cond, bool last_condition);

	//returns true if the cache already has the label specified
	inline bool DoesHaveLabel(StringInternPool::StringID label_id)
	{
		return sbfds.DoesHaveLabel(label_id);
	}

	//makes sure any labels needed for cond are in the cache
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	void EnsureLabelsAreCached(EntityQueryCondition *cond, Concurrency::ReadLock &lock);
#else
	void EnsureLabelsAreCached(EntityQueryCondition *cond);
#endif

	//returns the set matching_entities of entity ids in the cache that match the provided query condition cond, will fill compute_results with numeric results if KNN query
	//if is_first is true, optimizes to skip unioning results with matching_entities (just overwrites instead).
	void GetMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, std::vector<DistanceReferencePair<size_t>> &compute_results, bool is_first, bool update_matching_entities);

	//like GetMatchingEntities, but returns a string id
	bool ComputeValueFromMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, StringInternPool::StringID &compute_result, bool is_first);

	//like GetMatchingEntities, but returns a flat_hash_map of numbers to numbers
	void ComputeValuesFromMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, FastHashMap<double, double, std::hash<double>, DoubleNanHashComparator> &compute_results, bool is_first);

	//like GetMatchingEntities, but returns a flat_hash_map of string ids to numbers
	//returns true if value was computed, false if not valid
	void ComputeValuesFromMatchingEntities(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, FastHashMap<StringInternPool::StringID, double> &compute_results, bool is_first);

	//like GetMatchingEntities, but returns entity_indices_sampled
	void GetMatchingEntitiesViaSamplingWithReplacement(EntityQueryCondition *cond, BitArrayIntegerSet &matching_entities, std::vector<size_t> &entity_indices_sampled, bool is_first, bool update_matching_entities);

	//the container this is a cache for
	Entity *container;

	SeparableBoxFilterDataStore sbfds;

	//buffers to be reused for less memory churn
	struct QueryCachesBuffers
	{
		//for storting compute results
		std::vector<DistanceReferencePair<size_t>> computeResultsIdToValue;

		//buffer to keep track of which entities are currently matching
		BitArrayIntegerSet currentMatchingEntities;

		//temporary buffer when needed to perform set operations with currentMatchingEntities
		BitArrayIntegerSet tempMatchingEntityIndices;

		//buffer for entity indices
		std::vector<size_t> entityIndices;

		//buffer for sampled entity indices with replacement / duplicates
		std::vector<size_t> entityIndicesWithDuplicates;

		//buffer for doubles
		std::vector<double> doubleVector;

		//buffer for doubles pairs
		std::vector<std::pair<double,double>> pairDoubleVector;

		//nearest neighbors cache
		KnnNonZeroDistanceQuerySBFCache knnCache;

		//for conviction calculations
		ConvictionProcessor<KnnNonZeroDistanceQuerySBFCache, size_t, BitArrayIntegerSet>::ConvictionProcessorBuffers convictionBuffers;
	};

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//mutex for operations that may edit or modify the query cache
	Concurrency::ReadWriteMutex mutex;
#endif

	//for multithreading, there should be one of these per thread
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
		//buffers that can be used for less memory churn (per-thread if multithreaded)
		static QueryCachesBuffers buffers;
};
