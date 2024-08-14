#pragma once

//project headers:
#include "EvaluableNodeManagement.h"

//system headers:
#include <codecvt>
#include <locale>
#include <string_view>
#include <tuple>

class EvaluableNodeIDPathTraverser
{
public:
	inline EvaluableNodeIDPathTraverser()
		: idPath(nullptr), idPathEntries(nullptr), curIndex(0), destSidReference(nullptr)
	{	}

	//calls AnalyzeIDPath with the same parameters
	inline EvaluableNodeIDPathTraverser(EvaluableNode *id_path, StringRef *dest_sid_ref)
	{
		AnalyzeIDPath(id_path, dest_sid_ref);
	}

	//populates attributes based on the id_path
	//if has non-null dest_sid_ref, then it will store the pointer and use it to populate the destination string id
	void AnalyzeIDPath(EvaluableNode *id_path, StringRef *dest_sid_ref)
	{
		idPath = nullptr;
		idPathEntries = nullptr;
		curIndex = 0;
		containerIdIndex = 0;
		entityIdIndex = 0;
		lastIdIndex = 0;

		destSidReference = dest_sid_ref;
		//if the destination sid is requested, initialize it
		if(destSidReference != nullptr)
			destSidReference->Clear();

		//if single value, then just set and return
		if(EvaluableNode::IsNull(id_path))
		{
			idPath = id_path;
			return;
		}
		else if(id_path->GetType() != ENT_LIST)
		{
			idPath = id_path;
			if(destSidReference == nullptr)
			{
				entityIdIndex = 1;
				lastIdIndex = 1;
			}
			return;
		}

		//size of the entity list excluding trailing nulls
		auto id_path_entries = &id_path->GetOrderedChildNodesReference();
		size_t non_null_size = id_path_entries->size();
		while(non_null_size > 0 && EvaluableNode::IsNull((*id_path_entries)[non_null_size - 1]))
			non_null_size--;

		//if no entities, nothing to traverse
		if(non_null_size == 0)
			return;

		idPath = id_path;
		idPathEntries = id_path_entries;

		//find first index
		while(curIndex < non_null_size && EvaluableNode::IsNull((*idPathEntries)[curIndex]))
			curIndex++;

		lastIdIndex = non_null_size - 1;
		entityIdIndex = lastIdIndex;

		if(destSidReference != nullptr)
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
		if(idPathEntries == nullptr)
		{
			if(curIndex == 0)
				return idPath;
			return nullptr;
		}

		if(curIndex > entityIdIndex)
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

	//if not nullptr, then will be set to a reference to the destination string id
	StringRef *destSidReference;
};

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
