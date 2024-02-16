#pragma once

//project headers:
#include "Amalgam.h"
#include "AssetManager.h"
#include "Entity.h"
#include "EntityWriteListener.h"
#include "HashMaps.h"
#include "PrintListener.h"

//system headers:
#include <string>
#include <vector>

//status from LoadEntity
class LoadEntityStatus
{
public:
	LoadEntityStatus();
	LoadEntityStatus(bool loaded, std::string message = std::string(), std::string version = std::string());
	void SetStatus(bool loaded_in, std::string message_in = std::string(), std::string version_in = std::string());

	bool loaded;
	std::string message;
	std::string version;
};

/*
 * This class constitutes the C++ backing for the C API, and is fully functional as a C++ API.
 * 
 * Amalgam functions through the use of "Entities" which will have a predetermined set of "labels".
 * Loading an .amlg file with the LoadEntity command will assign the entity to a given handle.
 * The majority of the methods provided here allow manipulation of data associated with a label within an entity.
 * Some labels will be loaded with functions which can be executed (refer to the instructions for the entity you loaded).
 */

class EntityExternalInterface
{
public:
	LoadEntityStatus LoadEntity(std::string &handle, std::string &path, bool persistent, bool load_contained_entities,
		std::string &write_log_filename, std::string &print_log_filename, std::string rand_seed = std::string(""));
	void StoreEntity(std::string &handle, std::string &path, bool update_persistence_location, bool store_contained_entities);
	void ExecuteEntity(std::string &handle, std::string &label);
	void DeleteEntity(std::string &handle);
	bool SetRandomSeed(std::string &handle, std::string &rand_seed);
	std::vector<std::string> GetEntities();

	void AppendToLabel(std::string &handle, std::string &label, double value);
	void AppendToLabel(std::string &handle, std::string &label, std::string &value);

	void SetLabel(std::string &handle, std::string &label, double value);
	void SetLabel(std::string &handle, std::string &label, std::string &value);

	double GetNumber(std::string &handle, std::string &label);
	std::string GetString(std::string &handle, std::string &label);
	std::string GetStringFromList(std::string &handle, std::string &label, size_t index);

	size_t GetNumberListLength(std::string &handle, std::string &label);
	void GetNumberList(std::string &handle, std::string &label, double *out_arr, size_t len);
	void GetNumberList(EvaluableNode *label_val, double *out_arr, size_t len);
	void SetNumberList(std::string &handle, std::string &label, double *arr, size_t len);
	void AppendNumberList(std::string &handle, std::string &label, double *arr, size_t len);
	
	size_t GetNumberMatrixWidth(std::string &handle, std::string &label);
	size_t GetNumberMatrixHeight(std::string &handle, std::string &label);
	void GetNumberMatrix(std::string &handle, std::string &label, double *out_arr, size_t w, size_t h);
	void SetNumberMatrix(std::string &handle, std::string &label, double *arr, size_t w, size_t h);

	size_t GetStringListLength(std::string &handle, std::string &label);
	void GetStringList(std::string &handle, std::string &label, std::string *out_arr, size_t len);
	void SetStringList(std::string &handle, std::string &label, char **arr, size_t len);

	bool SetJSONToLabel(std::string &handle, std::string &label, std::string_view json);
	std::string GetJSONFromLabel(std::string &handle, std::string &label);
	std::string ExecuteEntityJSON(std::string &handle, std::string &label, std::string_view json);

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

		~EntityListenerBundle()
		{
			if(entity != nullptr)
			{
				asset_manager.DestroyEntity(entity);
				delete entity;
			}

			if(printListener != nullptr)
				delete printListener;
			if(writeListeners.size() > 0 && writeListeners[0] != nullptr)
				delete writeListeners[0];
		}

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

		handleToBundle.erase(handle);
		delete bundle_handle->second;
	}

	//for concurrent reading and writing the interface management data below
#ifdef MULTITHREAD_INTERFACE
	Concurrency::ReadWriteMutex mutex;
#endif
	
	//map between entity name and the bundle of the entity and its listeners, etc.
	FastHashMap<std::string, EntityListenerBundle *> handleToBundle;
};
