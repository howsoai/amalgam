#pragma once

//project headers:
#include "Concurrency.h"
#include "HashMaps.h"
#include "PlatformSpecific.h"
#include "StringManipulation.h"

//system headers:
#include <memory>
#include <string>
#include <vector>

//if STRING_INTERN_POOL_VALIDATION is defined, it will validate
//every string reference, at the cost of performance

class StringInternStringData
{
public:
	inline StringInternStringData()
		: refCount(0), string()
	{	}

	inline StringInternStringData(const std::string &string)
		: refCount(1), string(string)
	{	}

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	std::atomic<size_t> refCount;
#else
	size_t refCount;
#endif
	std::string string;
};

class StringInternPool;
extern StringInternPool string_intern_pool;

//manages all strings so they can be referred and compared easily by integers, across threads
//depends on a method defined outside of this class, StringInternPool::InitializeStaticStrings()
// to set up all internal strings; see the function's declaration for details
//additionally StringInternPool string_intern_pool; should be defined elsewhere
// if a global intern pool is desired
class StringInternPool
{
public:
	using StringID = StringInternStringData *;

	inline StringInternPool()
	{
		//create the empty string first
		auto inserted = stringToID.emplace("", std::make_unique<StringInternStringData>(""));
		emptyStringId = inserted.first->second.get();
		InitializeStaticStrings();
	}

	//translates the id to a string, empty string if it does not exist
	//note that the reference is only valid as long as the string id is valid; if a string is needed
	//after a reference is destroyed, the caller must make a copy first
	inline const std::string &GetStringFromID(StringID id)
	{
		if(id == NOT_A_STRING_ID)
			return EMPTY_STRING;

	#ifdef STRING_INTERN_POOL_VALIDATION
		ValidateStringIdExistence(id);
	#endif

		return id->string;
	}

	//translates the string to the corresponding ID, 0 is the empty string, maximum value of size_t means it does not exist
	inline StringID GetIDFromString(const std::string &str)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif

		auto id_iter = stringToID.find(str);
		if(id_iter == end(stringToID))
			return NOT_A_STRING_ID;	//the string was never entered in and don't want to cause more errors

		StringID id = id_iter->second.get();
	#ifdef STRING_INTERN_POOL_VALIDATION
		ValidateStringIdExistenceUnderLock(id);
	#endif
		return id;
	}

	//makes a new reference to the string specified, returning the ID
	inline StringID CreateStringReference(const std::string &str)
	{
		if(str.size() == 0)
			return emptyStringId;

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif

		//try to insert it as a new string
		auto inserted = stringToID.emplace(str, nullptr);
		if(inserted.second)
			inserted.first->second = std::make_unique<StringInternStringData>(str);
		else
			inserted.first->second->refCount++;

		StringID id = inserted.first->second.get();
	#ifdef STRING_INTERN_POOL_VALIDATION
		ValidateStringIdExistenceUnderLock(id);
	#endif
		return id;
	}

	//makes a new reference to the string id specified, returning the id passed in
	//note that this assumes that the caller guarantees that the id will exist for the duration of this call
	inline StringID CreateStringReference(StringID id)
	{
		if(id != NOT_A_STRING_ID)
		{
		#ifdef STRING_INTERN_POOL_VALIDATION
			ValidateStringIdExistence(id);
		#endif
			id->refCount++;
		}
		return id;
	}

	//creates new references from the references container and function
	//note that this assumes that the caller guarantees that the ids will exist for the duration of this call
	template<typename ReferencesContainer,
		typename GetStringIdFunction = StringID(StringID)>
	inline void CreateStringReferences(ReferencesContainer &references_container,
		GetStringIdFunction get_string_id = [](auto sid) { return sid;  })
	{
		for(auto r : references_container)
		{
			StringID id = get_string_id(r);
			if(id != NOT_A_STRING_ID)
			{
			#ifdef STRING_INTERN_POOL_VALIDATION
				ValidateStringIdExistence(id);
			#endif
				id->refCount++;
			}
		}
	}

	//creates additional_reference_count new references from the references container and function
	// specialized for size_t indexed containers, where the index is desired
	//note that this assumes that the caller guarantees that the ids will exist for the duration of this call
	template<typename ReferencesContainer,
		typename GetStringIdFunction = StringID(StringID)>
	inline void CreateMultipleStringReferences(ReferencesContainer &references_container,
		size_t additional_reference_count,
		GetStringIdFunction get_string_id = [](auto sid) { return sid;  })
	{
		for(auto r : references_container)
		{
			StringID id = get_string_id(r);
			if(id != NOT_A_STRING_ID)
			{
			#ifdef STRING_INTERN_POOL_VALIDATION
				ValidateStringIdExistence(id);
			#endif
				id->refCount += additional_reference_count;
			}
		}
	}

	//creates new references from the references container and function
	// specialized for size_t indexed containers, where the index is desired
	//note that this assumes that the caller guarantees that the ids will exist for the duration of this call
	template<typename ReferencesContainer,
		typename GetStringIdFunction = StringID(StringID)>
	inline void CreateStringReferencesByIndex(ReferencesContainer &references_container,
		GetStringIdFunction get_string_id = [](auto sid) { return sid;  })
	{
		for(size_t i = 0; i < references_container.size(); i++)
		{
			StringID id = get_string_id(references_container[i], i);
			if(id != NOT_A_STRING_ID)
			{
			#ifdef STRING_INTERN_POOL_VALIDATION
				ValidateStringIdExistence(id);
			#endif
				id->refCount++;
			}
		}
	}

	//removes a reference to the string specified by the ID
	inline void DestroyStringReference(StringID id)
	{
		if(id == NOT_A_STRING_ID || id == emptyStringId)
			return;

	#ifdef STRING_INTERN_POOL_VALIDATION
		ValidateStringIdExistence(id);
	#endif

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		//refCount must be decremented in an atomic fashion, but if down to the last reference,
		//then don't want to decrement outside of a lock.  This is because if this thread decremented
		//refCount, then another thread could acquire the lock, create a reference, then acquire the
		// lock and delete a reference, and now a double delete will occur.  
		while(true)
		{
			size_t ref_count = id->refCount.load();
			if(ref_count <= 1)
				break;

			//if can decrement, return
			if(std::atomic_compare_exchange_weak(&id->refCount, &ref_count, ref_count - 1))
				return;
		}

		Concurrency::Lock lock(mutex);
	#endif

		//remove any that aren't the last reference
		size_t ref_count = id->refCount--;
		if(ref_count > 1)
			return;
		
		stringToID.erase(id->string);
	}

	//creates new references from the references container and function
	template<typename ReferencesContainer,
		typename GetStringIdFunction = StringID(StringID)>
	inline void DestroyStringReferences(ReferencesContainer &references_container,
		GetStringIdFunction get_string_id = [](auto sid) { return sid;  })
	{
		for(auto r : references_container)
			DestroyStringReference(get_string_id(r));
	}

	//returns the number of strings that are still allocated
	//even when "empty" it will still return 2 since the NOT_A_STRING_ID and emptyStringId take up slots
	inline size_t GetNumStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif

		return stringToID.size();
	}

	//returns the number of strings that are still in use
	inline size_t GetNumDynamicStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif

		return stringToID.size() - staticStringIDToIndex.size();
	}

	//returns a vector of all the strings still in use.  Intended for debugging.
	inline std::vector<std::pair<std::string, size_t>> GetDynamicStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif

		std::vector<std::pair<std::string, size_t>> in_use;
		for(auto &[str, sisd] : stringToID)
		{
			StringID sid(sisd.get());
			if(staticStringIDToIndex.find(sid) == end(staticStringIDToIndex))
				in_use.emplace_back(str, sisd->refCount);
		}

		return in_use;
	}

	//validates the string id, throwing an assert if it is not valid
	inline void ValidateStringIdExistence(StringID sid)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::Lock lock(mutex);
	#endif
		ValidateStringIdExistenceUnderLock(sid);
	}

protected:

	//validates the string id, throwing an assert if it is not valid
	//requires being under a lock
	inline void ValidateStringIdExistenceUnderLock(StringID sid)
	{
		if(sid == NOT_A_STRING_ID)
			return;

		auto found = stringToID.find(sid->string);
		if(found == end(stringToID))
		{
			assert(false);
			return;
		}

		StringID found_sid = found->second.get();
		if(sid != found_sid)
		{
			assert(false);
		}
	}

	//must be defined outside of this class and initialize all static strings
	void InitializeStaticStrings();

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::SingleMutex mutex;
#endif

	//mapping from string to ID (index of idToRefCountAndString)
	FastHashMap<std::string, std::unique_ptr<StringInternStringData>> stringToID;

public:
	//indicates that it is not a string, like NaN or null
	static constexpr StringID NOT_A_STRING_ID = nullptr;
	StringID emptyStringId;
	inline static const std::string EMPTY_STRING = std::string("");

	//data structures for static strings
	std::vector<StringInternPool::StringID> staticStringsIndexToStringID;
	FastHashMap<StringInternPool::StringID, size_t> staticStringIDToIndex;
};

//A reference to a string
//maintains reference counts and will clear upon destruction
class StringRef
{
public:
	inline StringRef()
		: id(StringInternPool::NOT_A_STRING_ID)
	{	}

	inline StringRef(StringInternPool::StringID sid)
	{
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(sid);
	#endif
		id = string_intern_pool.CreateStringReference(sid);
	}

	inline StringRef(std::string &str)
	{
		id = string_intern_pool.CreateStringReference(str);
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(id);
	#endif
	}

	//copy constructor
	inline StringRef(const StringRef &sir)
	{
		id = string_intern_pool.CreateStringReference(sir.id);
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(id);
	#endif
	}

	//move constructor
	inline StringRef(StringRef &&sir)
	{
		id = sir.id;
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(id);
	#endif
		sir.id = nullptr;
	}

	inline ~StringRef()
	{
		string_intern_pool.DestroyStringReference(id);
	}

	inline void Clear()
	{
		string_intern_pool.DestroyStringReference(id);
		id = StringInternPool::NOT_A_STRING_ID;
	}

	//easy-to-read way of creating an empty string
	inline static StringRef EmptyString()
	{	return StringRef();	}

	//assign another string reference
	inline StringRef &operator =(const StringRef &sir)
	{
		if(id != sir.id)
		{
			string_intern_pool.DestroyStringReference(id);
			id = string_intern_pool.CreateStringReference(sir.id);
		#ifdef STRING_INTERN_POOL_VALIDATION
			string_intern_pool.ValidateStringIdExistence(id);
		#endif
		}
		return *this;
	}

	//allow being able to use as a string
	inline operator const std::string()
	{
		return string_intern_pool.GetStringFromID(id);
	}

	//allow being able to use as a string id
	inline operator StringInternPool::StringID()
	{
		return id;
	}

	//call this to set the id and create a reference
	inline void SetIDAndCreateReference(StringInternPool::StringID sid)
	{
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(sid);
	#endif

		//if changing id, need to delete previous
		if(id != sid)
			string_intern_pool.DestroyStringReference(id);

		if(id != sid)
		{
			id = sid;
			string_intern_pool.CreateStringReference(id);
		}
	}

	//only call this when the sid already has a reference and this is being used to manage it
	inline void SetIDWithReferenceHandoff(StringInternPool::StringID sid)
	{
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistence(sid);
	#endif
		
		//if the ids are different, then need to delete old
		//if the ids are the same, then have a duplicate reference, so need to delete one
		//so delete a reference either way
		string_intern_pool.DestroyStringReference(id);

		id = sid;
	}

private:

	StringInternPool::StringID id;
};

inline int StringNaturalCompare(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	return StringManipulation::StringNaturalCompare(string_intern_pool.GetStringFromID(a), string_intern_pool.GetStringFromID(b));
}

inline bool StringIDNaturalCompareSort(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	int comp = StringManipulation::StringNaturalCompare(string_intern_pool.GetStringFromID(a), string_intern_pool.GetStringFromID(b));
	return comp < 0;
}

inline bool StringIDNaturalCompareSortReverse(const StringInternPool::StringID a, const StringInternPool::StringID b)
{
	int comp = StringNaturalCompare(a, b);
	return comp > 0;
}
