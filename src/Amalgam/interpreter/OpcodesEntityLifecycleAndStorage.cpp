//project headers:
#include "AssetManager.h"
#include "EntityManipulation.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"


static std::string _opcode_group = "Entity Lifecycle and Storage";

static OpcodeInitializer _ENT_CREATE_ENTITIES(ENT_CREATE_ENTITIES, &Interpreter::InterpretNode_ENT_CREATE_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] * node1 [id_path entity2] [* node2] [...])";
	d.returns = R"(list of id_path)";
	d.description = R"(Creates a new entity for id path `entity1` with code specified by `node1`, repeating this for all entity-node pairs, returning a list of the id paths for each of the entities created.  If the execution does not have permission to create the entities, it will evaluate to null.  If the `entity` is omitted, then it will create an unnamed new entity in the calling entity.  If `entity1` specifies an existing entity, then it will create the new entity within that existing entity.  If the last id path in the string is not an existing entity, then it will attempt to create that entity (returning null if it cannot).  If the node is of any other type than assoc, it will create an assoc as the top node and place the node under the null key.  Unlike the rest of the entity creation commands, create_entities specifies the optional id path first to make it easy to read entity definitions.  If more than 2 parameters are specified, create_entities will iterate through all of the pairs of parameters, treating them like the first two as it creates new entities.)";
	d.examples = MakeAmalgamExamples({
		{R"&((create_entities
	"Entity"
	(lambda
		{
			a (+ 3 4)
		}
	)
))&", R"(["Entity"])", "", R"((destroy_entities "Entity"))"},
			{R"&((seq
	(create_entities
		"EntityWithContainedEntities"
		{a 3 b 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["EntityWithContainedEntities" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["EntityWithContainedEntities"]
		{m 3 n 4}
	)
	(contained_entities "EntityWithContainedEntities")
))&", R"(["NamedEntity1" "NamedEntity2" "_hIcoPxJ8LiS"])", "", R"((destroy_entities "EntityWithContainedEntities"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CREATE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//code will be the last parameter
		EvaluableNodeReference root = EvaluableNodeReference::Null();
		if(i + 1 == ocn.size())
			root = InterpretNodeForImmediateUse(ocn[i]);
		else
			root = InterpretNodeForImmediateUse(ocn[i + 1]);

		//get destination if applicable
		EntityWriteReference entity_container;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
		{
			node_stack.PushEvaluableNode(root);
			std::tie(entity_container, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i]);
			node_stack.PopEvaluableNode();
		}
		else
		{
			entity_container = EntityWriteReference(curEntity);
		}

		if(entity_container == nullptr || !CanCreateNewEntityFromConstraints(entity_container, new_entity_id))
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		auto &new_entity_id_string = string_intern_pool.GetStringFromID(new_entity_id);
		std::string rand_state = entity_container->CreateRandomStreamFromStringAndRand(new_entity_id_string);

		//pause while allocating the new entity
		auto lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		//create new entity
		Entity *new_entity = new Entity(root, rand_state);

		lab_pause.Resume();

		//accumulate usage
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

		entity_container->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(entity_container == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity));
	}

	return new_entity_ids_list;
}

static OpcodeInitializer _ENT_CLONE_ENTITIES(ENT_CLONE_ENTITIES, &Interpreter::InterpretNode_ENT_CLONE_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...])";
	d.returns = R"(list of id_path)";
	d.description = R"(Creates a clone of `source_entity1`.  If `destination_entity1` is not specified, then it clones the entity into an unnamed entity in the current entity.  If `destination_entity1` is specified, then it clones it into the location specified by `destination_entity1`; if `destination_entity1` is an existing entity, then it will create it as a contained entity within `destination_entity1`, if not, it will attempt to create it with the given id path of `destination_entity1`.  Evaluates to the id path of the new entity.  Can only be performed by an entity that contains both `source_entity1` and the specified path of `destination_entity1`. If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(clone_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
))&", R"(["NamedEntity1" "NamedEntity2" "_539JylCpbqn"])", "", R"((destroy_entities "Entity1" "Entity2"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CLONE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[i]);
		if(source_entity == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		auto erbr = source_entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
		size_t num_new_entities = erbr->size();

		//pause while allocating the new entity
		auto lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		//create new entity
		Entity *new_entity = new Entity(source_entity);

		lab_pause.Resume();

		//clear previous locks
		source_entity = EntityReadReference();
		erbr.Clear();

		//get destination if applicable
		EntityWriteReference destination_entity_parent;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
			std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i + 1]);

		if(destination_entity_parent == nullptr
			|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//accumulate usage
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

		destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			delete new_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(destination_entity_parent == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity));
	}

	return new_entity_ids_list;
}

static OpcodeInitializer _ENT_MOVE_ENTITIES(ENT_MOVE_ENTITIES, &Interpreter::InterpretNode_ENT_MOVE_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path source_entity1 [id_path destination_entity1] [id_path source_entity2] [id_path destination_entity2] [...])";
	d.returns = R"(list of id_path)";
	d.description = R"(Moves the entity from location specified by `source_entity1` to destination `destination_entity1`.  If `destination_entity1` exists, it will move `source_entity1` using `source_entity1`'s current id path into `destination_entity1`.  If `destination_entity1` does not exist, then it will move `source_entity1` and rename it to the end of the id path specified by `destination_entity1`. Can only be performed by a containing entity relative to both ids.  If multiple entities are specified, it will move each from the source to the destination.  Evaluates to a list of the new entity ids.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "NamedEntity1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "NamedEntity2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{m 3 n 4}
	)
	(move_entities "Entity1" "Entity2")
	(contained_entities "Entity2")
))&", R"(["NamedEntity1" "NamedEntity2" "_539JylCpbqn"])", "", R"((destroy_entities "Entity2"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MOVE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	EvaluableNodeReference new_entity_ids_list(evaluableNodeManager->AllocNode(ENT_LIST), true);
	new_entity_ids_list->ReserveOrderedChildNodes((ocn.size() + 1) / 2);
	auto node_stack = CreateOpcodeStackStateSaver(new_entity_ids_list);

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get the id of the source entity
		auto source_id_node = InterpretNodeForImmediateUse(ocn[i]);

		auto [source_entity, source_entity_parent]
			= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, source_id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(source_id_node);

		if(source_entity == nullptr || source_entity_parent == nullptr || source_entity == curEntity)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//can't move if being executed
		if(source_entity->IsEntityCurrentlyBeingExecuted())
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		//remove source entity from its parent
		source_entity_parent->RemoveContainedEntity(source_entity->GetIdStringId(), writeListeners);

		//clear lock if applicable
		source_entity_parent.ReleaseReference();

		//get destination if applicable
		EntityWriteReference destination_entity_parent;
		StringRef new_entity_id;
		if(i + 1 < ocn.size())
			std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[i + 1]);
		else
			destination_entity_parent = EntityWriteReference(curEntity);

		if(destination_entity_parent == nullptr)
		{
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			source_entity.ReleaseReference();
			delete source_entity;
			continue;
		}

		//put it in the destination
		destination_entity_parent->AddContainedEntityViaReference(source_entity, new_entity_id, writeListeners);

		if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
		{
			source_entity.ReleaseReference();
			delete source_entity;
			new_entity_ids_list->AppendOrderedChildNode(nullptr);
			continue;
		}

		if(destination_entity_parent == curEntity)
			new_entity_ids_list->AppendOrderedChildNode(evaluableNodeManager->AllocNode(ENT_STRING, new_entity_id));
		else //need an id path
			new_entity_ids_list->AppendOrderedChildNode(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, source_entity));
	}

	return new_entity_ids_list;
}

static OpcodeInitializer _ENT_DESTROY_ENTITIES(ENT_DESTROY_ENTITIES, &Interpreter::InterpretNode_ENT_DESTROY_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] [id_path entity2] [...])";
	d.returns = R"(bool)";
	d.description = R"(Destroys the entities specified by the ids `entity1`, `entity2`, etc. Can only be performed by containing entity.  Returns true if all entities were successfully destroyed, false if not.  Generally entities can be destroyed unless they do not exist or if there is code currently being run in it.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 3 b 4}
	)
	(destroy_entities "Entity")
	(contained_entities)
))&", R"([])"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::UNORDERED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 1.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_DESTROY_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	bool all_destroys_successful = true;
	for(auto &cn : en->GetOrderedChildNodesReference())
	{
		//get the id of the source entity
		auto id_node = InterpretNodeForImmediateUse(cn);
		auto [entity, entity_container]
			= TraverseToEntityReferenceAndContainerViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, id_node);
		evaluableNodeManager->FreeNodeTreeIfPossible(id_node);

		//need a valid entity that isn't itself or currently has execution
		if(entity == nullptr || entity == curEntity || entity->IsEntityCurrentlyBeingExecuted())
		{
			all_destroys_successful = false;
			continue;
		}

		//lock all entities
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
		{
			all_destroys_successful = false;
			continue;
		}

		if(entity_container != nullptr)
			entity_container->RemoveContainedEntity(entity->GetIdStringId(), writeListeners);

		contained_entities.Clear();

		//accumulate usage -- gain back freed resources
		if(ConstrainedAllocatedNodes())
			interpreterConstraints->curNumAllocatedNodesAllocatedToEntities -= entity->GetDeepSizeInNodes();

		entity.ReleaseReference();
		delete entity;
	}

	return AllocReturn(all_destroys_successful, immediate_result);
}

static OpcodeInitializer _ENT_LOAD(ENT_LOAD, &Interpreter::InterpretNode_ENT_LOAD, []() {
	OpcodeDetails d;
	d.parameters = R"(string resource_path [string resource_type] [assoc params])";
	d.returns = R"(any)";
	d.description = R"(Loads the data specified by `resource_path`, parses it into the appropriate code and data, and returns it. If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.amlg") (system "system" "rm file.amlg")))"},
			{R"&((seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.json") (system "system" "rm file.json")))"},
			{R"&((seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.yaml") (system "system" "rm file.yaml")))"},
			{R"&((seq
	(store "file.txt" "This is text.")
	(load "test.txt")
))&", R"((null))"},
			{R"&((seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.caml") (system "system" "rm file.caml")))"},
			{R"&((seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
))&", R"([
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.csv") (system "system" "rm file.csv")))"}
		});
	d.permissions = ExecutionPermissions::Permission::LOAD;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 8.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::LOAD))
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 1)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[1]);
		if(valid)
			file_type = file_type_temp;
	}

	AssetManager::AssetParameters asset_params(path, file_type, false);

	if(ocn.size() > 2)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[2]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params.SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params.UpdateResources();

	EntityExternalInterface::LoadEntityStatus status;
	return asset_manager.LoadResource(&asset_params, evaluableNodeManager, status);
}

static OpcodeInitializer _ENT_LOAD_ENTITY(ENT_LOAD_ENTITY, &Interpreter::InterpretNode_ENT_LOAD_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(string resource_path [id_path entity] [string resource_type] [bool persistent] [assoc params])";
	d.returns = R"(id_path)";
	d.description = R"(Loads the data specified by `resource_path` and parse it into the appropriate code and data, and stores it in `entity`.  It follows the same id path creation rules as `(create_entities)`, except that if no id path is specified, it may default to a name based on the resource if available.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(seq
		(create_entities
			"Entity"
			{a 1 b 2}
		)
		(create_entities
			["Entity" "Contained1"]
			{c 3}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_1"]
			{d 4}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_2"]
			{e 5}
		)
		(store_entity "entity.amlg" "Entity")
		(load_entity "entity.amlg" "EntityCopy")
		(difference_entities "Entity" "EntityCopy")
	)
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))", "", R"((seq (destroy_entities "Entity" "EntityCopy") (if (= (system "os") "Windows") (seq (system "system" "del /q entity*") (system "system" "rmdir /s /q entity")) (system "system" "rm -rf entity*"))))"},
			{R"&((seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))"}
		});
	d.permissions = ExecutionPermissions::Permission::LOAD;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_LOAD_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::LOAD))
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	bool persistent = false;
	if(ocn.size() > 3)
		persistent = InterpretNodeIntoBoolValue(ocn[3]);

	AssetManager::AssetParametersRef asset_params
		= std::make_shared<AssetManager::AssetParameters>(path, file_type, true);
	if(ocn.size() > 4)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[4]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params->SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params->UpdateResources();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 1)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[1]);

	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	EntityExternalInterface::LoadEntityStatus status;
	std::string random_seed = destination_entity_parent->CreateRandomStreamFromStringAndRand(asset_params->resourcePath);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	//don't want to call evaluableNodeManager->PauseLocalAllocationBuffer() here because loading entity may execute code
	Entity *loaded_entity = asset_manager.LoadEntityFromResource(asset_params, persistent, random_seed, this, status);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock.lock();
#endif

	//handle errors
	if(!status.loaded)
		return EvaluableNodeReference::Null();

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += loaded_entity->GetDeepSizeInNodes();

	//put it in the destination
	destination_entity_parent->AddContainedEntityViaReference(loaded_entity, new_entity_id, writeListeners);

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id path
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, loaded_entity), true);
}

static OpcodeInitializer _ENT_STORE(ENT_STORE, &Interpreter::InterpretNode_ENT_STORE, []() {
	OpcodeDetails d;
	d.parameters = R"(string resource_path * node [string resource_type] [assoc params])";
	d.returns = R"(bool)";
	d.description = R"(Stores `node` into `resource_path`.  Returns true if successful, false if not.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(store
		"file.amlg"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.amlg")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.amlg") (system "system" "rm file.amlg")))"},
			{R"&((seq
	(store
		"file.json"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.json")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.json") (system "system" "rm file.json")))"},
			{R"&((seq
	(store
		"file.yaml"
		[
			1
			2
			3
			{a 1 b 2 c (null)}
		]
	)
	(load "file.yaml")
))&", R"([
	1
	2
	3
	{a 1 b 2 c (null)}
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.yaml") (system "system" "rm file.yaml")))"},
			{R"&((seq
	(store "file.txt" "This is text.")
	(load "test.txt")
))&", R"((null))"},
			{R"&((seq
	(store
		"file.caml"
		(lambda
			(seq
				(print "hello")
			)
		)
	)
	(load "file.caml")
))&", R"((seq
	(print "hello")
))", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.caml") (system "system" "rm file.caml")))"},
			{R"&((seq
	(declare
		{
			csv_data [
					[6.4 2.8 5.6 2.2 "virginica"]
					[4.9 2.5 4.5 1.7 "virg\"inica"]
					[]
					["" "" "" (null)]
					[4.9 3.1 1.5 0.1 "set\nosa" 3]
					[4.4 3.2 1.3 0.2 "setosa"]
				]
		}
	)
	(store "file.csv" csv_data)
	(load "file.csv")
))&", R"([
	[6.4 2.8 5.6 2.2 "virginica"]
	[4.9 2.5 4.5 1.7 "virg\"inica"]
	[(null)]
	[(null) (null) (null) (null)]
	[4.9 3.1 1.5 0.1 "set\nosa" 3]
	[4.4 3.2 1.3 0.2 "setosa"]
])", "", R"((if (= (system "os") "Windows") (system "system" "del /q file.csv") (system "system" "rm file.csv")))"}
		});
	d.permissions = ExecutionPermissions::Permission::STORE;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::STORE))
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	auto to_store = InterpretNodeForImmediateUse(ocn[1]);
	auto node_stack = CreateOpcodeStackStateSaver(to_store);

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	AssetManager::AssetParameters asset_params(path, file_type, false);
	if(ocn.size() > 3)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[3]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params.SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params.UpdateResources();

	bool successful_save = asset_manager.StoreResource(to_store, &asset_params, evaluableNodeManager);
	evaluableNodeManager->FreeNodeTreeIfPossible(to_store);

	return AllocReturn(successful_save, immediate_result);
}

static OpcodeInitializer _ENT_STORE_ENTITY(ENT_STORE_ENTITY, &Interpreter::InterpretNode_ENT_STORE_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(string resource_path id_path entity [string resource_type] [bool persistent] [assoc params])";
	d.returns = R"(bool)";
	d.description = R"(Stores `entity` into `resource_path`.  Returns true if successful, false if not.  If `persistent` is true, default is false, then any modifications to the entity or any entity contained within it will be written out to the resource, so that the memory and persistent storage are synchronized.  If `resource_type` is specified and not null, it will use `resource_type` as the format instead of inferring the format from the extension of the `resource_path`.  File formats supported are amlg, json, yaml, csv, and caml; anything not in this list will be loaded as a binary string.  `params` is a per resource type set of parameters described in Amalgam Syntax.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(seq
		(create_entities
			"Entity"
			{a 1 b 2}
		)
		(create_entities
			["Entity" "Contained1"]
			{c 3}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_1"]
			{d 4}
		)
		(create_entities
			["Entity" "Contained1" "Contained2_2"]
			{e 5}
		)
		(store_entity "entity.amlg" "Entity")
		(load_entity "entity.amlg" "EntityCopy")
		(difference_entities "Entity" "EntityCopy")
	)
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))", "", R"((seq (destroy_entities "Entity" "EntityCopy") (if (= (system "os") "Windows") (seq (system "system" "del /q entity*") (system "system" "rmdir /s /q entity")) (system "system" "rm -rf entity*"))))"},
			{R"&((seq
	(create_entities
		"Entity"
		[1 2 3 4]
	)
	(create_entities
		["Entity" "Contained1"]
		[5 6 7]
	)
	(create_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 8 nine 9}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_3"]
		[12 13]
	)
	(store_entity
		"entity.caml"
		"Entity"
		(null)
		.true
		{flatten .true transactional .true}
	)
	(create_entities
		["Entity" "Contained1" "Contained1_2"]
		[10 11]
	)
	(destroy_entities
		["Entity" "Contained1" "Contained1_3"]
	)
	(assign_to_entities
		["Entity" "Contained1" "Contained1_1"]
		{eight 88}
	)
	(load_entity
		"entity.caml"
		"EntityCopy"
		(null)
		.false
		{execute_on_load .true require_version_compatibility .true transactional .true}
	)
	(declare
		{
			diff (difference_entities "EntityCopy" "Entity")
		}
	)
	(destroy_entities "EntityCopy" "Entity")
	diff
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))"}
		});
	d.permissions = ExecutionPermissions::Permission::STORE;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_STORE_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto permissions = asset_manager.GetEntityPermissions(curEntity);
	if(!permissions.HasPermission(ExecutionPermissions::Permission::STORE))
		return EvaluableNodeReference::Null();

	std::string path = InterpretNodeIntoStringValueEmptyNull(ocn[0]);
	if(path.empty())
		return EvaluableNodeReference::Null();

	std::string file_type = "";
	if(ocn.size() > 2)
	{
		auto [valid, file_type_temp] = InterpretNodeIntoStringValue(ocn[2]);
		if(valid)
			file_type = file_type_temp;
	}

	bool update_persistence = false;
	bool persistent = false;
	if(ocn.size() > 3)
	{
		auto persistence_node = InterpretNodeForImmediateUse(ocn[3]);
		if(!EvaluableNode::IsNull(persistence_node))
		{
			update_persistence = true;
			persistent = EvaluableNode::ToBool(persistence_node);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(persistence_node);
	}

	AssetManager::AssetParametersRef asset_params
		= std::make_shared<AssetManager::AssetParameters>(path, file_type, true);
	if(ocn.size() > 4)
	{
		EvaluableNodeReference params = InterpretNodeForImmediateUse(ocn[4]);

		if(EvaluableNode::IsAssociativeArray(params))
			asset_params->SetParams(params->GetMappedChildNodesReference());

		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}
	asset_params->UpdateResources();

	//get the id of the source entity to store.  Don't need to keep the reference because it won't be used once the source entity pointer is looked up
	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	//StoreEntityToResource will read lock all contained entities appropriately
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[1]);
	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	bool stored_successfully = asset_manager.StoreEntityToResource(source_entity, asset_params,
		update_persistence, persistent);

	return AllocReturn(stored_successfully, immediate_result);
}

static OpcodeInitializer _ENT_CONTAINS_ENTITY(ENT_CONTAINS_ENTITY, &Interpreter::InterpretNode_ENT_CONTAINS_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity)";
	d.returns = R"(bool)";
	d.description = R"(Returns true if `entity` exists, false if not.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(create_entities
		["Entity" "Contained1"]
		{c 3}
	)
	[
		(contains_entity "Entity")
		(contains_entity
			["Entity" "NotAnEntity"]
		)
	]
))&", R"([.true .false])", "", R"((destroy_entities "Entity")"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	return AllocReturn(entity != nullptr, immediate_result);
}

static OpcodeInitializer _ENT_FLATTEN_ENTITY(ENT_FLATTEN_ENTITY, &Interpreter::InterpretNode_ENT_FLATTEN_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity [bool include_rand_seeds] [bool parallel_create] [bool include_version])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to code that, if called, would completely reproduce the `entity`, as well as all contained entities.  If `include_rand_seeds` is true, by default, it will include all entities' random seeds.  If `parallel_create` is true, then the creates will be performed with parallel markers as appropriate for each group of contained entities.  If `include_version` is true, it will include a comment on the top node that is the current version of the Amalgam interpreter, which can be used for validating interoperability when loading code.  The code returned accepts two parameters, `create_new_entity`, which defaults to true, and `new_entity`, which defaults to null.  If `create_new_entity` is true, then it will create a new entity using the id path specified by `new_entity`, where null will create an unnamed entity.  If `create_new_entity` is false, then it will overwrite the current entity's code and create all contained entities.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"FlattenEntity"
		(lambda {})
	)
	(call_entity "FlattenEntity" "a")
	(create_entities
		["FlattenEntity" "DeepRand"]
		(lambda
			{a (rand)}
		)
	)
	(declare
		{
			flattened_code (flatten_entity "FlattenEntity" .true .true)
		}
	)
	(declare
		{first_rand (null) second_rand (null)}
	)
	(assign
		{
			first_rand (call_entity
					["FlattenEntity" "DeepRand"]
					"a"
				)
		}
	)
	(let
		{
			new_entity (call flattened_code)
		}
		(assign
			{
				second_rand (call_entity
						[new_entity "DeepRand"]
						"a"
					)
			}
		)
	)
	[first_rand second_rand]
))&", R"([0.611779739433564 0.611779739433564])", "", R"((apply "destroy_entities" (contained_entities)))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_FLATTEN_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	bool include_rand_seeds = true;
	if(ocn.size() > 1)
		include_rand_seeds = InterpretNodeIntoBoolValue(ocn[1]);

	bool parallel_create = false;
	if(ocn.size() > 2)
		parallel_create = InterpretNodeIntoBoolValue(ocn[2]);

	bool include_version = false;
	if(ocn.size() > 3)
		include_version = InterpretNodeIntoBoolValue(ocn[3]);

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	auto erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
	return EntityManipulation::FlattenEntity(evaluableNodeManager, entity, erbr,
		include_rand_seeds, parallel_create, include_version);
}

static OpcodeInitializer _ENT_RETRIEVE_ENTITY_ROOT(ENT_RETRIEVE_ENTITY_ROOT, &Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the code contained by `entity`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{1 2 three 3}
		)
	)
	(retrieve_entity_root "Entity")
))&", R"({1 2 three 3})", "", R"((destroy_entities "Entity"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_ENTITY_ROOT(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();
	auto &ocn = en->GetOrderedChildNodesReference();

	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();


	return target_entity->GetRoot(evaluableNodeManager);
}

static OpcodeInitializer _ENT_ASSIGN_ENTITY_ROOTS(ENT_ASSIGN_ENTITY_ROOTS, &Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] * root1 [id_path entity2] [* root2] [...])";
	d.returns = R"(bool)";
	d.description = R"(Sets the code of the `entity1 to `root1`, as well as all subsequent entity-code pairs of parameters.  If `entity1` is not specified or null, then uses the current entity.  On assigning the code to the new entity, any root that is not of a type assoc will be put into an assoc under the null key.  If all assignments were successful, then returns true, otherwise returns false.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{1 2 three 3}
		)
	)
	(assign_entity_roots
		"Entity"
		{a 4 b 5 c 6}
	)
	(retrieve_entity_root "Entity")
))&", R"({a 4 b 5 c 6})", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_ENTITY_ROOTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	bool all_assignments_successful = true;

	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get value to assign first before getting the entity in case it needs to be locked
		EvaluableNodeReference new_code = EvaluableNodeReference::Null();
		if(i + 1 < ocn.size())
			new_code = InterpretNodeForImmediateUse(ocn[i + 1]);
		else
			new_code = InterpretNodeForImmediateUse(ocn[i]);
		auto node_stack = CreateOpcodeStackStateSaver(new_code);

		EntityWriteReference target_entity;
		if(i + 1 < ocn.size())
		{
			target_entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[i]);

			//if didn't find an entity, then use current one
			if(target_entity == nullptr)
			{
				all_assignments_successful = false;
				evaluableNodeManager->FreeNodeTreeIfPossible(new_code);
				continue;
			}
		}
		else
		{
			target_entity = EntityWriteReference(curEntity);
		}

		//pause if allocating to another entity
		EvaluableNodeManager::LocalAllocationBufferPause lab_pause;
		if(target_entity != curEntity)
			lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		size_t prev_size = 0;
		if(ConstrainedAllocatedNodes())
			prev_size = target_entity->GetSizeInNodes();

		target_entity->SetRoot(new_code, false, writeListeners);

		if(ConstrainedAllocatedNodes())
		{
			size_t cur_size = target_entity->GetSizeInNodes();
			//don't get credit for freeing memory, but do count toward memory consumed
			if(cur_size > prev_size)
				interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += cur_size - prev_size;
		}

		lab_pause.Resume();

		if(target_entity != curEntity)
		{
			//don't need to set side effects because the data was copied, not directly assigned
		#ifdef AMALGAM_MEMORY_INTEGRITY
			VerifyEvaluableNodeIntegrity();
		#endif

			target_entity->CollectGarbageWithEntityWriteReference();

		#ifdef AMALGAM_MEMORY_INTEGRITY
			VerifyEvaluableNodeIntegrity();
		#endif
		}

		evaluableNodeManager->FreeNodeTreeIfPossible(new_code);
	}

	return AllocReturn(all_assignments_successful, immediate_result);
}

static OpcodeInitializer _ENT_GET_ENTITY_PERMISSIONS(ENT_GET_ENTITY_PERMISSIONS, &Interpreter::InterpretNode_ENT_GET_ENTITY_PERMISSIONS, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity])";
	d.returns = R"(assoc)";
	d.description = R"(Returns an assoc of the permissions of `entity`, the current entity if `entity` is not specified or null, where each key is the permission and each value is either true or false.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		(lambda
			(print (system_time))
		)
	)
	(get_entity_permissions "Entity")
))&", R"({
	alter_performance .false
	environment .false
	load .false
	std_in .false
	std_out_and_std_err .false
	store .false
	system .false
})", "", R"((destroy_entities "Entity"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_PERMISSIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	EntityReadReference entity;
	if(ocn.size() > 0)
		entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		entity = EntityReadReference(curEntity);

	auto entity_permissions = asset_manager.GetEntityPermissions(entity);
	//clear lock
	entity = EntityReadReference();

	return EvaluableNodeReference(entity_permissions.GetPermissionsAsEvaluableNode(evaluableNodeManager), true);
}

static OpcodeInitializer _ENT_SET_ENTITY_PERMISSIONS(ENT_SET_ENTITY_PERMISSIONS, &Interpreter::InterpretNode_ENT_SET_ENTITY_PERMISSIONS, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity bool|assoc permissions [bool deep])";
	d.returns = R"(id_path)";
	d.description = R"(Sets the permissions on the `entity`.  If permissions is true, then it grants all permissions, if it is false, then it removes all.  If permissions is an assoc, it alters the permissions of the assoc keys to the boolean values of the assoc's values.  Permission keys consist of: "std_out_and_std_err", which allows output; "std_in", which allows input; "load", which allows reading files; "store", which allows writing files; "environment", which allows reading information about the environment; "alter_performance", which allows adjusting performance characteristics; and "system", which allows running system commands.  The parameter `deep` defaults to false, but if it is true, all contained entities have their permissions updated.  Returns the id path of `entity`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		(lambda
			(print (system_time))
		)
	)
	(set_entity_permissions "Entity" .true)
	(get_entity_permissions "Entity")
))&", R"({
	alter_performance .true
	environment .true
	load .true
	std_in .true
	std_out_and_std_err .true
	store .true
	system .true
})", "", R"((destroy_entities "Entity"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ENTITY_PERMISSIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	size_t num_params = ocn.size();

	if(num_params < 2)
		return EvaluableNodeReference::Null();

	//retrieve parameter to determine whether to deep set the seeds, if applicable
	bool deep_set = true;
	if(num_params > 2)
		deep_set = InterpretNodeIntoBoolValue(ocn[2], true);

	EvaluableNodeReference permissions_en = InterpretNodeForImmediateUse(ocn[1]);

	auto [permissions_to_set, permission_values] = ExecutionPermissions::EvaluableNodeToPermissions(permissions_en);

	//any permissions set by this entity need to be filtered by the current entity's permissions
	auto current_entity_permissions = asset_manager.GetEntityPermissions(curEntity);
	permissions_to_set.allPermissions &= current_entity_permissions.allPermissions;
	permission_values.allPermissions &= current_entity_permissions.allPermissions;

	//get the id of the entity
	auto id_node = InterpretNode(ocn[0]);
	EntityWriteReference entity = TraverseToExistingEntityReferenceViaEvaluableNodeIDPath<EntityWriteReference>(curEntity, id_node);

	if(entity == nullptr)
		return EvaluableNodeReference::Null();

#ifdef MULTITHREAD_SUPPORT
	if(deep_set)
	{
		auto contained_entities = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityWriteReference>();
		if(contained_entities == nullptr)
			return EvaluableNodeReference::Null();

		entity->SetPermissions(permissions_to_set, permission_values, true, writeListeners, &contained_entities);
	}
	else
	#endif
		entity->SetPermissions(permissions_to_set, permission_values, deep_set, writeListeners);

	return id_node;
}
