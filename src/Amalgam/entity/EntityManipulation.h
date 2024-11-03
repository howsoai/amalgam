#pragma once

//project headers:
#include "Entity.h"
#include "EvaluableNodeTreeManipulation.h"
#include "Merger.h"

//Contains various classes and functions to manipulate entities
class EntityManipulation
{
public:
	//functionality to merge two Entities
	class EntitiesMergeMethod : public Merger<Entity *>
	{
	public:
		constexpr EntitiesMergeMethod(Interpreter *_interpreter, bool keep_all_of_both)
			: interpreter(_interpreter), keepAllOfBoth(keep_all_of_both)
		{	}

		virtual MergeMetricResults<Entity *> MergeMetric(Entity *a, Entity *b)
		{
			return NumberOfSharedNodes(a, b);
		}

		virtual Entity *MergeValues(Entity *a, Entity *b, bool must_merge = false);

		virtual bool KeepAllNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableValue()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableAInsteadOfB()
		{	return keepAllOfBoth;	}

		virtual bool KeepNonMergeableA()
		{	return keepAllOfBoth;	}
		virtual bool KeepNonMergeableB()
		{	return keepAllOfBoth;	}

		virtual bool AreMergeable(Entity *a, Entity *b)
		{	return keepAllOfBoth;	}

		Interpreter *interpreter;

	protected:
		bool keepAllOfBoth;
	};

	//functionality to difference two Entities
	// merged entities will *not* contain any code, this is simply for mapping which entities should be merged
	class EntitiesMergeForDifferenceMethod : public EntitiesMergeMethod
	{
	public:
		inline EntitiesMergeForDifferenceMethod(Interpreter *_interpreter)
			: EntitiesMergeMethod(_interpreter, false)
		{	}

		virtual Entity *MergeValues(Entity *a, Entity *b, bool must_merge = false);

		constexpr CompactHashMap<Entity *, Entity *> &GetAEntitiesIncludedFromB()
		{	return aEntitiesIncludedFromB;		}
		constexpr CompactHashMap<Entity *, std::pair<Entity *, bool>> &GetMergedEntitiesIncludedFromB()
		{	return mergedEntitiesIncludedFromB;		}

	protected:
		//key is the entity contained (perhaps deeply) by b
		CompactHashMap<Entity *, Entity *> aEntitiesIncludedFromB;
		//key is the entity contained (perhaps deeply) by b
		// value is a pair, the first being the entity from the merged entity group and the second being a bool as to whether or not the code is identical
		CompactHashMap<Entity *, std::pair<Entity *, bool>> mergedEntitiesIncludedFromB;
	};

	//functionality to mix Entities
	class EntitiesMixMethod : public EntitiesMergeMethod
	{
	public:
		EntitiesMixMethod(Interpreter *_interpreter,
			double fraction_a, double fraction_b, double similar_mix_chance, double fraction_entities_to_mix);

		virtual Entity *MergeValues(Entity *a, Entity *b, bool must_merge);

		virtual bool KeepAllNonMergeableValues()
		{	return false;	}

		virtual bool KeepSomeNonMergeableValues()
		{	return true;	}

		virtual bool KeepNonMergeableValue();
		virtual bool KeepNonMergeableAInsteadOfB();

		virtual bool KeepNonMergeableA();
		virtual bool KeepNonMergeableB();

		virtual bool AreMergeable(Entity *a, Entity *b);

	protected:

		double fractionA;
		double fractionB;
		double fractionAOrB;
		double fractionAInsteadOfB;
		double similarMixChance;
		double fractionEntitiesToMix;
	};

	//Entity merging functions
	static Entity *IntersectEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2);

	static Entity *UnionEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2);

	//returns code that will transform entity1 into entity2, allocated with enm
	static EvaluableNodeReference DifferenceEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2);

	static Entity *MixEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2,
		double fractionA, double fractionB, double similar_mix_chance, double fraction_entities_to_mix);

	//Computes the total number of nodes in both trees that are equal
	static MergeMetricResults<Entity *> NumberOfSharedNodes(Entity *entity1, Entity *entity2);

	//computes the edit distance between the two entities
	static double EditDistance(Entity *entity1, Entity *entity2);

	static Entity *MutateEntity(Interpreter *interpreter, Entity *entity, double mutation_rate,
		CompactHashMap<EvaluableNodeBuiltInStringId, double> *mutation_weights, CompactHashMap<EvaluableNodeType, double> *operation_type);

	//flattens only the top entity using enm to allocate code that can recreate it;
	// this is the first step of flattening an entity, and contained entities can be concatenated
	// if include_rand_seeds is true, it will emit code that includes them; otherwise it won't
	//if ensure_en_flags_correct is false, then it may save compute if an update pass will be done later
	//it will set the top node's cycle check flag to the appropriate value, so if the result contains a cycle
	//that can be determined by the top node
	static EvaluableNode *FlattenOnlyTopEntity(EvaluableNodeManager *enm, Entity *entity,
		bool include_rand_seeds, bool ensure_en_flags_correct);

	//like FlattenOnlyTopEntity, but for an entity contained somewhere in from_entity
	static EvaluableNode *FlattenOnlyOneContainedEntity(EvaluableNodeManager *enm, Entity *entity, Entity *from_entity,
		bool include_rand_seeds, bool ensure_en_flags_correct);

	//flattens entity using enm to allocate code that can recreate it
	// all_contained_entities must be populated via Entity::GetAllDeeplyContainedEntityReadReferencesGroupedByDepth
	// if include_rand_seeds is true, it will emit code that includes them; otherwise it won't
	// if parallel_create is true, it will emit slightly more complex code that creates entities in parallel
	template<typename EntityReferenceType>
	static EvaluableNodeReference FlattenEntity(EvaluableNodeManager *enm, Entity *entity,
		Entity::EntityReferenceBufferReference<EntityReferenceType> &all_contained_entities,
		bool include_rand_seeds, bool parallel_create)
	{
		EvaluableNode *declare_flatten = FlattenOnlyTopEntity(enm, entity,
			include_rand_seeds, false);
		bool cycle_free = !declare_flatten->GetNeedCycleCheck();

		//preallocate the assoc, set_entity_rand_seed, create and set_entity_rand_seed for each contained entity, then the return new_entity
		if(!parallel_create)
			declare_flatten->ReserveOrderedChildNodes(3 + 2 * all_contained_entities->size());

		//where to create new entities into
		EvaluableNode *cur_entity_creation_list = declare_flatten;

		size_t start_index_of_next_group = 0;
		for(size_t i = 0; i < all_contained_entities->size(); i++)
		{
			auto &cur_entity = (*all_contained_entities)[i];
			if(parallel_create && i == start_index_of_next_group)
			{
				//insert another parallel for the this group of entities
				EvaluableNode *parallel_create_node = enm->AllocNode(ENT_PARALLEL);
				parallel_create_node->SetConcurrency(true);

				declare_flatten->AppendOrderedChildNode(parallel_create_node);
				cur_entity_creation_list = parallel_create_node;

				size_t num_contained = cur_entity->GetNumContainedEntities();
				start_index_of_next_group = i + num_contained;
			}

			EvaluableNode *create_entity = FlattenOnlyOneContainedEntity(enm, cur_entity, entity, include_rand_seeds, false);

			cur_entity_creation_list->AppendOrderedChildNode(create_entity);
		}

		//add new_entity to return value of let statement to return the newly created id
		declare_flatten->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));

		//if anything isn't cycle free, then need to recompute everything
		if(!cycle_free)
			EvaluableNodeManager::UpdateFlagsForNodeTree(declare_flatten);

		return EvaluableNodeReference(declare_flatten, true);
	}

	static void SortEntitiesByID(std::vector<Entity *> &entities);

	//converts a set of DistanceReferencePair into the appropriate EvaluableNode structure
	template<typename EntityReference, typename GetEntityFunction>
	static inline EvaluableNodeReference ConvertResultsToEvaluableNodes(
		std::vector<DistanceReferencePair<EntityReference>> &results,
		EvaluableNodeManager *enm, bool as_sorted_list, std::vector<StringInternPool::StringID> &additional_sorted_list_labels,
		GetEntityFunction get_entity)
	{
		if(as_sorted_list)
		{
			//build list of results
			EvaluableNode *query_return = enm->AllocNode(ENT_LIST);
			auto &qr_ocn = query_return->GetOrderedChildNodesReference();
			//returning ids and computed values plus any additional values being retrieved
			qr_ocn.resize(2 + additional_sorted_list_labels.size());

			qr_ocn[0] = CreateListOfStringsIdsFromIteratorAndFunction(results, enm,
				[get_entity](auto &drp) {  return get_entity(drp.reference)->GetIdStringId(); });
			qr_ocn[1] = CreateListOfNumbersFromIteratorAndFunction(results, enm, [](auto drp) { return drp.distance; });

			//if adding on a label, retrieve the values from the entities
			for(size_t label_offset = 0; label_offset < additional_sorted_list_labels.size(); label_offset++)
			{
				auto label = additional_sorted_list_labels[label_offset];

				//make a copy of the value at additionalSortedListLabel for each entity
				EvaluableNode *list_of_values = enm->AllocNode(ENT_LIST);
				qr_ocn[2 + label_offset] = list_of_values;
				auto &list_ocn = list_of_values->GetOrderedChildNodes();
				list_ocn.resize(results.size());
				for(size_t i = 0; i < results.size(); i++)
				{
					Entity *entity = get_entity(results[i].reference);
					list_ocn[i] = entity->GetValueAtLabel(label, enm, false);

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

	//creates an associative lookup of the entities contained by entity from the string id to the entity pointer
	static inline Entity::EntityLookupAssocType CreateContainedEntityLookupByStringId(Entity *entity)
	{
		Entity::EntityLookupAssocType contained_entities_lookup;
		if(entity != nullptr)
		{
			auto &contained_entities = entity->GetContainedEntities();
			contained_entities_lookup.reserve(contained_entities.size());
			for(auto ce : entity->GetContainedEntities())
				contained_entities_lookup.insert(std::make_pair(ce->GetIdStringId(), ce));
		}
		return contained_entities_lookup;
	}

	//adds to merged_entity's contained entities to consist of Entities that are common across all of the Entities specified
	//merged_entity should already have its code merged, as MergeContainedEntities may edit the strings in merged_entity to update
	// new names of merged contained entities
	static void MergeContainedEntities(EntitiesMergeMethod *mm, Entity *entity1, Entity *entity2, Entity *merged_entity);

	//traverses entity and all contained entities and for each of the entities, finds any string that matches a key of
	// entities_renamed and replaces it with the value
	//assumes that entity is not nullptr
	static void RecursivelyRenameAllEntityReferences(Entity *entity, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &entities_renamed);
};
