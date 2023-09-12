#pragma once

//project headers:
#include "EntityQueryCaches.h"
#include "IntegerSet.h"

//system headers:
#include <memory>

class EntityQueryManager
{
public:

	//searches container for contained entities matching query.
	// if return_query_value is false, then returns a list of all IDs of matching contained entities
	// if return_query_value is true, then returns whatever the appropriate structure is for the query type for the final query
	static EvaluableNodeReference GetEntitiesMatchingQuery(Entity *container, std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm, bool return_query_value);

	//returns the collection of entities (and optionally associated compute values) that satisfy the specified chain of query conditions
	// uses efficient querying methods with a query database, one database per container
	static EvaluableNodeReference GetMatchingEntitiesFromQueryCaches(Entity *container, std::vector<EntityQueryCondition> &conditions, EvaluableNodeManager *enm, bool return_query_value);

	//returns the numeric query cache associated with the specified container, creates one if one does not already exist
	static EntityQueryCaches *GetQueryCachesForContainer(Entity *container);

	//updates when entity contents have changed
	// container should contain entity
	// entity_index is the index that the entity should be stored as
	inline static void UpdateAllEntityLabels(Entity *container, Entity *entity, size_t entity_index)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
			found_cache->second->UpdateAllEntityLabels(entity, entity_index);
	}

	//like UpdateAllEntityLabels, but only updates labels for the keys of labels_updated
	inline static void UpdateEntityLabels(Entity *container, Entity *entity, size_t entity_index,
		EvaluableNode::AssocType &labels_updated)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
			found_cache->second->UpdateEntityLabels(entity, entity_index, labels_updated);
	}

	//like UpdateEntityLabels, but only updates labels for the keys of labels_updated that are not in labels_previous
	// or where the value has changed
	inline static void UpdateEntityLabelsAddedOrChanged(Entity *container, Entity *entity, size_t entity_index,
		EvaluableNode::AssocType &labels_previous, EvaluableNode::AssocType &labels_updated)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
			found_cache->second->UpdateEntityLabelsAddedOrChanged(entity, entity_index, labels_previous, labels_updated);
	}

	//like UpdateAllEntityLabels, but only updates labels for label_updated
	inline static void UpdateEntityLabel(Entity *container, Entity *entity, size_t entity_index,
		StringInternPool::StringID label_updated)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
			found_cache->second->UpdateEntityLabel(entity, entity_index, label_updated);
	}

	//like UpdateEntityLabels, but adds the entity to the cache
	inline static void AddEntity(Entity *container, Entity *entity, size_t entity_index)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::ReadLock lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
			found_cache->second->AddEntity(entity, entity_index);
	}

	//like UpdateEntityLabels, but removes the entity from the cache and reassigns entity_index_to_reassign to use the old
	// entity_index; for example, if entity_index 3 is being removed and 5 is the highest index, if entity_index_to_reassign is 5,
	// then this function will move the entity data that was previously in index 5 to be referenced by index 3 for all caches
	inline static void RemoveEntity(Entity *container, Entity *entity, size_t entity_index, size_t entity_index_to_reassign)
	{
		if(entity == nullptr || container == nullptr)
			return;

	#ifdef MULTITHREAD_SUPPORT
		Concurrency::WriteLock write_lock(queryCacheMutex);
	#endif

		auto found_cache = queryCaches.find(container);
		if(found_cache != end(queryCaches))
		{
			found_cache->second->RemoveEntity(entity, entity_index, entity_index_to_reassign);
			queryCaches.erase(entity);
		}	
	}

	//sorts the entities by their string ids
	inline static void SortEntitiesByID(std::vector<Entity *> &entities)
	{
		//for performance reasons, it may be worth considering other data structures if sort ever becomes or remains significant
		std::sort(begin(entities), end(entities),
			[](Entity *a, Entity *b)
			{
				const std::string a_id = a->GetId();
				const std::string b_id = b->GetId();

				int comp = StringNaturalCompare(a_id, b_id);
				return comp < 0;
			});
	}

	//converts a set of DistanceReferencePair into the appropriate EvaluableNode structure
	template<typename EntityReference, typename GetEntityFunction>
	static inline EvaluableNodeReference ConvertResultsToEvaluableNodes(
		std::vector<DistanceReferencePair<EntityReference>> &results,
		EvaluableNodeManager *enm, bool as_sorted_list, StringInternPool::StringID additional_sorted_list_label,
		GetEntityFunction get_entity)
	{
		if(as_sorted_list)
		{
			//build list of results
			EvaluableNode *query_return = enm->AllocNode(ENT_LIST);
			auto &qr_ocn = query_return->GetOrderedChildNodesReference();
			qr_ocn.resize(additional_sorted_list_label == string_intern_pool.NOT_A_STRING_ID ? 2 : 3);

			qr_ocn[0] = CreateListOfStringsIdsFromIteratorAndFunction(results, enm,
				[get_entity](auto &drp) {  return get_entity(drp.reference)->GetIdStringId(); });
			qr_ocn[1] = CreateListOfNumbersFromIteratorAndFunction(results, enm, [](auto drp) { return drp.distance; });

			//if adding on a label, retrieve the values from the entities
			if(additional_sorted_list_label != string_intern_pool.NOT_A_STRING_ID)
			{
				//make a copy of the value at additionalSortedListLabel for each entity
				EvaluableNode *list_of_values = enm->AllocNode(ENT_LIST);
				qr_ocn[2] = list_of_values;
				auto &list_ocn = list_of_values->GetOrderedChildNodes();
				list_ocn.resize(results.size());
				for(size_t i = 0; i < results.size(); i++)
				{
					Entity *entity = get_entity(results[i].reference);
					list_ocn[i] = entity->GetValueAtLabel(additional_sorted_list_label, enm, false);

					//update cycle checks and idempotency
					if(list_ocn[i] != nullptr)
					{
						if(list_ocn[i]->GetNeedCycleCheck())
							query_return->SetNeedCycleCheck(true);

						if(!list_ocn[i]->GetIsIdempotent())
							query_return->SetIsIdempotent(false);
					}
				}
			}

			return EvaluableNodeReference(query_return, true);
		}
		else //return as assoc
		{
			return CreateAssocOfNumbersFromIteratorAndFunctions(results,
				[get_entity](auto &drp) { return get_entity(drp.reference)->GetIdStringId(); },
				[](auto &drp) { return drp.distance; },
				enm
			);
		}
	}

protected:

#ifdef MULTITHREAD_SUPPORT
	//mutex for operations that may edit or modify the entity's properties and attributes
	static Concurrency::ReadWriteMutex queryCacheMutex;
#endif

	//set of caches for numeric queries
	static FastHashMap<Entity *, std::unique_ptr<EntityQueryCaches>> queryCaches;

	//maximum number of entities which to apply a brute force search (not building up caches, etc.)
	static size_t maxEntitiesBruteForceSearch;
};
