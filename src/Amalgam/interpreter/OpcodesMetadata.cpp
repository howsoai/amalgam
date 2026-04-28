//project headers:
#include "Interpreter.h"
#include "OpcodeDetails.h"

static std::string _opcode_group = "Metadata";

static OpcodeInitializer _ENT_GET_ANNOTATIONS(ENT_GET_ANNOTATIONS, &Interpreter::InterpretNode_ENT_GET_ANNOTATIONS, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(string)";
	d.description = R"(Returns a string comprising all of the annotation lines for `node`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_annotations
	(lambda
		
		#annotation line 1
		#annotation line 2
		.true
	)
))&", R"("annotation line 1\r\nannotation line 2")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ANNOTATIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	std::string annotations;

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n != nullptr)
	{
		annotations = std::string(n->GetAnnotationsString());
		evaluableNodeManager->FreeNodeTreeIfPossible(n);
	}
	return AllocReturn(annotations, immediate_result);
}

static OpcodeInitializer _ENT_SET_ANNOTATIONS(ENT_SET_ANNOTATIONS, &Interpreter::InterpretNode_ENT_SET_ANNOTATIONS, []() {
	OpcodeDetails d;
	d.parameters = R"(* node [string new_annotation])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a new copy of `node` with the annotation specified by `new_annotation`, where each newline is a separate line of annotation.  If `new_annotation` is null or missing, it will clear annotations for `node`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
))&", R"("#[\"labelD\" \"labelE\"]\r\n.true\r\n")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_ANNOTATIONS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the annotations
	auto [valid, new_annotations] = InterpretNodeIntoStringValue(ocn[1]);
	if(valid)
		source->SetAnnotationsString(new_annotations);
	else
		source->ClearAnnotations();

	return source;
}

static OpcodeInitializer _ENT_GET_COMMENTS(ENT_GET_COMMENTS, &Interpreter::InterpretNode_ENT_GET_COMMENTS, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(string)";
	d.description = R"(Returns a strings comprising all of the comment lines for `node`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_comments
	(lambda
		
		;comment line 1
		;comment line 2
		.true
	)
))&", R"("comment line 1\r\ncomment line 2")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	std::string comment;
	auto n = InterpretNodeForImmediateUse(ocn[0]);
	if(n != nullptr)
	{
		comment = std::string(n->GetCommentsString());
		evaluableNodeManager->FreeNodeTreeIfPossible(n);
	}
	return AllocReturn(comment, immediate_result);
}

static OpcodeInitializer _ENT_SET_COMMENTS(ENT_SET_COMMENTS, &Interpreter::InterpretNode_ENT_SET_COMMENTS, []() {
	OpcodeDetails d;
	d.parameters = R"(* node [string new_comment])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a new copy of `node` with the comment specified by `new_comment`, where each newline is a separate line of comment.  If `new_comment` is null or missing, it will clear comments for `node`.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
	(set_annotations
		(lambda
			
			#labelC
			.true
		)
		["labelD" "labelE"]
	)
	.true
	.true
	.true
))&", R"("#[\"labelD\" \"labelE\"]\r\n.true\r\n")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the comments
	auto [valid, new_comments] = InterpretNodeIntoStringValue(ocn[1]);
	if(valid)
		source->SetCommentsString(new_comments);
	else
		source->ClearComments();

	return source;
}

static OpcodeInitializer _ENT_GET_CONCURRENCY(ENT_GET_CONCURRENCY, &Interpreter::InterpretNode_ENT_GET_CONCURRENCY, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(bool)";
	d.description = R"(Returns true if `node` has a preference to be processed in a manner where its operations are run concurrentl, false if it is not.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_concurrency
	(lambda
		(print "hello")
	)
))&", R"(.false)"},
			{R"&((get_concurrency
	(lambda
		||(print "hello")
	)
))&", R"(.true)"},
			{R"&((get_concurrency
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
))&", R"(.true)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_CONCURRENCY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNodeForImmediateUse(ocn[0]);
	return AllocReturn(n != nullptr && n->GetConcurrency(), immediate_result);
}

static OpcodeInitializer _ENT_SET_CONCURRENCY(ENT_SET_CONCURRENCY, &Interpreter::InterpretNode_ENT_SET_CONCURRENCY, []() {
	OpcodeDetails d;
	d.parameters = R"(* node bool concurrent)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a new copy of `node` with the preference for concurrency set by `concurrent`.  Note that concurrency is potentially subject to race conditions or inconsistent results if tasks write to the same locations without synchronization.)";
	d.examples = MakeAmalgamExamples({
		{R"&((unparse
	(set_concurrency
		(lambda
			(print "hello")
		)
		.true
	)
	.true
	.true
	.true
))&", R"("||(print \"hello\")\r\n")"},
			{R"&((unparse
	(set_concurrency
		(lambda
			
			;complex test
			
			#some annotation
			{a "hello" b 4}
		)
		.true
	)
	.true
	.true
	.true
))&", R"("#some annotation\r\n;complex test\r\n||{a \"hello\" b 4}\r\n")"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_CONCURRENCY(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	else
		evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the concurrent flag
	bool concurrency = InterpretNodeIntoBoolValue(ocn[1]);
	source->SetConcurrency(concurrency);

	return source;
}

static OpcodeInitializer _ENT_GET_VALUE(ENT_GET_VALUE, &Interpreter::InterpretNode_ENT_GET_VALUE, []() {
	OpcodeDetails d;
	d.parameters = R"(* node)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a new copy of `node` without annotations, comments, or concurrency.)";
	d.examples = MakeAmalgamExamples({
		{R"&((get_value
	
	;first comment
	(lambda
		
		;second comment
		
		#annotation part 1
		#annotation part 2
		.true
	)
))&", R"(.true)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 1.0;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() == 0)
		return EvaluableNodeReference::Null();

	auto n = InterpretNode(ocn[0]);
	if(n == nullptr)
		return EvaluableNodeReference::Null();

	if(n.uniqueUnreferencedTopNode)
		n->ClearMetadata();
	else
		evaluableNodeManager->EnsureNodeIsModifiable(n, false, false);

	return n;
}

static OpcodeInitializer _ENT_SET_VALUE(ENT_SET_VALUE, &Interpreter::InterpretNode_ENT_SET_VALUE, []() {
	OpcodeDetails d;
	d.parameters = R"(* target * val)";
	d.returns = R"(any)";
	d.description = R"(Evaluates to a new copy of `node` with the value set to `val`, keeping existing annotations, comments, and concurrency).)";
	d.examples = MakeAmalgamExamples({
		{R"&((set_value
	
	;first comment
	(lambda
		
		;second comment
		.true
	)
	3
))&", R"(;second comment
3)"}
		});
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::PARTIAL;
	d.frequencyPer10000Opcodes = 0.01;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_SET_VALUE(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	auto &ocn = en->GetOrderedChildNodesReference();
	if(ocn.size() < 2)
		return EvaluableNodeReference::Null();

	auto source = InterpretNode(ocn[0]);
	if(source == nullptr)
		source = EvaluableNodeReference(evaluableNodeManager->AllocNode(ENT_NULL), true);
	else
		evaluableNodeManager->EnsureNodeIsModifiable(source);

	auto node_stack = CreateOpcodeStackStateSaver(source);

	//get the new value
	auto value_node = InterpretNode(ocn[1]);
	source->CopyValueFrom(value_node);
	source.UpdatePropertiesBasedOnAttachedNode(value_node, true);

	return source;
}

static OpcodeInitializer _ENT_GET_ENTITY_ANNOTATIONS(ENT_GET_ENTITY_ANNOTATIONS, &Interpreter::InterpretNode_ENT_GET_ENTITY_ANNOTATIONS_and_GET_ENTITY_COMMENTS, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] [string label] [bool deep_annotations])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the corresponding annotations for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve annotations for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the annotations for that label, null if the label doesn't exist.  If `deep_annotations` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the annotation of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_annotations is true, then it will return an assoc of label to annotation for each label in the entity.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													.null
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments .null .null .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_annotations "descriptive_entity")
		(get_entity_annotations "descriptive_entity" .null .true)
		(get_entity_annotations "descriptive_entity" "foo" .true)
	]
))&", R"([
	"entity annotations"
	{
		^containervar "containervar annotation"
		foo "foo annotation"
		get_api "get_api annotation"
		publicvar "publicvar annotation"
	}
	[
		{
			x ["x annotation\r\nx value annotation" 1]
			y ["y value annotation" 2]
		}
		"return annotation"
	]
])", "", R"((destroy_entities "descriptive_entity"))"},
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

static OpcodeInitializer _ENT_GET_ENTITY_COMMENTS(ENT_GET_ENTITY_COMMENTS, &Interpreter::InterpretNode_ENT_GET_ENTITY_ANNOTATIONS_and_GET_ENTITY_COMMENTS, []() {
	OpcodeDetails d;
	d.parameters = R"([id_path entity] [string label] [bool deep_comments])";
	d.returns = R"(any)";
	d.description = R"(Evaluates to the corresponding comments for `entity`.  If `entity` is null then it will use the current entity.  If `label` is null or empty string, it will retrieve comments for the entity root, otherwise if it is a valid `label` it will attempt to retrieve the comments for that label, null if the label doesn't exist.  If `deep_comments` is specified and the label is a declare, then it will return a list of two elements.  The first element of this list is an assoc with the keys being the parameters and the values being lists of the descriptions followed by the default value.  The second element of this list is the comment of the assoc itself, which is intended to be used to describe what is returned.  If label is empty string or null and deep_comments is true, then it will return an assoc of label to comment for each label in the entity.)";
	d.examples = MakeAmalgamExamples({
		{R"&((seq
	(create_entities
		"descriptive_entity"
		(lambda
			
			;this is a fully described entity
			
			#entity annotations
			{
				!privatevar 
					;some private variable
					
					#privatevar annotation
					2
				^containervar 
					;a variable accessible to contained entities
					
					#containervar annotation
					3
				foo 
					;the function foo
					
					#foo annotation
					(declare
						
						;a number representing the sum
						
						#return annotation
						{
							x 
								;the value of x
								;the default value of x
								
								#x annotation
								#x value annotation
								1
							y 
								;the value of y
								
								#y value annotation
								2
						}
						(+ x y)
					)
				get_api 
					;returns the api details
					
					#get_api annotation
					(seq
						{
							description (get_entity_comments)
							labels (map
									(lambda
										{
											description (current_value 1)
											parameters (get_entity_comments
													.null
													(current_index 1)
													.true
												)
										}
									)
									(get_entity_comments .null .null .true)
								)
						}
					)
				publicvar 
					;some public variable
					
					#publicvar annotation
					1
			}
		)
	)
	[
		(get_entity_comments "descriptive_entity")
		(get_entity_comments "descriptive_entity" .null .true)
		(get_entity_comments "descriptive_entity" "foo" .true)
	]
))&", R"([
	"this is a fully described entity"
	{
		^containervar "a variable accessible to contained entities"
		foo "the function foo"
		get_api "returns the api details"
		publicvar "some public variable"
	}
	[
		{
			x ["the value of x\r\nthe default value of x" 1]
			y ["the value of y" 2]
		}
		"a number representing the sum"
	]
])", "", R"((destroy_entities "descriptive_entity"))"}
		});
	d.requiresEntity = true;
	d.valueNewness = OpcodeDetails::OpcodeReturnNewnessType::NEW;
	d.frequencyPer10000Opcodes = 0.1;
	d.opcodeGroup = _opcode_group;
	return d;
});

EvaluableNodeReference Interpreter::InterpretNode_ENT_GET_ENTITY_ANNOTATIONS_and_GET_ENTITY_COMMENTS(EvaluableNode *en, EvaluableNodeRequestedValueTypes immediate_result)
{
	if(curEntity == nullptr)
		return EvaluableNodeReference::Null();

	auto &ocn = en->GetOrderedChildNodesReference();

	bool get_entity_comments = (en->GetType() == ENT_GET_ENTITY_COMMENTS);

	StringInternPool::StringID label_sid = StringInternPool::NOT_A_STRING_ID;
	if(ocn.size() > 1)
		label_sid = InterpretNodeIntoStringIDValueIfExists(ocn[1]);

	bool deep_comments_or_annotations = false;
	if(ocn.size() > 2)
		deep_comments_or_annotations = InterpretNodeIntoBoolValue(ocn[2]);

	//retrieve the entity after other parameters to minimize time in locks
	// and prevent deadlock if one of the params accessed the entity
	EntityReadReference target_entity;
	if(ocn.size() > 0)
		target_entity = InterpretNodeIntoRelativeSourceEntityReadReference(ocn[0]);
	else
		target_entity = EntityReadReference(curEntity);

	if(target_entity == nullptr)
		return EvaluableNodeReference::Null();

	if(label_sid == StringInternPool::NOT_A_STRING_ID)
	{
		if(!deep_comments_or_annotations)
		{
			EvaluableNode *root = target_entity->GetRoot();
			auto entity_description = (get_entity_comments ? root->GetCommentsString() : root->GetAnnotationsString());
			//if the top node doesn't have a description, try to obtain from the node with the null key
			if(entity_description.empty())
			{
				EvaluableNode **null_code = root->GetMappedChildNode(string_intern_pool.NOT_A_STRING_ID);
				if(null_code != nullptr)
					entity_description = (get_entity_comments ? EvaluableNode::GetCommentsString(*null_code) : EvaluableNode::GetAnnotationsString(*null_code));
			}
			return AllocReturn(entity_description, immediate_result);
		}

		EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_ASSOC), true);

		//collect comments or annotations of each label
		target_entity->IterateFunctionOverLabels(
			[this, &retval, get_entity_comments]
			(StringInternPool::StringID label_sid, EvaluableNode *node)
		{
			//only include publicly facing labels
			if(Entity::IsLabelValidAndPublic(label_sid))
				retval->SetMappedChildNode(label_sid,
					evaluableNodeManager->AllocNode(get_entity_comments ? EvaluableNode::GetCommentsString(node) : EvaluableNode::GetAnnotationsString(node)));
		}
		);

		return retval;
	}

	auto label_value = target_entity->GetValueAtLabel(label_sid, nullptr).first;
	if(label_value == nullptr)
		return EvaluableNodeReference::Null();

	//has valid label
	if(!deep_comments_or_annotations)
		return AllocReturn(get_entity_comments ? label_value->GetCommentsString() : label_value->GetAnnotationsString(), immediate_result);

	//make sure a function based on declare that has parameters
	if(label_value->GetType() != ENT_DECLARE || label_value->GetOrderedChildNodes().size() < 1)
		return EvaluableNodeReference::Null();

	//the first element is an assoc of the parameters, the second element is the return value
	EvaluableNodeReference retval(evaluableNodeManager->AllocNode(ENT_LIST), true);

	//if the vars are already initialized, then pull the comments or annotations from their values
	EvaluableNode *vars = label_value->GetOrderedChildNodes()[0];
	if(!EvaluableNode::IsAssociativeArray(vars))
		return retval;

	auto &retval_ocn = retval->GetOrderedChildNodesReference();
	retval_ocn.resize(2);

	//deep_comments_or_annotations of label, so get the parameters and their respective labels
	EvaluableNodeReference params_list(evaluableNodeManager->AllocNode(ENT_ASSOC), true);
	retval_ocn[0] = params_list;

	//get return comments or annotations
	retval_ocn[1] = evaluableNodeManager->AllocNode(
		get_entity_comments ? vars->GetCommentsString() : vars->GetAnnotationsString());

	auto &mcn = vars->GetMappedChildNodesReference();
	params_list->ReserveMappedChildNodes(mcn.size());

	//create the string references all at once and hand off
	for(auto &[cn_id, cn] : mcn)
	{
		//create list with comment and default value
		EvaluableNodeReference param_info(evaluableNodeManager->AllocNode(ENT_LIST), true);
		auto &param_info_ocn = param_info->GetOrderedChildNodesReference();
		param_info_ocn.resize(2);
		param_info_ocn[0] = evaluableNodeManager->AllocNode(
			get_entity_comments ? EvaluableNode::GetCommentsString(cn) : EvaluableNode::GetAnnotationsString(cn));
		param_info_ocn[1] = evaluableNodeManager->DeepAllocCopy(cn, false);

		//add to the params
		params_list->SetMappedChildNode(cn_id, param_info);
	}

	//ensure flags are updated since the node was already attached
	retval.UpdatePropertiesBasedOnAttachedNode(params_list);

	return retval;
}
