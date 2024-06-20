#pragma once

//project headers:
#include "Entity.h"
#include "EvaluableNode.h"

//system headers:
#include <codecvt>
#include <locale>
#include <string_view>

//forward declarations:
class Entity;
class Interpreter;

//used for any operation that must sort different values - for passing in a lambda to run on every operation
class CustomEvaluableNodeComparator
{
public:
	constexpr CustomEvaluableNodeComparator(Interpreter *_interpreter, EvaluableNode *_function, EvaluableNode *target_list)
		: interpreter(_interpreter), function(_function), targetList(target_list)
	{ }

	bool operator()(EvaluableNode *a, EvaluableNode *b);

private:
	Interpreter *interpreter;
	EvaluableNode *function;
	EvaluableNode *targetList;
};

//sorts list based on the specified CustomEvaluableNodeComparator using a stable merge sort
// does not require weak ordering from cenc
// merge sort is the preferrable sort due to the lack of weak ordering and bottleneck being interpretation
//returns a newly sorted list
std::vector<EvaluableNode *> CustomEvaluableNodeOrderedChildNodesSort(std::vector<EvaluableNode *> &list, CustomEvaluableNodeComparator &cenc);

class EvaluableNodeIDPathTraverser
{
public:
	inline EvaluableNodeIDPathTraverser()
		: idPath(nullptr), idPathEntries(nullptr), curIndex(0)
	{	}

	//calls AnalyzeIDPath with the same parameters
	inline EvaluableNodeIDPathTraverser(EvaluableNode *id_path, bool has_destination_id)
	{
		AnalyzeIDPath(id_path, has_destination_id);
	}

	//populates attributes based on the id_path
	//if has_destination_id, then it will leave one id at the end for the destination
	void AnalyzeIDPath(EvaluableNode *id_path, bool has_destination_id)
	{
		idPath = id_path;
		idPathEntries = &idPath->GetOrderedChildNodes();
		curIndex = 0;
		containerIdIndex = 0;
		entityIdIndex = 0;
		lastIdIndex = 0;

		//size of the entity list excluding trailing nulls
		size_t non_null_size = idPathEntries->size();
		while(non_null_size > 0 && EvaluableNode::IsNull((*idPathEntries)[non_null_size - 1]))
			non_null_size--;

		//if no entities, nothing to traverse
		if(non_null_size == 0)
		{
			idPathEntries = nullptr;
			return;
		}

		//find first index
		while(curIndex < non_null_size && EvaluableNode::IsNull((*idPathEntries)[curIndex]))
			curIndex++;

		lastIdIndex = non_null_size - 1;
		entityIdIndex = lastIdIndex;

		if(has_destination_id)
		{
			//walk down to find the entity id
			while(entityIdIndex > curIndex && EvaluableNode::IsNull((*idPathEntries)[entityIdIndex - 1]))
				entityIdIndex--;
		}

		//index of the target entity's container's id; start at curIndex,
		//and if there's room, try to work downward to find the id previous to entityIdIndex
		//if there's nothing between, it won't execute, or it will set them back to being the same
		containerIdIndex = curIndex;
		if(entityIdIndex > curIndex)
		{
			containerIdIndex = entityIdIndex - 1;
			while(containerIdIndex > curIndex && EvaluableNode::IsNull((*idPathEntries)[containerIdIndex - 1]))
				containerIdIndex--;
		}
	}

	constexpr bool IsContainer()
	{	return (curIndex == containerIdIndex);	}

	constexpr bool IsEntity()
	{	return (curIndex == entityIdIndex);	}

	constexpr bool IsLastIndex()
	{	return (curIndex == lastIdIndex);	}

	inline void AdvanceIndex()
	{
		do
		{
			//advance to next step
			curIndex++;
		} while(curIndex < entityIdIndex && EvaluableNode::IsNull((*idPathEntries)[curIndex]));
	}

	//gets the current ID, nullptr if out of ids
	inline EvaluableNode *GetCurId()
	{
		if(idPathEntries == nullptr || curIndex > entityIdIndex)
			return nullptr;
		return (*idPathEntries)[curIndex];
	}

	//the node for the id path and a pointer to its ordered child nodes
	EvaluableNode *idPath;
	std::vector<EvaluableNode *> *idPathEntries;

	//current index in idPath
	size_t curIndex;

	//index of the container of the target entity in idPath
	size_t containerIdIndex;

	//index of the target entity entity in idPath
	size_t entityIdIndex;

	//index of the last entity id, if applicable
	size_t lastIdIndex;
};

template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeID(Entity *from_entity,
	EvaluableNode *id_node,
	StringRef *dest_sid_ref)
{
	if(EvaluableNode::IsEmptyNode(id_node))
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
		TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, id_node_2, dest_sid_ref);
	if(EvaluableNode::IsNull(id_node_2))
		TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, id_node_1, dest_sid_ref);

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
//if id_path does not exist or is invalid then returns nullptr for both
//if id_path specifies the entity in from_entity, then it returns a reference to it
//if dest_sid_ref is not null, it will assume it is a location to store a string id with reference
// that must be managed by caller
template<typename EntityReferenceType>
std::pair<EntityReferenceType, EntityReferenceType>
TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNode *id_path,
	StringRef *dest_sid_ref = nullptr)
{
	//if the destination sid is requested, initialize it
	if(dest_sid_ref != nullptr)
		*dest_sid_ref = StringRef(string_intern_pool.NOT_A_STRING_ID);

	if(from_entity == nullptr)
		return std::make_pair(EntityReferenceType(nullptr), EntityReferenceType(nullptr));

	if(EvaluableNode::IsEmptyNode(id_path) || id_path->GetType() != ENT_LIST)
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, id_path, dest_sid_ref);

	EvaluableNodeIDPathTraverser traverser(id_path, dest_sid_ref != nullptr);

	//if already at the entity, return
	if(traverser.IsEntity())
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, traverser.GetCurId(), dest_sid_ref);

	//if at the container, lock the container and return the entity
	if(traverser.IsContainer())
	{
		EvaluableNode *node_id_1 = traverser.GetCurId();
		traverser.AdvanceIndex();
		EvaluableNode *node_id_2 = traverser.GetCurId();
		return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(from_entity, node_id_1, node_id_2, dest_sid_ref);
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
			return TraverseToEntityReferenceAndContainerViaEvaluableNodeID<EntityReferenceType>(next_entity, next_node_id_1, next_node_id_2, dest_sid_ref);
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
	Entity *from_entity, EvaluableNode *id_path)
{
	auto [entity, container]
		= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReferenceType>(
			from_entity, id_path);

	return std::move(entity);
}

//constructs an ID or list of IDs that will traverse frome a to b, assuming that b is contained somewhere within a
EvaluableNode *GetTraversalIDPathFromAToB(EvaluableNodeManager *enm, Entity *a, Entity *b);

//similar to Parser::GetCodeForPathFromAToB, but instead returns a list of how to traverse each node, such as which index or which key to use to traverse
// returns nullptr if no path exists
EvaluableNode *GetTraversalPathListFromAToB(EvaluableNodeManager *enm, EvaluableNode::ReferenceAssocType &node_parents, EvaluableNode *a, EvaluableNode *b);

//Starts at source and traverses based on the indexes in the index_path, assuming that index_path is a list of ordered nodes each
// of which specifies the index (number of string) to traverse
//if the index_path_nodes should be a pointer to an array of EvaluableNodes *s of length num_index_path_nodes
//if enm is non-null, then it will enlarge lists sizes, add assoc keys, and create entirely new nodes (of default types) if the target does not exist, up to a maximum of max_num_nodes
// (unless max_num_nodes is 0, in which case it is ignored)
// if it is null, then it will only return existing nodes
EvaluableNode **GetRelativeEvaluableNodeFromTraversalPathList(EvaluableNode **source, EvaluableNode **index_path_nodes, size_t num_index_path_nodes, EvaluableNodeManager *enm, size_t max_num_nodes);

//accumulates variable_value_node into value_destination_node and returns the result
// will free the top node of variable_value_node if possible; e.g., if appending a list, to a list, will free the second list if possible
EvaluableNodeReference AccumulateEvaluableNodeIntoEvaluableNode(EvaluableNodeReference value_destination_node, EvaluableNodeReference variable_value_node, EvaluableNodeManager *enm);

//using enm, builds an assoc from id_value_container using get_string_id and get_number to get the id and number of each entry
//note that get_string_id will be called twice and will be called under locks in multithreading, so it should be a very simple function
template<typename IDValueContainer, typename IDFunction, typename ValueFunction>
inline EvaluableNodeReference CreateAssocOfNumbersFromIteratorAndFunctions(IDValueContainer &id_value_container,
	IDFunction get_string_id, ValueFunction get_number, EvaluableNodeManager *enm)
{
	EvaluableNode *assoc = enm->AllocNode(ENT_ASSOC);
	assoc->ReserveMappedChildNodes(id_value_container.size());

	string_intern_pool.CreateStringReferences(id_value_container, get_string_id);

	for(auto &id_value_iterator : id_value_container)
	{
		StringInternPool::StringID entity_sid = get_string_id(id_value_iterator);
		assoc->SetMappedChildNodeWithReferenceHandoff(entity_sid, enm->AllocNode(get_number(id_value_iterator)));
	}

	return EvaluableNodeReference(assoc, true);
}

//using enm, builds a list from value_container using get_string_id and get_number to get the id and number of each entry
template<typename ValueContainer, typename GetNumberFunction>
inline EvaluableNodeReference CreateListOfNumbersFromIteratorAndFunction(ValueContainer &value_container,
	EvaluableNodeManager *enm, GetNumberFunction get_number)
{
	EvaluableNode *list = enm->AllocListNodeWithOrderedChildNodes(ENT_NUMBER, value_container.size());
	auto &ocn = list->GetOrderedChildNodes();

	size_t index = 0;
	for(auto value_element : value_container)
		ocn[index++]->SetNumberValue(get_number(value_element));

	return EvaluableNodeReference(list, true);
}

//using enm, builds a list from string_container to id_value_iterator_end using get_string_id and get_number to get the id and number of each entry
//note that get_string_id will be called twice and will be called under locks in multithreading, so it should be a very simple function
template<typename StringContainer, typename GetStringFunction>
inline EvaluableNodeReference CreateListOfStringsIdsFromIteratorAndFunction(StringContainer &string_container,
	EvaluableNodeManager *enm, GetStringFunction get_string_id)
{
	EvaluableNode *list = enm->AllocListNodeWithOrderedChildNodes(ENT_STRING, string_container.size());
	auto &ocn = list->GetOrderedChildNodes();

	string_intern_pool.CreateStringReferences(string_container, get_string_id);

	size_t index = 0;
	for(auto string_element : string_container)
		ocn[index++]->SetStringIDWithReferenceHandoff(get_string_id(string_element));

	return EvaluableNodeReference(list, true);
}

//using enm, builds a list from string_container to id_value_iterator_end using get_string_id and get_number to get the id and number of each entry
//note that get_string_id will be called twice and will be called under locks in multithreading, so it should be a very simple function
template<typename StringContainer, typename GetStringFunction>
inline EvaluableNodeReference CreateListOfStringsFromIteratorAndFunction(StringContainer &string_container,
	EvaluableNodeManager *enm, GetStringFunction get_string)
{
	EvaluableNode *list = enm->AllocListNodeWithOrderedChildNodes(ENT_STRING, string_container.size());
	auto &ocn = list->GetOrderedChildNodes();

	size_t index = 0;
	for(auto string_element : string_container)
		ocn[index++]->SetStringValue(get_string(string_element));

	return EvaluableNodeReference(list, true);
}

//removes the top conclude or return node and, if possible, will free it, saving memory
inline EvaluableNodeReference RemoveTopConcludeOrReturnNode(EvaluableNodeReference result, EvaluableNodeManager *enm)
{
	if(result == nullptr)
		return EvaluableNodeReference::Null();

	if(result->GetOrderedChildNodes().size() == 0)
	{
		enm->FreeNodeTreeIfPossible(result);
		return EvaluableNodeReference::Null();
	}

	EvaluableNode *conclusion = result->GetOrderedChildNodes()[0];
	enm->FreeNodeIfPossible(result);

	return EvaluableNodeReference(conclusion, result.unique);
}
