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

//Starts at the container specified and traverses the id path specified, finding the relative Entity from container
// if id_path is nullptr, then it will set relative_entity to the container itself, leaving relative_entity_container to nullptr
// if id_path is invalid or container is nullptr, then it will set both relative_entity and relative_entity_container to nullptr
// if id_path is any form of a list, then it will treat the ids as a sequence of subcontainers
// otherwise the id_path is transformed to a string and used as an id
//sets relative_entity_container to the base entity found, sets id to the value of the id relative to the base, and relative_entity to the entity being pointed to
// if the path exists (as in a destination of where to put an entity) but the target entity does not, then relative_entity_container may be a valid reference and relative_entity may be nullptr
//Note that id is allocated in the string_intern_pool, and the caller is responsible for freeing the allocation
void TraverseToEntityViaEvaluableNodeIDPath(Entity *container, EvaluableNode *id_path, Entity *&relative_entity_container, StringInternRef &id, Entity *&relative_entity);

//Starts at the container specified and traverses the id path specified, finding the relative Entity to from_entity
//returns a reference of the entity specified by the id path followed by a reference to its container
//if id_path does not exist or is invalid then returns nullptr for both
//if id_path specifies the entity in from_entity, then it returns a reference to it
template<typename EntityReferenceType, typename ContainerEntityReferenceType = EntityReadReference>
std::pair<EntityReferenceType, ContainerEntityReferenceType>
	TraverseToExistingEntityReferenceAndContainerViaEvaluableNodeIDPath(
																Entity *from_entity, EvaluableNode *id_path)
{
	if(from_entity == nullptr)
		return std::make_pair(EntityReferenceType(nullptr), ContainerEntityReferenceType(nullptr));

	if(EvaluableNode::IsEmptyNode(id_path))
		return std::make_pair(EntityReferenceType(from_entity), ContainerEntityReferenceType(nullptr));

	if(id_path->GetType() != ENT_LIST)
	{
		//if the string doesn't exist, then there can't be an entity with that name, which won't create a reference
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(id_path);
		//need to lock the container first
		ContainerEntityReferenceType container_reference(from_entity);
		return std::make_pair(EntityReferenceType(from_entity->GetContainedEntity(sid)),
			std::move(container_reference));
	}

	auto &ocn = id_path->GetOrderedChildNodes();

	//size of the entity list excluding trailing nulls
	size_t non_null_size = ocn.size();
	while(non_null_size > 0 && EvaluableNode::IsNull(ocn[non_null_size - 1]))
		non_null_size--;

	//if empty list, return the entity itself
	if(non_null_size == 0)
		return std::make_pair(EntityReferenceType(from_entity), ContainerEntityReferenceType(nullptr));

	//index of the id that will be used for the target entity
	size_t target_entity_id_index = non_null_size - 1;

	//find first index
	size_t cur_index = 0;
	while(cur_index < non_null_size && EvaluableNode::IsNull(ocn[cur_index]))
		cur_index++;

	//if there's only one valid entry in the list, retrieve it
	if(cur_index == target_entity_id_index)
	{
		//if the string doesn't exist, then there can't be an entity with that name, which won't create a reference
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(ocn[cur_index]);
		//need to lock the container first
		ContainerEntityReferenceType container_reference(from_entity);
		return std::make_pair(EntityReferenceType(from_entity->GetContainedEntity(sid)),
			std::move(container_reference));
	}

	//index of the target entity's container's id; start at cur_index,
	//and if there's room, try to work downward to find the id previous to target_entity_id_index
	//if there's nothing between, it won't execute, or it will set them back to being the same
	size_t target_container_id_index = cur_index;
	if(target_entity_id_index > cur_index)
	{
		target_container_id_index = target_entity_id_index - 1;
		while(target_container_id_index > 0 && EvaluableNode::IsNull(ocn[target_container_id_index - 1]))
			target_container_id_index--;
	}

	//the entity is deeper than one of the container's entities, so put a read lock on it and traverse
	//always keep one to two locks active at once to walk down the entity containers
	//keep track of a reference for the current entity being considered
	//and a reference of the type that will be used for the target container
	EntityReadReference relative_entity_container(from_entity);
	ContainerEntityReferenceType target_container(nullptr);

	for(; cur_index < non_null_size; cur_index++)
	{
		EvaluableNode *cn = ocn[cur_index];

		//null means current entity wherever it is in the traversal
		if(EvaluableNode::IsNull(cn))
			continue;

		//if the string doesn't exist, then there can't be an entity with that name
		StringInternPool::StringID sid = EvaluableNode::ToStringIDIfExists(cn);
		from_entity = from_entity->GetContainedEntity(sid);

		//if entity doesn't exist, exit gracefully
		if(from_entity == nullptr)
			return std::make_pair(EntityReferenceType(nullptr), ContainerEntityReferenceType(nullptr));

		if(cur_index < target_container_id_index)
		{
			relative_entity_container = EntityReadReference(from_entity);
		}
		else if(cur_index == target_container_id_index)
		{
			//first acquire the container's reference, then free its container's reference
			target_container = ContainerEntityReferenceType(from_entity);
			relative_entity_container = EntityReadReference(nullptr);
		}
		else //cur_index == target_entity_id_index
		{
			return std::make_pair(EntityReferenceType(from_entity),
				std::move(relative_entity_container));
		}
	}

	//shouldn't make it here
	return std::make_pair(EntityReferenceType(nullptr), ContainerEntityReferenceType(nullptr));
}

//like TraverseToExistingEntityReferenceAndContainerViaEvaluableNodeIDPath
//except only returns the entity requested
template<typename EntityReferenceType>
inline EntityReferenceType TraverseToExistingEntityReferenceViaEvaluableNodeIDPath(
	Entity *from_entity, EvaluableNode *id_path)
{
	auto [entity, container]
		= TraverseToExistingEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityReferenceType, EntityReadReference>(
			from_entity, id_path
		);

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
