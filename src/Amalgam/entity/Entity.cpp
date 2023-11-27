//project headers:
#include "Entity.h"
#include "AssetManager.h"
#include "EntityQueries.h"
#include "EntityWriteListener.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNodeTreeFunctions.h"
#include "Interpreter.h"

std::vector<Entity *> Entity::emptyContainedEntities;

Entity::Entity()
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;

	SetRoot(nullptr, false);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(Entity *_container, std::string &code_string, const std::string &rand_state, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = _container;

	SetRoot(code_string, metadata_modifier);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(Entity *_container, EvaluableNode *_root, const std::string &rand_state, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = _container;

	//since this is the constructor, can't have had this entity's EntityNodeManager
	SetRoot(_root, false, metadata_modifier);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(Entity *t)
{
	//start with an empty entity to make sure SetRoot works fine
	randomStream = t->randomStream;
	hasContainedEntities = false;
	entityRelationships.container = nullptr;

	SetRoot(t->evaluableNodeManager.GetRootNode(), false);

	idStringId = StringInternPool::NOT_A_STRING_ID;

	hasContainedEntities = t->hasContainedEntities;

	if(hasContainedEntities)
	{
		entityRelationships.relationships = new EntityRelationships();

		auto &t_contained_entities = t->GetContainedEntities();

		//copy all contained entities
		entityRelationships.relationships->containedEntities.reserve(t_contained_entities.size());
		for(Entity *e : t_contained_entities)
		{
			Entity *child_copy = new Entity(e);
			AddContainedEntity(child_copy, e->GetIdStringId());
		}

		entityRelationships.relationships->container = nullptr;
	}
	else
	{
		entityRelationships.container = nullptr;
	}
}

Entity::~Entity()
{
#ifdef MULTITHREAD_SUPPORT
	Concurrency::WriteLock lock(mutex);
#endif

	//clear query caches before destroying contained entities for performance
	ClearQueryCaches();

	//if contained in another entity, remove it
	EntityQueryCaches *container_caches = GetContainerQueryCaches();
	if(container_caches != nullptr)
	{
		//must have a container, overwrite with the entity in the last index
		Entity *container = GetContainer();
		size_t last_index_of_container = container->GetNumContainedEntities() - 1;

		container_caches->RemoveEntity(this, GetEntityIndexOfContainer(), last_index_of_container);
	}

	if(hasContainedEntities)
	{
		//delete the entities from highest index to lowest index to reduce churn when freeing the query caches
		auto &contained_entities = entityRelationships.relationships->containedEntities;
		for(size_t i = contained_entities.size(); i > 0; i--)
		{
			size_t index = i - 1;
			delete contained_entities[index];
		}

		delete entityRelationships.relationships;
	}

	string_intern_pool.DestroyStringReference(idStringId);
	string_intern_pool.DestroyStringReferences(labelIndex, [](auto l) { return l.first; });
}

EvaluableNodeReference Entity::GetValueAtLabel(StringInternPool::StringID label_sid, EvaluableNodeManager *destination_temp_enm, bool direct_get, bool on_self, bool batch_call)
{
	if(label_sid <= StringInternPool::EMPTY_STRING_ID)
		return EvaluableNodeReference::Null();

	if(!on_self && IsLabelPrivate(label_sid))
		return EvaluableNodeReference::Null();

	const auto &label = labelIndex.find(label_sid);

	if(label == end(labelIndex))
		return EvaluableNodeReference::Null();

	if(label->second == nullptr)
		return EvaluableNodeReference::Null();

	EvaluableNodeReference retval(label->second, false);

	//if didn't give a valid destination, just return what we have
	if(destination_temp_enm == nullptr)
		return retval;

	return destination_temp_enm->DeepAllocCopy(retval, direct_get ? EvaluableNodeManager::ENMM_NO_CHANGE : EvaluableNodeManager::ENMM_REMOVE_ALL);
}

bool Entity::GetValueAtLabelAsNumber(StringInternPool::StringID label_sid, double &value_out, bool on_self)
{
	constexpr double value_if_not_found = std::numeric_limits<double>::quiet_NaN();

	if(label_sid <= StringInternPool::EMPTY_STRING_ID)
	{
		value_out = value_if_not_found;
		return false;
	}

	if(!on_self && IsLabelPrivate(label_sid))
	{
		value_out = value_if_not_found;
		return false;
	}

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
	{
		value_out = value_if_not_found;
		return false;
	}

	value_out = EvaluableNode::ToNumber(label->second);
	return true;
}

bool Entity::GetValueAtLabelAsStringId(StringInternPool::StringID label_sid, StringInternPool::StringID &value_out, bool on_self)
{
	if(label_sid <= StringInternPool::EMPTY_STRING_ID)
	{
		value_out = StringInternPool::NOT_A_STRING_ID;
		return false;
	}

	if(!on_self && IsLabelPrivate(label_sid))
	{
		value_out = StringInternPool::NOT_A_STRING_ID;
		return false;
	}

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
	{
		value_out = StringInternPool::NOT_A_STRING_ID;
		return false;
	}

	value_out = EvaluableNode::ToStringIDIfExists(label->second);
	return true;
}

bool Entity::GetValueAtLabelAsString(StringInternPool::StringID label_sid, std::string &value_out, bool on_self)
{
	if(label_sid <= StringInternPool::EMPTY_STRING_ID)
	{
		value_out = "";
		return false;
	}

	if(!on_self && IsLabelPrivate(label_sid))
	{
		value_out = "";
		return false;
	}

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
	{
		value_out = "";
		return false;
	}

	value_out = Parser::Unparse(label->second, &evaluableNodeManager, false, false);
	return true;
}

EvaluableNodeImmediateValueType Entity::GetValueAtLabelAsImmediateValue(StringInternPool::StringID label_sid,
	EvaluableNodeImmediateValue &value_out, bool on_self)
{
	if(!on_self && IsLabelPrivate(label_sid))
	{
		value_out.number = std::numeric_limits<double>::quiet_NaN();
		return ENIVT_NOT_EXIST;
	}

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
	{
		value_out.number = std::numeric_limits<double>::quiet_NaN();
		return ENIVT_NOT_EXIST;
	}

	return value_out.CopyValueFromEvaluableNode(label->second);
}

bool Entity::SetValueAtLabel(StringInternPool::StringID label_sid, EvaluableNodeReference &new_value, bool direct_set,
	std::vector<EntityWriteListener *> *write_listeners, bool on_self, bool batch_call)
{
	if(label_sid <= StringInternPool::EMPTY_STRING_ID)
		return false;

	if(!on_self)
	{
		if(IsLabelPrivate(label_sid))
			return EvaluableNodeReference(nullptr, true);

		//since it's not setting on self, another entity owns the data so it isn't unique to this entity
		new_value.unique = false;
	}

	auto current_node = labelIndex.find(label_sid);

	//if the label is not in the system, then can't do anything
	if(current_node == end(labelIndex))
		return false;

	EvaluableNode *destination = current_node->second;

	//can't replace if the label points to null - shouldn't happen
	if(destination == nullptr)
		return false;

	if(!direct_set)
	{
		if(new_value == nullptr || new_value->GetNumChildNodes() == 0)
		{
			//if simple copy value, then just do it
			destination->CopyValueFrom(new_value);
		}
		else //need to copy child nodes
		{
			//remove all labels and allocate if needed
			if(new_value.unique)
				EvaluableNodeManager::ModifyLabelsForNodeTree(new_value, EvaluableNodeManager::ENMM_REMOVE_ALL);
			else
				new_value = evaluableNodeManager.DeepAllocCopy(new_value, EvaluableNodeManager::ENMM_REMOVE_ALL);

			//copy over the existing node, but don't update labels, etc.
			destination->CopyValueFrom(new_value);
		}
	}
	else //direct set
	{
		//allocate and remove any extra label indirections
		//if replacement is null, create a new null node because will want to retain the fact that an addressable
		// node exists in case it is reused in multiple places
		if(new_value != nullptr)
		{
			if(new_value.unique)
				EvaluableNodeManager::ModifyLabelsForNodeTree(new_value, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT);
			else
				new_value = evaluableNodeManager.DeepAllocCopy(new_value, EvaluableNodeManager::ENMM_LABEL_ESCAPE_DECREMENT);
		}
		else
		{
			new_value = EvaluableNodeReference(evaluableNodeManager.AllocNode(ENT_NULL), true);
		}
		//the value is being used in the entity, so no longer unique if it was before
		new_value.unique = false;

		//update the index
		labelIndex[label_sid] = new_value;

		//need to replace label in case there are any collapses of labels if multiple labels set
		EvaluableNode *root = evaluableNodeManager.GetRootNode();

		EvaluableNodeTreeManipulation::ReplaceLabelInTree(root, label_sid, new_value);
		evaluableNodeManager.SetRootNode(root);

		if(!batch_call)
			RebuildLabelIndex();
	}

	if(!batch_call)
	{
		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(container_caches != nullptr)
			container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());

		asset_manager.UpdateEntity(this);
		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteValueToEntity(this, new_value, label_sid, direct_set);
		}
	}

	return true;
}

//like SetValuesAtLabels, except accumulates each value at each label instead
std::pair<bool, bool> Entity::SetValuesAtLabels(EvaluableNodeReference new_label_values, bool accum_values, bool direct_set,
	std::vector<EntityWriteListener *> *write_listeners, size_t *num_new_nodes_allocated, bool on_self, bool copy_entity)
{
	//can only work with assoc arrays
	if(!EvaluableNode::IsAssociativeArray(new_label_values))
		return std::make_pair(false, false);

	//if it's not setting on self, another entity owns the data so it isn't unique to this entity
	if(!on_self)
		new_label_values.unique = false;

	if(copy_entity)
		SetRoot(GetRoot(), false);

	//if relevant, keep track of new memory allocated to the entity
	size_t prev_size = 0;
	if(num_new_nodes_allocated != nullptr)
		prev_size = GetDeepSizeInNodes();

	//make assignments
	bool any_successful_assignment = false;
	bool all_successful_assignments = true;
	auto &new_label_values_mcn = new_label_values->GetMappedChildNodesReference();
	for(auto &[assignment_id, assignment] : new_label_values_mcn)
	{
		StringInternPool::StringID variable_sid = assignment_id;
		EvaluableNodeReference variable_value_node(assignment, new_label_values.unique);

		if(accum_values)
		{
			//if copy_entity is set, then can treat variable_value_node as unique because it is working on an isolated copy
			EvaluableNodeReference value_destination_node(GetValueAtLabel(variable_sid, nullptr, true, true, true), copy_entity);
			//can't assign to a label if it doesn't exist
			if(value_destination_node == nullptr)
				continue;

			variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, variable_value_node, &evaluableNodeManager);
		}

		if(SetValueAtLabel(variable_sid, variable_value_node, direct_set, write_listeners, on_self, true))
			any_successful_assignment = true;
		else
			all_successful_assignments = false;
	}

	if(any_successful_assignment)
	{
		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(direct_set)
		{
			//direct assigments need a rebuild of the index just in case a label collision occurs
			RebuildLabelIndex();
			if(container_caches != nullptr)
				container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());
		}
		else
		{
			if(container_caches != nullptr)
				container_caches->UpdateEntityLabels(this, GetEntityIndexOfContainer(), new_label_values_mcn);
		}

		asset_manager.UpdateEntity(this);
		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteValuesToEntity(this, new_label_values, direct_set);
		}

		if(num_new_nodes_allocated != nullptr)
		{
			size_t cur_size = GetDeepSizeInNodes();
			//don't get credit for freeing memory, but do count toward memory consumed
			if(cur_size > prev_size)
				*num_new_nodes_allocated = cur_size - prev_size;
		}
	}

	return std::make_pair(any_successful_assignment, all_successful_assignments);
}

EvaluableNodeReference Entity::Execute(ExecutionCycleCount max_num_steps, ExecutionCycleCount &num_steps_executed,
	size_t max_num_nodes, size_t &num_nodes_allocated,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	EvaluableNode *call_stack, bool on_self, EvaluableNodeManager *destination_temp_enm,
#ifdef MULTITHREAD_SUPPORT
	Concurrency::ReadLock *locked_memory_modification_lock,
	Concurrency::WriteLock *entity_write_lock,
#endif
	StringInternPool::StringID label_sid,
	Interpreter *calling_interpreter, bool copy_call_stack)
{
	if(!on_self && IsLabelPrivate(label_sid))
		return EvaluableNodeReference(nullptr, true);

#ifdef MULTITHREAD_SUPPORT
	if(locked_memory_modification_lock != nullptr)
		locked_memory_modification_lock->unlock();
#endif

	EvaluableNode *node_to_execute = nullptr;
	if(label_sid <= StringInternPool::EMPTY_STRING_ID)	//if not specified, then use root
		node_to_execute = evaluableNodeManager.GetRootNode();
	else //get code at label
	{
		const auto &label = labelIndex.find(label_sid);

		if(label != end(labelIndex))
			node_to_execute = label->second;
	}

	//if label not found or no code, can't do anything
	if(node_to_execute == nullptr)
	{
	#ifdef MULTITHREAD_SUPPORT
		//put lock back in place
		if(locked_memory_modification_lock != nullptr)
			locked_memory_modification_lock->lock();
	#endif
		return EvaluableNodeReference::Null();
	}

	size_t a_priori_entity_storage = evaluableNodeManager.GetNumberOfUsedNodes();

	Interpreter interpreter(&evaluableNodeManager, max_num_steps, max_num_nodes, randomStream.CreateOtherStreamViaRand(),
		write_listeners, print_listener, this, calling_interpreter);

#ifdef MULTITHREAD_SUPPORT
	interpreter.memoryModificationLock = Concurrency::ReadLock(interpreter.evaluableNodeManager->memoryModificationMutex);
	if(entity_write_lock != nullptr)
		entity_write_lock->unlock();
#endif

	if(copy_call_stack)
		call_stack = evaluableNodeManager.DeepAllocCopy(call_stack);

	EvaluableNodeReference retval = interpreter.ExecuteNode(node_to_execute, call_stack);
	num_steps_executed = interpreter.GetNumStepsExecuted();

#ifdef MULTITHREAD_SUPPORT
	//make sure have lock before copy into destination_temp_enm
	if(locked_memory_modification_lock != nullptr)
		locked_memory_modification_lock->lock();
#endif
	//make a copy in the appropriate location if possible and necessary
	if(destination_temp_enm != nullptr)
	{
		//only need to make a copy if it's a different destination
		if(destination_temp_enm != &evaluableNodeManager)
		{
			//make a copy and free the original
			EvaluableNodeReference retval_copy = destination_temp_enm->DeepAllocCopy(retval);
			evaluableNodeManager.FreeNodeTreeIfPossible(retval);
			retval = retval_copy;
		}
	}
	else //don't want anything back
	{
		evaluableNodeManager.FreeNodeTreeIfPossible(retval);
		retval = EvaluableNodeReference::Null();
	}

	//find difference in entity size
	size_t post_entity_storage = evaluableNodeManager.GetNumberOfUsedNodes() + interpreter.GetNumEntityNodesAllocated();
	if(a_priori_entity_storage > post_entity_storage)
		num_nodes_allocated = 0;
	else
		num_nodes_allocated = post_entity_storage - a_priori_entity_storage;

#ifdef MULTITHREAD_SUPPORT
	interpreter.memoryModificationLock.unlock();
#endif
		
	return retval;
}

bool Entity::IsEntityCurrentlyBeingExecuted()
{
	if(hasContainedEntities)
	{
		for(auto ce : entityRelationships.relationships->containedEntities)
		{
			if(ce->IsEntityCurrentlyBeingExecuted())
				return true;
		}
	}

	return evaluableNodeManager.IsAnyNodeReferencedOtherThanRoot();
}

EvaluableNodeReference Entity::GetRoot(EvaluableNodeManager *destination_temp_enm, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier)
{
	EvaluableNode *root = evaluableNodeManager.GetRootNode();

	if(destination_temp_enm == nullptr)
		return EvaluableNodeReference(root, false);

	return destination_temp_enm->DeepAllocCopy(root, metadata_modifier);
}

size_t Entity::GetDeepSizeInNodes()
{
	size_t total_size = GetSizeInNodes();

	//count one more for being an entity
	total_size += 1;

	//count one more if customly named
	if(IsNamedEntity(GetId()))
		total_size += 1;

	for(auto entity : GetContainedEntities())
		total_size += entity->GetDeepSizeInNodes();

	return total_size;
}

size_t Entity::GetEstimatedReservedDeepSizeInBytes()
{
	size_t total_size = evaluableNodeManager.GetEstimatedTotalReservedSizeInBytes();
	
	for(auto entity : GetContainedEntities())
		total_size += entity->GetEstimatedReservedDeepSizeInBytes();
	
	return total_size;
}

size_t Entity::GetEstimatedUsedDeepSizeInBytes()
{
	size_t total_size = evaluableNodeManager.GetEstimatedTotalUsedSizeInBytes();

	for(auto entity : GetContainedEntities())
		total_size += entity->GetEstimatedReservedDeepSizeInBytes();

	return total_size;
}

EvaluableNode::LabelsAssocType Entity::RebuildLabelIndex()
{
	auto [new_labels, renormalized] = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTreeAndNormalize(evaluableNodeManager.GetRootNode());

	//update references (create new ones before destroying old ones so they do not need to be recreated)
	string_intern_pool.CreateStringReferences(new_labels, [](auto l) { return l.first; } );
	string_intern_pool.DestroyStringReferences(labelIndex, [](auto l) { return l.first; });

	//let the destructor of new_labels deallocate the old labelIndex
	std::swap(labelIndex, new_labels);

	if(renormalized)
		new_labels.clear();

	//new_labels now holds the previous labels
	return new_labels;
}

StringInternPool::StringID Entity::AddContainedEntity(Entity *t, StringInternPool::StringID id_sid, std::vector<EntityWriteListener *> *write_listeners)
{
	if(t == nullptr)
		return StringInternPool::NOT_A_STRING_ID;

	EnsureHasContainedEntities();

	auto &id_to_index_lookup = entityRelationships.relationships->containedEntityStringIdToIndex;
	auto &contained_entities = entityRelationships.relationships->containedEntities;

	//the index that t will be inserted to
	size_t t_index = contained_entities.size();

	StringInternPool::StringID previous_t_sid = t->idStringId;
	
	//autoassign an ID if not specified
	if(id_sid == StringInternPool::NOT_A_STRING_ID)
	{
		std::string new_id;
		for(;;)
		{
			//add a _ in front to differentiate from numbers
			new_id = "_" + EvaluableNode::NumberToString(static_cast<size_t>(randomStream.RandUInt32()));

			t->idStringId = string_intern_pool.CreateStringReference(new_id);
			
			//if not currently in use, then use it and stop searching
			if(id_to_index_lookup.insert(std::make_pair(t->idStringId, t_index)).second == true)
				break;

			//couldn't add it, so must already be in use.  Free and make another
			string_intern_pool.DestroyStringReference(t->idStringId);
		}
	}
	else
	{
		//attempt to insert, or return empty string if fail
		if(id_to_index_lookup.insert(std::make_pair(id_sid, t_index)).second == false)
			return StringInternPool::NOT_A_STRING_ID;

		t->idStringId = string_intern_pool.CreateStringReference(id_sid);
	}

	//insert the entity pointer
	contained_entities.push_back(t);

	//clear previous references if applicable
	string_intern_pool.DestroyStringReference(previous_t_sid);

	t->SetEntityContainer(this);

	EntityQueryCaches *container_caches = GetQueryCaches();
	if(container_caches != nullptr)
		container_caches->AddEntity(t, t_index);

	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogCreateEntity(t);
		asset_manager.CreateEntity(t);
	}

	return t->idStringId;
}

StringInternPool::StringID Entity::AddContainedEntity(Entity *t, std::string id_string, std::vector<EntityWriteListener *> *write_listeners)
{
	if(t == nullptr)
		return StringInternPool::NOT_A_STRING_ID;

	EnsureHasContainedEntities();

	auto &id_to_index_lookup = entityRelationships.relationships->containedEntityStringIdToIndex;
	auto &contained_entities = entityRelationships.relationships->containedEntities;

	//the index that t will be inserted to
	size_t t_index = contained_entities.size();

	StringInternPool::StringID previous_t_sid = t->idStringId;

	//autoassign an ID if not specified
	if(id_string == "")
	{
		for(;;)
		{
			//add a _ in front to differentiate from numbers
			id_string = "_" + EvaluableNode::NumberToString(static_cast<size_t>(randomStream.RandUInt32()));

			t->idStringId = string_intern_pool.CreateStringReference(id_string);

			//if the string is currently in use, but not in this entity then use it and stop searching
			if(id_to_index_lookup.insert(std::make_pair(t->idStringId, t_index)).second == true)
				break;

			//couldn't add it, so must already be in use.  Free and make another
			string_intern_pool.DestroyStringReference(t->idStringId);
		}
	}
	else
	{
		t->idStringId = string_intern_pool.CreateStringReference(id_string);

		//attempt to insert, or return empty string if fail
		if(id_to_index_lookup.insert(std::make_pair(t->idStringId, t_index)).second == false)
		{
			string_intern_pool.DestroyStringReference(t->idStringId);
			return StringInternPool::NOT_A_STRING_ID;
		}
	}

	//insert the entity pointer
	contained_entities.push_back(t);

	//clear previous references if applicable
	string_intern_pool.DestroyStringReference(previous_t_sid);

	t->SetEntityContainer(this);

	EntityQueryCaches *container_caches = GetQueryCaches();
	if(container_caches != nullptr)
		container_caches->AddEntity(t, t_index);

	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogCreateEntity(t);
		asset_manager.CreateEntity(t);
	}

	return t->idStringId;
}


void Entity::RemoveContainedEntity(StringInternPool::StringID id, std::vector<EntityWriteListener *> *write_listeners)
{
	if(!hasContainedEntities)
		return;

	auto &id_to_index_lookup = entityRelationships.relationships->containedEntityStringIdToIndex;
	auto &contained_entities = entityRelationships.relationships->containedEntities;

	//find the entity by id
	const auto &id_to_index_it_to_remove = id_to_index_lookup.find(id);
	if(id_to_index_it_to_remove == end(id_to_index_lookup))
		return;

	//get the index
	size_t index_to_remove = id_to_index_it_to_remove->second;
	size_t index_to_replace = contained_entities.size() - 1;
	Entity *entity_to_remove = contained_entities[index_to_remove];
		
	//record the entity as being deleted
	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogDestroyEntity(entity_to_remove);

		asset_manager.DestroyEntity(entity_to_remove);
	}

	EntityQueryCaches *caches = GetQueryCaches();
	if(caches != nullptr)
		caches->RemoveEntity(entity_to_remove, index_to_remove, index_to_replace);

	entity_to_remove->SetEntityContainer(nullptr);

	//remove the lookup
	id_to_index_lookup.erase(id_to_index_it_to_remove);

	//if there's at least one entity left, then move the last one into this removed slot
	if(index_to_replace > 0)
	{
		//if not removing the last entity, then swap the last into the empty slot
		if(index_to_remove != index_to_replace)
		{
			//update the last entity's index and move it into the location removed
			id_to_index_lookup[contained_entities[index_to_replace]->GetIdStringId()] = index_to_remove;

			//swap last entity with this new one, remove from contained_entities
			std::swap(contained_entities[index_to_remove], contained_entities[index_to_replace]);
		}

		contained_entities.resize(index_to_replace);
	}
	else // removed the last entity, clean up
	{
		Entity *container = entityRelationships.relationships->container;
		delete entityRelationships.relationships;

		entityRelationships.container = container;
		hasContainedEntities = false;
	}
}

Entity *Entity::GetContainedEntity(StringInternPool::StringID id)
{
	if(!hasContainedEntities)
		return nullptr;

	auto &id_to_index_lookup = entityRelationships.relationships->containedEntityStringIdToIndex;
	const auto &it = id_to_index_lookup.find(id);
	if(it == end(id_to_index_lookup))
		return nullptr;

	//look up the pointer by its index
	return entityRelationships.relationships->containedEntities[it->second];
}

size_t Entity::GetContainedEntityIndex(StringInternPool::StringID id)
{
	if(!hasContainedEntities)
		return std::numeric_limits<size_t>::max();

	auto &id_to_index_lookup = entityRelationships.relationships->containedEntityStringIdToIndex;
	const auto &it = id_to_index_lookup.find(id);
	if(it == end(id_to_index_lookup))
		return std::numeric_limits<size_t>::max();

	//return the index
	return it->second;
}

StringInternPool::StringID Entity::GetContainedEntityIdFromIndex(size_t entity_index)
{
	if(!hasContainedEntities)
		return StringInternPool::NOT_A_STRING_ID;

	auto &contained_entities = entityRelationships.relationships->containedEntities;
	if(entity_index >= contained_entities.size())
		return StringInternPool::NOT_A_STRING_ID;

	return contained_entities[entity_index]->GetIdStringId();
}

EntityQueryCaches *Entity::GetOrCreateQueryCaches()
{
	EnsureHasContainedEntities();

	if(!entityRelationships.relationships->queryCaches)
		entityRelationships.relationships->queryCaches = std::make_unique<EntityQueryCaches>(this);

	return entityRelationships.relationships->queryCaches.get();
}

void Entity::SetRandomState(const std::string &new_state, bool deep_set_seed, std::vector<EntityWriteListener *> *write_listeners)
{
	randomStream.SetState(new_state);

	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogSetEntityRandomSeed(this, new_state, false);

		asset_manager.UpdateEntity(this);
	}

	if(deep_set_seed)
	{
		for(auto entity : GetContainedEntities())
			entity->SetRandomState(randomStream.CreateOtherStreamStateViaString(entity->GetId()), true, write_listeners);
	}
}

void Entity::SetRandomStream(const RandomStream &new_stream, std::vector<EntityWriteListener *> *write_listeners)
{
	randomStream = new_stream;

	if(write_listeners != nullptr)
	{
		if(write_listeners->size() > 0)
		{
			std::string new_state_string = randomStream.GetState();
			for(auto &wl : *write_listeners)
				wl->LogSetEntityRandomSeed(this, new_state_string, false);
		}

		asset_manager.UpdateEntity(this);
	}
}

std::string Entity::CreateRandomStreamFromStringAndRand(const std::string &seed_string)
{
	//consume a random number to advance the state for creating the new state
	randomStream.RandUInt32();
	return randomStream.CreateOtherStreamStateViaString(seed_string);
}

void Entity::SetRoot(EvaluableNode *_code, bool allocated_with_entity_enm, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier, std::vector<EntityWriteListener *> *write_listeners)
{
	EvaluableNode *previous_root = evaluableNodeManager.GetRootNode();

	if(_code == nullptr)
	{
		evaluableNodeManager.SetRootNode(evaluableNodeManager.AllocNode(ENT_NULL));
	}
	else if(allocated_with_entity_enm && metadata_modifier == EvaluableNodeManager::ENMM_NO_CHANGE)
	{		
		evaluableNodeManager.SetRootNode(_code);
	}
	else
	{
		auto code_copy = evaluableNodeManager.DeepAllocCopy(_code, metadata_modifier);
		evaluableNodeManager.SetRootNode(code_copy.reference);
	}

	//keep reference for current root
	evaluableNodeManager.KeepNodeReference(evaluableNodeManager.GetRootNode());

	//free current root reference
	evaluableNodeManager.FreeNodeReference(previous_root);

	RebuildLabelIndex();

	EntityQueryCaches *container_caches = GetContainerQueryCaches();
	if(container_caches != nullptr)
		container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());

	if(write_listeners != nullptr)
	{
		if(write_listeners->size() > 0)
		{
			std::string new_code_string = Parser::Unparse(evaluableNodeManager.GetRootNode(), &evaluableNodeManager);

			for(auto &wl : *write_listeners)
				wl->LogWriteToEntity(this, new_code_string);
		}

		asset_manager.UpdateEntity(this);
	}
}

void Entity::SetRoot(std::string &code_string, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier, std::vector<EntityWriteListener *> *write_listeners)
{
	EvaluableNodeReference new_code = Parser::Parse(code_string, &evaluableNodeManager);
	SetRoot(new_code.reference, true, metadata_modifier, write_listeners);
}

void Entity::AccumRoot(EvaluableNodeReference accum_code, bool allocated_with_entity_enm, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier, std::vector<EntityWriteListener *> *write_listeners)
{
	if( !(allocated_with_entity_enm && metadata_modifier == EvaluableNodeManager::ENMM_NO_CHANGE))
		accum_code = evaluableNodeManager.DeepAllocCopy(accum_code, metadata_modifier);

	bool accum_has_labels = EvaluableNodeTreeManipulation::DoesTreeContainLabels(accum_code);

	EvaluableNode *previous_root = evaluableNodeManager.GetRootNode();
	EvaluableNodeReference new_root = AccumulateEvaluableNodeIntoEvaluableNode(EvaluableNodeReference(previous_root, true), accum_code, &evaluableNodeManager);

	//need to check if still cycle free as it may no longer be
	EvaluableNodeManager::UpdateFlagsForNodeTree(new_root);

	if(new_root != previous_root)
	{
		//keep reference for current root (mainly in case a new node was added if the entity were previously empty)
		evaluableNodeManager.KeepNodeReference(new_root);

		evaluableNodeManager.SetRootNode(new_root);

		//free current root reference
		evaluableNodeManager.FreeNodeReference(previous_root);
	}

	size_t num_root_labels_to_update = 0;
	if(new_root != nullptr)
		num_root_labels_to_update = new_root->GetNumLabels();

	EntityQueryCaches *container_caches = GetContainerQueryCaches();

	if(accum_has_labels)
	{
		EvaluableNode::LabelsAssocType prev_labels = RebuildLabelIndex();

		//if have all new labels or RebuildLabelIndex had to renormalize (in which case prev_labels will be empty)
		// then update all labels just in case
		if(prev_labels.size() == 0 && labelIndex.size() > 0)
		{
			if(container_caches != nullptr)
				container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());

			//root labels have been updated
			num_root_labels_to_update = 0;
		}
		else //clean rebuild
		{
			if(container_caches != nullptr)
				container_caches->UpdateEntityLabelsAddedOrChanged(this, GetEntityIndexOfContainer(),
					prev_labels, labelIndex);
		}
	}
	
	//if any root labels left to update, then update them
	if(num_root_labels_to_update > 0)
	{
		//only need to update labels on root
		for(size_t i = 0; i < num_root_labels_to_update; i++)
		{
			auto label_sid = new_root->GetLabelStringId(i);
			if(container_caches != nullptr)
				container_caches->UpdateEntityLabel(this, GetEntityIndexOfContainer(), label_sid);
		}
	}

	if(write_listeners != nullptr)
	{
		if(write_listeners->size() > 0)
		{
			std::string new_code_string = Parser::Unparse(new_root, &evaluableNodeManager);

			for(auto &wl : *write_listeners)
				wl->LogWriteToEntity(this, new_code_string);
		}

		asset_manager.UpdateEntity(this);
	}
}

void Entity::GetAllDeeplyContainedEntitiesGroupedRecurse(std::vector<Entity *> &entities)
{
	if(!hasContainedEntities)
		return;

	auto &contained_entities = GetContainedEntities();
	entities.insert(end(entities), begin(contained_entities), end(contained_entities));

	//insert a nullptr at the end to indicate this group is complete
	entities.emplace_back(nullptr);

	for(auto &ce : contained_entities)
		ce->GetAllDeeplyContainedEntitiesGroupedRecurse(entities);
}
