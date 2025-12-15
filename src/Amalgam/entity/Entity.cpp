//project headers:
#include "Entity.h"
#include "AssetManager.h"
#include "EntityQueries.h"
#include "EntityWriteListener.h"
#include "EvaluableNodeTreeManipulation.h"
#include "EvaluableNodeTreeFunctions.h"
#include "Interpreter.h"

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
std::vector<EntityReadReference> Entity::entityReadReferenceBuffer;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
thread_local
#endif
std::vector<EntityWriteReference> Entity::entityWriteReferenceBuffer;

std::vector<Entity *> Entity::emptyContainedEntities;

Entity::Entity()
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;

	SetRoot(nullptr, false);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(std::string &code_string, const std::string &rand_state, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;

	SetRoot(code_string, metadata_modifier);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(EvaluableNode *_root, const std::string &rand_state, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;

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
#ifdef AMALGAM_FAST_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
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

std::pair<EvaluableNodeReference, bool> Entity::GetValueAtLabel(
	StringInternPool::StringID label_sid, EvaluableNodeManager *destination_temp_enm,
	bool direct_get, bool on_self, bool batch_call)
{
	//TODO 21800: allow this to return references with immediate values, ensure they're marked as unique

	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(EvaluableNodeReference::Null(), false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(EvaluableNodeReference::Null(), false);

	const auto &label = labelIndex.find(label_sid);

	if(label == end(labelIndex))
		return std::pair(EvaluableNodeReference::Null(), false);

	if(label->second == nullptr)
		return std::pair(EvaluableNodeReference::Null(), true);

	EvaluableNodeReference retval(label->second, false);

	//if didn't give a valid destination, just return what we have
	if(destination_temp_enm == nullptr)
		return std::pair(retval, true);

	return std::pair(destination_temp_enm->DeepAllocCopy(retval,
		direct_get ? EvaluableNodeManager::ENMM_NO_CHANGE : EvaluableNodeManager::ENMM_REMOVE_ALL), true);
}

std::pair<bool, bool> Entity::GetValueAtLabelAsBool(StringInternPool::StringID label_sid, bool on_self)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(false, false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(false, false);

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
		return std::pair(false, false);

	return std::pair(EvaluableNode::ToBool(label->second), true);
}

std::pair<double, bool> Entity::GetValueAtLabelAsNumber(StringInternPool::StringID label_sid, bool on_self)
{
	constexpr double value_if_not_found = std::numeric_limits<double>::quiet_NaN();

	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(value_if_not_found, false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(value_if_not_found, false);

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
		return std::pair(value_if_not_found, false);

	return(std::pair(EvaluableNode::ToNumber(label->second), true));
}

std::pair<std::string, bool> Entity::GetValueAtLabelAsString(
	StringInternPool::StringID label_sid, bool on_self, bool key_string)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair("", false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair("", false);

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
		return std::pair("", false);

	return std::pair(EvaluableNode::ToString(label->second, key_string), true);
}

std::pair<StringInternPool::StringID, bool> Entity::GetValueAtLabelAsStringIdWithReference(
	StringInternPool::StringID label_sid, bool on_self, bool key_string)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(StringInternPool::NOT_A_STRING_ID, false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(StringInternPool::NOT_A_STRING_ID, false);

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
		return std::pair(StringInternPool::NOT_A_STRING_ID, false);
	
	return std::pair(EvaluableNode::ToStringIDWithReference(label->second, key_string), true);
}

std::pair<EvaluableNodeImmediateValueWithType, bool> Entity::GetValueAtLabelAsImmediateValue(StringInternPool::StringID label_sid,
	bool on_self, EvaluableNodeManager *destination_temp_enm)
{
	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(EvaluableNodeImmediateValueWithType(std::numeric_limits<double>::quiet_NaN(), ENIVT_NOT_EXIST), false);

	const auto &label = labelIndex.find(label_sid);
	if(label == end(labelIndex))
		return std::pair(EvaluableNodeImmediateValueWithType(std::numeric_limits<double>::quiet_NaN(), ENIVT_NOT_EXIST), false);

	EvaluableNodeImmediateValueWithType retval;
	retval.CopyValueFromEvaluableNode(label->second, destination_temp_enm);
	return std::pair(retval, true);
}

bool Entity::SetValueAtLabel(StringInternPool::StringID label_sid, EvaluableNodeReference &new_value, bool direct_set,
	std::vector<EntityWriteListener *> *write_listeners, bool on_self, bool batch_call, bool *need_node_flags_updated)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return false;

	if(!on_self)
	{
		if(IsLabelPrivate(label_sid))
			return EvaluableNodeReference::Null();

		//since it's not setting on self, another entity owns the data so it isn't unique to this entity
		new_value.unique = false;
		new_value.uniqueUnreferencedTopNode = false;
	}

	auto current_node = labelIndex.find(label_sid);

	//if the label is not in the system, then can't do anything
	if(current_node == end(labelIndex))
		return false;

	EvaluableNode *destination = current_node->second;

	//can't replace if the label points to null - shouldn't happen
	if(destination == nullptr)
		return false;

	//determine whether this label is cycle free -- if the value changes, then need to update the entity
	bool dest_prev_value_need_cycle_check = destination->GetNeedCycleCheck();
	bool dest_prev_value_idempotent = destination->GetIsIdempotent();
	bool root_rebuilt = false;

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
		//the value is being used in the entity, so no longer unique if it was before
		//however, the top node may still be unique, so leave it alone
		new_value.unique = false;
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
		//the top node was placed in the entity so it is no longer unique
		new_value.unique = false;
		new_value.uniqueUnreferencedTopNode = false;

		//update the index
		labelIndex[label_sid] = new_value;

		//need to replace label in case there are any collapses of labels if multiple labels set
		EvaluableNode *root = evaluableNodeManager.GetRootNode();

		EvaluableNodeTreeManipulation::ReplaceLabelInTree(root, label_sid, new_value);
		evaluableNodeManager.SetRootNode(root);

		if(!batch_call)
			root_rebuilt = RebuildLabelIndex();
	}

	bool dest_new_value_need_cycle_check = (new_value != nullptr && new_value->GetNeedCycleCheck());
	bool dest_new_value_idempotent = (new_value != nullptr && new_value->GetIsIdempotent());

	if(batch_call)
	{
		//if any cycle check has changed, notify caller that flags need to be updated
		if(need_node_flags_updated != nullptr && dest_prev_value_need_cycle_check != dest_new_value_need_cycle_check)
			*need_node_flags_updated = true;
	}
	else
	{
		//if cycle check was changed, and wasn't rebuilt, then need to do so now
		if(!root_rebuilt && (
				dest_prev_value_need_cycle_check != dest_new_value_need_cycle_check
				|| dest_prev_value_idempotent != dest_new_value_idempotent))
			EvaluableNodeManager::UpdateFlagsForNodeTree(evaluableNodeManager.GetRootNode());

		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(container_caches != nullptr)
			container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());

		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteLabelValueToEntity(this, label_sid, new_value, direct_set);
		}
		asset_manager.UpdateEntityLabelValue(this, label_sid, new_value, direct_set);
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
	{
		new_label_values.unique = false;
		new_label_values.uniqueUnreferencedTopNode = false;
	}

	if(copy_entity)
		SetRoot(GetRoot(), false);

	//if relevant, keep track of new memory allocated to the entity
	size_t prev_size = 0;
	if(num_new_nodes_allocated != nullptr)
		prev_size = GetDeepSizeInNodes();

	//make assignments
	bool any_successful_assignment = false;
	bool all_successful_assignments = true;
	bool need_node_flags_updated = false;
	auto &new_label_values_mcn = new_label_values->GetMappedChildNodesReference();

	for(auto &[assignment_id, assignment] : new_label_values_mcn)
	{
		StringInternPool::StringID variable_sid = assignment_id;
		EvaluableNodeReference variable_value_node(assignment, false);

		if(accum_values)
		{
			//need to make a copy in case it is modified, so pass in evaluableNodeManager
			EvaluableNodeReference value_destination_node(
				GetValueAtLabel(variable_sid, &evaluableNodeManager, true, true, true).first,
				true);
			//can't assign to a label if it doesn't exist
			if(value_destination_node == nullptr)
				continue;

			variable_value_node = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node, variable_value_node, &evaluableNodeManager);
		}

		if(SetValueAtLabel(variable_sid, variable_value_node, direct_set, write_listeners, on_self, true, &need_node_flags_updated))
			any_successful_assignment = true;
		else
			all_successful_assignments = false;
	}

	if(any_successful_assignment)
	{
		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(direct_set)
		{
			//direct assignments need a rebuild of the index just in case a label collision occurs -- will update node flags if needed
			RebuildLabelIndex();
			if(container_caches != nullptr)
				container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());
		}
		else
		{
			if(need_node_flags_updated)
				EvaluableNodeManager::UpdateFlagsForNodeTree(evaluableNodeManager.GetRootNode());

			if(container_caches != nullptr)
				container_caches->UpdateEntityLabels(this, GetEntityIndexOfContainer(), new_label_values_mcn);
		}

		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteLabelValuesToEntity(this, new_label_values, accum_values, direct_set);
		}
		asset_manager.UpdateEntityLabelValues(this, new_label_values, accum_values, direct_set);

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

EvaluableNodeReference Entity::ExecuteCodeAsEntity(EvaluableNode *code,
	EvaluableNode *scope_stack, Interpreter *calling_interpreter,
	std::vector<EntityWriteListener *> *write_listeners, PrintListener *print_listener,
	InterpreterConstraints *interpreter_constraints
#ifdef MULTITHREAD_SUPPORT
	, Concurrency::ReadLock *enm_lock
#endif
)
{
	//no code, can't do anything
	if(code == nullptr)
		return EvaluableNodeReference::Null();

	Interpreter interpreter(&evaluableNodeManager, randomStream.CreateOtherStreamViaRand(),
		write_listeners, print_listener, interpreter_constraints, this, calling_interpreter);

#ifdef MULTITHREAD_SUPPORT
	if(enm_lock == nullptr)
		interpreter.memoryModificationLock = Concurrency::ReadLock(evaluableNodeManager.memoryModificationMutex);
	else
		interpreter.memoryModificationLock = std::move(*enm_lock);
#endif

	EvaluableNodeReference retval = interpreter.ExecuteNode(code, scope_stack);

#ifdef MULTITHREAD_SUPPORT
	if(enm_lock != nullptr)
		*enm_lock = std::move(interpreter.memoryModificationLock);
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

	//count for creating the entity
	total_size += GetEntityCreationSizeInNodes();

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

bool Entity::RebuildLabelIndex()
{
	auto [new_labels, collision_free] = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTreeAndNormalize(evaluableNodeManager.GetRootNode());

	//update references (create new ones before destroying old ones so they do not need to be recreated)
	string_intern_pool.CreateStringReferences(new_labels, [](auto l) { return l.first; } );
	string_intern_pool.DestroyStringReferences(labelIndex, [](auto l) { return l.first; });

	//let the destructor of new_labels deallocate the old labelIndex
	std::swap(labelIndex, new_labels);
	return !collision_free;
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
			if(id_to_index_lookup.emplace(t->idStringId, t_index).second == true)
				break;

			//couldn't add it, so must already be in use.  Free and make another
			string_intern_pool.DestroyStringReference(t->idStringId);
		}
	}
	else
	{
		//attempt to insert, or return empty string if fail
		if(id_to_index_lookup.emplace(id_sid, t_index).second == false)
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
	}
	asset_manager.CreateEntity(t);

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
	if(id_string.empty())
	{
		for(;;)
		{
			//add a _ in front to differentiate from numbers
			id_string = "_" + EvaluableNode::NumberToString(static_cast<size_t>(randomStream.RandUInt32()));

			t->idStringId = string_intern_pool.CreateStringReference(id_string);

			//if the string is currently in use, but not in this entity then use it and stop searching
			if(id_to_index_lookup.emplace(t->idStringId, t_index).second == true)
				break;

			//couldn't add it, so must already be in use.  Free and make another
			string_intern_pool.DestroyStringReference(t->idStringId);
		}
	}
	else
	{
		t->idStringId = string_intern_pool.CreateStringReference(id_string);

		//attempt to insert, or return empty string if fail
		if(id_to_index_lookup.emplace(t->idStringId, t_index).second == false)
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
	}
	asset_manager.CreateEntity(t);

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
	if(!hasContainedEntities || id == string_intern_pool.NOT_A_STRING_ID)
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
	if(!hasContainedEntities || id == string_intern_pool.NOT_A_STRING_ID)
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

Entity *Entity::GetContainedEntityFromIndex(size_t entity_index)
{
	if(!hasContainedEntities)
		return nullptr;

	if(entity_index >= entityRelationships.relationships->containedEntities.size())
		return nullptr;

	//look up the pointer by its index
	return entityRelationships.relationships->containedEntities[entity_index];
}

void Entity::CreateQueryCaches()
{
	EnsureHasContainedEntities();

	if(!entityRelationships.relationships->queryCaches)
		entityRelationships.relationships->queryCaches = std::make_unique<EntityQueryCaches>(this);
}

void Entity::SetRandomState(const std::string &new_state, bool deep_set_seed,
	std::vector<EntityWriteListener *> *write_listeners,
	Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities)
{
	randomStream.SetState(new_state);

	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogSetEntityRandomSeed(this, new_state, false);

		asset_manager.UpdateEntityRandomSeed(this, new_state, deep_set_seed, all_contained_entities);
	}

	if(deep_set_seed)
	{
		for(auto entity : GetContainedEntities())
			entity->SetRandomState(randomStream.CreateOtherStreamStateViaString(entity->GetId()), true,
				write_listeners, all_contained_entities);
	}
}

void Entity::SetRandomStream(const RandomStream &new_stream, std::vector<EntityWriteListener *> *write_listeners,
	Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities)
{
	randomStream = new_stream;

	if(write_listeners != nullptr)
	{
		std::string new_state = randomStream.GetState();
		for(auto &wl : *write_listeners)
			wl->LogSetEntityRandomSeed(this, new_state, false);

		asset_manager.UpdateEntityRandomSeed(this, new_state, false, all_contained_entities);
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
	EvaluableNode *cur_root = GetRoot();
	bool entity_previously_empty = (cur_root == nullptr || cur_root->GetNumChildNodes() == 0);

	if(_code == nullptr
		|| (allocated_with_entity_enm && metadata_modifier == EvaluableNodeManager::ENMM_NO_CHANGE))
	{		
		evaluableNodeManager.SetRootNode(_code);
	}
	else
	{
		auto code_copy = evaluableNodeManager.DeepAllocCopy(_code, metadata_modifier);
		evaluableNodeManager.SetRootNode(code_copy);
	}

	if(entity_previously_empty)
		evaluableNodeManager.UpdateGarbageCollectionTrigger();

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	RebuildLabelIndex();

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	EntityQueryCaches *container_caches = GetContainerQueryCaches();
	if(container_caches != nullptr)
		container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());

	if(write_listeners != nullptr)
	{
		if(write_listeners->size() > 0)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteToEntityRoot(this);
		}
		asset_manager.UpdateEntityRoot(this);
	}
}

void Entity::SetPermissions(EntityPermissions permissions_to_set, EntityPermissions permission_values,
		bool deep_set_permissions, std::vector<EntityWriteListener *> *write_listeners,
		Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities)
{
	asset_manager.SetEntityPermissions(this, permissions_to_set, permission_values);

	if(write_listeners != nullptr)
	{
		for(auto &wl : *write_listeners)
			wl->LogSetEntityPermissions(this, permissions_to_set, permission_values, deep_set_permissions);

		asset_manager.UpdateEntityPermissions(this, permissions_to_set, permission_values,
			deep_set_permissions, all_contained_entities);
	}

	if(deep_set_permissions)
	{
		for(auto entity : GetContainedEntities())
			entity->SetPermissions(permissions_to_set, permission_values, true,
				write_listeners, all_contained_entities);
	}
}

void Entity::SetRoot(std::string &code_string, EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier,
	std::vector<EntityWriteListener *> *write_listeners)
{
	auto [node, warnings, char_with_error] = Parser::Parse(code_string, &evaluableNodeManager);
	SetRoot(node, true, metadata_modifier, write_listeners);
}

void Entity::AccumRoot(EvaluableNodeReference accum_code, bool allocated_with_entity_enm,
	EvaluableNodeManager::EvaluableNodeMetadataModifier metadata_modifier,
	std::vector<EntityWriteListener *> *write_listeners)
{
#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	if(!(allocated_with_entity_enm && metadata_modifier == EvaluableNodeManager::ENMM_NO_CHANGE))
		accum_code = evaluableNodeManager.DeepAllocCopy(accum_code, metadata_modifier);

	auto [new_labels, no_label_collisions] = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTree(accum_code);

	EvaluableNode *previous_root = evaluableNodeManager.GetRootNode();

	//before accumulating, check to see if flags will need to be updated
	bool node_flags_need_update = false;
	if(previous_root == nullptr)
	{
		node_flags_need_update = true;
	}
	else
	{
		//need to update node flags if new_root is cycle free, but accum_node isn't
		if(previous_root->GetNeedCycleCheck() && (accum_code != nullptr && !accum_code->GetNeedCycleCheck()))
			node_flags_need_update = true;

		//need to update node flags if new_root is idempotent and accum_node isn't
		if(previous_root->GetIsIdempotent() && (accum_code != nullptr && !accum_code->GetIsIdempotent()))
			node_flags_need_update = true;
	}

	if(write_listeners != nullptr)
	{
		if(write_listeners->size() > 0)
		{
			for(auto &wl : *write_listeners)
				wl->LogEntityAccumRoot(this, accum_code);
		}
		asset_manager.UpdateEntityRoot(this);
	}

	//accum, but can't treat as unique in case any other thread is accessing the data
	EvaluableNodeReference new_root = AccumulateEvaluableNodeIntoEvaluableNode(
		EvaluableNodeReference(previous_root, false), accum_code, &evaluableNodeManager);

	if(new_root != previous_root)
		evaluableNodeManager.SetRootNode(new_root);

	//attempt to insert the new labels as long as there's no collision
	for(auto &[label, value] : new_labels)
	{
		auto [new_entry, inserted] = labelIndex.emplace(label, value);
		if(inserted)
			string_intern_pool.CreateStringReference(label);
		else
			no_label_collisions = false;
	}

	EntityQueryCaches *container_caches = GetContainerQueryCaches();

	//can do a much more straightforward update if there are no label collisions and the root has no labels
	if(no_label_collisions && new_root->GetNumLabels() == 0)
	{
		if(node_flags_need_update)
			EvaluableNodeManager::UpdateFlagsForNodeTree(new_root);

		if(container_caches != nullptr)
			container_caches->UpdateEntityLabels(this, GetEntityIndexOfContainer(), new_labels);
	}
	else //either collisions or root node has at least one label
	{
		if(!no_label_collisions)
		{
			//all new labels have already been inserted
			auto [new_label_index, collision_free] = EvaluableNodeTreeManipulation::RetrieveLabelIndexesFromTreeAndNormalize(
				evaluableNodeManager.GetRootNode());

			std::swap(labelIndex, new_label_index);
		}
		else //RetrieveLabelIndexesFromTreeAndNormalize will update flags, but if no collisions, still need to check
		{
			if(node_flags_need_update)
				EvaluableNodeManager::UpdateFlagsForNodeTree(new_root);
		}

		if(container_caches != nullptr)
			container_caches->UpdateAllEntityLabels(this, GetEntityIndexOfContainer());
	}

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif
}

void Entity::VerifyEvaluableNodeIntegrity()
{
	EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(GetRoot(), &evaluableNodeManager);

	auto &nr = evaluableNodeManager.GetNodesReferenced();
	for(auto &[en, _] : nr.nodesReferenced)
		EvaluableNodeManager::ValidateEvaluableNodeTreeMemoryIntegrity(en);
}

void Entity::VerifyEvaluableNodeIntegrityAndAllContainedEntities()
{
	VerifyEvaluableNodeIntegrity();
	for(auto ce : GetContainedEntities())
		ce->VerifyEvaluableNodeIntegrity();
}
