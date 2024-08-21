#pragma once

//project headers:
#include "Entity.h"
#include "EntityTreeFunctions.h"
#include "Merger.h"

//forward declarations:
class EvaluableNodeContext;

//Contains various classes and functions to manipulate entities
class EntityManipulation
{
public:
	//functionality to merge two Entities
	class EntitiesMergeMethod : public Merger<Entity *>
	{
	public:
		constexpr EntitiesMergeMethod(EvaluableNodeContext *_context, bool keep_all_of_both)
			: context(_context), keepAllOfBoth(keep_all_of_both)
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

		EvaluableNodeContext *context;

	protected:
		bool keepAllOfBoth;
	};

	//functionality to difference two Entities
	// merged entities will *not* contain any code, this is simply for mapping which entities should be merged
	class EntitiesMergeForDifferenceMethod : public EntitiesMergeMethod
	{
	public:
		inline EntitiesMergeForDifferenceMethod(EvaluableNodeContext *_context)
			: EntitiesMergeMethod(_context, false)
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
		EntitiesMixMethod(EvaluableNodeContext *_context,
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
	static Entity *IntersectEntities(EvaluableNodeContext *context, Entity *entity1, Entity *entity2);

	static Entity *UnionEntities(EvaluableNodeContext *context, Entity *entity1, Entity *entity2);

	//returns code that will transform entity1 into entity2, allocated with enm
	static EvaluableNodeReference DifferenceEntities(EvaluableNodeContext *context, Entity *entity1, Entity *entity2);

	static Entity *MixEntities(EvaluableNodeContext *context, Entity *entity1, Entity *entity2,
		double fractionA, double fractionB, double similar_mix_chance, double fraction_entities_to_mix);

	//Computes the total number of nodes in both trees that are equal
	static MergeMetricResults<Entity *> NumberOfSharedNodes(Entity *entity1, Entity *entity2);

	//computes the edit distance between the two entities
	static double EditDistance(Entity *entity1, Entity *entity2);

	static Entity *MutateEntity(EvaluableNodeContext *context, Entity *entity, double mutation_rate,
		CompactHashMap<EvaluableNodeBuiltInStringId, double> *mutation_weights, CompactHashMap<EvaluableNodeType, double> *operation_type);

	//flattens entity using enm to allocate code that can recreate it
	// all_contained_entities must be populated via Entity::GetAllDeeplyContainedEntityReadReferencesGroupedByDepth
	// if include_rand_seeds is true, it will emit code that includes them; otherwise it won't
	// if parallel_create is true, it will emit slightly more complex code that creates entities in parallel
	template<typename EntityReferenceType>
	static EvaluableNodeReference FlattenEntity(EvaluableNodeManager *enm, Entity *entity,
		Entity::EntityReferenceBufferReference<EntityReferenceType> &all_contained_entities,
		bool include_rand_seeds, bool parallel_create)
	{
		//////////
		//build code to look like:
		// (declare (assoc new_entity (null) create_new_entity (true))
		//   (let (assoc _ (lambda *entity code*))
		//     (if create_new_entity
		//       (assign "new_entity" (first
		//         (create_entities new_entity _)
		//       ))
		//       (assign_entity_roots new_entity _)
		//     )
		//   )
		//
		//   [if include_rand_seeds]
		//   (set_entity_rand_seed
		//          new_entity
		//          *rand seed string* )
		//
		//   [for each contained entity specified by the list representing the relative location to new_entity]
		//   [if parallel_create, will group these in ||(parallel ...) by container entity
		//
		//   [if include_rand_seeds]
		//   (set_entity_rand_seed
		//       (first
		//   [always]
		//           (create_entities
		//                (append new_entity *relative id*)
		//                (lambda *entity code*) )         
		//                (append new_entity *relative id*)
		//                *rand seed string* )
		//   [if include_rand_seeds]
		//       )
		//       *rand seed string* )
		//   )
		// )

		bool cycle_free = true;

		// (declare (assoc new_entity (null) create_new_entity (true))
		EvaluableNode *declare_flatten = enm->AllocNode(ENT_DECLARE);
		//preallocate the assoc, set_entity_rand_seed, create and set_entity_rand_seed for each contained entity, then the return new_entity
		declare_flatten->ReserveOrderedChildNodes(3 + 2 * all_contained_entities->size());

		EvaluableNode *flatten_params = enm->AllocNode(ENT_ASSOC);
		declare_flatten->AppendOrderedChildNode(flatten_params);
		flatten_params->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_new_entity), nullptr);
		flatten_params->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_create_new_entity), enm->AllocNode(ENT_TRUE));

		//   (let (assoc _ (lambda *entity code*))
		EvaluableNode *let_entity_code = enm->AllocNode(ENT_LET);
		declare_flatten->AppendOrderedChildNode(let_entity_code);
		EvaluableNode *let_assoc = enm->AllocNode(ENT_ASSOC);
		let_entity_code->AppendOrderedChildNode(let_assoc);

		EvaluableNode *lambda_for_create_root = enm->AllocNode(ENT_LAMBDA);
		let_assoc->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI__), lambda_for_create_root);

		EvaluableNodeReference root_copy = entity->GetRoot(enm, EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT);
		lambda_for_create_root->AppendOrderedChildNode(root_copy);
		if(root_copy.GetNeedCycleCheck())
			cycle_free = false;

		//   (if create_new_entity
		EvaluableNode *if_create_new = enm->AllocNode(ENT_IF);
		let_entity_code->AppendOrderedChildNode(if_create_new);
		if_create_new->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_create_new_entity)));

		//     (assign "new_entity" (first
		//       (create_entities new_entity _)
		//     ))
		EvaluableNode *assign_new_entity_from_create = enm->AllocNode(ENT_ASSIGN);
		if_create_new->AppendOrderedChildNode(assign_new_entity_from_create);
		assign_new_entity_from_create->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));
		EvaluableNode *create_root_entity = enm->AllocNode(ENT_CREATE_ENTITIES);
		create_root_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));
		create_root_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI__)));
		EvaluableNode *first_of_create_entity = enm->AllocNode(ENT_FIRST);
		first_of_create_entity->AppendOrderedChildNode(create_root_entity);
		assign_new_entity_from_create->AppendOrderedChildNode(first_of_create_entity);

		//     (assign_entity_roots new_entity _)
		EvaluableNode *assign_new_entity_into_current = enm->AllocNode(ENT_ASSIGN_ENTITY_ROOTS);
		if_create_new->AppendOrderedChildNode(assign_new_entity_into_current);
		assign_new_entity_into_current->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));
		assign_new_entity_into_current->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI__)));

		if(include_rand_seeds)
		{
			//   (set_entity_rand_seed
			//        new_entity
			//        *rand seed string* )
			EvaluableNode *set_rand_seed_root = enm->AllocNode(ENT_SET_ENTITY_RAND_SEED);
			set_rand_seed_root->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));
			set_rand_seed_root->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, entity->GetRandomState()));

			declare_flatten->AppendOrderedChildNode(set_rand_seed_root);
		}

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

			//   (create_entities
			//        (append new_entity *relative id*)
			//        (lambda *entity code*)
			//   )
			EvaluableNode *create_entity = enm->AllocNode(ENT_CREATE_ENTITIES);

			EvaluableNode *src_id_list = GetTraversalIDPathFromAToB(enm, entity, cur_entity);
			EvaluableNode *src_append = enm->AllocNode(ENT_APPEND);
			src_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, GetStringIdFromBuiltInStringId(ENBISI_new_entity)));
			src_append->AppendOrderedChildNode(src_id_list);
			create_entity->AppendOrderedChildNode(src_append);

			EvaluableNode *lambda_for_create = enm->AllocNode(ENT_LAMBDA);
			create_entity->AppendOrderedChildNode(lambda_for_create);

			EvaluableNodeReference contained_root_copy = cur_entity->GetRoot(enm, EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT);
			lambda_for_create->AppendOrderedChildNode(contained_root_copy);
			if(contained_root_copy.GetNeedCycleCheck())
				cycle_free = false;

			if(include_rand_seeds)
			{
				//   (set_entity_rand_seed
				//        (first ...create_entity... )
				//        *rand seed string* )
				EvaluableNode *set_rand_seed = enm->AllocNode(ENT_SET_ENTITY_RAND_SEED);
				EvaluableNode *first = enm->AllocNode(ENT_FIRST);
				set_rand_seed->AppendOrderedChildNode(first);
				first->AppendOrderedChildNode(create_entity);
				set_rand_seed->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, cur_entity->GetRandomState()));

				//replace the old create_entity with the one surrounded by setting rand seed
				create_entity = set_rand_seed;
			}

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
