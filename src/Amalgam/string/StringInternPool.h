#pragma once

//project headers:
#include "Concurrency.h"
#include "HashMaps.h"
#include "PlatformSpecific.h"
#include "StringManipulation.h"

//system headers:
#include <queue>
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
	std::atomic<int64_t> refCount;
#else
	int64_t refCount;
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
	//TODO 20465: put this back in or remove entirely:
	/*
	class StringID
	{
	public:
		constexpr StringID()
			: stringId(nullptr)
		{	}

		inline StringID(const StringID &sid)
			: stringId(sid.stringId)
		{	}

		inline StringID(StringInternStringData *sisd)
			: stringId(sisd)
		{	}

		inline StringID &operator=(StringID sid)
		{
			string_intern_pool.ValidateStringIdExistance(sid);

			stringId = sid.stringId;
			return *this;
		}

		inline StringInternStringData *operator->()
		{
			return stringId;
		}

		inline StringInternStringData &operator*()
		{
			return *stringId;
		}

		inline StringID &operator =(StringInternStringData *sisd)
		{
			stringId = sisd;
			return *this;
		}

		inline bool operator==(StringID sid)
		{
			return stringId == sid.stringId;
		}

		inline bool operator!=(StringID sid)
		{
			return stringId != sid.stringId;
		}

		inline bool operator==(StringInternStringData *sisd)
		{
			return stringId == sisd;
		}

		inline bool operator!=(StringInternStringData *sisd)
		{
			return stringId != sisd;
		}

		size_t hash_value()
		{
			return std::hash<StringInternStringData *>{}(stringId);
		}

	private:
		StringInternStringData *stringId;
	};


	//indicates that it is not a string, like NaN or null
	static constexpr StringInternStringData *NOT_A_STRING_ID = nullptr;
	*/
	static constexpr StringID NOT_A_STRING_ID = nullptr;
	StringID emptyStringId;
	inline static const std::string EMPTY_STRING = std::string("");

	inline StringInternPool()
	{
		//create the empty string first
		auto [new_entry, inserted] = stringToID.emplace(std::make_pair("", std::make_unique<StringInternStringData>("")));
		emptyStringId = new_entry->second.get();
		InitializeStaticStrings();
	}

	//translates the id to a string, empty string if it does not exist
	//because a flat hash map is used as the storage container, it is possible that any allocation or deallocation
	//may invalidate the location, so a copy must be made to return the value
	inline const std::string GetStringFromID(StringID id)
	{
		if(id == NOT_A_STRING_ID)
			return EMPTY_STRING;

	#ifdef STRING_INTERN_POOL_VALIDATION
		ValidateStringIdExistance(id);
	#endif

		return id->string;
	}

	//translates the string to the corresponding ID, 0 is the empty string, maximum value of size_t means it does not exist
	inline StringID GetIDFromString(const std::string &str)
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::ReadLock lock(sharedMutex);
	#endif

		auto id_iter = stringToID.find(str);
		if(id_iter == end(stringToID))
			return NOT_A_STRING_ID;	//the string was never entered in and don't want to cause more errors

		return id_iter->second.get();
	}

	//makes a new reference to the string specified, returning the ID
	inline StringID CreateStringReference(const std::string &str)
	{
		if(str == "")
			return emptyStringId;

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::WriteLock lock(sharedMutex);
	#endif

		//try to insert it as a new string
		auto [new_entry, inserted] = stringToID.emplace(std::make_pair(str, nullptr));
		if(inserted)
			new_entry->second = std::make_unique<StringInternStringData>(str);
		else
			new_entry->second->refCount++;

		return new_entry->second.get();
	}

	//makes a new reference to the string id specified, returning the id passed in
	inline StringID CreateStringReference(StringID id)
	{
		if(id != NOT_A_STRING_ID)
		{
		#ifdef STRING_INTERN_POOL_VALIDATION
			ValidateStringIdExistance(id);
		#endif
			id->refCount++;
		}
		return id;
	}

	//creates new references from the references container and function
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
				ValidateStringIdExistance(id);
			#endif
				id->refCount++;
			}
		}
	}

	//creates additional_reference_count new references from the references container and function
	// specialized for size_t indexed containers, where the index is desired
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
				ValidateStringIdExistance(id);
			#endif
				id->refCount += additional_reference_count;
			}
		}
	}

	//creates new references from the references container and function
	// specialized for size_t indexed containers, where the index is desired
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
				ValidateStringIdExistance(id);
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
		ValidateStringIdExistance(id);
	#endif

		int64_t refcount = id->refCount--;

		//if other references, then can't clear it; signed, so it won't wrap around
		if(refcount > 1)
			return;

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		//this thread is about to free the reference, but need to acquire a write lock
		// so, keep the reference alive by incrementing it *before* attempting the write lock
		id->refCount++;

		Concurrency::WriteLock write_lock(sharedMutex);

		//with the write lock, decrement reference count in case this string should stay active
		refcount = id->refCount--;

		//if other references, then can't clear it
		if(refcount > 1)
			return;
	#endif

		stringToID.erase(id->string);
	}


	//creates new references from the references container and function
	template<typename ReferencesContainer,
		typename GetStringIdFunction = StringID(StringID)>
	inline void DestroyStringReferences(ReferencesContainer &references_container,
		GetStringIdFunction get_string_id = [](auto sid) { return sid;  })
	{
	#if !defined(MULTITHREAD_SUPPORT) && !defined(MULTITHREAD_INTERFACE)
		for(auto r : references_container)
			DestroyStringReference(get_string_id(r));
	#else
		if(references_container.size() == 0)
			return;

		//as it goes through, if any id needs removal, will set this to true so that
		// removal can be done after reference count decreases are done
		bool ids_need_removal = false;

		for(auto r : references_container)
		{
			StringID id = get_string_id(r);
			if(id == NOT_A_STRING_ID || id == emptyStringId)
				continue;

		#ifdef STRING_INTERN_POOL_VALIDATION
			ValidateStringIdExistance(id);
		#endif

			int64_t refcount = id->refCount--;

			//if extra references, just return, but if it is 1, then it will try to clear
			if(refcount <= 1)
				ids_need_removal = true;
		}

		if(!ids_need_removal)
			return;

		//need to remove at least one reference, so put all counts back while wait for write lock
		for(auto r : references_container)
		{
			StringID id = get_string_id(r);
			if(id != NOT_A_STRING_ID && id != emptyStringId)
				id->refCount++;
		}

		Concurrency::WriteLock write_lock(sharedMutex);

		for(auto r : references_container)
		{
			StringID id = get_string_id(r);
			if(id == NOT_A_STRING_ID || id == emptyStringId)
				continue;

		#ifdef STRING_INTERN_POOL_VALIDATION
			ValidateStringIdExistance(id);
		#endif

			//remove any that are the last reference
			int64_t refcount = id->refCount--;

			if(id->refCount < 1)
			{
				stringToID.erase(id->string);
			}
		}

	#endif
	}

	//destroys 2 StringReferences
	inline void DestroyStringReferences(StringID sid_1, StringID sid_2)
	{
		std::array<StringID, 2> string_ids = { sid_1, sid_2 };
		DestroyStringReferences(string_ids);
	}

	//returns the number of strings that are still allocated
	//even when "empty" it will still return 2 since the NOT_A_STRING_ID and emptyStringId take up slots
	inline size_t GetNumStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::ReadLock lock(sharedMutex);
	#endif

		return stringToID.size();
	}

	//returns the number of strings that are still in use
	inline size_t GetNumDynamicStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::ReadLock lock(sharedMutex);
	#endif

		return stringToID.size() - staticStringIDToIndex.size();
	}

	//returns a vector of all the strings still in use.  Intended for debugging.
	inline std::vector<std::pair<std::string, int64_t>> GetDynamicStringsInUse()
	{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		Concurrency::ReadLock lock(sharedMutex);
	#endif

		std::vector<std::pair<std::string, int64_t>> in_use;
		for(auto &[str, sisd] : stringToID)
		{
			if(staticStringIDToIndex.find(sisd.get()) == end(staticStringIDToIndex))
				in_use.emplace_back(str, sisd->refCount);
		}

		return in_use;
	}

	//validates the string id, throwing an assert if it is not valid
	inline void ValidateStringIdExistance(StringID sid)
	{
		if(sid == NOT_A_STRING_ID)
			return;

		auto found = stringToID.find(sid->string);
		if(sid != found->second.get())
		{
			assert(false);
		}
	}

protected:

	//must be defined outside of this class and initialize all static strings
	void InitializeStaticStrings();

#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	Concurrency::ReadWriteMutex sharedMutex;
#endif

	//mapping from string to ID (index of idToRefCountAndString)
	FastHashMap<std::string, std::unique_ptr<StringInternStringData>> stringToID;

public:
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
		string_intern_pool.ValidateStringIdExistance(sid);
	#endif
		id = string_intern_pool.CreateStringReference(sid);
	}

	inline StringRef(std::string &str)
	{
		id = string_intern_pool.CreateStringReference(str);
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistance(id);
	#endif
	}

	//copy constructor
	inline StringRef(const StringRef &sir)
	{
		id = string_intern_pool.CreateStringReference(sir.id);
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistance(id);
	#endif
	}

	//move constructor
	inline StringRef(StringRef &&sir)
	{
		id = sir.id;
	#ifdef STRING_INTERN_POOL_VALIDATION
		string_intern_pool.ValidateStringIdExistance(id);
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
	inline StringRef &operator =(StringRef &sir)
	{
		if(sir.id != id)
		{
			string_intern_pool.DestroyStringReference(id);
			id = string_intern_pool.CreateStringReference(sir.id);
		#ifdef STRING_INTERN_POOL_VALIDATION
			string_intern_pool.ValidateStringIdExistance(id);
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
		string_intern_pool.ValidateStringIdExistance(sid);
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
		string_intern_pool.ValidateStringIdExistance(sid);
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
