//project headers:
#include "EntityManipulation.h"
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Entity Comparison and Evolution";

static OpcodeInitializer _ENT_TOTAL_ENTITY_SIZE(ENT_TOTAL_ENTITY_SIZE, &Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity)";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the total count of all of the nodes of `entity` and all of its contained entities.  Each entity itself counts as multiple nodes, corresponding to flattening an entity via the `flatten_entity` opcode.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"Entity1"
		{a 3 b 4}
	)
	(create_entities
		["Entity1" "EntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["Entity1" "EntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["Entity1"]
		{E 3 F 4}
	)
	(create_entities
		["Entity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(total_entity_size "Entity1")
))&", R"(67)", "", R"((destroy_entities "Entity1"))"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_TOTAL_ENTITY_SIZE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	EntityReadReference entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	if(entity == nullptr)
		return EvaluableNodeReference::Null();

	auto erbr = entity->GetAllDeeplyContainedEntityReferencesGroupedByDepth<EntityReadReference>();
	double size = static_cast<double>(entity->GetDeepSizeInNodes());
	return AllocReturn(size, immediate_result);
}

static OpcodeInitializer _ENT_MUTATE_ENTITY(ENT_MUTATE_ENTITY, &Interpreter::InterpretNode_ENT_MUTATE_ENTITY, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path source_entity [number mutation_rate] [id_path dest_entity] [assoc mutation_weights] [assoc operation_type] [preserve_type_depth])";
	d.returns = R"(id_path)";
	d.description = R"(Creates a mutated version of the entity specified by `source_entity` like mutate. Returns the id path of a new entity created contained by the entity that ran it.  The value specified by `mutation_rate`, from 0.0 to 1.0 and defaulting to 0.00001, indicates the probability that any node will experience a mutation.  Uses `dest_entity` as the optional destination.  The parameter `mutation_weights` is an assoc where the keys are the allowed opcode names and the values are the probabilities that each opcode would be chosen; if null or unspecified, it defaults to all opcodes each with their own default probability.  The `operation_type` is an assoc where the keys are mutation operations and the values are the probabilities that the operations will be performed.  The operations can consist of the strings "change_type", "delete", "insert", "swap_elements", "deep_copy_elements", and "delete_elements".  If `preserve_type_depth` is specified, it will retain the types of node down to and including whatever depth is specified, and defaults to 1 indicating that the top level of the entities will have a preserved type, namely an assoc.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"MutateEntity"
		(lambda
			{
				a 1
				b 2
				c 3
				d 4
				e 5
				f 6
				g 7
				h 8
				i 9
				j 10
				k 11
				l 12
				m 13
				n 14
				o (associate "a" 1 "b" 2)
			}
		)
	)
	(mutate_entity "MutateEntity" 0.4 "MutatedEntity1")
	(mutate_entity "MutateEntity" 0.5 "MutatedEntity2")
	(mutate_entity
		"MutateEntity"
		0.5
		"MutatedEntity3"
		(associate "+" 0.5 "-" 0.3 "*" 0.2)
		(associate "change_type" 0.08 "delete" 0.02 "insert" 0.9)
	)
	[
		(retrieve_entity_root "MutatedEntity1")
		(retrieve_entity_root "MutatedEntity2")
		(retrieve_entity_root "MutatedEntity3")
	]
))&", R"([
	{
		a 1
		b 2
		c 3
		d (set_type)
		e (if)
		f (>=)
		g (<=)
		h 8
		i 9
		j 10
		k 11
		l 12
		m 13
		n -20.325081516830192
		o "b"
	}
	{
		a 1
		b (map)
		c (min)
		d 4
		e 5
		f (apply)
		g 7
		h (get_type_string)
		i (round)
		j 10
		k (lambda)
		l 12
		m (declare)
		n 14
		o (map)
	}
	{
		a (*)
		b (*)
		c 3
		d 4
		e (+)
		f (*)
		g (-)
		h 8
		i 9
		j 10
		k 11
		l 12
		m (+)
		n 14
		o (associate (-) 1 (*) (+))
	}
])", ".*", R"((destroy_entities "MutateEntity" "MutatedEntity1" "MutatedEntity2" "MutatedEntity3" ))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MUTATE_ENTITY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 1)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	//get mutation rate if applicable
	double mutation_rate = 0.00001;
	if(ocn.size() > 1)
		mutation_rate = InterpretNodeIntoNumberValue(ocn[1]);

	bool ow_exists = false;
	CompactHashMap<EvaluableNodeType, double> opcode_weights;
	if(ocn.size() > 3)
	{
		auto opcode_weights_node = InterpretNode(ocn[3]);
		if(!EvaluableNode::IsNull(opcode_weights_node))
		{
			ow_exists = true;
			for(auto &[node_id, node] : opcode_weights_node->GetMappedChildNodes())
				opcode_weights[GetEvaluableNodeTypeFromStringId(node_id)] = EvaluableNode::ToNumber(node);

			evaluableNodeManager->FreeNodeTreeIfPossible(opcode_weights_node);
		}
	}

	bool mtw_exists = false;
	CompactHashMap<EvaluableNodeBuiltInStringId, double> mutation_type_weights;
	if(ocn.size() > 4)
	{
		auto mutation_weights_node = InterpretNode(ocn[4]);
		if(!EvaluableNode::IsNull(mutation_weights_node))
		{
			mtw_exists = true;
			for(auto &[node_id, node] : mutation_weights_node->GetMappedChildNodes())
			{
				auto bisid = GetBuiltInStringIdFromStringId(node_id);
				mutation_type_weights[bisid] = EvaluableNode::ToNumber(node);
			}

			evaluableNodeManager->FreeNodeTreeIfPossible(mutation_weights_node);
		}
	}

	size_t preserve_type_depth = 1;
	if(ocn.size() > 5)
		preserve_type_depth = static_cast<size_t>(std::max(0.0, InterpretNodeIntoNumberValue(ocn[5])));

	//retrieve the entities after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	//get the id of the first source entity
	EntityReadReference source_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	//need a source entity, and can't copy self! (that could cause badness)
	if(source_entity == nullptr || source_entity == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by mutating
	Entity *new_entity = EntityManipulation::MutateEntity(this, source_entity, mutation_rate,
		mtw_exists ? &mutation_type_weights : nullptr, ow_exists ? &opcode_weights : nullptr, preserve_type_depth);

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	//clear lock if applicable
	source_entity = EntityReadReference();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 2)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[2]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr)
		return EvaluableNodeReference::Null();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

static OpcodeInitializer _ENT_COMMONALITY_ENTITIES(ENT_COMMONALITY_ENTITIES, &Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2 [assoc params])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the total count of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	[
		(commonality_entities "MergeEntity1" "MergeEntity2")
		(commonality_entities
			"MergeEntity1"
			"MergeEntity2"
			{nominal_strings .false types_must_match .false}
		)
	]
))&", R"([64 64.74178574543642])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" )"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_COMMONALITY_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNode(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	auto commonality = EntityManipulation::NumberOfSharedNodes(source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	return AllocReturn(commonality.commonality, immediate_result);
}

static OpcodeInitializer _ENT_EDIT_DISTANCE_ENTITIES(ENT_EDIT_DISTANCE_ENTITIES, &Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2 [assoc params])";
	d.returns = R"(number)";
	d.description = R"(Evaluates to the edit distance of all of the nodes referenced within `entity1` and `entity2` that are equivalent, including all contained entities.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		[
			(edit_distance_entities "MergeEntity1" "MergeEntity2")
			(edit_distance_entities
				"MergeEntity1"
				"MergeEntity2"
				{nominal_strings .false types_must_match .false}
			)
		]
	)
))&", R"([11 9.516428509127167])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" )"},

		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_EDIT_DISTANCE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNode(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	double edit_distance = EntityManipulation::EditDistance(source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);
	return AllocReturn(edit_distance, immediate_result);
}

static OpcodeInitializer _ENT_INTERSECT_ENTITIES(ENT_INTERSECT_ENTITIES, &Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2 [assoc params] [id_path entity3])";
	d.returns = R"(id_path)";
	d.description = R"(Creates an entity of whatever is common between the entities `entity1` and `entity2` exclusive.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call create_contained_entity.  Any contained entities will be intersected either based on matching name or maximal similarity for nameless entities.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(intersect_entities "MergeEntity1" "MergeEntity2" (null) "IntersectedEntity")
		[
			(retrieve_entity_root "IntersectedEntity")
			(sort
				(contained_entities "IntersectedEntity")
			)
		]
	)
))&", R"([
	{b 4 c (null)}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" "IntersectedEntity")"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_INTERSECT_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = true;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNode(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::IntersectEntities(this, source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 3)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[3]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

static OpcodeInitializer _ENT_UNION_ENTITIES(ENT_UNION_ENTITIES, &Interpreter::InterpretNode_ENT_UNION_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2 [assoc params] [id_path entity3])";
	d.returns = R"(id_path)";
	d.description = R"(Creates an entity of whatever is inclusive when merging the entities `entity1` and `entity2`.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true, the default, then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  Uses `entity3` as the optional destination via an internal call to create_contained_entity.  Any contained entities will be unioned either based on matching name or maximal similarity for nameless entities.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(seq
		(create_entities
			"MergeEntity1"
			{a 3 b 4 c "c1"}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild1"]
			{x 3 y 4}
		)
		(create_entities
			["MergeEntity1" "MergeEntityChild2"]
			{p 3 q 4}
		)
		(create_entities
			["MergeEntity1"]
			{E 3 F 4}
		)
		(create_entities
			["MergeEntity1"]
			{
				e 3
				f 4
				g 5
				h 6
			}
		)
		(create_entities
			"MergeEntity2"
			{b 4 c "c2"}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild1"]
			{x 3 y 4 z 5}
		)
		(create_entities
			["MergeEntity2" "MergeEntityChild2"]
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
		(create_entities
			["MergeEntity2"]
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
		(create_entities
			["MergeEntity2"]
			{e 3 f 4}
		)
		(union_entities "MergeEntity1" "MergeEntity2" (null) "UnionedEntity")
		[
			(retrieve_entity_root "UnionedEntity")
			(sort
				(contained_entities "UnionedEntity")
			)
		]
	)
))&", R"([
	{a 3 b 4 c (null)}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", "", R"((destroy_entities "MergeEntity1" "MergeEntity2" "UnionedEntity")"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_UNION_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = true;
	bool nominal_strings = true;
	bool recursive_matching = true;
	if(ocn.size() > 2)
	{
		auto params = InterpretNode(ocn[2]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::UnionEntities(this, source_entity_1, source_entity_2,
		types_must_match, nominal_numbers, nominal_strings, recursive_matching);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 3)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[3]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}

static OpcodeInitializer _ENT_DIFFERENCE_ENTITIES(ENT_DIFFERENCE_ENTITIES, &Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2)";
	d.returns = R"(any)";
	d.description = R"(Finds the difference between the entities specified by `entity1` and `entity2` and generates code that, if evaluated passing the entity id_path as its parameter "_", would create a new entity into the id path specified by its parameter "new_entity" (null if unspecified), which would contain the applied difference between the two entities and returns the newly created entity id path.  Useful for finding a small difference of what needs to be changed to apply it to new (and possibly slightly different) entity.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"DiffEntity1"
		(lambda
			{a 3 b 4}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 6}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity1" "OnlyIn1"]
		(lambda
			{m 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{e 3 f 4}
		)
	)
	(create_entities
		["DiffEntity1"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		"DiffEntity2"
		(lambda
			{b 4 c 3}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1"]
		(lambda
			{x 3 y 4 z 5}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2"]
		(lambda
			{
				p 3
				q 4
				u 5
				v 6
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "DiffEntityChild1" "DiffEntityChild2" "DiffEntityChild3"]
		(lambda
			{
				a 5
				e 3
				o 6
				p 4
				w 7
			}
		)
	)
	(create_entities
		["DiffEntity2" "OnlyIn2"]
		(lambda
			{o 6}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{
				E 3
				F 4
				G 5
				H 6
			}
		)
	)
	(create_entities
		["DiffEntity2"]
		(lambda
			{e 3 f 4}
		)
	)
	
	;applying the difference to DiffEntity1 results in an entity identical to DiffEntity2
	(let
		{
			new_entity (call
					(difference_entities "DiffEntity1" "DiffEntity2")
					{_ "DiffEntity1"}
				)
		}
		(difference_entities "DiffEntity2" new_entity)
	)
))&", R"((declare
	{_ (null) new_entity (null)}
	(clone_entities _ new_entity)
))", "", R"((apply "destroy_entities" (contained_entities)))"}
	});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.05;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_DIFFERENCE_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto [entity_1, entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(entity_1 == nullptr || entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//can't difference with self
	if(entity_1 == curEntity || entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	return EntityManipulation::DifferenceEntities(this, entity_1, entity_2);
}

static OpcodeInitializer _ENT_MIX_ENTITIES(ENT_MIX_ENTITIES, &Interpreter::InterpretNode_ENT_MIX_ENTITIES, []() {
	OpcodeDetails d;
	d.parameters = R"(id_path entity1 id_path entity2 [number keep_chance_entity1] [number keep_chance_entity2] [assoc params] [id_path entity3])";
	d.returns = R"(id_path)";
	d.description = R"(Performs a union operation on the entities represented by `entity1` and `entity2`, but randomly ignores nodes from one or the other tree if not equal.  If only `keep_chance_entity1` is specified, `keep_chance_entity2` defaults to 1 - `keep_chance_entity1`.  `keep_chance_entity1` specifies the probability that a node from the entity represented by `entity1` will be kept, and `keep_chance_entity2` the probability that a node from the entity represented by `entity2` will be kept.  The assoc `params` can contain the keys "types_must_match", "nominal_numbers", "nominal_strings", and "recursive_matching".  If the key "types_must_match" is true (the default), it will only consider nodes common if the types match.  If the key "nominal_numbers" is true (the default is false), then it will assume that all numbers will match only if identical; if false, it will compare similarity of values.  The key "nominal_strings" defaults to true, but works similar to "nominal_numbers" except on strings using string edit distance.  If the key "recursive_matching" is true or null, then it will attempt to recursively match any part of the data structure of one node to another.  If the key "recursive_matching" is false, then it will only attempt to merge the two at the same level, which yield better results if the data structures are common, and additionally will be much faster.  `similar_mix_chance` is the additional probability that two nodes will mix if they have some commonality, which will include interpolating number and string values based on `keep_chance_node1` and `keep_chance_node2`, and defaults to 0.0.  If `similar_mix_chance` is negative, then 1 minus the value will be anded with the commonality probability, so -1 means that it will never mix and 0 means it will only mix when sufficiently common.  `unnamed_entity_mix_chance` represents the probability that an unnamed entity pair will be mixed versus preserved as independent chunks, where 0.2 would yield 20% of the entities mixed. Returns the id path of a new entity created contained by the entity that ran it.  Uses `entity3` as the optional destination entity.   Any contained entities will be mixed either based on matching name or maximal similarity for nameless entities.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"MergeEntity1"
		{a 3 b 4 c "c1"}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild1"]
		{x 3 y 4}
	)
	(create_entities
		["MergeEntity1" "MergeEntityChild2"]
		{p 3 q 4}
	)
	(create_entities
		["MergeEntity1"]
		{E 3 F 4}
	)
	(create_entities
		["MergeEntity1"]
		{
			e 3
			f 4
			g 5
			h 6
		}
	)
	(create_entities
		"MergeEntity2"
		{b 4 c "c2"}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild1"]
		{x 3 y 4 z 5}
	)
	(create_entities
		["MergeEntity2" "MergeEntityChild2"]
		{
			p 3
			q 4
			u 5
			v 6
			w 7
		}
	)
	(create_entities
		["MergeEntity2"]
		{
			E 3
			F 4
			G 5
			H 6
		}
	)
	(create_entities
		["MergeEntity2"]
		{e 3 f 4}
	)
	(mix_entities
		"MergeEntity1"
		"MergeEntity2"
		0.5
		0.5
		{similar_mix_chance 0.5 unnamed_entity_mix_chance 0.2}
		"MixedEntities"
	)
	[
		(retrieve_entity_root "MixedEntities")
		(sort
			(contained_entities "MixedEntities")
		)
	]
))&", R"([
	{b 4 c "c1"}
	["MergeEntityChild1" "MergeEntityChild2" "_2bW5faQkVxs" "_ldZa276M1io"]
])", ".*", R"((apply "destroy_entities" (contained_entities)))"},
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.hasSideEffects = true;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_MIX_ENTITIES(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();

	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	//not allowed if don't have a Entity to create within
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	double blend2 = 0.5; //default to half
	if(ocn.size() > 2)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[2]);
		if(!FastIsNaN(new_value))
			blend2 = new_value;
	}

	double blend1 = 1.0 - blend2; //default to the remainder
	if(ocn.size() > 3)
	{
		double new_value = InterpretNodeIntoNumberValue(ocn[3]);
		if(!FastIsNaN(new_value))
			blend1 = new_value;

		//if have a third parameter, then use the fractions in order (so need to swap)
		std::swap(blend1, blend2);
	}

	//clamp both blend values to be nonnegative
	blend1 = std::max(0.0, blend1);
	blend2 = std::max(0.0, blend2);

	//stop if have nothing
	if(blend1 == 0.0 && blend2 == 0.0)
		return EvaluableNodeReference::Null();

	bool types_must_match = true;
	bool nominal_numbers = false;
	bool nominal_strings = true;
	bool recursive_matching = true;
	double similar_mix_chance = 0.0;
	double unnamed_entity_mix_chance = 0.2;
	if(ocn.size() > 4)
	{
		auto params = InterpretNode(ocn[4]);
		if(EvaluableNode::IsAssociativeArray(params))
		{
			auto &mcn = params->GetMappedChildNodesReference();
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_types_must_match, types_must_match);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_numbers, nominal_numbers);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_nominal_strings, nominal_strings);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_recursive_matching, recursive_matching);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_similar_mix_chance, similar_mix_chance);
			EvaluableNode::GetValueFromMappedChildNodesReference(mcn, ENBISI_unnamed_entity_mix_chance, unnamed_entity_mix_chance);
		}
		evaluableNodeManager->FreeNodeTreeIfPossible(params);
	}

	auto [source_entity_1, source_entity_2, erbr] = InterpretNodeIntoRelativeSourceEntityReadReferences(ocn[0], ocn[1]);
	if(source_entity_1 == nullptr || source_entity_2 == nullptr)
		return EvaluableNodeReference::Null();

	//need a source entity, and shouldn't copy self
	if(source_entity_1 == curEntity || source_entity_2 == curEntity)
		return EvaluableNodeReference::Null();

	//create new entity by merging
	Entity *new_entity = EntityManipulation::MixEntities(this, source_entity_1, source_entity_2, blend1, blend2,
		similar_mix_chance, types_must_match, nominal_numbers, nominal_strings, recursive_matching, unnamed_entity_mix_chance);

	//no longer need entity references
	erbr.Clear();

	size_t num_new_entities = new_entity->GetTotalNumContainedEntitiesIncludingSelf();

	//get destination if applicable
	EntityWriteReference destination_entity_parent;
	StringRef new_entity_id;
	if(ocn.size() > 5)
		std::tie(destination_entity_parent, new_entity_id) = InterpretNodeIntoDestinationEntity(ocn[5]);
	else
		destination_entity_parent = EntityWriteReference(curEntity);

	if(destination_entity_parent == nullptr
		|| !CanCreateNewEntityFromConstraints(destination_entity_parent, new_entity_id, num_new_entities))
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	//accumulate usage
	if(ConstrainedAllocatedNodes())
		interpreterConstraints->curNumAllocatedNodesAllocatedToEntities += new_entity->GetDeepSizeInNodes();

	destination_entity_parent->AddContainedEntityViaReference(new_entity, new_entity_id, writeListeners);

	if(new_entity_id == StringInternPool::NOT_A_STRING_ID)
	{
		delete new_entity;
		return EvaluableNodeReference::Null();
	}

	if(destination_entity_parent == curEntity)
		return AllocReturn(static_cast<StringInternPool::StringID>(new_entity_id), immediate_result);
	else //need to return an id list
		return EvaluableNodeReference(GetTraversalIDPathFromAToB(evaluableNodeManager, curEntity, new_entity), true);
}
