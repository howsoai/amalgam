//project headers:
#include "Entity.h"
#include "EntityManipulation.h"
#include "EvaluableNodeTreeDifference.h"
#include "EvaluableNodeTreeFunctions.h"
#include "EvaluableNodeTreeManipulation.h"
#include "Interpreter.h"
#include "Merger.h"

Entity *EntityManipulation::EntitiesMergeMethod::MergeValues(Entity *a, Entity *b, bool must_merge)
{
	if(a == nullptr && b == nullptr)
		return nullptr;

	//create new entity
	Entity *merged_entity = new Entity();
	if(a != nullptr)
		merged_entity->SetRandomStream(a->GetRandomStream());
	else if(b != nullptr)
		merged_entity->SetRandomStream(b->GetRandomStream());

	//merge entitys' code
	EvaluableNodeReference code_a = (a != nullptr ? a->GetRoot() : EvaluableNodeReference::Null());
	EvaluableNodeReference code_b = (b != nullptr ? b->GetRoot() : EvaluableNodeReference::Null());

	EvaluableNodeTreeManipulation::NodesMergeMethod mm(&merged_entity->evaluableNodeManager, keepAllOfBoth, true);
	EvaluableNode *result = mm.MergeValues(code_a, code_b);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	merged_entity->SetRoot(result, true);

	MergeContainedEntities(this, a, b, merged_entity);

	return merged_entity;
}

//////////////////////////////

Entity *EntityManipulation::EntitiesMergeForDifferenceMethod::MergeValues(Entity *a, Entity *b, bool must_merge)
{
	if(a == nullptr && b == nullptr)
		return nullptr;

	//create new entity
	Entity *result = new Entity();

	//compare entitys' code
	EvaluableNodeReference code_a = (a != nullptr ? a->GetRoot() : EvaluableNodeReference::Null());
	EvaluableNodeReference code_b = (b != nullptr ? b->GetRoot() : EvaluableNodeReference::Null());

	if(a != nullptr)
		aEntitiesIncludedFromB[b] = a;
	if(b != nullptr)
	{
		bool identical_code = EvaluableNode::AreDeepEqual(code_a, code_b);
		mergedEntitiesIncludedFromB[b] = std::pair<Entity *, bool>(result, identical_code);
	}

	MergeContainedEntities(this, a, b, result);

	return result;
}

//////////////////////////////

EntityManipulation::EntitiesMixMethod::EntitiesMixMethod(Interpreter *_interpreter,
	double fraction_a, double fraction_b, double similar_mix_chance, double fraction_entities_to_mix)
	: EntitiesMergeMethod(_interpreter, true)
{
	interpreter = _interpreter;

	//clamp each to the appropriate range, 0 to 1 for fractions, -1 to 1 for similarMixChance
	if(FastIsNaN(fraction_a))
		fractionA = 0.0;
	else
		fractionA = std::min(1.0, std::max(0.0, fraction_a));

	if(FastIsNaN(fraction_b))
		fractionB = 0.0;
	else
		fractionB = std::min(1.0, std::max(0.0, fraction_b));

	fractionAOrB = fractionA + fractionB - fractionA * fractionB;
	fractionAInsteadOfB = fractionA / (fractionA + fractionB);

	if(FastIsNaN(similar_mix_chance))
		similarMixChance = 0.0;
	else
		similarMixChance = std::min(1.0, std::max(-1.0, similar_mix_chance));

	if(FastIsNaN(fraction_entities_to_mix))
		fractionEntitiesToMix = 0.0;
	else
		fractionEntitiesToMix = std::min(1.0, std::max(0.0, fraction_entities_to_mix));
}

Entity *EntityManipulation::EntitiesMixMethod::MergeValues(Entity *a, Entity *b, bool must_merge)
{
	if(a == nullptr && b == nullptr)
		return nullptr;

	//if the entities aren't required to be merged, then see if they're mergeable
	// if so, then merge
	// if not, then pick one or none
	if(!must_merge)
	{
		if(!AreMergeable(a, b))
		{
			if(KeepNonMergeableValue())
			{
				if(KeepNonMergeableAInsteadOfB() || b == nullptr)
					return new Entity(a);
				else
					return new Entity(b);
			}
			
			return nullptr;
		}
	}

	//create new entity to merge into
	Entity *merged_entity = new Entity();
	if(a != nullptr)
		merged_entity->SetRandomStream(a->GetRandomStream());
	else if(b != nullptr)
		merged_entity->SetRandomStream(b->GetRandomStream());

	//merge entity's code
	EvaluableNodeReference code_a = (a != nullptr ? a->GetRoot() : EvaluableNodeReference::Null());
	EvaluableNodeReference code_b = (b != nullptr ? b->GetRoot() : EvaluableNodeReference::Null());

	EvaluableNodeTreeManipulation::NodesMixMethod mm(interpreter->randomStream.CreateOtherStreamViaRand(),
		&merged_entity->evaluableNodeManager, fractionA, fractionB, similarMixChance);

	EvaluableNode *result = mm.MergeValues(code_a, code_b);
	EvaluableNodeManager::UpdateFlagsForNodeTree(result);
	merged_entity->SetRoot(result, true);

	MergeContainedEntities(this, a, b, merged_entity);
	return merged_entity;
}

bool EntityManipulation::EntitiesMixMethod::KeepNonMergeableValue()
{
	return interpreter->randomStream.Rand() < fractionAOrB;
}

bool EntityManipulation::EntitiesMixMethod::KeepNonMergeableAInsteadOfB()
{
	return interpreter->randomStream.Rand() < fractionAInsteadOfB;
}

bool EntityManipulation::EntitiesMixMethod::KeepNonMergeableA()
{
	return interpreter->randomStream.Rand() < fractionA;
}
bool EntityManipulation::EntitiesMixMethod::KeepNonMergeableB()
{
	return interpreter->randomStream.Rand() < fractionB;
}

bool EntityManipulation::EntitiesMixMethod::AreMergeable(Entity *a, Entity *b)
{
	return interpreter->randomStream.Rand() < fractionEntitiesToMix;
}

Entity *EntityManipulation::IntersectEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2)
{
	EntitiesMergeMethod mm(interpreter, false);
	return mm.MergeValues(entity1, entity2);
}

Entity *EntityManipulation::UnionEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2)
{
	EntitiesMergeMethod mm(interpreter, true);
	return mm.MergeValues(entity1, entity2);
}

//returns true if root_entity can be deep copied because all contained entities (recursively)
// are identical to those matched in entities_included
// regardless, it will accumulate contained entities examined into top_entities_identical if they
// can be deep copied and different_entities otherwise
bool IsEntityIdenticalToComparedEntity(Entity *root_entity,
	CompactHashMap<Entity *, std::pair<Entity *, bool>> &entities_included,
	std::vector<Entity *> &top_entities_identical, std::vector<Entity *> &different_entities)
{
	if(root_entity == nullptr)
		return true;

	//if not included, then don't mark this entity for copying at all
	auto paired_entity = entities_included.find(root_entity);
	if(paired_entity == end(entities_included) || paired_entity->second.first == nullptr)
		return false;

	//iterate over all contained entries and recursively check if they are identical, if so, record in a list
	std::vector<Entity *> contained_nodes_identical;
	bool all_contained_entities_identical = true;
	for(auto entity : root_entity->GetContainedEntities())
	{
		if(IsEntityIdenticalToComparedEntity(entity, entities_included, top_entities_identical, different_entities))
			contained_nodes_identical.emplace_back(entity);
		else
		{
			all_contained_entities_identical = false;
			different_entities.emplace_back(entity);
		}
	}

	//if the root_entity matches its pair, then can deep copy
	if(paired_entity->second.second && all_contained_entities_identical)
		return true;
	else //something doesn't match, only copy those that are identical, different_entities will contain those entities
	{
		for(auto &ce : contained_nodes_identical)
			top_entities_identical.emplace_back(ce);
		return false;
	}
}

EvaluableNodeReference EntityManipulation::DifferenceEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2)
{
	//find commonality
	EntitiesMergeForDifferenceMethod mm(interpreter);
	Entity *root_merged = mm.MergeValues(entity1, entity2, true);
	auto &entity2_to_entity_a = mm.GetAEntitiesIncludedFromB();
	auto &entity2_to_merged_entity = mm.GetMergedEntitiesIncludedFromB();

	EvaluableNodeManager *enm = interpreter->evaluableNodeManager;

	//////////
	//build code to look like:
	// (declare (assoc _ (null) new_entity (null)) 
	//  (assign "new_entity"  (first (create_entities new_entity
	//                         (call (lambda *entity difference code*)
	//                           (assoc _ (get_entity_code _) )
	//                    ) ) )
	//
	//   [for each contained entity specified by the list representing the relative location to _ and new_entity]
	//
	//   [if must be deleted, ignore]
	//
	//    [if must be merged]
	//    (create_entity
	//         (append new_entity *relative id*)
	//         (call *entity difference code*
	//           (assoc _ (get_entity_code (append _ *relative id*) ) )
	//    )
	//
	//    [if must be created]
	//    (clone_entity
	//      (append _ *relative id*)
	//      (append new_entity *relative id*)
	//    )
	//
	//    new_entity
	//  )
	// )

	//create: (declare (assoc new_entity (null) create_new_entity (null)) )
	EvaluableNode *difference_function = enm->AllocNode(ENT_DECLARE);

	auto node_stack = interpreter->CreateInterpreterNodeStackStateSaver(difference_function);

	EvaluableNode *df_assoc = enm->AllocNode(ENT_ASSOC);
	difference_function->AppendOrderedChildNode(df_assoc);
	df_assoc->SetMappedChildNode(ENBISI__, nullptr);
	df_assoc->SetMappedChildNode(ENBISI_new_entity, nullptr);

	//find entities that match up, and if no difference, then can shortcut the function
	std::vector<Entity *> top_entities_identical;
	std::vector<Entity *> different_entities;
	if(IsEntityIdenticalToComparedEntity(entity2, entity2_to_merged_entity, top_entities_identical, different_entities))
	{
		EvaluableNode *clone_entity = enm->AllocNode(ENT_CLONE_ENTITIES);
		difference_function->AppendOrderedChildNode(clone_entity);
		clone_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));
		clone_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
		delete root_merged;
		return EvaluableNodeReference(difference_function, true);
	}

	//create the following:
	//  (assign "new_entity" (first (create_entities new_entity
	EvaluableNode *assign_new_entity = enm->AllocNode(ENT_ASSIGN);
	difference_function->AppendOrderedChildNode(assign_new_entity);
	assign_new_entity->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, ENBISI_new_entity));
	EvaluableNode *create_root_entity = enm->AllocNode(ENT_CREATE_ENTITIES);
	create_root_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
	EvaluableNode *first_of_create_entity = enm->AllocNode(ENT_FIRST);
	first_of_create_entity->AppendOrderedChildNode(create_root_entity);
	assign_new_entity->AppendOrderedChildNode(first_of_create_entity);

	//apply difference in code from source to build:
	//  (assign "new_entity"  (first (create_entities new_entity
	//                         (call (lambda *entity difference code*)
	//                           (assoc _ (get_entity_code _) )
	//                    ) ) )
	EvaluableNode *entity_difference_apply_call = enm->AllocNode(ENT_CALL);
	create_root_entity->AppendOrderedChildNode(entity_difference_apply_call);
	EvaluableNode *lambda_for_difference = enm->AllocNode(ENT_LAMBDA);
	entity_difference_apply_call->AppendOrderedChildNode(lambda_for_difference);
	EvaluableNode *edac_assoc = enm->AllocNode(ENT_ASSOC);
	entity_difference_apply_call->AppendOrderedChildNode(edac_assoc);
	EvaluableNode *get_entity_code = enm->AllocNode(ENT_RETRIEVE_ENTITY_ROOT);
	edac_assoc->SetMappedChildNode(ENBISI__, get_entity_code);
	get_entity_code->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));

	//apply difference function for root entities
	// make sure to make a copy of each root so don't end up with mixed entity nodes
	lambda_for_difference->AppendOrderedChildNode(EvaluableNodeTreeDifference::DifferenceTrees(enm,
		entity1->GetRoot(enm), entity2->GetRoot(enm)));

	//can ensure cycle free only if all different entities are cycle free
	// it doesn't matter if identical entities are cycle free because they're just cloned -- the code doesn't show up in the difference
	bool cycle_free = true;
	for(auto &entity_to_create : different_entities)
	{
		//create the following code:
		//    (create_entities
		//         (append new_entity *relative id*)
		//         (call *entity difference code*
		//           (assoc _ (get_entity_code (append _ *relative id*)) )
		//    )
		EvaluableNode *src_id_list = GetTraversalIDPathFromAToB(enm, entity2, entity_to_create);
		EvaluableNode *src_append = enm->AllocNode(ENT_APPEND);
		src_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
		src_append->AppendOrderedChildNode(src_id_list);

		EvaluableNode *dest_id_list = enm->DeepAllocCopy(src_id_list);
		EvaluableNode *dest_append = enm->AllocNode(ENT_APPEND);
		dest_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));
		dest_append->AppendOrderedChildNode(dest_id_list);

		EvaluableNode *create_entity = enm->AllocNode(ENT_CREATE_ENTITIES);
		difference_function->AppendOrderedChildNode(create_entity);
		create_entity->AppendOrderedChildNode(dest_append);

		//if identical to merged, then just copy
		auto merged = entity2_to_merged_entity.find(entity_to_create);
		if(merged == end(entity2_to_merged_entity) || merged->second.second == true)
		{
			EvaluableNode *copy_lambda = enm->AllocNode(ENT_LAMBDA);
			create_entity->AppendOrderedChildNode(copy_lambda);
			copy_lambda->AppendOrderedChildNode(enm->DeepAllocCopy(entity_to_create->GetRoot(), EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT));
		}
		else //need to difference
		{
			EvaluableNode *call_diff = enm->AllocNode(ENT_CALL);
			create_entity->AppendOrderedChildNode(call_diff);
			EvaluableNode *call_lambda = enm->AllocNode(ENT_LAMBDA);
			call_diff->AppendOrderedChildNode(call_lambda);

			//look up corresponding entity from a, then grab its code
			// make sure to make a copy of each root so don't end up with mixed entity nodes
			auto entity_from_a = entity2_to_entity_a.find(entity_to_create);
			EvaluableNode *a_code = nullptr;
			if(entity_from_a != end(entity2_to_entity_a) && entity_from_a->second != nullptr)
				a_code = entity_from_a->second->GetRoot(enm);

			EvaluableNode *b_code = entity_to_create->GetRoot(enm);

			//if either entity needs a cycle check, then everything will need to be checked for cycles later
			if( (a_code != nullptr && a_code->GetNeedCycleCheck())
					|| (b_code != nullptr && b_code->GetNeedCycleCheck()) )
				cycle_free = false;

			EvaluableNode *entity_difference = EvaluableNodeTreeDifference::DifferenceTrees(enm, a_code, b_code);
			call_lambda->AppendOrderedChildNode(entity_difference);

			EvaluableNode *call_assoc = enm->AllocNode(ENT_ASSOC);
			call_diff->AppendOrderedChildNode(call_assoc);

			EvaluableNode *entity_code = enm->AllocNode(ENT_RETRIEVE_ENTITY_ROOT);
			call_assoc->SetMappedChildNode(ENBISI__, entity_code);
			entity_code->AppendOrderedChildNode(src_append);
		}
	}

	//clone any identical parts. since they are effectively leaf nodes they can be all created at the end
	for(auto &entity_to_clone : top_entities_identical)
	{
		//create the following code:
		//    (clone_entities
		//      (append _ *relative id*)
		//      (append new_entity *relative id*)
		//    )
		EvaluableNode *clone_entity = enm->AllocNode(ENT_CLONE_ENTITIES);
		difference_function->AppendOrderedChildNode(clone_entity);

		EvaluableNode *src_id_list = GetTraversalIDPathFromAToB(enm, entity2, entity_to_clone);
		EvaluableNode *src_append = enm->AllocNode(ENT_APPEND);
		src_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));
		src_append->AppendOrderedChildNode(src_id_list);

		EvaluableNode *dest_id_list = enm->DeepAllocCopy(src_id_list);
		EvaluableNode *dest_append = enm->AllocNode(ENT_APPEND);
		dest_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
		dest_append->AppendOrderedChildNode(dest_id_list);

		clone_entity->AppendOrderedChildNode(src_append);
		clone_entity->AppendOrderedChildNode(dest_append);
	}

	//add new_entity to return value of let statement to return the newly created id
	difference_function->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));

	delete root_merged;

	//if anything isn't cycle free, then need to recompute everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(difference_function);

	return EvaluableNodeReference(difference_function, true);
}

Entity *EntityManipulation::MixEntities(Interpreter *interpreter, Entity *entity1, Entity *entity2,
	double fractionA, double fractionB, double similar_mix_chance, double fraction_entities_to_mix)
{
	EntitiesMixMethod mm(interpreter, fractionA, fractionB, similar_mix_chance, fraction_entities_to_mix);
	return mm.MergeValues(entity1, entity2, true);
}

MergeMetricResults<Entity *> EntityManipulation::NumberOfSharedNodes(Entity *entity1, Entity *entity2)
{
	if(entity1 == nullptr || entity2 == nullptr)
		return MergeMetricResults(0.0, entity1, entity2, false, false);

	MergeMetricResults commonality(0.0, entity1, entity2);
	commonality += EvaluableNodeTreeManipulation::NumberOfSharedNodes(entity1->GetRoot(), entity2->GetRoot());

	Entity::EntityLookupAssocType entity1_unmatched = CreateContainedEntityLookupByStringId(entity1);
	Entity::EntityLookupAssocType entity2_unmatched = CreateContainedEntityLookupByStringId(entity2);

	//find all contained entities that have the same name
	std::vector<StringInternPool::StringID> matching_entities(entity1_unmatched.size());	//reserve enough in one block for all in entity1, as an upper bound
	for(auto &[e1c_id, _] : entity1_unmatched)
	{
		if(entity2_unmatched.find(e1c_id) != end(entity2_unmatched))
			matching_entities.emplace_back(e1c_id);
	}

	//count up all shared entities and remove from unmatched maps
	for(auto &entity_name : matching_entities)
	{
		commonality += NumberOfSharedNodes(entity1_unmatched[entity_name], entity2_unmatched[entity_name]);
		
		entity1_unmatched.erase(entity_name);
		entity2_unmatched.erase(entity_name);
	}

	//pair up all remaining contained entities that don't have matching names
	for(auto &[e1c_id, e1c] : entity1_unmatched)
	{
		//find the node that best matches this one, greedily
		bool best_match_found = false;
		StringInternPool::StringID best_match_key = StringInternPool::NOT_A_STRING_ID;
		MergeMetricResults<Entity *> best_match_value;
		for(auto& [e2c_id, e2c] : entity2_unmatched)
		{
			auto match_value = NumberOfSharedNodes(e1c, e2c);
			//entities won't necessarily must-match even if the labels are the same; those are the matching_entities by name covered above
			match_value.mustMatch = false;

			if(match_value.IsNontrivialMatch()
				&& (!best_match_found || match_value > best_match_value) )
			{
				best_match_found = true;
				best_match_value = match_value;
				best_match_key = e2c_id;

				//don't need to check any more
				if(match_value.mustMatch)
					break;
			}
		}

		//if found a match, then remove it from the match list and put it in the list
		if(best_match_found)
		{
			//count this for whatever match it is
			commonality += best_match_value;

			entity2_unmatched.erase(best_match_key);
		}

	}

	return commonality;
}

double EntityManipulation::EditDistance(Entity *entity1, Entity *entity2)
{
	auto shared_nodes = NumberOfSharedNodes(entity1, entity2);

	double entity_1_size = 0;
	if(entity1 != nullptr)
		entity_1_size = static_cast<double>(entity1->GetDeepSizeInNodes());
	double entity_2_size = 0;
	if(entity2 != nullptr)
		entity_2_size = static_cast<double>(entity2->GetDeepSizeInNodes());

	//find the distance to edit from tree1 to shared, then from shared to tree_2.  Shared is the smallest, so subtract from each.
	return (entity_1_size - shared_nodes.commonality) + (entity_2_size - shared_nodes.commonality);
}

void EntityManipulation::MergeContainedEntities(EntitiesMergeMethod *mm, Entity *entity1, Entity *entity2, Entity *merged_entity)
{
	//shortcut for merging empty entities
	if(entity1 == nullptr && entity2 == nullptr)
		return;

	//shortcut for when requiring intersection of entities
	if(!mm->KeepSomeNonMergeableValues() && (entity1 == nullptr || entity2 == nullptr))
		return;

	//any entity that is renamed that may have references is stored here
	CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> entities_renamed;
	
	//keep track of contained entities to merge
	Entity::EntityLookupAssocType entity1_unmatched = CreateContainedEntityLookupByStringId(entity1);
	Entity::EntityLookupAssocType entity2_unmatched = CreateContainedEntityLookupByStringId(entity2);

	//find all contained entities that have the same id
	std::vector<StringInternPool::StringID> matching_entities;
	matching_entities.reserve(entity1_unmatched.size());	//reserve enough in one block for all in entity1 to reduce potential reallocations
	for(auto &[_, e1c] : entity1_unmatched)
	{
		StringInternPool::StringID e1c_id = e1c->GetIdStringId();
		if(entity2_unmatched.find(e1c_id) != end(entity2_unmatched))
			matching_entities.emplace_back(e1c_id);
	}

	//merge all shared entities and remove from unmatched contained entities
	for(auto &entity_name : matching_entities)
	{
		merged_entity->AddContainedEntity(mm->MergeValues(entity1_unmatched[entity_name], entity2_unmatched[entity_name], true), entity_name);
		entity1_unmatched.erase(entity_name);
		entity2_unmatched.erase(entity_name);
	}

	//entityX_unmatched only contain entries that do not have matching names
	//If mm->KeepAllNonMergeableValues(), then merge named entities against nulls
	// Regardless, keep the rest to match up as best as possible
	Entity::EntityLookupAssocType entity1_unmatched_unnamed;
	Entity::EntityLookupAssocType entity2_unmatched_unnamed;

	for(auto &e : entity1_unmatched)
	{
		if(Entity::IsNamedEntity(e.first))
		{
			Entity *merged = mm->MergeValues(e.second, nullptr, true);
			if(merged != nullptr)
				merged_entity->AddContainedEntity(merged, e.first);
		}
		else
			entity1_unmatched_unnamed.insert(e);
	}

	for(auto &e : entity2_unmatched)
	{
		if(Entity::IsNamedEntity(e.first))
		{
			Entity *merged = mm->MergeValues(nullptr, e.second, true);
			if(merged != nullptr)
				merged_entity->AddContainedEntity(merged, e.first);
		}
		else
			entity2_unmatched_unnamed.insert(e);
	}


	//merge any remaining entities that didn't have anything to merge with
	for(auto &[e1_current_id, e1_current] : entity1_unmatched_unnamed)
	{
		//find the entity that best matches this one, greedily
		bool best_match_found = false;
		StringInternPool::StringID best_match_key = StringInternPool::NOT_A_STRING_ID;
		MergeMetricResults<Entity *> best_match_value;
		for(auto &[e2_current_id, e2_current] : entity2_unmatched_unnamed)
		{
			auto match_value = NumberOfSharedNodes(e1_current, e2_current);

			if(match_value.IsNontrivialMatch()
				&& (!best_match_found || match_value > best_match_value) )
			{
				best_match_found = true;
				best_match_value = match_value;
				best_match_key = e2_current_id;

				//have already merged all values that match by name, so if this is an exact match so count it
				// to reduce the number of total of comparisons needed
				if(best_match_value.exactMatch)
					break;
			}
		}

		//if found a match, then remove it from the match list and put it in the list
		if(best_match_found)
		{
			Entity *merged = mm->MergeValues(e1_current, entity2_unmatched_unnamed[best_match_key], best_match_value.exactMatch);
			//only count if it worked
			if(merged != nullptr)
			{
				merged_entity->AddContainedEntity(merged, e1_current_id);	//add using id of first to attempt to preserve any references
				entities_renamed[best_match_key] = e1_current_id;	//remember the replacement

				//merged, so remove from potential merge list
				entity2_unmatched_unnamed.erase(best_match_key);
			}
		}
		else //nothing found, merge versus nullptr
		{
			Entity *merged = mm->MergeValues(e1_current, nullptr, false);
			if(merged != nullptr)
				merged_entity->AddContainedEntity(merged, e1_current_id);
		}
	}

	if(mm->KeepAllNonMergeableValues())
	{
		//merge anything remaining from entity2_unmatched_unnamed versus nullptr
		for(auto &[e_id, e] : entity2_unmatched_unnamed)
		{
			Entity *merged = mm->MergeValues(nullptr, e, false);
			if(merged != nullptr)
				merged_entity->AddContainedEntity(merged, e_id);
		}
	}

	if(entities_renamed.size() > 0)
		RecursivelyRenameAllEntityReferences(merged_entity, entities_renamed);
}

Entity *EntityManipulation::MutateEntity(Interpreter *interpreter, Entity *entity, double mutation_rate, CompactHashMap<StringInternPool::StringID, double> *mutation_weights, CompactHashMap<EvaluableNodeType, double> *operation_type)
{
	if(entity == nullptr)
		return nullptr;

	//make a new entity with mutated code
	Entity *new_entity = new Entity();
	EvaluableNode *mutated_code = EvaluableNodeTreeManipulation::MutateTree(interpreter, &new_entity->evaluableNodeManager, entity->GetRoot(), mutation_rate, mutation_weights, operation_type);
	EvaluableNodeManager::UpdateFlagsForNodeTree(mutated_code);
	new_entity->SetRoot(mutated_code, true);
	new_entity->SetRandomStream(entity->GetRandomStream());

	//make mutated copies of all contained entities
	for(auto e : entity->GetContainedEntities())
		new_entity->AddContainedEntity(MutateEntity(interpreter, e, mutation_rate, mutation_weights, operation_type), entity->GetIdStringId());

	return new_entity;
}

EvaluableNodeReference EntityManipulation::FlattenEntity(EvaluableNodeManager *enm,
	Entity *entity, Entity::EntityReferenceBufferReference<EntityReadReference> &all_contained_entities,
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
	flatten_params->SetMappedChildNode(ENBISI_new_entity, nullptr);
	flatten_params->SetMappedChildNode(ENBISI_create_new_entity, enm->AllocNode(ENT_TRUE));

	//   (let (assoc _ (lambda *entity code*))
	EvaluableNode *let_entity_code = enm->AllocNode(ENT_LET);
	declare_flatten->AppendOrderedChildNode(let_entity_code);
	EvaluableNode *let_assoc = enm->AllocNode(ENT_ASSOC);
	let_entity_code->AppendOrderedChildNode(let_assoc);

	EvaluableNode *lambda_for_create_root = enm->AllocNode(ENT_LAMBDA);
	let_assoc->SetMappedChildNode(ENBISI__, lambda_for_create_root);

	EvaluableNodeReference root_copy = entity->GetRoot(enm, EvaluableNodeManager::ENMM_LABEL_ESCAPE_INCREMENT);
	lambda_for_create_root->AppendOrderedChildNode(root_copy);
	if(root_copy.GetNeedCycleCheck())
		cycle_free = false;

	//   (if create_new_entity
	EvaluableNode *if_create_new = enm->AllocNode(ENT_IF);
	let_entity_code->AppendOrderedChildNode(if_create_new);
	if_create_new->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_create_new_entity));

	//     (assign "new_entity" (first
	//       (create_entities new_entity _)
	//     ))
	EvaluableNode *assign_new_entity_from_create = enm->AllocNode(ENT_ASSIGN);
	if_create_new->AppendOrderedChildNode(assign_new_entity_from_create);
	assign_new_entity_from_create->AppendOrderedChildNode(enm->AllocNode(ENT_STRING, ENBISI_new_entity));
	EvaluableNode *create_root_entity = enm->AllocNode(ENT_CREATE_ENTITIES);
	create_root_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
	create_root_entity->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));
	EvaluableNode *first_of_create_entity = enm->AllocNode(ENT_FIRST);
	first_of_create_entity->AppendOrderedChildNode(create_root_entity);
	assign_new_entity_from_create->AppendOrderedChildNode(first_of_create_entity);

	//     (assign_entity_roots new_entity _)
	EvaluableNode *assign_new_entity_into_current = enm->AllocNode(ENT_ASSIGN_ENTITY_ROOTS);
	if_create_new->AppendOrderedChildNode(assign_new_entity_into_current);
	assign_new_entity_into_current->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
	assign_new_entity_into_current->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI__));

	if(include_rand_seeds)
	{
		//   (set_entity_rand_seed
		//        new_entity
		//        *rand seed string* )
		EvaluableNode *set_rand_seed_root = enm->AllocNode(ENT_SET_ENTITY_RAND_SEED);
		set_rand_seed_root->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
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
		src_append->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
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
	declare_flatten->AppendOrderedChildNode(enm->AllocNode(ENT_SYMBOL, ENBISI_new_entity));
	
	//if anything isn't cycle free, then need to recompute everything
	if(!cycle_free)
		EvaluableNodeManager::UpdateFlagsForNodeTree(declare_flatten);

	return EvaluableNodeReference(declare_flatten, true);
}

void EntityManipulation::SortEntitiesByID(std::vector<Entity *> &entities)
{
	//for performance reasons, it may be worth considering other data structures if sort ever becomes or remains significant
	std::sort(begin(entities), end(entities),
		[](Entity *a, Entity *b)
		{
			const std::string a_id = a->GetId();
			const std::string b_id = b->GetId();

			int comp = StringManipulation::StringNaturalCompare(a_id, b_id);
			return comp < 0;
		});
}

void EntityManipulation::RecursivelyRenameAllEntityReferences(Entity *entity, CompactHashMap<StringInternPool::StringID, StringInternPool::StringID> &entities_renamed)
{
	EvaluableNodeTreeManipulation::ReplaceStringsInTree(entity->GetRoot(), entities_renamed);

	for(auto e : entity->GetContainedEntities())
		RecursivelyRenameAllEntityReferences(e, entities_renamed);
}
