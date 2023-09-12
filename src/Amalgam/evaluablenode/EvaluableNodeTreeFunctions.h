#pragma once

//project headers:
#include "Entity.h"
#include "EvaluableNode.h"

//system headers:
#include <codecvt>
#include <locale>
#include <string_view>

//forward declarations:
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

//Returns positive if a is less than b,
// negative if greater, or 0 if equal or not numerically comparable
int StringNaturalCompare(const std::string &a, const std::string &b);

inline int StringNaturalCompare(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	return StringNaturalCompare(string_intern_pool.GetStringFromID(a), string_intern_pool.GetStringFromID(b));
}

inline bool StringNaturalCompareSort(const std::string &a, const std::string &b)
{ 
	int comp = StringNaturalCompare(a, b);
	return comp < 0;
}

inline bool StringIDNaturalCompareSort(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	int comp = StringNaturalCompare(string_intern_pool.GetStringFromID(a), string_intern_pool.GetStringFromID(b));
	return comp < 0;
}

inline bool StringNaturalCompareSortReverse(const std::string &a, const std::string &b)
{
	int comp = StringNaturalCompare(a, b);
	return comp > 0;
}

inline bool StringIDNaturalCompareSortReverse(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	int comp = StringNaturalCompare(a, b);
	return comp > 0;
}

//Starts at the container specified and traverses the id list specified, finding the relative Entity from container
// if id_path is nullptr, then it will set relative_entity to the container itself, leaving relative_entity_parent to nullptr
// if id_path is invalid or container is nullptr, then it will set both relative_entity and relative_entity_parent to nullptr
// if id_path is any form of a list, then it will treat the ids as a sequence of subcontainers
// otherwise the id_path is transformed to a string and used as an id
//sets relative_entity_parent to the base entity found, sets id to the value of the id relative to the base, and relative_entity to the entity being pointed to
// if the path exists (as in a destination of where to put an entity) but the target entity does not, then relative_entity_parent may be a valid reference and relative_entity may be nullptr
//Note that id is allocated in the string_intern_pool, and the caller is responsible for freeing the allocation
void TraverseToEntityViaEvaluableNodeIDPath(Entity *container, EvaluableNode *id_path, Entity *&relative_entity_parent, StringInternRef &id, Entity *&relative_entity);

//Starts at the container specified and traverses the id list specified, finding the relative Entity from container
// if id_path does not exist or is invalid then returns nullptr
Entity *TraverseToExistingEntityViaEvaluableNodeIDPath(Entity *container, EvaluableNode *id_path);

//Like TraverseToEntityViaEvaluableNodeIDPath, except ensures that the final destination does not exist (or if it does, it will place it within the entity specified
//Note that destination_id is allocated in the string_intern_pool, and the caller is responsible for freeing the allocation
void TraverseEntityToNewDestinationViaEvaluableNodeIDPath(Entity *container, EvaluableNode *id_path, Entity *&destination_entity_parent, StringInternRef &destination_id);

//constructs a list of IDs that will traverse frome a to b, assuming that b is contained somewhere within a
EvaluableNode *GetTraversalIDPathListFromAToB(EvaluableNodeManager *enm, Entity *a, Entity *b);

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
