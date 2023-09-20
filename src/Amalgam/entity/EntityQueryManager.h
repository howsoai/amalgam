#pragma once

//project headers:
#include "EntityQueryCaches.h"
#include "IntegerSet.h"

//system headers:
#include <memory>

//TODO 17568: remove this entire class and move remaining methods over to EntityQueries

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
};
