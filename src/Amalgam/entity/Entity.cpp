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
	rootNode = evaluableNodeManager.AllocNode(ENT_ASSOC);
	evaluableNodeManager.KeepNodeReferences(rootNode);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(std::string &code_string, const std::string &rand_state)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;
	rootNode = nullptr;

	SetRoot(code_string);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(EvaluableNode *_root, const std::string &rand_state)
	: randomStream(rand_state)
{
	hasContainedEntities = false;
	entityRelationships.container = nullptr;
	rootNode = nullptr;

	//since this is the constructor, can't have had this entity's EntityNodeManager
	SetRoot(_root, false);

	idStringId = StringInternPool::NOT_A_STRING_ID;
}

Entity::Entity(Entity *t)
{
	//start with an empty entity to make sure SetRoot works fine
	randomStream = t->randomStream;
	hasContainedEntities = false;
	entityRelationships.container = nullptr;
	rootNode = nullptr;

	SetRoot(t->rootNode, false);

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
}

std::pair<EvaluableNodeReference, bool> Entity::GetValueAtLabel(
	StringInternPool::StringID label_sid, EvaluableNodeManager *destination_temp_enm,
	EvaluableNodeRequestedValueTypes immediate_result, bool on_self, bool batch_call)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(EvaluableNodeReference::Null(), false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(EvaluableNodeReference::Null(), false);

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);

	if(label == end(label_index))
		return std::pair(EvaluableNodeReference::Null(), false);

	auto retval = EvaluableNodeReference::CoerceNonUniqueEvaluableNodeToImmediateIfPossible(label->second, immediate_result);
	if(retval.IsImmediateValue())
		return std::pair(retval, true);

	//if didn't give a valid destination, just return what we have
	if(destination_temp_enm == nullptr)
		return std::pair(retval, true);

	return std::pair(destination_temp_enm->DeepAllocCopy(retval), true);
}

std::pair<bool, bool> Entity::GetValueAtLabelAsBool(StringInternPool::StringID label_sid, bool on_self)
{
	if(label_sid == string_intern_pool.NOT_A_STRING_ID)
		return std::pair(false, false);

	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(false, false);

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);
	if(label == end(label_index))
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

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);
	if(label == end(label_index))
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

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);
	if(label == end(label_index))
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

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);
	if(label == end(label_index))
		return std::pair(StringInternPool::NOT_A_STRING_ID, false);
	
	return std::pair(EvaluableNode::ToStringIDWithReference(label->second, key_string), true);
}

std::pair<EvaluableNodeImmediateValueWithType, bool> Entity::GetValueAtLabelAsImmediateValue(StringInternPool::StringID label_sid,
	bool on_self, EvaluableNodeManager *destination_temp_enm)
{
	if(!on_self && IsLabelPrivate(label_sid))
		return std::pair(EvaluableNodeImmediateValueWithType(std::numeric_limits<double>::quiet_NaN(), ENIVT_NOT_EXIST), false);

	auto &label_index = GetLabelIndex();
	const auto &label = label_index.find(label_sid);
	if(label == end(label_index))
		return std::pair(EvaluableNodeImmediateValueWithType(std::numeric_limits<double>::quiet_NaN(), ENIVT_NOT_EXIST), false);

	EvaluableNodeImmediateValueWithType retval;
	retval.CopyValueFromEvaluableNode(label->second, destination_temp_enm);
	return std::pair(retval, true);
}

//like SetValuesAtLabels, except accumulates each value at each label instead
std::pair<bool, bool> Entity::SetValuesAtLabels(EvaluableNodeReference new_label_values, bool accum_values,
	std::vector<EntityWriteListener *> *write_listeners, size_t *num_new_nodes_allocated, bool on_self)
{
	//can only work with assoc arrays
	if(!EvaluableNode::IsAssociativeArray(new_label_values))
		return std::make_pair(false, false);

	//if relevant, keep track of new memory allocated to the entity
	size_t prev_size = 0;
	if(num_new_nodes_allocated != nullptr)
		prev_size = GetDeepSizeInNodes();

	bool any_successful_assignment = false;
	bool all_successful_assignments = true;
	bool need_node_flags_updated = false;
	auto &new_label_values_mcn = new_label_values->GetMappedChildNodesReference();

	for(auto &[label_sid, new_value_node] : new_label_values_mcn)
	{
		if(!on_self && IsLabelPrivate(label_sid))
		{
			all_successful_assignments = false;
			continue;
		}

		//re-retrieve label_index each iteration in case root changes
		auto &label_index = GetLabelIndex();
		const auto &label_iterator = label_index.find(label_sid);

		EvaluableNodeReference new_value_reference(new_value_node, false);

		if(accum_values)
		{
			//can't accum into an empty location
			if(label_iterator == end(label_index))
			{
				all_successful_assignments = false;
				continue;
			}

			//need to make a copy in case it is modified, so pass in evaluableNodeManager
			EvaluableNodeReference value_destination_node(label_iterator->second, false);
			EvaluableNodeReference accumulated_value = AccumulateEvaluableNodeIntoEvaluableNode(value_destination_node,
				new_value_reference, &evaluableNodeManager);

			//overwrite the root's flags and value at the location
			rootNode->UpdateFlagsBasedOnNewChildNode(accumulated_value);

		#ifdef MULTITHREAD_SUPPORT
			//fence memory to ensure flags are up to date by flushing by using an atomic store
			//TODO 15993: once C++20 is widely supported, change type to atomic_ref
			std::atomic<EvaluableNode *> *atomic_ref
				= reinterpret_cast<std::atomic<EvaluableNode *> *>(&variable_location);
			atomic_ref->store(label_iterator->second, std::memory_order_release);
		#else
			label_iterator->second = accumulated_value;
		#endif
		}
		else
		{
			//make copy if needed
			if(!on_self)
				new_value_reference = evaluableNodeManager.DeepAllocCopy(new_value_reference);

			//if label doesn't exist, create new root to contain it
			if(label_iterator == end(label_index))
			{
				EvaluableNode *new_root = evaluableNodeManager.AllocNode(rootNode);
				//ensure flags are updated before new_root is exposed
				new_root->UpdateFlagsBasedOnNewChildNode(new_value_reference);
				auto &new_root_mcn = new_root->GetMappedChildNodesReference();
				new_root_mcn.emplace(label_sid, new_value_reference);
				string_intern_pool.CreateStringReference(label_sid);

				//can only free the root if nothing is running on this entity
				if(!evaluableNodeManager.AreAnyInterpretersRunning())
					evaluableNodeManager.FreeNode(rootNode);

				SetRootNode(new_root);
			}
			else
			{
				//overwrite the root's flags before value at the location
				rootNode->UpdateFlagsBasedOnNewChildNode(new_value_reference);

			#ifdef MULTITHREAD_SUPPORT
				//fence memory to ensure flags are up to date by flushing by using an atomic store
				//TODO 15993: once C++20 is widely supported, change type to atomic_ref
				std::atomic<EvaluableNode *> *atomic_ref
					= reinterpret_cast<std::atomic<EvaluableNode *> *>(&variable_location);
				atomic_ref->store(label_iterator->second, std::memory_order_release);
			#else
				label_iterator->second = new_value_reference;
			#endif
			}
		}

		any_successful_assignment = true;
	}

	if(any_successful_assignment)
	{
		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(container_caches != nullptr)
			container_caches->UpdateEntityLabels(this, GetEntityIndexOfContainer(), new_label_values_mcn);

		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogWriteLabelValuesToEntity(this, new_label_values, accum_values);
		}
		asset_manager.UpdateEntityLabelValues(this, new_label_values, accum_values);

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

std::pair<bool, bool> Entity::RemoveLabels(EvaluableNodeReference labels_to_remove,
		std::vector<EntityWriteListener *> *write_listeners, size_t *num_new_nodes_allocated, bool on_self)
{
	//can only work with ordered child nodes
	if(!EvaluableNode::IsOrderedArray(labels_to_remove))
		return std::make_pair(false, false);

	bool any_successful_remove = false;
	bool all_successful_removes = true;
	auto &labels_to_remove_ocn = labels_to_remove->GetOrderedChildNodesReference();
	std::vector<std::pair<StringInternPool::StringID, EvaluableNode *>> label_sids_and_values_to_remove;
	label_sids_and_values_to_remove.reserve(labels_to_remove_ocn.size());

	EvaluableNodeReference new_root = evaluableNodeManager.AllocNode(GetRoot());
	auto &new_root_mcn = new_root->GetMappedChildNodesReference();

	//captures all of the label data to remove and remove from new_root
	for(auto label_node : labels_to_remove_ocn)
	{
		StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(label_node, true);
		if(!on_self && IsLabelPrivate(label_sid))
		{
			all_successful_removes = false;
			continue;
		}

		auto found = new_root_mcn.find(label_sid);
		if(found == end(new_root_mcn))
			continue;

		label_sids_and_values_to_remove.emplace_back(label_sid, found->second);

		new_root_mcn.erase(found);
		string_intern_pool.DestroyStringReference(label_sid);
		any_successful_remove = true;
	}

	new_root->UpdateAllFlagsBasedOnNoReferencingChildNodes();

	if(any_successful_remove)
	{
		EntityQueryCaches *container_caches = GetContainerQueryCaches();
		if(container_caches != nullptr)
			container_caches->RemoveEntityLabels(this, GetEntityIndexOfContainer(), label_sids_and_values_to_remove);

		SetRootNode(new_root);

		if(write_listeners != nullptr)
		{
			for(auto &wl : *write_listeners)
				wl->LogRemoveLabesFromEntity(this, labels_to_remove);
		}
		asset_manager.RemoveEntityLabelValues(this, labels_to_remove);

		//needed to allocate new top node
		if(num_new_nodes_allocated != nullptr)
			(*num_new_nodes_allocated)++;
	}
	else //keep current root
	{
		evaluableNodeManager.FreeNode(new_root);
	}

	return std::make_pair(any_successful_remove, all_successful_removes);
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

	return evaluableNodeManager.AreAnyInterpretersRunning();
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

//digits for 62-base encoding
static constexpr std::array<char, 62> _base_62_digits = [] {
	std::array<char, 62> a{};
	std::size_t i = 0;
	for(char c = '0'; c <= '9'; c++)
		a[i++] = c;
	for(char c = 'a'; c <= 'z'; c++)
		a[i++] = c;
	for(char c = 'A'; c <= 'Z'; c++)
		a[i++] = c;
	return a;
}();

//powers of 62 for 62-base encoding
static constexpr std::array<uint64_t, 11> _powers_of_62 = [] {
	std::array<uint64_t, 11> powers{};
	powers[0] = 1;
	for(std::size_t i = 1; i < powers.size(); i++)
		powers[i] = powers[i - 1] * 62ULL;
	return powers;
}();

//encodes high_and low into a base 62 string starting with an underscore
//this encoding uses only characters that are available across all major file systems and thus do not need escaping
static std::string EncodeBase62(uint32_t high, uint32_t low)
{
	uint64_t combined_value = (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);

	std::string buffer(12, _base_62_digits[0]);
	//begin with leading underscore
	buffer[0] = '_';

	//convert to digits from most significant to least significant, starting with highest power of 62
	for(int i = 10; i >= 0; i--)
	{
		uint64_t divisor = _powers_of_62[i];
		uint64_t digit = combined_value / divisor;
		//skip over leading underscore
		buffer[1 + (10 - i)] = _base_62_digits[static_cast<std::size_t>(digit)];
		combined_value -= digit * divisor;
	}

	return buffer;
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
			new_id = EncodeBase62(randomStream.RandUInt32(), randomStream.RandUInt32());

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
			id_string = EncodeBase62(randomStream.RandUInt32(), randomStream.RandUInt32());

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

void Entity::SetRoot(EvaluableNode *_code, bool allocated_with_entity_enm, std::vector<EntityWriteListener *> *write_listeners)
{
	EvaluableNode *cur_root = GetRoot();
	bool entity_previously_empty = (cur_root == nullptr || cur_root->GetNumChildNodes() == 0);

	if(_code == nullptr || allocated_with_entity_enm)
	{
		SetRootNode(_code);
	}
	else
	{
		auto code_copy = evaluableNodeManager.DeepAllocCopy(_code);
		SetRootNode(code_copy);
	}

	//ensure the top node is an assoc
	if(!EvaluableNode::IsAssociativeArray(rootNode))
	{
		EvaluableNode *new_root = evaluableNodeManager.AllocNode(ENT_ASSOC);
		new_root->SetMappedChildNode(string_intern_pool.NOT_A_STRING_ID, rootNode);
		SetRootNode(new_root);
	}

	evaluableNodeManager.ExchangeNodeReference(rootNode, cur_root);

#ifdef AMALGAM_MEMORY_INTEGRITY
	VerifyEvaluableNodeIntegrity();
#endif

	if(entity_previously_empty)
		evaluableNodeManager.UpdateGarbageCollectionTrigger();

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

void Entity::SetRoot(std::string &code_string, std::vector<EntityWriteListener *> *write_listeners)
{
	auto [node, warnings, char_with_error] = Parser::Parse(code_string, &evaluableNodeManager);
	SetRoot(node, true, write_listeners);
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
