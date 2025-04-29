//project headers:
#include "EvaluableNodeTreeFunctions.h"
#include "FastMath.h"
#include "Interpreter.h"

//system headers:
#include <tuple>

bool CustomEvaluableNodeComparator::operator()(EvaluableNode *a, EvaluableNode *b)
{
	//create context with "a" and "b" variables
	interpreter->PushNewConstructionContext(targetList, nullptr, EvaluableNodeImmediateValueWithType(), a);
	interpreter->PushNewConstructionContext(targetList, nullptr, EvaluableNodeImmediateValueWithType(), b);

	//compare
	bool retval = (interpreter->InterpretNodeIntoNumberValue(function) > 0);

	if(interpreter->PopConstructionContextAndGetExecutionSideEffectFlag())
		hadExecutionSideEffects = true;
	if(interpreter->PopConstructionContextAndGetExecutionSideEffectFlag())
		hadExecutionSideEffects = true;

	return retval;
}

//performs a top-down stable merge on the sub-lists from start_index to middle_index and middle_index to _end_index
//  from source into destination using cenc
void CustomEvaluableNodeOrderedChildNodesTopDownMerge(std::vector<EvaluableNode *> &source,
	size_t start_index, size_t middle_index, size_t end_index, std::vector<EvaluableNode *> &destination, CustomEvaluableNodeComparator &cenc)
{
	size_t left_pos = start_index;
	size_t right_pos = middle_index;

	//for all elements, pull from the appropriate buffer (left or right)
	for(size_t cur_index = start_index; cur_index < end_index; cur_index++)
	{
		//if left_pos has elements left and is less than the right, use it
		if(left_pos < middle_index && (right_pos >= end_index || cenc(source[left_pos], source[right_pos])))
		{
			destination[cur_index] = source[left_pos];
			left_pos++;
		}
		else //the right is less, use that
		{
			destination[cur_index] = source[right_pos];
			right_pos++;
		}
	}
}

//performs a stable merge sort of source (which *will* be modified and is not constant)
// from start_index to end_index into destination; uses cenc for comparison
void CustomEvaluableNodeOrderedChildNodesSort(std::vector<EvaluableNode *> &source,
	size_t start_index, size_t end_index, std::vector<EvaluableNode *> &destination, CustomEvaluableNodeComparator &cenc)
{
	//if one element, then sorted
	if(start_index + 1 >= end_index)
		return;

	size_t middle_index = (start_index + end_index) / 2;

	//sort left into list
	CustomEvaluableNodeOrderedChildNodesSort(destination, start_index, middle_index, source, cenc);
	//sort right into list
	CustomEvaluableNodeOrderedChildNodesSort(destination, middle_index, end_index, source, cenc);

	//merge buffers back into buffer
	CustomEvaluableNodeOrderedChildNodesTopDownMerge(source, start_index, middle_index, end_index, destination, cenc);
}

std::vector<EvaluableNode *> CustomEvaluableNodeOrderedChildNodesSort(std::vector<EvaluableNode *> &list, CustomEvaluableNodeComparator &cenc)
{
	//must make two copies of the list to edit, because switch back and forth and there is a chance that an element may be invalid
	// in either list.  Therefore, can't use the original list in the off chance that something is garbage collected
	std::vector<EvaluableNode *> list_copy_1(list);
	std::vector<EvaluableNode *> list_copy_2(list);
	CustomEvaluableNodeOrderedChildNodesSort(list_copy_1, 0, list.size(), list_copy_2, cenc);
	return list_copy_2;
}

std::tuple<Entity *, Entity *, Entity::EntityReferenceBufferReference<EntityReadReference>>
	TraverseToDeeplyContainedEntityReadReferencesViaEvaluableNodeIDPath(Entity *from_entity,
	EvaluableNode *id_path_1, EvaluableNode *id_path_2)
{
	if(from_entity == nullptr)
		return std::make_tuple(nullptr, nullptr,
			Entity::EntityReferenceBufferReference<EntityReadReference>());

	EvaluableNodeIDPathTraverser traverser_1(id_path_1, nullptr);
	if(traverser_1.IsEntity())
	{
		//lock everything in entity_1, and it will contain everything in entity_2
		auto erbr = from_entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(true);
		Entity *entity_2 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<Entity *>(from_entity, id_path_2);
		return std::make_tuple(from_entity, entity_2, std::move(erbr));
	}

	EvaluableNodeIDPathTraverser traverser_2(id_path_2, nullptr);
	if(traverser_2.IsEntity())
	{
		//lock everything in entity_2, and it will contain everything in entity_1
		auto erbr = from_entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(true);
		Entity *entity_1 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<Entity *>(from_entity, id_path_1);
		return std::make_tuple(entity_1, from_entity, std::move(erbr));
	}

	EntityReadReference relative_entity_container(from_entity);

	//infinite loop, but logic inside will break it out appropriately
	while(true)
	{
		EvaluableNode *cur_node_id_1 = traverser_1.GetCurId();
		StringInternPool::StringID sid_1 = EvaluableNode::ToStringIDIfExists(cur_node_id_1);
		
		EvaluableNode *cur_node_id_2 = traverser_2.GetCurId();
		StringInternPool::StringID sid_2 = EvaluableNode::ToStringIDIfExists(cur_node_id_2);

		if(sid_1 != sid_2)
		{
			size_t entity_index_1 = relative_entity_container->GetContainedEntityIndex(sid_1);
			size_t entity_index_2 = relative_entity_container->GetContainedEntityIndex(sid_2);

			if(entity_index_1 < entity_index_2)
			{
				EntityReadReference entity_1 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(relative_entity_container, traverser_1);
				Entity *entity_1_ptr = entity_1;
				if(entity_1_ptr == nullptr)
					return std::make_tuple(nullptr, nullptr,
						Entity::EntityReferenceBufferReference<EntityReadReference>());

				auto erbr = entity_1->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(false);
				erbr->emplace_back(std::move(entity_1));

				EntityReadReference entity_2 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(relative_entity_container, traverser_2);
				Entity *entity_2_ptr = entity_2;
				if(entity_2_ptr == nullptr)
					return std::make_tuple(nullptr, nullptr,
						Entity::EntityReferenceBufferReference<EntityReadReference>());

				entity_2->AppendAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(erbr);
				erbr->emplace_back(std::move(entity_2));

				return std::make_tuple(entity_1_ptr, entity_2_ptr, std::move(erbr));
			}
			else
			{
				EntityReadReference entity_2 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(relative_entity_container, traverser_2);
				Entity *entity_2_ptr = entity_2;
				if(entity_2_ptr == nullptr)
					return std::make_tuple(nullptr, nullptr,
						Entity::EntityReferenceBufferReference<EntityReadReference>());

				auto erbr = entity_2->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(false);
				erbr->emplace_back(std::move(entity_2));

				EntityReadReference entity_1 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityReadReference>(relative_entity_container, traverser_1);
				Entity *entity_1_ptr = entity_1;
				if(entity_1_ptr == nullptr)
					return std::make_tuple(nullptr, nullptr,
						Entity::EntityReferenceBufferReference<EntityReadReference>());

				entity_1->AppendAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(erbr);
				erbr->emplace_back(std::move(entity_1));

				return std::make_tuple(entity_1_ptr, entity_2_ptr, std::move(erbr));
			}

			break;
		}

		if(traverser_1.IsEntity())
		{
			//lock everything in entity_1, and it will contain everything in entity_2
			auto erbr = relative_entity_container->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(true);

			//both are the same entity
			if(traverser_1.IsEntity())
				return std::make_tuple(relative_entity_container.entity, relative_entity_container.entity, std::move(erbr));

			Entity *entity_2 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<Entity *>(relative_entity_container, traverser_2);
			return std::make_tuple(relative_entity_container.entity, entity_2, std::move(erbr));
		}

		if(traverser_2.IsEntity())
		{
			//lock everything in entity_2, and it will contain everything in entity_1
			auto erbr = relative_entity_container->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>(true);
			Entity *entity_1 = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<Entity *>(relative_entity_container, traverser_1);
			return std::make_tuple(entity_1, relative_entity_container.entity, std::move(erbr));
		}

		//ids are the same, continue traversing
		Entity *next_entity = relative_entity_container->GetContainedEntity(sid_1);
		if(next_entity == nullptr)
			return std::make_tuple(nullptr, nullptr,
				Entity::EntityReferenceBufferReference<EntityReadReference>());

		relative_entity_container = EntityReadReference(next_entity);
		traverser_1.AdvanceIndex();
		traverser_2.AdvanceIndex();
	}

	return std::make_tuple(nullptr, nullptr,
		Entity::EntityReferenceBufferReference<EntityReadReference>());
}

EvaluableNode *GetTraversalIDPathFromAToB(EvaluableNodeManager *enm, Entity *a, Entity *b)
{
	//shouldn't happen, but check
	if(b == nullptr)
		return nullptr;

	//if immediate entity, can return a string instead of a list
	if(b->GetContainer() == a)
		return enm->AllocNode(ENT_STRING, b->GetIdStringId());

	//create list to address entity
	EvaluableNode *id_list = enm->AllocNode(ENT_LIST);
	auto &ocn = id_list->GetOrderedChildNodes();
	while(b != nullptr && b != a)
	{
		ocn.push_back(enm->AllocNode(ENT_STRING, b->GetIdStringId()));
		b = b->GetContainer();
	}

	std::reverse(begin(ocn), end(ocn));
	return id_list;
}

EvaluableNode *GetTraversalPathListFromAToB(EvaluableNodeManager *enm, EvaluableNode::ReferenceAssocType &node_parents, EvaluableNode *a, EvaluableNode *b)
{
	if(a == nullptr || b == nullptr)
		return nullptr;

	EvaluableNodeReference path_list(enm->AllocNode(ENT_LIST), true);

	//find a path from b back to a by way of parents
	EvaluableNode::ReferenceSetType nodes_visited;
	EvaluableNode *b_ancestor = b;
	EvaluableNode *b_ancestor_parent = node_parents[b_ancestor];

	while(b_ancestor_parent != nullptr
		&& b_ancestor != a		//stop if it's the target
		&& nodes_visited.insert(b_ancestor_parent).second == true) //make sure not visited yet
	{
		//find where the node matches
		if(b_ancestor_parent->IsAssociativeArray())
		{
			//look up which key corresponds to the value
			StringInternPool::StringID key_sid = StringInternPool::NOT_A_STRING_ID;
			for(auto &[s_id, s] : b_ancestor_parent->GetMappedChildNodesReference())
			{
				if(s == b_ancestor)
				{
					key_sid = s_id;
					break;
				}
			}

			EvaluableNodeReference key_node = Parser::ParseFromKeyStringId(key_sid, enm);
			path_list->AppendOrderedChildNode(key_node);
			path_list.UpdatePropertiesBasedOnAttachedNode(key_node);
		}
		else if(b_ancestor_parent->IsOrderedArray())
		{
			auto &b_ancestor_parent_ocn = b_ancestor_parent->GetOrderedChildNodesReference();
			const auto &found = std::find(begin(b_ancestor_parent_ocn), end(b_ancestor_parent_ocn), b_ancestor);
			auto index = std::distance(begin(b_ancestor_parent_ocn), found);
			path_list->AppendOrderedChildNode(enm->AllocNode(static_cast<double>(index)));
		}
		else //didn't work... odd/error condition
		{
			enm->FreeNodeTree(path_list);
			return nullptr;
		}

		b_ancestor = b_ancestor_parent;
		b_ancestor_parent = node_parents[b_ancestor];
	}

	//if didn't end up hitting our target, then we can't get there
	if(b_ancestor != a)
	{
		enm->FreeNodeTree(path_list);
		return nullptr;
	}
	
	//reverse because assembled in reverse order
	auto &ocn = path_list->GetOrderedChildNodes();
	std::reverse(begin(ocn), end(ocn));
	return path_list;
}

EvaluableNode **GetRelativeEvaluableNodeFromTraversalPathList(EvaluableNode **source, EvaluableNode **index_path_nodes,
	size_t num_index_path_nodes, EvaluableNodeManager *enm, size_t max_num_nodes)
{
	//walk through address list to find target
	EvaluableNode **destination = source;
	for(size_t i = 0; i < num_index_path_nodes; i++)
	{
		//make sure valid and traversable, since at least one more address will be dereferenced
		if(destination == nullptr)
			break;

		//fetch the new destination based on what is being fetched
		EvaluableNode *addr = index_path_nodes[i];
		bool addr_empty = EvaluableNode::IsNull(addr);

		//if out of nodes but need to traverse further in the index, then will need to create new nodes
		if((*destination) == nullptr)
		{
			if(enm == nullptr)
			{
				destination = nullptr;
				break;
			}

			//need to create a new node to fill in, but create the most generic type possible that uses the type of the index as the way to access it
			if(!addr_empty && DoesEvaluableNodeTypeUseNumberData(addr->GetType())) //used to access lists
				*destination = enm->AllocNode(ENT_LIST);
			else
				*destination = enm->AllocNode(ENT_ASSOC);
		}

		if(EvaluableNode::IsAssociativeArray(*destination))
		{
			auto &mcn = (*destination)->GetMappedChildNodesReference();

			if(enm == nullptr)
			{
				auto key_sid = StringInternPool::NOT_A_STRING_ID;
				if(!addr_empty)
				{
					//string must already exist if can't create anything
					key_sid = EvaluableNode::ToStringIDIfExists(addr, true);
					if(key_sid == StringInternPool::NOT_A_STRING_ID)
					{
						destination = nullptr;
						break;
					}
				}

				//try to find key
				auto found = mcn.find(key_sid);
				if(found == end(mcn))
				{
					destination = nullptr;
					break;
				}
				
				destination = &(found->second);
			}
			else //create entry if it doesn't exist
			{
				auto key_sid = EvaluableNode::ToStringIDWithReference(addr, true);

				//attempt to insert the new key
				auto [inserted_key, inserted] = mcn.emplace(key_sid, nullptr);

				//if not inserted, then destroy the reference
				if(!inserted)
					string_intern_pool.DestroyStringReference(key_sid);

				//regardless of whether or not the result was inserted, grab the value portion
				destination = &(inserted_key->second);
			}
		}
		else if(!addr_empty && EvaluableNode::IsOrderedArray(*destination))
		{
			auto &ocn = (*destination)->GetOrderedChildNodesReference();
			double index = EvaluableNode::ToNumber(addr);
			//if negative, start from end and wrap around if the negative index is larger than the size
			if(index < 0)
			{
				index += ocn.size();
				if(index < 0) //clamp at zero
					index = 0;
			}

			//NaNs are not valid list indices, return null
			if(FastIsNaN(index))
			{
				destination = nullptr;
				break;
			}

			//make sure within bounds
			if(index < ocn.size())
				destination = &(ocn[static_cast<size_t>(index)]);
			else //beyond index
			{
				if(enm == nullptr)
					destination = nullptr;
				else //resize to fit
				{
					//if the index is more than can be referenced in 53 bits of 64-bit float mantissa,
					// then can't deal with it
					if(index >= 9007199254740992)
					{
						destination = nullptr;
						break;
					}

					//find the index and validate it
					size_t new_index = static_cast<size_t>(index);
					//if have specified a maximum number of nodes (not zero), then abide by it
					if(max_num_nodes > 0 && new_index > max_num_nodes)
					{
						destination = nullptr;
						break;
					}

					ocn.resize(new_index + 1, nullptr);
					destination = &(ocn[new_index]);
				}
			}
		}
		else //an immediate value -- can't get anything on the immediate
		{
			destination = nullptr;
		}
	}

	return destination;
}

EvaluableNodeReference AccumulateEvaluableNodeIntoEvaluableNode(EvaluableNodeReference value_destination_node,
	EvaluableNodeReference variable_value_node, EvaluableNodeManager *enm)
{
	//if the destination is empty, then just use the value specified
	if(value_destination_node == nullptr)
		return variable_value_node;

	//set up initial flags
	bool result_unique = (value_destination_node.unique && variable_value_node.unique);
	bool result_need_cycle_check = value_destination_node->GetNeedCycleCheck();
	if(!variable_value_node.unique || (variable_value_node != nullptr && variable_value_node->GetNeedCycleCheck()))
		result_need_cycle_check = true;
	bool result_idempotent = (value_destination_node->GetIsIdempotent() && (variable_value_node == nullptr || variable_value_node->GetIsIdempotent()));

	//if the value is unique, then can just edit in place
	if(value_destination_node.uniqueUnreferencedTopNode)
	{
		if(value_destination_node->GetType() == ENT_NUMBER)
		{
			double cur_value = EvaluableNode::ToNumber(value_destination_node);
			double inc_value = EvaluableNode::ToNumber(variable_value_node);
			value_destination_node->SetTypeViaNumberValue(cur_value + inc_value);
		}
		else if(value_destination_node->IsAssociativeArray())
		{
			if(EvaluableNode::IsAssociativeArray(variable_value_node))
			{
				auto &vvn_mcn = variable_value_node->GetMappedChildNodes();
				value_destination_node->ReserveMappedChildNodes(value_destination_node->GetMappedChildNodesReference().size()
																+ vvn_mcn.size());
				value_destination_node->AppendMappedChildNodes(vvn_mcn);
			}
			else if(variable_value_node != nullptr) //treat ordered pairs as new entries as long as not nullptr
			{
				value_destination_node->ReserveMappedChildNodes(value_destination_node->GetMappedChildNodesReference().size()
																+ variable_value_node->GetOrderedChildNodes().size() / 2);

				//iterate as long as pairs exist
				auto &vvn_ocn = variable_value_node->GetOrderedChildNodes();
				for(size_t i = 0; i + 1 < vvn_ocn.size(); i += 2)
				{
					StringInternPool::StringID key_sid = EvaluableNode::ToStringIDWithReference(vvn_ocn[i], true);
					value_destination_node->SetMappedChildNodeWithReferenceHandoff(key_sid, vvn_ocn[i + 1]);
				}
			}

			enm->FreeNodeIfPossible(variable_value_node);

			value_destination_node->SetNeedCycleCheck(result_need_cycle_check);
			value_destination_node->SetIsIdempotent(result_idempotent);
			value_destination_node.unique = result_unique;
		}
		else if(value_destination_node->GetType() == ENT_STRING)
		{
			//concatenate a string only if it is a valid string
			if(variable_value_node != nullptr && variable_value_node->GetType() == ENT_STRING)
			{
				value_destination_node->SetType(ENT_STRING, nullptr, false);
				std::string result = value_destination_node->GetStringValue() + variable_value_node->GetStringValue();
				value_destination_node->SetStringValue(result);
			}
			else
			{
				value_destination_node->SetType(ENT_NULL, nullptr, false);
			}

			value_destination_node.unique = true;
		}
		else //add ordered child node
		{
			if(EvaluableNode::IsAssociativeArray(variable_value_node))
			{
				//expand out into pairs
				value_destination_node->ReserveOrderedChildNodes(value_destination_node->GetOrderedChildNodes().size()
																+ 2 * variable_value_node->GetMappedChildNodesReference().size());
				
				for(auto &[cn_id, cn] : variable_value_node->GetMappedChildNodesReference())
				{
					EvaluableNodeReference key_node = Parser::ParseFromKeyStringId(cn_id, enm);
					value_destination_node->AppendOrderedChildNode(key_node);
					value_destination_node->AppendOrderedChildNode(cn);
					value_destination_node.UpdatePropertiesBasedOnAttachedNode(key_node);
				}

				enm->FreeNodeIfPossible(variable_value_node);
			}
			else if(EvaluableNode::IsOrderedArray(variable_value_node))
			{
				value_destination_node->ReserveOrderedChildNodes(value_destination_node->GetOrderedChildNodes().size()
																+ variable_value_node->GetOrderedChildNodesReference().size());
				value_destination_node->AppendOrderedChildNodes(variable_value_node->GetOrderedChildNodesReference());

				enm->FreeNodeIfPossible(variable_value_node);
			}
			else //just append one value
			{
				value_destination_node->AppendOrderedChildNode(variable_value_node);
			}

			value_destination_node->SetNeedCycleCheck(result_need_cycle_check);
			value_destination_node->SetIsIdempotent(result_idempotent);
			value_destination_node.unique = result_unique;
		}

		return value_destination_node;
	}

	//not unique, so need to make a new list
	if(value_destination_node->GetType() == ENT_NUMBER)
	{
		double cur_value = EvaluableNode::ToNumber(value_destination_node);
		double inc_value = EvaluableNode::ToNumber(variable_value_node);
		value_destination_node.SetReference(enm->AllocNode(cur_value + inc_value), true);
	}
	else if(value_destination_node->IsAssociativeArray())
	{
		EvaluableNode *new_list = enm->AllocNode(value_destination_node);

		if(EvaluableNode::IsAssociativeArray(variable_value_node))
		{
			new_list->AppendMappedChildNodes(variable_value_node->GetMappedChildNodesReference());
		}
		else if(variable_value_node != nullptr) //treat ordered pairs as new entries as long as not nullptr
		{
			//iterate as long as pairs exist
			auto &vvn_ocn = variable_value_node->GetOrderedChildNodes();
			for(size_t i = 0; i + 1 < vvn_ocn.size(); i += 2)
			{
				StringInternPool::StringID key_sid = EvaluableNode::ToStringIDWithReference(vvn_ocn[i], true);
				new_list->SetMappedChildNodeWithReferenceHandoff(key_sid, vvn_ocn[i + 1]);
			}
		}

		enm->FreeNodeIfPossible(variable_value_node);

		value_destination_node.SetReference(new_list, result_unique, true);
		value_destination_node->SetNeedCycleCheck(result_need_cycle_check);
		value_destination_node->SetIsIdempotent(result_idempotent);
	}
	else if(value_destination_node->GetType() == ENT_STRING)
	{
		//concatenate a string only if it is a valid string
		if(variable_value_node != nullptr && variable_value_node->GetType() == ENT_STRING)
		{
			value_destination_node->SetType(ENT_STRING, nullptr, false);
			std::string result = value_destination_node->GetStringValue() + variable_value_node->GetStringValue();
			value_destination_node->SetStringValue(result);
			value_destination_node.SetReference(enm->AllocNode(ENT_STRING, result), true);
		}
		else
		{
			value_destination_node.SetReference(enm->AllocNode(ENT_NULL), true);
		}
	}
	else //add ordered child node
	{
		EvaluableNodeReference new_list(enm->AllocNode(value_destination_node), true);
		if(EvaluableNode::IsAssociativeArray(variable_value_node))
		{
			auto &vvn_mcn = variable_value_node->GetMappedChildNodesReference();
			//expand out into pairs
			new_list->ReserveOrderedChildNodes(value_destination_node->GetOrderedChildNodes().size() + 2 * vvn_mcn.size());
			for(auto &[cn_id, cn] : vvn_mcn)
			{
				EvaluableNodeReference key_node = Parser::ParseFromKeyStringId(cn_id, enm);
				new_list->AppendOrderedChildNode(key_node);
				new_list->AppendOrderedChildNode(cn);
				new_list.UpdatePropertiesBasedOnAttachedNode(key_node);
			}

			enm->FreeNodeIfPossible(variable_value_node);
		}
		else if(EvaluableNode::IsOrderedArray(variable_value_node))
		{
			new_list->ReserveOrderedChildNodes(value_destination_node->GetOrderedChildNodes().size() + variable_value_node->GetOrderedChildNodes().size());
			new_list->AppendOrderedChildNodes(variable_value_node->GetOrderedChildNodes());

			enm->FreeNodeIfPossible(variable_value_node);
		}
		else //just append one value
		{
			new_list->ReserveOrderedChildNodes(value_destination_node->GetOrderedChildNodes().size() + 1);
			new_list->AppendOrderedChildNode(variable_value_node);
		}

		value_destination_node.SetReference(new_list, result_unique, true);
		value_destination_node->SetNeedCycleCheck(result_need_cycle_check);
		value_destination_node->SetIsIdempotent(result_idempotent);
	}

	return value_destination_node;
}
