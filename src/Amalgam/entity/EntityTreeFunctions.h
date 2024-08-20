#pragma once

//project headers:
#include "Entity.h"
#include "EvaluableNodeTreeFunctions.h"

template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeID(Entity *from_entity,
	EvaluableNode *id_node,
	StringRef *dest_sid_ref)
{
	if(EvaluableNode::IsNull(id_node))
		return std::make_pair(EntityReferenceType(from_entity), EntityReferenceType(nullptr));

	//get the string id, get a reference if returning it
	if(dest_sid_ref == nullptr)
	{
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(id_node);

		//need to lock the container first
		EntityReferenceType container_reference(from_entity);
		return std::make_pair(EntityReferenceType(from_entity->GetContainedEntity(sid)),
			std::move(container_reference));
	}
	else
	{
		StringInternPool::StringID sid = EvaluableNode::ToStringIDWithReference(id_node);

		//if there exists an entity with sid, then return it
		Entity *container = from_entity->GetContainedEntity(sid);
		if(container != nullptr)
		{
			string_intern_pool.DestroyStringReference(sid);
			return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(container));
		}

		dest_sid_ref->SetIDWithReferenceHandoff(sid);
		return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(from_entity));
	}
}

template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeID(Entity *from_entity,
	EvaluableNode *id_node_1, EvaluableNode *id_node_2,
	StringRef *dest_sid_ref)
{
	if(EvaluableNode::IsNull(id_node_1))
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, id_node_2, dest_sid_ref);
	if(EvaluableNode::IsNull(id_node_2))
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, id_node_1, dest_sid_ref);

	if(dest_sid_ref == nullptr)
	{
		//assume from_entity contains the container
		EntityReadReference container_container(from_entity);

		//assume id_node_1 references container
		StringInternPool::StringID sid_1 = EvaluableNode::ToStringIDIfExists(id_node_1);
		EntityReferenceType container(container_container->GetContainedEntity(sid_1));
		if(container == nullptr)
			return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(nullptr));

		//assume id_node_2 references entity
		StringInternPool::StringID sid_2 = EvaluableNode::ToStringIDIfExists(id_node_2);
		return std::make_pair(EntityReferenceType(container->GetContainedEntity(sid_2)), std::move(container));
	}
	else
	{
		//assume from_entity might be the container
		StringInternPool::StringID sid_1 = EvaluableNode::ToStringIDIfExists(id_node_1);
		EntityReferenceType possible_container(from_entity->GetContainedEntity(sid_1));

		//if didn't find a valid possible_container, return nothing
		if(possible_container == nullptr)
			return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(nullptr));

		//see if id_node_2 represents an existing entity
		StringInternPool::StringID sid_2 = EvaluableNode::ToStringIDWithReference(id_node_2);
		EntityReferenceType possible_target_entity(possible_container->GetContainedEntity(sid_2));
		if(possible_target_entity != nullptr)
		{
			string_intern_pool.DestroyStringReference(sid_2);
			return std::make_pair(EntityReferenceType(nullptr), std::move(possible_target_entity));
		}

		dest_sid_ref->SetIDWithReferenceHandoff(sid_2);
		return std::make_pair(EntityReferenceType(nullptr), std::move(possible_container));
	}
}

//Starts at the container specified and traverses the id path specified, finding the relative Entity to from_entity
//returns a reference of the entity specified by the id path followed by a reference to its container
template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNodeIDPathTraverser &traverser)
{
	if(from_entity == nullptr)
		return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(nullptr));

	//if already at the entity, return
	if(traverser.IsEntity())
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity,
			traverser.GetCurId(), traverser.destSidReference);

	//if at the container, lock the container and return the entity
	if(traverser.IsContainer())
	{
		EvaluableNode *node_id_1 = traverser.GetCurId();
		traverser.AdvanceIndex();
		EvaluableNode *node_id_2 = traverser.GetCurId();
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity,
			node_id_1, node_id_2, traverser.destSidReference);
	}

	//the entity is deeper than one of the container's entities, so put a read lock on it and traverse
	//always keep one to two locks active at once to walk down the entity containers
	//keep track of a reference for the current entity being considered
	//and a reference of the type that will be used for the target container
	EntityReadReference relative_entity_container(from_entity);

	//infinite loop, but logic inside will break it out appropriately
	while(true)
	{
		EvaluableNode *cur_node_id = traverser.GetCurId();
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(cur_node_id);
		Entity *next_entity = relative_entity_container->GetContainedEntity(sid);
		if(next_entity == nullptr)
			break;

		traverser.AdvanceIndex();

		if(traverser.IsContainer())
		{
			EvaluableNode *next_node_id_1 = traverser.GetCurId();
			traverser.AdvanceIndex();
			EvaluableNode *next_node_id_2 = traverser.GetCurId();
			return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(next_entity,
				next_node_id_1, next_node_id_2, traverser.destSidReference);
		}

		//traverse the id path for the next loop
		
		relative_entity_container = EntityReadReference(next_entity);
	}

	//something failed
	return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(nullptr));
}

//like TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath
//except only returns the entity requested
template<typename EntityReferenceType>
inline EntityReferenceType TraverseToExistingEntityReferenceViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNodeIDPathTraverser &traverser)
{
	auto [entity, container]
		= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReferenceType>(
			from_entity, traverser);

	return std::move(entity);
}

//like corresponding TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath
//but uses an id path and populates dest_sid_ref with the destination string id
// if dest_sid_ref is not nullptr
template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNode *id_path, StringRef *dest_sid_ref = nullptr)
{
	EvaluableNodeIDPathTraverser traverser(id_path, dest_sid_ref);
	auto [entity, container]
		= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReferenceType>(
			from_entity, traverser);

	return std::make_pair(std::move(entity), std::move(container));
}

//like corresponding TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath
//but uses an id path
template<typename EntityReferenceType>
inline EntityReferenceType TraverseToExistingEntityReferenceViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNode *id_path)
{
	EvaluableNodeIDPathTraverser traverser(id_path, nullptr);
	auto [entity, container]
		= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReferenceType>(
			from_entity, traverser);

	return std::move(entity);
}

//traverses id_path_1 and id_path_2 from from_entity, returns the corresponding entities, as well as
//read references to those entities and all entities they contain
std::tuple<Entity *, Entity *, Entity::EntityReferenceBufferReference<EntityReadReference>>
	TraverseToDeeplyContainedEntityReadReferencesViaEvaluableNodeIDPath(Entity *from_entity,
		EvaluableNode *id_path_1, EvaluableNode *id_path_2);

//constructs an ID or list of IDs that will traverse from a to b, assuming that b is contained somewhere within a
EvaluableNode *GetTraversalIDPathFromAToB(EvaluableNodeManager *enm, Entity *a, Entity *b);