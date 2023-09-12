//project headers:
#include "StringInternPool.h"

StringInternPool string_intern_pool;

const std::string &StringInternPool::GetStringFromID(StringInternPool::StringID id)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(sharedMutex);
#endif

	return idToStringAndRefCount[id].first;
}

StringInternPool::StringID StringInternPool::GetIDFromString(const std::string &str)
{
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadLock lock(sharedMutex);
#endif

	auto id_iter = stringToID.find(str);
	if(id_iter == end(stringToID))
		return NOT_A_STRING_ID;	//the string was never entered in and don't want to cause more errors

	return id_iter->second;
}

StringInternPool::StringID StringInternPool::CreateStringReference(const std::string &str)
{
	if(str.size() == 0)
		return EMPTY_STRING_ID;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::WriteLock lock(sharedMutex);
#endif

	//try to insert it as a new string
	auto [inserted_id, inserted] = stringToID.insert(std::make_pair(str, 0));
	if(inserted)
	{
		StringInternPool::StringID id;
		//new string, see if any ids are ready for reuse
		if(unusedIDs.size() > 0)
		{
			//reuse existing, so overwrite it
			id = unusedIDs.top();
			unusedIDs.pop();
			idToStringAndRefCount[id] = std::make_pair(str, 1);
		}
		else //need a new one
		{
			id = idToStringAndRefCount.size();
			idToStringAndRefCount.emplace_back(std::make_pair(str, 1));
		}
		
		//store the id along with the string
		inserted_id->second = id;

		return id;
	}

	//found, so count the reference if applicable
	StringInternPool::StringID id = inserted_id->second;
	if(!IsStringIDStatic(id))
		idToStringAndRefCount[id].second++;
	return id;
}

StringInternPool::StringID StringInternPool::CreateStringReference(StringInternPool::StringID id)
{
	if(IsStringIDStatic(id))
		return id;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//only need a ReadLock because the count is atomic
	Concurrency::ReadLock lock(sharedMutex);
#endif
	IncrementRefCount(id);

	return id;
}

void StringInternPool::DestroyStringReference(StringInternPool::StringID id)
{
	if(IsStringIDStatic(id))
		return;

	//get the reference count before decrement
#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//make sure have a readlock first so that the idToStringAndRefCount vector heap location doesn't change
	Concurrency::ReadLock lock(sharedMutex);
#endif

	int64_t refcount = DecrementRefCount(id);

	//if other references, then can't clear it; signed, so it won't wrap around
	if(refcount > 1)
		return;

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	//this thread is about to free the reference, but need to acquire a write lock
	// so, keep the reference alive by incrementing it *before* attempting the write lock
	IncrementRefCount(id);

	//grab a write lock
	lock.unlock();
	Concurrency::WriteLock write_lock(sharedMutex);

	//with the write lock, decrement reference count in case this string should stay active
	refcount = DecrementRefCount(id);

	//if other references, then can't clear it
	if(refcount > 1)
		return;
#endif

	RemoveId(id);
}
