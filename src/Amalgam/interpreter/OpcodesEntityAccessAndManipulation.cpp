//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"
#include "PerformanceProfiler.h"

static std::string _opcode_group = "Entity Access and Manipulation";

static OpcodeInitializer _ENT_CONTAINS_LABEL(ENT_CONTAINS_LABEL, &Interpreter::InterpretNode_ENT_CONTAINS_LABEL, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] string label_name)";
	d.returns = R"(bool)";
	d.description = R"(Evaluates to true if the label represented by `label_name` exists for `entity`.  If `entity` is omitted or null, then it uses the current entity.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	[
		(contains_label "Entity" "a")
		(contains_label "Entity" "z")
	]
))&", R"([.true .false])", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CONTAINS_LABEL(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get label to look up
	size_t label_param_index = (ocn.size() > 1 ? 1 : 0);
	//don't need an extra reference because will be false anyway if the string doesn't exist
	StringInternPool::StringID label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[label_param_index]);
	if(label_sid == StringInternPool::NOT_A_STRING_ID)
		return EvaluableNodeReference::Null();

	//get the id of the entity
	EntityReadReference target_entity;
	if(ocn.size() > 1)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	//if no entity, clean up assignment assoc
	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	//make sure not trying to access a private label
	if(target_entity != curEntity && Entity::IsLabelPrivate(label_sid))
		return EvaluableNodeReference::Null();

	return AllocReturn(target_entity->DoesLabelExist(label_sid), immediate_result);
}

static OpcodeInitializer _ENT_ASSIGN_TO_ENTITIES(ENT_ASSIGN_TO_ENTITIES, &Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_REMOVE_FROM_ENTITIES_and_ACCUM_TO_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...])";
	d.returns = R"(bool)";
	d.description = R"(For each index-value pair of `label_value_pairs`, assigns the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	(assign_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{three 12}
	)
	(retrieve_entity_root "Entity")
))&", R"({
	a 2
	b 3
	c 4
	three 12
})", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 10.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_ACCUM_TO_ENTITIES(ENT_ACCUM_TO_ENTITIES, &Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_REMOVE_FROM_ENTITIES_and_ACCUM_TO_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] assoc label_value_pairs1 [id_path entity2] [assoc label_value_pairs2] [...])";
	d.returns = R"(bool)";
	d.description = R"(For each index-value pair of `label_value_pairs`, it accumulates the value to the label on the contained entity represented by the respective `entity`, itself if `entity` is not specified or is null.  If the label is not found, it will create it.  Returns true if all assignments were successful, false if not.  Accumulation is performed differently based on the type: for numeric values it adds, for strings, it concatenates, for lists it appends, and for assocs it appends based on the pair.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2 c 3}
	)
	(accum_to_entities
		"Entity"
		{a 2 b 3 c 4}
		"Entity"
		{doesnt_exist 12}
	)
	(retrieve_entity_root "Entity")
))&", R"({a 3 b 5 c 7})", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 3.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_REMOVE_FROM_ENTITIES(ENT_REMOVE_FROM_ENTITIES, &Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_REMOVE_FROM_ENTITIES_and_ACCUM_TO_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity1] string|list label_names1 [id_path entity2] [list string|label_names2] [...])";
	d.returns = R"(bool)";
	d.description = R"(Removes all labels in `label_names1` from `entity1` and so on for each respective entity and label list.  Returns true if all removes were successful, false otherwise.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{
			a 1
			b 2
			c 3
			d 4
		}
	)
	(remove_from_entities
		"Entity"
		"a"
		"Entity"
		["b" "c"]
	)
	(retrieve_entity_root "Entity")
))&", R"({d 4})", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::PAIRED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_ASSIGN_TO_ENTITIES_and_REMOVE_FROM_ENTITIES_and_ACCUM_TO_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(!CanModifyEntityFromConstraints())
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	bool remove_from_entities = (en->GetType() == ENT_REMOVE_FROM_ENTITIES);
	bool accum_to_entities = (en->GetType() == ENT_ACCUM_TO_ENTITIES);

	bool all_assignments_successful = true;
	for(size_t i = 0; i < ocn.size(); i += 2)
	{
		//get variables to assign
		size_t assoc_param_index = (i + 1 < ocn.size() ? i + 1 : i);
		auto assigned_vars = InterpretNode(ocn[assoc_param_index]);

		if((!remove_from_entities && !EvaluableNode::IsAssociativeArray(assigned_vars))
			|| (remove_from_entities
				&& !EvaluableNode::IsOrderedArray(assigned_vars)
				&& !EvaluableNode::IsString(assigned_vars)))
		{
			all_assignments_successful = false;
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
			continue;
		}
		auto node_stack = CreateOpcodeStackStateSaver(assigned_vars);

		EntityWriteReference target_entity;
		if(i + 1 < ocn.size())
			target_entity = InterpretNodeIntoRelativeSourceEntityWriteReference(ocn[i]);
		else
			target_entity = EntityWriteReference(curEntity);

		//if no entity, can't successfully assign
		if(target_entity == nullptr)
		{
			all_assignments_successful = false;
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
			continue;
		}

		size_t num_new_nodes_allocated = 0;

		//pause if allocating to another entity
		EvaluableNodeManager::LocalAllocationBufferPause lab_pause;
		if(target_entity != curEntity)
			lab_pause = evaluableNodeManager->PauseLocalAllocationBuffer();

		bool any_success = false;
		bool all_success = false;

		if(remove_from_entities)
			std::tie(any_success, all_success) = target_entity->RemoveLabels(
										assigned_vars, writeListeners,
										(ConstrainedAllocatedNodes() ? &num_new_nodes_allocated : nullptr), target_entity == curEntity);
		else
			std::tie(any_success, all_success) = target_entity->SetValuesAtLabels(
										assigned_vars, accum_to_entities, writeListeners,
										(ConstrainedAllocatedNodes() ? &num_new_nodes_allocated : nullptr), target_entity == curEntity);

		lab_pause.Resume();

		if(any_success)
		{
			if(ConstrainedAllocatedNodes())
				interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += num_new_nodes_allocated;

			if(target_entity == curEntity)
			{
				if(!assigned_vars.unique)
					SetSideEffectFlagsAndAccumulatePerformanceCounters(en);
			}
			else
			{
			#ifdef AMALGAM_MEMORY_INTEGRITY
				VerifyEvaluableNodeIntegrity();
			#endif

				target_entity->CollectGarbageWithEntityWriteReference();

			#ifdef AMALGAM_MEMORY_INTEGRITY
				VerifyEvaluableNodeIntegrity();
			#endif
			}
		}
		//clear write lock as soon as possible, but pull out pointer first to compare for gc
		Entity *target_entity_raw_ptr = target_entity;
		target_entity.ReleaseReference();

		//if assigning to a different entity, it can be cleared
		if(target_entity_raw_ptr != curEntity)
		{
			node_stack.PopEvaluableNode();
			evaluableNodeManager->FreeNodeTreeIfPossible(assigned_vars);
		}

		if(!all_success)
			all_assignments_successful = false;

		//check this at the end of each iteration in case need to exit
		if(AreExecutionResourcesExhausted())
			return EvaluableNodeReference::Null();
	}

	return AllocReturn(all_assignments_successful, immediate_result);
}

static OpcodeInitializer _ENT_RETRIEVE_FROM_ENTITY(ENT_RETRIEVE_FROM_ENTITY, &Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] [string|list|assoc label_names])";
	d.returns = R"(any)";
	d.description = R"(Retrieves one or more labels from `entity`, using its own entity if `entity` is omitted or null.  If `label_names` is a string, it returns the value at the corresponding label.  If `label_names` is a list, it returns a list of the values of the labels of the corresponding labels.  If `label_names` is an assoc, it an assoc with label names as keys and the label values as the values.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 12 b 13}
	)
	[
		(retrieve_from_entity "Entity" "a")
		(retrieve_from_entity
			"Entity"
			["a" "b"]
		)
		(retrieve_from_entity
			"Entity"
			(zip
				["a" "b"]
				null
			)
		)
	]
))&", R"([
	12
	[12 13]
	{a 12 b 13}
])", "", R"((destroy_entities "Entity"))"}
		});
	d.orderedChildNodeType = OpcodeDetails::OrderedChildNodeType::ORDERED;
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.frequencyPer10000Opcodes = 7.5;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_RETRIEVE_FROM_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to work within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get lookup reference
	size_t lookup_param_index = (ocn.size() > 1 ? 1 : 0);
	auto to_lookup = InterpretNodeForImmediateUse(ocn[lookup_param_index]);
	auto node_stack = CreateOpcodeStackStateSaver(to_lookup);

	//get the id of the source to check
	EntityReadReference target_entity;
	if(ocn.size() > 1)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	//get the value(s)
	if(EvaluableNode::IsImmediate(to_lookup))
	{
		StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(to_lookup);
		EvaluableNodeReference value = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager,
			immediate_result, target_entity == curEntity).first;

		evaluableNodeManager->FreeNodeTreeIfPossible(to_lookup);

		return value;
	}
	else if(to_lookup->IsAssociativeArray())
	{
		//reference to keep track of to_lookup nodes to free
		EvaluableNodeReference cnr(static_cast<EvaluableNode *>(nullptr), to_lookup.unique);

		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		bool first_node = true;
		for(auto &[cn_id, cn] : to_lookup->GetMappedChildNodesReference())
		{
			//if there are values passed in, free them to be clobbered
			cnr.SetReference(cn);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			auto [value, _] = target_entity->GetValueAtLabel(cn_id, evaluableNodeManager,
				EvaluableNodeRequestedValueTypes::Type::NONE, target_entity == curEntity);

			cn = value;
			to_lookup.UpdatePropertiesBasedOnAttachedNode(value, first_node);
			first_node = false;
		}

		return to_lookup;
	}
	else //ordered params
	{
		//reference to keep track of to_lookup nodes to free
		EvaluableNodeReference cnr(static_cast<EvaluableNode *>(nullptr), to_lookup.unique);

		evaluableNodeManager->EnsureNodeIsModifiable(to_lookup);

		//overwrite values in the ordered
		auto &lookup_ocn = to_lookup->GetOrderedChildNodesReference();
		for(size_t i = 0; i < lookup_ocn.size(); i++)
		{
			auto &cn = lookup_ocn[i];
			StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(cn);

			//if there are values passed in, free them to be clobbered
			cnr.SetReference(cn);
			evaluableNodeManager->FreeNodeTreeIfPossible(cnr);

			auto [value, _] = target_entity->GetValueAtLabel(label_sid, evaluableNodeManager,
				EvaluableNodeRequestedValueTypes::Type::NONE, target_entity == curEntity);

			cn = value;
			to_lookup.UpdatePropertiesBasedOnAttachedNode(value, i == 0);
		}

		return to_lookup;
	}
}

static OpcodeInitializer _ENT_CALL_ENTITY(ENT_CALL_ENTITY, &Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ON_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity [string label_name] [assoc params] [bool|assoc constraints] [bool return_warnings] [bool get_changes])";
	d.returns = R"(any)";
	d.description = R"(Calls the contained `entity` and returns the result of the call.  If `label_name` is specified, then it will call the label specified by string, otherwise it will call the null label.  If `params` is specified, then it will pass those as the parameters on the scope stack.  If `constraints` is specified and not false or null, it will constrain execution.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.  If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.  If `get_changes` is true (the default is false), the value will be a tuple in the form of `[value change_log]`, where the change log is a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If both `return_warnings` and `get_changes` are true, then the tuple will be in the form of `[value warnings performance_constraint_violation change_log]`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{
				!private_method "should not access"
				copy_entity (while
						.true
						(clone_entities .null .null)
					)
				hello (declare
						{message ""}
						(concat "hello " message)
					)
				load (while .true)
			}
		)
	)
	[
		(call_entity
			"Entity"
			"hello"
			{message "world"}
		)
		(call_entity "Entity" "!private_method")
		(call_entity "Entity" "load" .null {max_node_operations 100})
		(call_entity
			"Entity"
			"copy_entity"
			.null
			{
				max_node_operations 1000
				max_node_allocations 1000
				max_operation_depth 10
				max_contained_entities 10
				max_contained_entity_depth 3
				max_entity_id_length 20
			}
			.true
			.true
		)
	]
)
)&", R"([
	"hello world"
	.null
	.null
	[.null {} "Execution step limit exceeded" (seq)]
])", "", R"((destroy_entities "Entity"))"},
{R"&((seq
	(create_entities
		"Entity"
		(lambda
			{
				a_assign (seq
						(create_entities
							"Contained"
							{a 4 b 6}
						)
						(assign_to_entities
							"Contained"
							{a 6 b 10}
						)
						(set_entity_rand_seed "Contained" "bbbb")
						(accum_to_entities
							"Contained"
							{b 12}
						)
						(destroy_entities "Contained")
					)
			}
		)
	)
	(set_entity_permissions "Entity" .true)
	(call_entity "Entity" "a_assign" .null .null .false .true)
))&", R"([
	.true
	(seq
		(create_entities
			["Entity" "Contained"]
			(lambda
				{a 4 b 6}
			)
		)
		(assign_to_entities
			["Entity" "Contained"]
			{a 6 b 10}
		)
		(set_entity_rand_seed
			["Entity" "Contained"]
			"bbbb"
			.false
		)
		(accum_to_entities
			["Entity" "Contained"]
			{b 12}
		)
		(destroy_entities
			["Entity" "Contained"]
		)
	)
])", "", R"((destroy_entities "Entity"))"}
		});
	d.requiresEntity = true;
	d.newScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::CONDITIONAL;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 48.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_CALL_ON_ENTITY(ENT_CALL_ON_ENTITY, &Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ON_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity * code [assoc params] [bool|assoc constraints] [bool return_warnings] [bool get_changes])";
	d.returns = R"(any)";
	d.description = R"(Calls `code` to be run on the contained `entity` and returns the result of the call.  If `params` is specified, then it will pass those as the parameters on the scope stack.  If `constraints` is specified and not false or null, it will constrain execution.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.  If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.  If `get_changes` is true (the default is false), the value will be a tuple in the form of `[value change_log]`, where the change log is a list of opcodes that hold an executable log of all of the changes that have elapsed to the entity and its contained entities.  The log may be evaluated to apply or re-apply the changes to any entity passed in to the executable log as the parameter "_".  If both `return_warnings` and `get_changes` are true, then the tuple will be in the form of `[value warnings performance_constraint_violation change_log]`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity"
		{a 1 b 2}
	)
	(set_entity_permissions "Entity" .true)
	(call_on_entity
		"Entity"
		(lambda
			[a b c]
		)
		{c 3}
	)
))&", R"([1 2 3])", "", R"((destroy_entities "Entity"))"}
		});
	d.requiresEntity = true;
	d.newScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_ENTITY_and_CALL_ON_ENTITY(
	EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to check within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	InterpreterConstraints interpreter_constraints;
	InterpreterConstraints *interpreter_constraints_ptr = nullptr;
	PopulateInterpreterConstraintsFromParams(ocn, 3, interpreter_constraints, true);
	if(interpreter_constraints.AnyActiveConstraints())
		interpreter_constraints_ptr = &interpreter_constraints;

	//need to return a more complex data structure, can't return immediate
	if(interpreter_constraints_ptr != nullptr && interpreter_constraints_ptr->collectWarnings)
		immediate_result = EvaluableNodeRequestedValueTypes::Type::NONE;

	//attempt to get arguments
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(ocn.size() > 2)
		args = InterpretNodeForImmediateUse(ocn[2]);

	auto node_stack = CreateOpcodeStackStateSaver(args);

	auto call_type = en->GetType();
	StringRef entity_label_sid;
	EvaluableNodeReference function = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
	{
		if(call_type == ENT_CALL_ON_ENTITY)
		{
			function = InterpretNodeForImmediateUse(ocn[1]);
			node_stack.PushEvaluableNode(function);
		}
		else
		{
			entity_label_sid.SetIDWithReferenceHandoff(InterpretNodeIntoStringIDValueWithReference(ocn[1]));
		}
	}

	bool get_changes = false;
	if(ocn.size() > 5)
		get_changes = InterpretNodeIntoBoolValue(ocn[5]);

	//current pointer to write listeners
	std::vector<EntityWriteListener *> *cur_write_listeners = writeListeners;
	//another storage container in case getting entity changes
	std::vector<EntityWriteListener *> get_changes_write_listeners;
	if(get_changes)
	{
		//add on extra listener and set pointer to this buffer
		// keep the copying here in this if statement so don't need to make copies when not calling ENT_CALL_ENTITY_GET_CHANGES
		if(writeListeners != nullptr)
			get_changes_write_listeners = *writeListeners;
		get_changes_write_listeners.push_back(new EntityWriteListener(curEntity, std::unique_ptr<std::ostream>(), true));
		cur_write_listeners = &get_changes_write_listeners;

		//ensure not returning an immediate value
		immediate_result = EvaluableNodeRequestedValueTypes::Type::NONE;
	}

	//get a write lock on the entity
	EntityReadReference called_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	if(called_entity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ce_enm = called_entity->evaluableNodeManager;

	if(_label_profiling_enabled)
		PerformanceProfiler::StartOperation(string_intern_pool.GetStringFromID(entity_label_sid),
			ce_enm.GetNumberOfUsedNodes());

#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack, then can release the entity lock
	Concurrency::ReadLock enm_lock = ce_enm.AcquireMemoryModificationReadLock();
	called_entity.lock.unlock();
#endif

	if(called_entity != curEntity)
	{
		if(call_type == ENT_CALL_ON_ENTITY)
		{
			//copy function to called_entity, free function from this entity
			EvaluableNodeReference called_entity_function(ce_enm.DeepAllocCopy(function), true);
			node_stack.PopEvaluableNode();
			//don't put freed nodes in local allocation buffer, because that will increase memory churn
			evaluableNodeManager->FreeNodeTreeIfPossible(function, false);
			function = called_entity_function;
		}

		//copy arguments to called_entity, free args from this entity
		EvaluableNodeReference called_entity_args(ce_enm.DeepAllocCopy(args), true);
		node_stack.PopEvaluableNode();
		//don't put freed nodes in local allocation buffer, because that will increase memory churn
		evaluableNodeManager->FreeNodeTreeIfPossible(args, false);
		args = called_entity_args;
	}

	auto scope_stack = ConvertArgsToScopeStack(args, ce_enm);

	PopulatePerformanceCounters(interpreter_constraints_ptr, called_entity);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result;
	if(call_type != ENT_CALL_ON_ENTITY)
		result = called_entity->Execute(StringInternPool::StringID(entity_label_sid),
			&scope_stack, called_entity == curEntity, this, cur_write_listeners, printListener,
			interpreter_constraints_ptr, immediate_result
#ifdef MULTITHREAD_SUPPORT
			, &enm_lock
#endif
		);
	else
		result = called_entity->ExecuteOnEntity(function, &scope_stack, this, cur_write_listeners, printListener,
			interpreter_constraints_ptr, immediate_result
#ifdef MULTITHREAD_SUPPORT
			, &enm_lock
#endif
		);

	//can't free args if the result might contain them
	if(result.unique)
		ce_enm.FreeNode(args);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock = evaluableNodeManager->AcquireMemoryModificationReadLock();
#endif

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, &ce_enm);

	//need to copy the value if nodes
	if(called_entity != curEntity && !result.IsImmediateValue())
	{
		EvaluableNodeReference copied_result = evaluableNodeManager->DeepAllocCopy(result);
		//don't put freed nodes in local allocation buffer, because that will increase memory churn
		ce_enm.FreeNodeTreeIfPossible(result, false);
		//function will be null if not ENT_CALL_ON_ENTITY
		ce_enm.FreeNodeTree(function, false);
		result = copied_result;
	}

	EvaluableNodeReference changes = EvaluableNodeReference::Null();
	if(get_changes)
	{
		EntityWriteListener *wl = get_changes_write_listeners.back();
		EvaluableNode *writes = wl->GetWrites();

		changes = evaluableNodeManager->DeepAllocCopy(writes);

		//delete the write listener and all of its memory
		delete wl;
	}

	if(_label_profiling_enabled)
		PerformanceProfiler::EndOperation(ce_enm.GetNumberOfUsedNodes());

	if(interpreterConstraints != nullptr)
		interpreterConstraints->AccruePerformanceCounters(interpreter_constraints_ptr);

	if(interpreter_constraints_ptr != nullptr && interpreter_constraints.constraintsExceeded)
		return BundleResultWithWarningsAndChangesIfNeeded(EvaluableNodeReference::Null(),
			interpreter_constraints_ptr, changes);

	return BundleResultWithWarningsAndChangesIfNeeded(result, interpreter_constraints_ptr, changes);
}

static OpcodeInitializer _ENT_CALL_CONTAINER(ENT_CALL_CONTAINER, &Interpreter::InterpretNode_ENT_CALL_CONTAINER, []() {
	OpcodeDetails d;
	d.parameters = R"(string parent_label_name [assoc params] [bool|assoc constraints] [bool return_warnings])";
	d.returns = R"(any)";
	d.description = R"(Attempts to call the container associated with `label_name` that must begin with a caret; the caret indicates that the label is allowed to be accessed by contained entities.  It will evaluate to the return value of the call.  If `params` is specified, then it will pass those as the params on the scope stack.  If `constraints` is true or an assoc, it will default all constraints to be on at reasonable values for small execution without access to any data beyond `params`.  They optional key-value combinations for `constraints` are as follows.  If "max_node_operations" is specified, it represents the number of operations that are allowed to be performed. If "max_node_operations" is 0, then an infinite of operations will be allotted, up to the limits of the current calling context.  If "max_node_allocations" is specified, it represents the maximum number of nodes that are allowed to be allocated, limiting the total memory, up to the current calling context's limit.   If "max_node_allocations" is 0 and the caller also has no limit, then there is no limit to the number of nodes to be allotted as long as the machine has sufficient memory.  Note that if "max_node_allocations" is specified while in a multithreaded environment, if the collective memory from all the executing threads exceeds the average memory specified by call_sandboxed, that may trigger a memory limit for the call_sandboxed.  If "max_operation_depth" is 0 or infinite and the caller also has no limit, then there is no limit to the depth that opcodes can execute, otherwise "max_operation_depth" limits how deep nested opcodes will be called. If `return_warnings` is true (default is false), the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is a list of all warnings, and perf_constraint_violation is a string denoting the performance constraint exceeded (or .null if none)).  The keys "read_access" and "write_access" are boolean and control whether the execution can read from or write to entities and access their relevant permissions (e.g., to load files, make system calls).  The keys "max_contained_entities", "max_contained_entity_depth", and "max_entity_id_length" constrain what they describe, and are primarily useful when ensuring that an entity and all its contained entities can be stored out to the file system.  The execution performed will use a random number stream created from the entity's random number stream.  If `return_warnings` is true (default is false), the result will be a tuple of the form `[value, warnings, performance_constraint_violation]`, where the value at "warnings" is an assoc mapping all warnings to their number of occurrences, and the value at "perf_constraint_violation" is a string denoting the constraint exceeded, or null if none.  If `return_warnings` is false just the value will be returned instead of a list.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"OuterEntity"
		(lambda
			{
				^available_method 5
				compute_value (call_entity
						"InnerEntity"
						"inner_call"
						{x x}
					)
			}
		)
	)
	(create_entities
		["OuterEntity" "InnerEntity"]
		(lambda
			{
				inner_call (+
						x
						(call_container "^available_method")
					)
			}
		)
	)
	[
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			{
				max_node_operations 30
				max_node_allocations 50
			}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			{
				max_node_operations 1
				max_node_allocations 1
			}
		)
		(call_entity
			"OuterEntity"
			"compute_value"
			{x 5}
			{
				max_node_operations 1
				max_node_allocations 1
				max_operation_depth 1
				max_contained_entities 1
				max_contained_entity_depth 1
				max_entity_id_length 1
			}
			.true
		)
	]
))&", R"([
	10
	10
	.null
	[.null {} "Execution step limit exceeded"]
])", "", R"((apply "destroy_entities" (contained_entities)))"}
		});
	d.requiresEntity = true;
	d.newScope = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.mayCauseNodeUpdateInCurrentEntity = true;
	d.frequencyPer10000Opcodes = 0.25;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_CALL_CONTAINER(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a containing Entity to call
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto container_label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[0]);
	if(container_label_sid == string_intern_pool.NOT_A_STRING_ID
			|| !Entity::IsLabelAccessibleToContainedEntities(container_label_sid))
		return EvaluableNodeReference::Null();

	InterpreterConstraints interpreter_constraints;
	InterpreterConstraints *interpreter_constraints_ptr = nullptr;
	PopulateInterpreterConstraintsFromParams(ocn, 2, interpreter_constraints);
	if(interpreter_constraints.AnyActiveConstraints())
		interpreter_constraints_ptr = &interpreter_constraints;

	//need to return a more complex data structure, can't return immediate
	if(interpreter_constraints_ptr != nullptr && interpreter_constraints_ptr->collectWarnings)
		immediate_result = EvaluableNodeRequestedValueTypes::Type::NONE;

	//attempt to get arguments
	EvaluableNodeReference args = EvaluableNodeReference::Null();
	if(ocn.size() > 1)
		args = InterpretNodeForImmediateUse(ocn[1]);

	//obtain a lock on the container
	EntityReadReference cur_entity(curEntity);
	StringInternPool::StringID cur_entity_sid = curEntity->GetIdStringId();
	EntityReadReference container(curEntity->GetContainer());
	if(container == nullptr)
		return EvaluableNodeReference::Null();
	//don't need the curEntity as a reference anymore -- can free the lock
	cur_entity = EntityReadReference();

	if(_label_profiling_enabled)
		PerformanceProfiler::StartOperation(string_intern_pool.GetStringFromID(container_label_sid),
			container->evaluableNodeManager.GetNumberOfUsedNodes());

#ifdef MULTITHREAD_SUPPORT
	//lock memory before allocating scope stack, then can release the entity lock
	Concurrency::ReadLock enm_lock = container->evaluableNodeManager.AcquireMemoryModificationReadLock();
	container.lock.unlock();
#endif

	//copy arguments to container, free args from this entity
	EvaluableNodeReference called_entity_args = container->evaluableNodeManager.DeepAllocCopy(args);
	//don't put freed nodes in local allocation buffer, because that will increase memory churn
	evaluableNodeManager->FreeNodeTreeIfPossible(args, false);

	auto scope_stack = ConvertArgsToScopeStack(called_entity_args, container->evaluableNodeManager);

	//add accessing_entity to arguments. If accessing_entity already specified (it shouldn't be), let garbage collection clean it up
	EvaluableNode *scope_stack_args = scope_stack[0];
	scope_stack_args->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_accessing_entity),
		container->evaluableNodeManager.AllocNode(ENT_STRING, cur_entity_sid));

	PopulatePerformanceCounters(interpreter_constraints_ptr, container);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is no longer executing
	memoryModificationLock.unlock();
#endif

	EvaluableNodeReference result = container->Execute(container_label_sid,
		&scope_stack, false, this, writeListeners, printListener, interpreter_constraints_ptr, immediate_result
#ifdef MULTITHREAD_SUPPORT
		, &enm_lock
#endif
	);

	container->evaluableNodeManager.FreeNode(called_entity_args);

#ifdef MULTITHREAD_SUPPORT
	//this interpreter is executing again
	memoryModificationLock = evaluableNodeManager->AcquireMemoryModificationReadLock();
#endif

	//call opcodes should consume the outer return opcode if there is one
	if(result.IsNonNullNodeReference() && result->GetType() == ENT_RETURN)
		result = RemoveTopConcludeOrReturnNode(result, &container->evaluableNodeManager);

	EvaluableNodeReference copied_result = evaluableNodeManager->DeepAllocCopy(result);
	//don't put freed nodes in local allocation buffer, because that will increase memory churn
	container->evaluableNodeManager.FreeNodeTreeIfPossible(result, false);

	if(_label_profiling_enabled)
		PerformanceProfiler::EndOperation(container->evaluableNodeManager.GetNumberOfUsedNodes());

	if(interpreterConstraints != nullptr)
		interpreterConstraints->AccruePerformanceCounters(interpreter_constraints_ptr);

	//if only want results, return them
	if(interpreter_constraints_ptr == nullptr || ocn.size() <= 2)
	{
		if(interpreter_constraints_ptr != nullptr && interpreter_constraints.constraintsExceeded)
			return EvaluableNodeReference::Null();
		return copied_result;
	}

	if(interpreter_constraints_ptr != nullptr && interpreter_constraints.constraintsExceeded)
		return BundleResultWithWarningsAndChangesIfNeeded(EvaluableNodeReference::Null(), interpreter_constraints_ptr);

	return BundleResultWithWarningsAndChangesIfNeeded(copied_result, interpreter_constraints_ptr);
}
