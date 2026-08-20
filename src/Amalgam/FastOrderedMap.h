#pragma once
//project headers
#include "VectorMap.h"

//system headers
#include <iterator>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <vector>

template<
	typename K, typename V,
	typename Hash = std::hash<K>,
	typename KeyEq = std::equal_to<K>,
	typename FastHashMap = std::unordered_map<K, V, Hash, KeyEq>,
	typename FastHashSet = std::unordered_set<K, Hash, KeyEq>,
	typename VecMap = VectorMap<K, V, KeyEq>
>
class FastOrderedMap
{
public:
	// --- Type Definitions ---
	using value_type = std::pair<K, V>;
	using key_type = K;
	using mapped_type = V;

	using size_type = std::size_t;

	using iterator = typename VecMap::iterator;
	using const_iterator = typename VecMap::const_iterator;

	//special iterator for find/emplace to compare to end of vectorMap, primarily used by find()
	struct LookupResult
	{
		//constructor used by find()
		LookupResult(FastHashMap::iterator mIt, iterator vIt)
			: mapIterator(mIt), vectorIteratorAsFlag(vIt)
		{}

		//overload -> to allow find(k)->first/second
		auto operator->() const -> decltype(auto)
		{
			return mapIterator.operator->();
		}

		//overload * to allow *find(k) if needed
		auto operator*() const -> decltype(auto)
		{
			return *mapIterator;
		}

		//overload == to compare against the vectorMap's iterator
		//this allows find(k) == end() to work.
		bool operator==(const iterator &other) const
		{
			return vectorIteratorAsFlag == other;
		}

		//overload == to compare against the vectorMap's iterator
		//this allows find(k) != end() to work.
		bool operator!=(const iterator &other) const
		{
			return vectorIteratorAsFlag != other;
		}

		iterator vectorIteratorAsFlag;
		FastHashMap::iterator mapIterator;
	};

	using hash = Hash;
	using key_equivalent = KeyEq;
	
	//--- Construction & Lifecycle ---
	inline FastOrderedMap() = default;
	inline FastOrderedMap(const FastOrderedMap &other) = default;
	inline FastOrderedMap(FastOrderedMap &&other) noexcept = default;

	inline FastOrderedMap(size_t initial_capacity)
	{
		vectorMap.reserve(initial_capacity);
		fastHashMap.reserve(initial_capacity);
	}

	inline FastOrderedMap &operator=(const FastOrderedMap &other)
	{
		if(this != &other)
		{
			vectorMap = other.vectorMap;
			fastHashMap = other.fastHashMap;
		}
		return *this;
	}

	inline FastOrderedMap &operator=(FastOrderedMap &&other) noexcept
	{
		if(this != &other)
		{
			vectorMap = std::move(other.vectorMap);
			fastHashMap = std::move(other.fastHashMap);
		}
		return *this;
	}

	inline ~FastOrderedMap() = default;

	//--- Iterators ---

	inline iterator begin() noexcept
	{
		return vectorMap.begin();
	}

	inline const_iterator cbegin() const noexcept
	{
		return vectorMap.cbegin();
	}

	inline iterator end() noexcept
	{
		return vectorMap.end();
	}

	inline const_iterator cend() const noexcept
	{
		return vectorMap.cend();
	}

	//--- Capacity & Size ---

	inline bool empty() const noexcept
	{
		return vectorMap.empty();
	}

	inline size_type size() const noexcept
	{
		return vectorMap.size();
	}

	inline size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max();
	}

	//--- Modifiers ---

	inline void clear()
	{
		vectorMap.clear();
		fastHashMap.clear();
	}

	template <typename... Args>
	inline auto emplace(Args&&... args) -> decltype(auto)
	{
		auto result = fastHashMap.emplace(std::forward<Args>(args)...);
		if(result.second)
			vectorMap.emplace(result.first->first, result.first->second);

		return result;
	}

	inline auto insert(const value_type &value) -> std::pair<iterator, bool>
	{
		auto result = fastHashMap.insert(value);
		if(result.second)
			vectorMap.emplace(result.first->first, result.first->second);

		return result;
	}

	inline auto insert_or_assign(const value_type &value) -> std::pair<iterator, bool>
	{
		auto result = fastHashMap.insert_or_assign(value.first, value.second);
		if(result.second)
			vectorMap.emplace(result.first->first, result.first->second);

		return result;
	}

	template <typename... Args>
	inline auto emplace_hint(size_type hint, Args&&... args) -> decltype(auto)
	{
		auto result = fastHashMap.emplace_hint(hint, std::forward<Args>(args)...);
		if(result.second)
			vectorMap.emplace(result.first->first, result.first->second);

		return result;
	}

	template <typename... Args>
	inline auto try_emplace(const key_type &key, Args&&... args) -> std::pair<iterator, bool>
	{
		auto result = fastHashMap.try_emplace(key, std::forward<Args>(args)...);
		if(result.second)
			vectorMap.emplace(result.first->first, result.first->second);

		return result;
	}

	iterator erase(const key_type &key)
	{
		auto it = fastHashMap.find(key);
		if(it != fastHashMap.end())
		{
			auto v_it = std::find(vectorMap.begin(), vectorMap.end(), std::make_pair(key, it->second));
			if(v_it != vectorMap.end())
				vectorMap.erase(v_it);

			fastHashMap.erase(it);
		}

		return vectorMap.end();
	}

	iterator erase(const LookupResult &iter)
	{
		//if invalid iterator, just return
		if(iter == end())
			return vectorMap.end();

		auto it = fastHashMap.find(iter.mapIterator->first);
		if(it != fastHashMap.end())
		{
			auto v_it = std::find(vectorMap.begin(), vectorMap.end(), std::make_pair(it->first, it->second));
			if(v_it != vectorMap.end())
				vectorMap.erase(v_it);

			fastHashMap.erase(it);
		}

		return vectorMap.end();
	}

	void EraseBatch(const std::vector<key_type> &keys_to_remove)
	{
		FastHashSet to_remove(keys_to_remove.begin(), keys_to_remove.end());

		size_t write_index = 0;
		for(size_t i = 0; i < vectorMap.size(); ++i)
		{
			if(to_remove.find(vectorMap[i].first) == to_remove.end())
			{
				vectorMap[write_index] = vectorMap[i];
				write_index++;
			}
		}
		vectorMap.resize(write_index);

		for(const auto &key : keys_to_remove)
			fastHashMap.erase(key);
	}

	inline void swap(FastOrderedMap &other) noexcept
	{
		std::swap(vectorMap, other.vectorMap);
		std::swap(fastHashMap, other.fastHashMap);
	}

	//--- Lookup ---

	inline mapped_type &at(const key_type &key)
	{
		return fastHashMap.at(key);
	}

	mapped_type &operator[](const key_type &key)
	{
		auto it = fastHashMap.find(key);
		if(it != fastHashMap.end())
			return it->second;

		auto result = fastHashMap.insert({ key, mapped_type() });
		if(result.second)
			vectorMap.emplace(key, result.first->second);

		return result.first->second;
	}

	inline size_type count(const key_type &key) const
	{
		return fastHashMap.count(key);
	}

	//note that this returns a different kind of iterator!  be careful
	inline LookupResult find(const key_type &key)
	{
		auto it = fastHashMap.find(key);
		if(it != fastHashMap.end())
		{
			//on success, return the map iterator and a non-end vector iterator
			return { it, vectorMap.begin() };
		}
		else
		{
			// On failure, we return the end of both maps
			return { fastHashMap.end(), vectorMap.end() };
		}
	}

	inline bool contains(const key_type &key) const
	{
		return fastHashMap.contains(key);
	}

	inline void reserve(size_type n)
	{
		vectorMap.reserve(n);
		fastHashMap.reserve(n);
	}

private:
	VecMap vectorMap;
	FastHashMap fastHashMap;
};
