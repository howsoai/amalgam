#pragma once

//project headers:
#include "Concurrency.h"
#include "EntityQueryCaches.h"
#include "HashMaps.h"
#include "Parser.h"
#include "RandomStream.h"

//system headers:
#include <string>
#include <type_traits>
#include <vector>

//forward declarations:
class Entity;
class EntityQueryCaches;
class EntityWriteListener;
class EvaluableNode;
class EvaluableNodeManagement;
class Interpreter;
class InterpreterConstraints;
class PrintListener;

//base class for accessing an entity via a reference
// includes everything that can be accessed via a read operation
// note that this class should not be used directly, which is why it does not yield access to edit entity other than nullptr
//need to templatize EntityType because can't forward declare a method
template<typename EntityType = Entity>
class EntityReference
{
public:
	constexpr EntityReference()
		: entity(nullptr)
	{	}

	constexpr EntityReference(EntityType *e)
		: entity(e)
	{	}

	//allow to use as an Entity *
	constexpr operator EntityType *()
	{
		return entity;
	}

	//allow to check for equality of pointers
	constexpr bool operator ==(EntityReference &other)
	{
		return entity == other.entity;
	}

	//allow to check for inequality of pointers
	constexpr bool operator !=(EntityReference &other)
	{
		return entity != other.entity;
	}

	//allow to use as an Entity *
	constexpr EntityType *operator->()
	{
		return entity;
	}

	//allow to use as an Entity *
	constexpr EntityType *operator*()
	{
		return entity;
	}

	EntityType *entity;
};

class EntityPermissions
{
public:
	using StorageType = uint8_t;
	enum class Permission : StorageType
	{
		NONE = 0,
		STD_OUT_AND_STD_ERR = 1 << 0,
		STD_IN = 1 << 1,
		LOAD = 1 << 2,
		STORE = 1 << 3,
		ENVIRONMENT = 1 << 4,
		ALTER_PERFORMANCE = 1 << 5,
		SYSTEM = 1 << 6,
		ALL = STD_OUT_AND_STD_ERR | STD_IN | LOAD | STORE | ENVIRONMENT | ALTER_PERFORMANCE | SYSTEM
	};

	EntityPermissions() = default;

	explicit inline EntityPermissions(Permission initial_permissions)
		: allPermissions(static_cast<StorageType>(initial_permissions))
	{}

	inline bool HasPermission(Permission permission) const
	{
		return (allPermissions & static_cast<StorageType>(permission)) != 0;
	}

	inline void SetPermission(Permission permission, bool enable = true)
	{
		if(enable)
			allPermissions |= static_cast<StorageType>(permission);
		else
			allPermissions &= ~static_cast<StorageType>(permission);
	}

	static inline EntityPermissions AllPermissions()
	{
		return EntityPermissions(Permission::ALL);
	}

	//builds a new assoc from enm and returns it populated with
	//the permissions
	inline EvaluableNode *GetPermissionsAsEvaluableNode(EvaluableNodeManager *enm)
	{
		EvaluableNode *permissions_en = enm->AllocNode(ENT_ASSOC);
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err),
			enm->AllocNode(HasPermission(Permission::STD_OUT_AND_STD_ERR)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_std_in),
			enm->AllocNode(HasPermission(Permission::STD_IN)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_load),
			enm->AllocNode(HasPermission(Permission::LOAD)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_store),
			enm->AllocNode(HasPermission(Permission::STORE)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_environment),
			enm->AllocNode(HasPermission(Permission::ENVIRONMENT)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_alter_performance),
			enm->AllocNode(HasPermission(Permission::ALTER_PERFORMANCE)));
		permissions_en->SetMappedChildNode(GetStringIdFromBuiltInStringId(ENBISI_system),
			enm->AllocNode(HasPermission(Permission::SYSTEM)));

		return permissions_en;
	}

	//returns a pair of [permissions_to_set, permission_values] corresponding to en
	//if en is an assoc, it will use key-value pairs to obtain the permissions and their values
	//otherwise it will set all permissions based on whether en is true
	static inline std::pair<EntityPermissions, EntityPermissions> EvaluableNodeToPermissions(EvaluableNode *en)
	{
		EntityPermissions permissions_to_set;
		EntityPermissions permission_values;

		if(EvaluableNode::IsAssociativeArray(en))
		{
			for(auto [permission_type, allow_en] : en->GetMappedChildNodesReference())
			{
				bool allow = EvaluableNode::ToBool(allow_en);

				if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_out_and_std_err))
				{
					permissions_to_set.SetPermission(Permission::STD_OUT_AND_STD_ERR, true);
					permission_values.SetPermission(Permission::STD_OUT_AND_STD_ERR, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_std_in))
				{
					permissions_to_set.SetPermission(Permission::STD_IN, true);
					permission_values.SetPermission(Permission::STD_IN, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_load))
				{
					permissions_to_set.SetPermission(Permission::LOAD, true);
					permission_values.SetPermission(Permission::LOAD, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_store))
				{
					permissions_to_set.SetPermission(Permission::STORE, true);
					permission_values.SetPermission(Permission::STORE, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_environment))
				{
					permissions_to_set.SetPermission(Permission::ENVIRONMENT, true);
					permission_values.SetPermission(Permission::ENVIRONMENT, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_alter_performance))
				{
					permissions_to_set.SetPermission(Permission::ALTER_PERFORMANCE, true);
					permission_values.SetPermission(Permission::ALTER_PERFORMANCE, allow);
				}
				else if(permission_type == GetStringIdFromBuiltInStringId(ENBISI_system))
				{
					permissions_to_set.SetPermission(Permission::SYSTEM, true);
					permission_values.SetPermission(Permission::SYSTEM, allow);
				}
			}
		}
		else if(EvaluableNode::ToBool(en))
		{
			permissions_to_set = AllPermissions();
			permission_values = AllPermissions();
		}

		return std::make_pair(permissions_to_set, permission_values);
	}

	//method to get the type into a basic permissions type easily
	Permission permissions() const noexcept
	{
		return static_cast<Permission>(allPermissions);
	}

	//permissions as a bit field for use with bitwise operations
	StorageType allPermissions = static_cast<StorageType>(Permission::NONE);
};

#ifdef MULTITHREAD_SUPPORT

//encapsulates EntityReference with a lock type
//need to templatize EntityType because can't forward declare a method
template<typename LockType, typename EntityType = Entity>
class EntityReferenceWithLock : public EntityReference<EntityType>
{
public:
	inline EntityReferenceWithLock() : EntityReference<EntityType>()
	{	}

	inline EntityReferenceWithLock(EntityType *e) : EntityReference<EntityType>(e)
	{
		if(e != nullptr)
			lock = e->template CreateEntityLock<LockType>();
		else
			lock = LockType();
	}

	LockType lock;
};

//primary reference to be used when reading from an entity
class EntityReadReference : public EntityReferenceWithLock<Concurrency::ReadLock, Entity>
{
public:
	using EntityReferenceWithLock<Concurrency::ReadLock, Entity>::EntityReferenceWithLock;
};

//primary reference to be used when writing to an entity
class EntityWriteReference : public EntityReferenceWithLock<Concurrency::WriteLock, Entity>
{
public:
	using EntityReferenceWithLock<Concurrency::WriteLock, Entity>::EntityReferenceWithLock;
};

#else //not MULTITHREAD_SUPPORT

//primary reference to be used when reading from an entity
class EntityReadReference : public EntityReference<Entity>
{
public:
	using EntityReference<Entity>::EntityReference;
};

//primary reference to be used when writing to an entity
class EntityWriteReference : public EntityReference<Entity>
{
public:
	using EntityReference<Entity>::EntityReference;
};

#endif

//An Entity is a container of code/data consisting comprised of a graph of EvaluableNode.
// They can contain other entities, can be queried and serialized.
class Entity
{
public:

	//type for looking up an entity based on a StringID
	using EntityLookupAssocType = FastHashMap<StringInternPool::StringID, Entity *>;

	//StringID to index
	using StringIdToIndexAssocType = FastHashMap<StringInternPool::StringID, size_t>;

	Entity();

	//create Entity from existing code, rand_state is the current state of the random number generator,
	Entity(std::string &code_string, const std::string &rand_state);
	Entity(EvaluableNode *_root, const std::string &rand_state);

	//Creates a new Entity as a copy of the Entity passed in; everything is identical except for the time created and id
	Entity(Entity *t);

	~Entity();

	//executes the code specified by code as if it were called on this entity using the specified scope_stack
	//note that code should be allocated from this entity
	// calling_interpreter should be the interpreter that is calling this, if applicable
	// write_listeners and print_listener will listen for any modifications to the entity and output as applicable
	// if interpreter_constraints is not nullptr, then it will constrain performance and update interpreter_constraints
	// if enm_lock is specified, it should be a lock on this entity's evaluableNodeManager.memoryModificationMutex
	EvaluableNodeReference ExecuteCodeAsEntity(EvaluableNode *code,
		EvaluableNode *scope_stack, Interpreter *calling_interpreter = nullptr,
		std::vector<EntityWriteListener *> *write_listeners = nullptr, PrintListener *print_listener = nullptr,
		InterpreterConstraints *interpreter_constraints = nullptr
#ifdef MULTITHREAD_SUPPORT
		, Concurrency::ReadLock *enm_lock = nullptr
#endif
	);

	//executes the entity on label_name,
	// if NOT_A_STRING_ID, then attempts to find corresponding label name or executes the root node
	// returns the result from the execution
	// if on_self is true, then it will be allowed to access private labels
	//see ExecuteCodeAsEntity for further parameter details
	EvaluableNodeReference Execute(StringInternPool::StringID label_sid,
		EvaluableNode *scope_stack, bool on_self = false, Interpreter *calling_interpreter = nullptr,
		std::vector<EntityWriteListener *> *write_listeners = nullptr, PrintListener *print_listener = nullptr,
		InterpreterConstraints *interpreter_constraints = nullptr
	#ifdef MULTITHREAD_SUPPORT
		, Concurrency::ReadLock *enm_lock = nullptr
	#endif
		)
	{
		if(!on_self && IsLabelPrivate(label_sid))
			return EvaluableNodeReference::Null();

		EvaluableNode *node_to_execute = nullptr;
		//if label is not specified, then check type to see if it has keys
		if(label_sid == string_intern_pool.NOT_A_STRING_ID
			&& !EvaluableNode::IsAssociativeArray(rootNode))
		{
			node_to_execute = rootNode;
		}
		else //get code at label
		{
			auto &label_index = GetLabelIndex();
			const auto &label = label_index.find(label_sid);

			if(label != end(label_index))
				node_to_execute = label->second;
		}

		return ExecuteCodeAsEntity(node_to_execute, scope_stack, calling_interpreter,
			write_listeners, print_listener, interpreter_constraints
		#ifdef MULTITHREAD_SUPPORT
			, enm_lock
		#endif
			);
	}

	//same as Execute but accepts a string for label name
	inline EvaluableNodeReference Execute(const std::string &label_name,
		EvaluableNode *scope_stack, bool on_self = false, Interpreter *calling_interpreter = nullptr,
		std::vector<EntityWriteListener *> *write_listeners = nullptr, PrintListener *print_listener = nullptr,
		InterpreterConstraints *interpreter_constraints = nullptr
	#ifdef MULTITHREAD_SUPPORT
		, Concurrency::ReadLock *enm_lock = nullptr
	#endif
		)
	{
		StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);
		return Execute(label_sid, scope_stack, on_self, calling_interpreter,
			write_listeners, print_listener, interpreter_constraints
		#ifdef MULTITHREAD_SUPPORT
			, enm_lock
		#endif
			);
	}

	//returns true if the entity or any of its contained entities are currently being executed, either because of multiple threads executing on it
	// or calls to contained entities back to the container etc., because certain operations (such as move and destroy)
	// cannot be completed if this is the case
	bool IsEntityCurrentlyBeingExecuted();

	//Returns the code for the Entity in string form
	inline std::string GetCodeAsString()
	{
		return Parser::Unparse(rootNode);
	}

	//returns the root of the entity
	// if destination_temp_enm is specified, then it will perform a copy
	EvaluableNodeReference GetRoot(EvaluableNodeManager *destination_temp_enm = nullptr)
	{
		if(destination_temp_enm == nullptr)
			return EvaluableNodeReference(rootNode, false);

		return destination_temp_enm->DeepAllocCopy(rootNode);
	}

	//Returns the number of nodes in the entity
	inline size_t GetSizeInNodes()
	{
		return EvaluableNode::GetDeepSize(rootNode);
	}

	//Returns the number of nodes in the entity and all contained entities
	size_t GetDeepSizeInNodes();

	//Returns the estimated size of all memory managers in this entity and all contained entities
	// only an estimate because the platform's underlying memory management system may need to allocate additional
	// memory that cannot be easily accounted for
	size_t GetEstimatedReservedDeepSizeInBytes();
	size_t GetEstimatedUsedDeepSizeInBytes();

	//Returns the EvaluableNode and true at the specified label_sid if the label is found
	// Returns nullptr and false if the label does not exist
	// Uses the EvaluableNodeManager destination_temp_enm to make a deep copy of the value.
	// If destination_temp_enm is nullptr, it will return the node reference directly.
	// If on_self is true, then it will be allowed to access private variables
	// If batch_call is true, then it assumes it will be called in a batch of updates and will
	//  not perform any cleanup or synchronization
	std::pair<EvaluableNodeReference, bool> GetValueAtLabel(StringInternPool::StringID label_sid,
		EvaluableNodeManager *destination_temp_enm,
		EvaluableNodeRequestedValueTypes immediate_result = EvaluableNodeRequestedValueTypes(),
		bool on_self = false, bool batch_call = false);

	//same as GetValueAtLabel but accepts a string for label_name
	inline std::pair<EvaluableNodeReference, bool> GetValueAtLabel(const std::string &label_name,
		EvaluableNodeManager *destination_temp_enm,
		EvaluableNodeRequestedValueTypes immediate_result = EvaluableNodeRequestedValueTypes(), bool on_self = false)
	{
		StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);
		return GetValueAtLabel(label_sid, destination_temp_enm, immediate_result, on_self);
	}

	//Returns true if the label specified by label_sid exists
	bool DoesLabelExist(StringInternPool::StringID label_sid)
	{
		auto &label_index = GetLabelIndex();
		auto cur_value_it = label_index.find(label_sid);
		return (cur_value_it != end(label_index));
	}

	//Evaluates the specified label into a bool and returns the value
	//If the label exists, returns the pair of the value and true.
	// Otherwise returns the pair of false and false.
	std::pair<bool, bool> GetValueAtLabelAsBool(StringInternPool::StringID label_sid, bool on_self = false);

	//Evaluates the specified label into a number returns the value
	//If the label exists, returns the pair of the value and true.
	// Otherwise returns the pair of NaN and false.
	std::pair<double, bool> GetValueAtLabelAsNumber(StringInternPool::StringID label_sid, bool on_self = false);

	//Evaluates the specified label into a string and returns the value
	//If the label exists, returns the pair of the value and true.
	// Otherwise returns the pair of empty string and false.
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	std::pair<std::string, bool> GetValueAtLabelAsString(StringInternPool::StringID label_sid, bool on_self = false, bool key_string = false);

	//Evaluates the specified label into a string and returns the value
	//If the label exists, returns the pair of the value and true.
	// Otherwise returns the pair of not a string and false.
	//if key_string is true, then it will generate a string used for comparing in assoc keys
	std::pair<StringInternPool::StringID, bool> GetValueAtLabelAsStringIdWithReference(StringInternPool::StringID label_sid, bool on_self = false, bool key_string = false);

	//Evaluates the specified label into a EvaluableNodeImmediateValueWithType
	//if destination_temp_enm is not null and code is needed, it will make a copy
	//If the label exists, returns the pair of the value and true.
	// Otherwise returns the pair of null and false.
	std::pair<EvaluableNodeImmediateValueWithType, bool> GetValueAtLabelAsImmediateValue(
		StringInternPool::StringID label_sid, bool on_self = false, EvaluableNodeManager *destination_temp_enm = nullptr);

	//Iterates over all of the labels, calling GetValueAtLabel for each,
	// and passing the label sid and the node to the user specified function func
	template<typename LabelFunc>
	inline void IterateFunctionOverLabels(LabelFunc func,
		EvaluableNodeManager *destination_temp_enm = nullptr,
		bool direct_get = false, bool on_self = false)
	{
		for(auto &[label_id, _] : GetLabelIndex())
		{
			EvaluableNode *node = GetValueAtLabel(label_id, destination_temp_enm,
				EvaluableNodeRequestedValueTypes::Type::NONE, on_self, true).first;
			if(node != nullptr)
				func(label_id, node);
		}
	}

	//Sets the node at label_name to new_value.
	// If new_value is unique (EvaluableNodeReference property) and on_self is true, then it will take ownership of new_value
	//Retains true if the value (or modification thereof) was able to be set, false if the label does not exist or it fails for other reasons
	// If direct_get is true, then it will return values with all labels
	// If on_self is true, then it will be allowed to access private variables
	// If batch_call is true, then it assumes it will be called in a batch of updates and will not perform any cleanup
	// need_node_flags_updated is used when batch_call = true.  if need_node_flags_updated is not null, then it set the value to true
	// if the Entity needs to have its node flags updated at the end of this batch update, because a cycle free flag has changed
	//note that this cannot be called concurrently on the same entity
	bool SetValueAtLabel(StringInternPool::StringID label_sid, EvaluableNodeReference &new_value, bool direct_set,
		std::vector<EntityWriteListener *> *write_listeners, bool on_self = false, bool batch_call = false,
		bool *need_node_flags_updated = nullptr);

	//For each label-value pair in an associative array new_label_values, attempts to set the value at the label
	// If new_value is unique (EvaluableNodeReference property) and on_self is true, then it will take ownership of new_value
	// returns a pair of values; the first is true if any assignment was successful, the second is only true if all assignments were successful
	// if accum_values is true, then it will accumulate the values to the labels rather than setting them
	// if num_new_nodes_allocated is not null, then it will be set to the total amount of new memory taken up by the entity at the end of the call
	// other parameters match those of SetValueAtLabel, and will call SetValueAtLabel with batch_call = true
	// if copy_entity is true, then it will make a full copy of the entity before setting the labels in a copy-on-write fashion (for concurrent access)
	std::pair<bool, bool> SetValuesAtLabels(EvaluableNodeReference new_label_values, bool accum_values, bool direct_set,
		std::vector<EntityWriteListener *> *write_listeners, size_t *num_new_nodes_allocated, bool on_self, bool copy_entity);

	//Returns the id for this Entity
	inline const std::string &GetId()
	{
		return string_intern_pool.GetStringFromID(GetIdStringId());
	}

	//Returns the Id String's StringID (the index pointing to the Entity's ID string)
	constexpr StringInternPool::StringID GetIdStringId()
	{
		return idStringId;
	}

	//Adds t to be contained by this Entity
	// if _id is empty, then it will automatically generate an _id
	//returns the id used, empty string on failure
	/// write_listeners is optional, and if specified, will log the event
	StringInternPool::StringID AddContainedEntity(Entity *t, StringInternPool::StringID id_sid, std::vector<EntityWriteListener *> *write_listeners = nullptr);

	StringInternPool::StringID AddContainedEntity(Entity *t, std::string id_string, std::vector<EntityWriteListener *> *write_listeners = nullptr);

	inline void AddContainedEntityViaReference(Entity *t, StringRef &sir, std::vector<EntityWriteListener *> *write_listeners = nullptr)
	{
		StringInternPool::StringID new_sid = AddContainedEntity(t, static_cast<StringInternPool::StringID>(sir), write_listeners);
		sir.SetIDAndCreateReference(new_sid);
	}

	//Removes the specified id from being contained by this Entity
	/// write_listeners is optional, and if specified, will log the event
	void RemoveContainedEntity(StringInternPool::StringID id, std::vector<EntityWriteListener *> *write_listeners = nullptr);

	//returns the Entity contained by this Entity for the given id, null if it does not exist
	Entity *GetContainedEntity(StringInternPool::StringID id);

	//returns the entity index for the given id
	// if not found, will return std::numeric_limits<size_t>::max()
	size_t GetContainedEntityIndex(StringInternPool::StringID id);

	//looks up the contained entity's string id based on its index in contained entities list
	StringInternPool::StringID GetContainedEntityIdFromIndex(size_t entity_index);

	//returns the Entity contained by this Entity for the given index, null if it does not exist
	Entity *GetContainedEntityFromIndex(size_t entity_index);

	//returns true if this entity has one or more contained entities
	constexpr bool HasContainedEntities()
	{
		return hasContainedEntities;
	}

	//returns the number of contained entities
	inline size_t GetNumContainedEntities()
	{
		if(hasContainedEntities)
			return entityRelationships.relationships->containedEntities.size();
		else
			return 0;
	}

	//returns the total number of all contained entities including indirectly contained entities
	inline size_t GetTotalNumContainedEntitiesIncludingSelf()
	{
		size_t total = 1;
		if(hasContainedEntities)
		{
			for(Entity *e : entityRelationships.relationships->containedEntities)
				total += e->GetTotalNumContainedEntitiesIncludingSelf();
		}

		return total;
	}

	//returns direct access to vector of pointers to Entity objects contained by this Entity
	inline std::vector<Entity *> &GetContainedEntities()
	{
		if(hasContainedEntities)
			return entityRelationships.relationships->containedEntities;
		else
			return emptyContainedEntities;
	}

	//returns the containing entity
	inline Entity *GetContainer()
	{
		if(hasContainedEntities)
			return entityRelationships.relationships->container;
		else
			return entityRelationships.container;
	}

	//returns true if the entity has one or more contained entities and has a query cache built
	bool HasQueryCaches()
	{
		if(!hasContainedEntities || !entityRelationships.relationships->queryCaches)
			return false;
		return true;
	}

	//clears any query caches if they exist
	//when calling this, must ensure that there is a write lock on the entity or that nothing can execute on it
	inline void ClearQueryCaches()
	{
		if(!HasQueryCaches())
			return;

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		//obtain a write lock and immediately release it to make sure there aren't any operations
		//waiting to complete. don't need to worry about new operations as they will not be able
		//to start with a write lock on this entity
		Concurrency::WriteLock write_lock(entityRelationships.relationships->queryCaches->mutex);
		write_lock.release();
	#endif

		entityRelationships.relationships->queryCaches.reset();
	}

	inline void ClearQueryCacheForLabel(StringInternPool::StringID label_sid)
	{
		if(!HasQueryCaches())
			return;

		entityRelationships.relationships->queryCaches->RemoveLabelFromCache(label_sid);
	}

	//creates a cache if it does not exist
	void CreateQueryCaches();

	//returns a pointer to the query caches for this entity
	//returns a nullptr if does not have an active cache
	inline EntityQueryCaches *GetQueryCaches()
	{
		if(!HasQueryCaches())
			return nullptr;
		return entityRelationships.relationships->queryCaches.get();
	}

	//returns a pointer to the query caches for this entity's container
	//returns a nullptr if it does not have a container or the container does not have an active cache
	inline EntityQueryCaches *GetContainerQueryCaches()
	{
		Entity *container = GetContainer();
		if(container == nullptr)
			return nullptr;
		return container->GetQueryCaches();
	}

	//returns the index of the entity as listed by its container
	// returns 0 if it has no container
	inline size_t GetEntityIndexOfContainer()
	{
		Entity *container = GetContainer();
		if(container == nullptr)
			return 0;

		auto index_it = container->entityRelationships.relationships->containedEntityStringIdToIndex.find(idStringId);
		return index_it->second;
	}

	//returns true if this Entity contains e within its own contained entities or any sub entity contains it
	bool DoesDeepContainEntity(Entity *e)
	{
		//climb back up and see if any container matches this
		while(e != nullptr)
		{
			Entity *e_container = e->GetContainer();
			if(e_container == this)
				return true;

			e = e_container;
		}
		return true;
	}

	//stores a buffer reference of entity references and cleans up the references when it goes out of scope
	template<typename EntityReferenceType>
	class EntityReferenceBufferReference
	{
	public:
		inline EntityReferenceBufferReference()
			: maxEntityPathDepth(0), bufferReference(nullptr)
		{ }

		inline EntityReferenceBufferReference(std::vector<EntityReferenceType> &buffer)
			: maxEntityPathDepth(0), bufferReference(&buffer)
		{ }

		inline EntityReferenceBufferReference(EntityReferenceBufferReference &&erbr)
		{
			bufferReference = erbr.bufferReference;
			maxEntityPathDepth = erbr.maxEntityPathDepth;
			erbr.bufferReference = nullptr;
		}

		inline ~EntityReferenceBufferReference()
		{
			Clear();
		}

		inline void Clear()
		{
			if(bufferReference != nullptr)
			{
				bufferReference->clear();
				bufferReference = nullptr;
				maxEntityPathDepth = 0;
			}
		}

		inline EntityReferenceBufferReference &operator=(EntityReferenceBufferReference &&erbr)
		{
			if(this != &erbr)
			{
				if(bufferReference != nullptr)
					bufferReference->clear();

				bufferReference = erbr.bufferReference;
				maxEntityPathDepth = erbr.maxEntityPathDepth;
				erbr.bufferReference = nullptr;
			}
			return *this;
		}

		constexpr operator std::vector<EntityReferenceType> *()
		{
			return bufferReference;
		}

		constexpr std::vector<EntityReferenceType> *operator->()
		{
			return bufferReference;
		}

		constexpr std::vector<EntityReferenceType> &operator*()
		{
			return *bufferReference;
		}

		//maximum depth of an id path
		size_t maxEntityPathDepth;

	protected:
		std::vector<EntityReferenceType> *bufferReference;
	};

	//returns a list of references for all entities contained, all entities they contain, etc. grouped by all
	//entities at the same level of depth
	//returns the thread_local static variable entity[Read|Write]ReferenceBuffer, so results will be invalidated
	//by subsequent calls
	//if include_this_entity is true, it will include the entity in the references
	//if exclude_entity is not nullptr, it will not include it, for example, if it's already locked
	template<typename EntityReferenceType>
	inline EntityReferenceBufferReference<EntityReferenceType> GetAllDeeplyContainedEntityReferencesGroupedByDepth(
		bool include_this_entity = false, Entity *exclude_entity = nullptr)
	{
		EntityReferenceBufferReference<EntityReferenceType> erbr;
		if constexpr(std::is_same<EntityReferenceType, EntityWriteReference>::value)
			erbr = EntityReferenceBufferReference(entityWriteReferenceBuffer);
		else
			erbr = EntityReferenceBufferReference(entityReadReferenceBuffer);

		erbr.maxEntityPathDepth = 0;

		if(include_this_entity)
		{
			//don't put the entity in the buffer if it's excluded,
			// as it should already have a lock, but include it in the count below
			if(this != exclude_entity)
			{
				if constexpr(std::is_same<EntityReferenceType, EntityWriteReference>::value)
				{
					if(IsEntityCurrentlyBeingExecuted())
						return erbr;
					entityWriteReferenceBuffer.emplace_back(this);
				}
				else
				{
					entityReadReferenceBuffer.emplace_back(this);
				}
			}

			erbr.maxEntityPathDepth++;
		}

		size_t max_depth = 0;
		GetAllDeeplyContainedEntityReferencesGroupedByDepthRecurse<EntityReferenceType>(0, max_depth, exclude_entity);
		erbr.maxEntityPathDepth += max_depth;
		return erbr;
	}

	//appends deeply contained entity references to erbr
	template<typename EntityReferenceType>
	void AppendAllDeeplyContainedEntityReferencesGroupedByDepth(EntityReferenceBufferReference<EntityReferenceType> &erbr)
	{
		size_t max_depth = 0;
		GetAllDeeplyContainedEntityReferencesGroupedByDepthRecurse<EntityReferenceType>(0, max_depth, nullptr);
		erbr.maxEntityPathDepth += max_depth;
	}

	//gets the current state of the random stream in string form
	inline std::string GetRandomState()
	{
		return randomStream.GetState();
	}

	//gets the current random stream in RandomStream form
	inline RandomStream GetRandomStream()
	{
		return randomStream;
	}

	//sets (seeds) the current state of the random stream based on string
	// if deep_set_seed is true, it will recursively set all contained entities with appropriate seeds
	// write_listeners is optional, and if specified, will log the event
	// all_contained_entities, if specified, may be used for updating entities
	void SetRandomState(const std::string &new_state, bool deep_set_seed,
		std::vector<EntityWriteListener *> *write_listeners = nullptr,
		Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities = nullptr);

	//sets (seeds) the current state of the random stream based on RandomStream
	// write_listeners is optional, and if specified, will log the event
	void SetRandomStream(const RandomStream &new_stream, std::vector<EntityWriteListener *> *write_listeners = nullptr,
		Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities = nullptr);

	//returns a random seed based on a random number consumed from the entity and seed_string parameter
	std::string CreateRandomStreamFromStringAndRand(const std::string &seed_string);

	//sets the permissions on the entity
	// if deep_set_permissions is true, it will recursively set all contained entities with appropriate seeds
	// write_listeners is optional, and if specified, will log the event
	// all_contained_entities, if specified, may be used for updating entities
	void SetPermissions(EntityPermissions permissions_to_set, EntityPermissions permission_values,
		bool deep_set_permissions, std::vector<EntityWriteListener *> *write_listeners = nullptr,
		Entity::EntityReferenceBufferReference<EntityWriteReference> *all_contained_entities = nullptr);

	//Returns true if the Entity is a named entity, that is, its ID is not autogenerated
	// An identity is considered named if the string represents anything other than an integer
	inline static bool IsNamedEntity(const std::string &id)
	{
		auto position_non_integer_underscore = id.find_first_not_of("_0123456789");
		return position_non_integer_underscore != std::string::npos;
	}

	inline static bool IsNamedEntity(StringInternPool::StringID id)
	{
		auto &id_name = string_intern_pool.GetStringFromID(id);
		if(id_name == StringInternPool::EMPTY_STRING)
			return false;
		return IsNamedEntity(id_name);
	}

	//Sets the code and recreates the index
	//if allocated_with_entity_enm is false, then it will copy the tree into the entity's EvaluableNodeManager, otherwise it will just assume it is already available
	//write_listeners is optional, and if specified, will log the event
	void SetRoot(EvaluableNode *_code, bool allocated_with_entity_enm,
		std::vector<EntityWriteListener *> *write_listeners = nullptr);
	void SetRoot(std::string &code_string,
		std::vector<EntityWriteListener *> *write_listeners = nullptr);

	//accumulates the code and recreates the index
	//if allocated_with_entity_enm is false, then it will copy the tree into the entity's EvaluableNodeManager, otherwise it will just assume it is already available
	//write_listeners is optional, and if specified, will log the event
	void AccumRoot(EvaluableNodeReference _code, bool allocated_with_entity_enm,
		std::vector<EntityWriteListener *> *write_listeners = nullptr);

	//collects garbage on evaluableNodeManager, assuming it has a write reference
#ifdef MULTITHREAD_SUPPORT
	__forceinline void CollectGarbageWithEntityWriteReference()
	{
		if(evaluableNodeManager.RecommendGarbageCollection()
				&& !evaluableNodeManager.AreAnyInterpretersRunning())
			evaluableNodeManager.CollectGarbage();
	}
#else
	__forceinline void CollectGarbageWithEntityWriteReference()
	{
		if(evaluableNodeManager.RecommendGarbageCollection())
			evaluableNodeManager.CollectGarbage();
	}
#endif

	//reclaims resources as requested by parameters; assumes there is a write lock on this entity
	inline void ReclaimResources(bool clear_query_caches, bool collect_garbage, bool force_free_memory)
	{
		if(clear_query_caches)
			ClearQueryCaches();

		if(collect_garbage)
		{
			evaluableNodeManager.UpdateGarbageCollectionTriggerForImmediateCollection();
			CollectGarbageWithEntityWriteReference();
		}

		if(force_free_memory)
			evaluableNodeManager.ShrinkMemoryToCurrentUtilization();
	}

	//returns true if the label can be queried upon
	static inline bool IsLabelValidAndPublic(StringInternPool::StringID label_sid)
	{
		if(label_sid == string_intern_pool.NOT_A_STRING_ID)
			return false;

		auto &label_name = string_intern_pool.GetStringFromID(label_sid);
		return IsLabelValidAndPublic(label_name);
	}

	//same as the same-named function with StringInternPool::StringID but with actual string
	static inline bool IsLabelValidAndPublic(const std::string &label_name)
	{
		//allow size zero label
		if(label_name.size() == 0)
			return true;
		//commented out label
		if(label_name[0] == '#')
			return false;
		return !IsLabelPrivate(label_name);
	}

	//returns true if the label is only accessible to itself (starts with !)
	static inline bool IsLabelPrivate(StringInternPool::StringID label_sid)
	{
		auto &label_name = string_intern_pool.GetStringFromID(label_sid);
		return IsLabelPrivate(label_name);
	}

	//same as the same-named function with StringInternPool::StringID but with actual string
	static inline bool IsLabelPrivate(const std::string &label_name)
	{
		if(label_name.size() == 0)
			return false;
		if(label_name[0] == '!')
			return true;
		return false;
	}

	//returns true if the label is accessible to contained entities (starts with ^)
	static inline bool IsLabelAccessibleToContainedEntities(StringInternPool::StringID label_sid)
	{
		auto &label_name = string_intern_pool.GetStringFromID(label_sid);
		return IsLabelAccessibleToContainedEntities(label_name);
	}

	//same as the same-named function with StringInternPool::StringID but with actual string
	static inline bool IsLabelAccessibleToContainedEntities(const std::string &label_name)
	{
		if(label_name.size() == 0)
			return false;
		if(label_name[0] == '^')
			return true;
		return false;
	}

#ifdef MULTITHREAD_SUPPORT
	//Returns an appropriate lock object for operations on this Entity
	//Note that it will only lock the Entity's immediate attributes, not contained entities, code, etc.
	template<typename LockType>
	inline LockType CreateEntityLock()
	{
		return LockType(mutex);
	}
#endif

	//ensures that there are no reachable nodes that are deallocated
	void VerifyEvaluableNodeIntegrity();

	//like VerifyEvaluableNodeIntegrity but includes all contained
	void VerifyEvaluableNodeIntegrityAndAllContainedEntities();

	//this is an estimate of the number of nodes required to reconstruct the entity if it were flattened
	// including amortization of all extra overhead
	static inline size_t GetEntityCreationSizeInNodes()
	{
		return 10;
	}

	//nodes used for storing the entity and for all interpreters for this entity
	//the 0th node is implicitly the root node of the entity
	EvaluableNodeManager evaluableNodeManager;

protected:

	//helper function for GetAllDeeplyContainedEntityReadReferencesGroupedByDepth
	template<typename EntityReferenceType>
	bool GetAllDeeplyContainedEntityReferencesGroupedByDepthRecurse(
		size_t cur_depth, size_t &max_depth, Entity *exclude_entity)
	{
		if(cur_depth > max_depth)
			max_depth = cur_depth;

		if(!hasContainedEntities)
			return true;

		auto &contained_entities = GetContainedEntities();
		for(Entity *e : contained_entities)
		{
			if(e == exclude_entity)
				continue;

			if constexpr(std::is_same<EntityReferenceType, EntityWriteReference>::value)
			{
				if(e->IsEntityCurrentlyBeingExecuted())
					return false;

				entityWriteReferenceBuffer.emplace_back(e);
			}
			else
			{
				entityReadReferenceBuffer.emplace_back(e);
			}
		}

		for(auto &ce : contained_entities)
		{
			if(!ce->GetAllDeeplyContainedEntityReferencesGroupedByDepthRecurse<EntityReferenceType>(cur_depth + 1,
					max_depth, exclude_entity))
				return false;
		}

		return true;
	}

	//ensures the data structures will exist for containing entities if they don't already
	inline void EnsureHasContainedEntities()
	{
		if(!hasContainedEntities)
		{
			Entity *container = entityRelationships.container;
			entityRelationships.relationships = new EntityRelationships;

			entityRelationships.relationships->container = container;
			hasContainedEntities = true;
		}
	}

	//sets or overwrites the current container of this entity
	inline void SetEntityContainer(Entity *container)
	{
		if(hasContainedEntities)
			entityRelationships.relationships->container = container;
		else
			entityRelationships.container = container;
	}

	//when an entity has contained entities, then it needs to store the container and the contained entities
	struct EntityRelationships
	{
		//entities contained by this Entity
		std::vector<Entity *> containedEntities;

		//lookup from StringInternPool::StringID to the index in containedEntities corresponding to that entity
		// Note that even though these are are references to StringInternPool::StringID, they are not counted as references
		// because the entities are keeping track; if an entity exists, then its ID will still be a valid string reference
		StringIdToIndexAssocType containedEntityStringIdToIndex;

		//reference to the Entity that this Entity is contained by
		Entity *container;

		//caches for querying contained entities, constructed if needed
		std::unique_ptr<EntityQueryCaches> queryCaches;
	};

	//pointer to either the container or the EntityRelationships
	union EntityRelationshipsReference
	{
		Entity *container;
		EntityRelationships *relationships;
	};

#ifdef MULTITHREAD_SUPPORT
	//mutex for operations that may edit or modify the entity's properties and attributes
	Concurrency::ReadWriteMutex mutex;
#endif

	inline EvaluableNode::AssocType &GetLabelIndex()
	{
		//TODO 24298: remove this?
		static EvaluableNode::AssocType emptyMappedChildNodes;
		if(!EvaluableNode::IsAssociativeArray(rootNode))
			return emptyMappedChildNodes;

		return rootNode->GetMappedChildNodesReference();
	}

	//root of the entity
	EvaluableNode *rootNode;

	//if true, then the entity has contained entities and will use the relationships reference of entityRelationships
	//note this is located after labelIndex because labelIndex is of a size that does not align tightly
	bool hasContainedEntities;

	//structure to compactly store parent and contained entities
	EntityRelationshipsReference entityRelationships;

	//the random stream associated with this Entity
	RandomStream randomStream;

	//id of the string of the string ID used to address Entity given by container Entity
	//Each entity has an ID, which is a string.  Since each string is stored in the StringInternPool and referenced by a StringId, this is the entity's id stored by the stringId thus the idStringId.
	StringInternPool::StringID idStringId;

	//buffer to store read locks for deep locking entities
	//one per thread to save memory on Interpreter objects
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
	static std::vector<EntityReadReference> entityReadReferenceBuffer;

	//buffer to store write locks for deep locking entities
	//one per thread to save memory on Interpreter objects
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
#endif
	static std::vector<EntityWriteReference> entityWriteReferenceBuffer;

	//container for when there are no contained entities but need to iterate over them
	static std::vector<Entity *> emptyContainedEntities;
};
