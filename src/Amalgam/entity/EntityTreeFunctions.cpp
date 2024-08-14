//project headers:
#include "EntityTreeFunctions.h"

//system headers:
#include <algorithm>
#include <cctype>
#include <tuple>

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
