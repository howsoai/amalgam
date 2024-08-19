#pragma once

//project headers:
#include "Entity.h"

/*
 * This class constitutes the C++ backing for the C API, and is fully functional as a C++ API.
 *
 * Amalgam functions through the use of "Entities" which will have a predetermined set of "labels".
 * Loading an .amlg file with the LoadEntity command will assign the entity to a given handle.
 * The majority of the methods provided here allow manipulation of data associated with a label within an entity.
 * Some labels will be loaded with functions which can be executed (refer to the instructions for the entity you loaded).
 */

class EntityExecution
{
public:
	//executes the entity on label_name (if empty string, then evaluates root node)
	// returns the result from the execution
	// if on_self is true, then it will be allowed to access private variables
	// if performance_constraints is not nullptr, then it will constrain performance and update performance_constraints
	// if enm_lock is specified, it should be a lock on this entity's evaluableNodeManager.memoryModificationMutex
	static EvaluableNodeReference ExecuteEntity(Entity &entity,
		StringInternPool::StringID label_sid,
		EvaluableNode *call_stack, bool on_self = false, Interpreter *calling_interpreter = nullptr,
		std::vector<EntityWriteListener *> *write_listeners = nullptr, PrintListener *print_listener = nullptr,
		PerformanceConstraints *performance_constraints = nullptr
	#ifdef MULTITHREAD_SUPPORT
		, Concurrency::ReadLock *enm_lock = nullptr
	#endif
	);
	
	//overload accepting a string for label name
	static inline EvaluableNodeReference ExecuteEntity(Entity &entity,
		std::string &label_name,
		EvaluableNode *call_stack, bool on_self = false, Interpreter *calling_interpreter = nullptr,
		std::vector<EntityWriteListener *> *write_listeners = nullptr, PrintListener *print_listener = nullptr,
		PerformanceConstraints *performance_constraints = nullptr
	#ifdef MULTITHREAD_SUPPORT
		, Concurrency::ReadLock *enm_lock = nullptr
	#endif
	)
	{
		StringInternPool::StringID label_sid = string_intern_pool.GetIDFromString(label_name);
		return ExecuteEntity(entity,
			label_sid, call_stack, on_self, calling_interpreter,
			write_listeners, print_listener, performance_constraints
		#ifdef MULTITHREAD_SUPPORT
			, enm_lock
		#endif
			);
	}

	void ExecuteEntity(std::string &handle, std::string &label);

protected:
	//a class that manages the entity
	// when the bundle is destroyed, everything in it is also destroyed
	class EntityListenerBundle
	{
	public:
		EntityListenerBundle(Entity *ent, std::vector<EntityWriteListener *> wl, PrintListener *pl = nullptr)
		{
			entity = ent;
			writeListeners = wl;
			printListener = pl;
		}

		~EntityListenerBundle();

		//Wraps around Entity::SetValueAtLabel but accepts a string for label name
		bool SetEntityValueAtLabel(std::string &label_name, EvaluableNodeReference new_value);

		//the type of mutex is dependent on whether individual entities can be accessed concurrently
	#ifdef MULTITHREAD_INTERFACE
	#ifdef MULTITHREAD_ENTITY_CALL_MUTEX
		Concurrency::SingleMutex mutex;
	#else
		Concurrency::ReadWriteMutex mutex;
	#endif
	#endif

		Entity *entity;
		std::vector<EntityWriteListener *> writeListeners;
		PrintListener *printListener;
	};

	class EntityListenerBundleReadReference
	{
	public:
		EntityListenerBundleReadReference(EntityListenerBundle *entity_listener_bundle)
		{
			entityListenerBundle = entity_listener_bundle;

		#ifdef MULTITHREAD_INTERFACE
			if(entityListenerBundle != nullptr)
			{
			#ifdef MULTITHREAD_ENTITY_CALL_MUTEX
				lock = Concurrency::SingleLock(entityListenerBundle->mutex);
			#else
				readLock = Concurrency::ReadLock(entityListenerBundle->mutex);
			#endif
			}
		#endif
		}

		//allow to use as an EntityListenerBundle *
		constexpr operator EntityListenerBundle *()
		{	return entityListenerBundle;	}

		//allow to use as an EntityListenerBundle *
		constexpr EntityListenerBundle *operator->()
		{	return entityListenerBundle;	}

		EntityListenerBundle *entityListenerBundle;

		//the type of mutex is dependent on whether individual entities can be accessed concurrently
	#ifdef MULTITHREAD_INTERFACE
	#ifdef MULTITHREAD_ENTITY_CALL_MUTEX
		Concurrency::SingleLock lock;
	#else
		Concurrency::ReadLock readLock;
	#endif
	#endif
	};

	class EntityListenerBundleWriteReference
	{
	public:
		EntityListenerBundleWriteReference(EntityListenerBundle *entity_listener_bundle)
		{
			entityListenerBundle = entity_listener_bundle;

		#ifdef MULTITHREAD_INTERFACE
			if(entityListenerBundle != nullptr)
			{
			#ifdef MULTITHREAD_ENTITY_CALL_MUTEX
				lock = Concurrency::SingleLock(entityListenerBundle->mutex);
			#else
				writeLock = Concurrency::WriteLock(entityListenerBundle->mutex);
			#endif
			}
		#endif
		}

		//allow to use as an EntityListenerBundle *
		constexpr operator EntityListenerBundle *()
		{	return entityListenerBundle;	}

		//allow to use as an EntityListenerBundle *
		constexpr EntityListenerBundle *operator->()
		{	return entityListenerBundle;	}

		EntityListenerBundle *entityListenerBundle;

		//the type of mutex is dependent on whether individual entities can be accessed concurrently
	#ifdef MULTITHREAD_INTERFACE
	#ifdef MULTITHREAD_ENTITY_CALL_MUTEX
		Concurrency::SingleLock lock;
	#else
		Concurrency::WriteLock writeLock;
	#endif
	#endif
	};

	//looks up the bundle and returns it, will return nullptr if not found
	inline EntityListenerBundleReadReference FindEntityBundle(std::string &handle)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::ReadLock read_lock(mutex);
	#endif

		auto bundle_handle = handleToBundle.find(handle);
		if(bundle_handle == end(handleToBundle) || bundle_handle->second == nullptr)
			return nullptr;

		return EntityListenerBundleReadReference(bundle_handle->second);
	}

	//adds a new bundle under the name handle
	// will delete any if it already exists
	inline void AddEntityBundle(std::string &handle, EntityListenerBundle *bundle)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock write_lock(mutex);
	#endif

		const auto &[bundle_handle, bundle_inserted] = handleToBundle.emplace(handle, bundle);
		if(!bundle_inserted)
		{
			//erase the previous
			if(bundle_handle->second != nullptr)
				delete bundle_handle->second;

			//overwrite
			bundle_handle->second = bundle;
		}
	}

	//erases the handle and returns the bundle reference.  Returns nullptr if not found.
	inline void EraseEntityBundle(std::string &handle)
	{
	#ifdef MULTITHREAD_INTERFACE
		Concurrency::WriteLock write_lock(mutex);
	#endif

		auto bundle_handle = handleToBundle.find(handle);
		if(bundle_handle == end(handleToBundle) || bundle_handle->second == nullptr)
			return;

		//copy out of the iterator, since changing the container may invalidate the iterator
		EntityListenerBundle *elb = bundle_handle->second;

		//if it's being executed, can't delete
		if(elb->entity->IsEntityCurrentlyBeingExecuted())
			return;

		handleToBundle.erase(handle);

	#ifdef MULTITHREAD_INTERFACE
		//obtain a write lock and release it -- just make sure nothing else has the entity locked
		EntityWriteReference ewr(elb->entity);
		ewr = EntityWriteReference();
	#endif

		delete elb;
	}

	//for concurrent reading and writing the interface management data below
#ifdef MULTITHREAD_INTERFACE
	Concurrency::ReadWriteMutex mutex;
#endif

	//map between entity name and the bundle of the entity and its listeners, etc.
	FastHashMap<std::string, EntityListenerBundle *> handleToBundle;
};
