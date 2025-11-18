#pragma once

////////////////////
// Defines hash set types in a generic way so they can be easily changed
// * * * Profile and choose whichever works fastest and with least memory  * * *
// Notes about the hashes:
// std::unordered is second best for maximizing debugability (due to IDE support) but not as easy as std::map, but is slow
// ska::flat_hash is best for performance, but eats a bit of memory
// ska::bytell_hash is good for compact memory and almost as fast as ska::flat_hash (should be used for things that need to be fairly fast but are not accessed as frequently, where minimizing memory is more important)

#ifdef USE_STL_HASH_MAPS

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using FastHashSet = std::unordered_set<T, H, E, A>;

template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using FastHashMap = std::unordered_map<K, V, H, E, A>;

template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using CompactHashSet = std::unordered_set<T, H, E, A>;

template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using CompactHashMap = std::unordered_map<K, V, H, E, A>;

#else

#include <array>
#include <cstddef>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

//fast hash maps
#include "skarupke_maps/bytell_hash_map.hpp"
#include "skarupke_maps/flat_hash_map.hpp"

template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using FastHashSet = ska::flat_hash_set<T, H, E, A>;

template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using FastHashMap = ska::flat_hash_map<K, V, H, E, A>;

template<typename T, typename H = std::hash<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using CompactHashSet = ska::bytell_hash_set<T, H, E, A>;

template<typename K, typename V, typename H = std::hash<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using CompactHashMap = ska::bytell_hash_map<K, V, H, E, A>;


template<
	typename K,
	typename V,
	typename H = std::hash<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<std::pair<const K, V>>,
	std::size_t ShardCount = 256   // must be a multiple of 16
>
class ConcurrentFastHashMap
{
public:

	ConcurrentFastHashMap() = default;
	inline ConcurrentFastHashMap(const H &hash,
								 const E &equal = E(),
								 const A &alloc = A())
		: hash(hash), equal(equal), alloc(alloc)
	{}

	bool empty() const
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lock(shards[i].mtx);
			if(!shards[i].map.empty()) return false;
		}
		return true;
	}

	std::size_t size() const
	{
		std::size_t total = 0;
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lock(shards[i].mtx);
			total += shards[i].map.size();
		}
		return total;
	}

	void clear()
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lock(shards[i].mtx);
			shards[i].map.clear();
		}
	}

	std::pair<typename ska::flat_hash_map<K, V, H, E, A>::iterator, bool>
		insert(const std::pair<K, V> &kv)
	{
		std::size_t idx = ShardIndex(kv.first);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map.insert(kv);
	}

	std::pair<typename ska::flat_hash_map<K, V, H, E, A>::iterator, bool>
		insert(std::pair<K, V> &&kv)
	{
		std::size_t idx = ShardIndex(kv.first);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map.insert(std::move(kv));
	}

	template<class... Args>
	std::pair<typename ska::flat_hash_map<K, V, H, E, A>::iterator, bool>
		emplace(Args&&... args)
	{
		using Pair = std::pair<K, V>;
		Pair tmp(std::forward<Args>(args)...);
		std::size_t idx = ShardIndex(tmp.first);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map.emplace(std::forward<Args>(args)...);
	}

	template<class... Args>
	std::pair<typename ska::flat_hash_map<K, V, H, E, A>::iterator, bool>
		try_emplace(const K &key, Args&&... args)
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map.try_emplace(key, std::forward<Args>(args)...);
	}

	std::size_t erase(const K &key)
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map.erase(key);
	}

	class const_iterator
	{
		using InnerIter = typename ska::flat_hash_map<K, V, H, E, A>::const_iterator;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = std::pair<const K, V>;
		using difference_type = std::ptrdiff_t;
		using pointer = typename std::iterator_traits<InnerIter>::pointer;
		using reference = typename std::iterator_traits<InnerIter>::reference;

		const_iterator() = default;

		const_iterator(const ConcurrentFastHashMap *parent,
					   std::size_t shardIdx,
					   InnerIter inner)
			: parent(parent), shardIdx(shardIdx), inner(inner)
		{}

		reference operator*()  const
		{
			return *inner;
		}

		pointer operator->() const
		{
			return &(*inner);
		}

		const_iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		const_iterator operator++(int)
		{
			const_iterator tmp = *this;
			++(*this);
			return tmp;
		}

		friend bool operator==(const const_iterator &a,
							   const const_iterator &b)
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}
		friend bool operator!=(const const_iterator &a,
							   const const_iterator &b)
		{
			return !(a == b);
		}

	private:
		//move forward until land on a non‑empty shard or reach end()
		void advance_until_valid()
		{
			while(parent && shardIdx < ShardCount)
			{
				std::lock_guard<std::mutex> lock(parent->shards[shardIdx].mtx);
				//check if still inside a valid element
				if(inner != parent->shards[shardIdx].map.end())
					return;
				++shardIdx;
				if(shardIdx < ShardCount)
					inner = parent->shards[shardIdx].map.begin();
			}

			parent = nullptr;
			shardIdx = ShardCount;
		}

		const ConcurrentFastHashMap *parent = nullptr;
		std::size_t shardIdx = ShardCount;
		InnerIter inner;
	};

	class iterator
	{
		using InnerIter = typename ska::flat_hash_map<K, V, H, E, A>::iterator;
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = std::pair<const K, V>;
		using difference_type = std::ptrdiff_t;
		using pointer = typename std::iterator_traits<InnerIter>::pointer;
		using reference = typename std::iterator_traits<InnerIter>::reference;

		iterator() = default;

		iterator(ConcurrentFastHashMap *parent,
				 std::size_t shardIdx,
				 InnerIter inner)
			: parent(parent), shardIdx(shardIdx), inner(inner)
		{}

		reference operator*()  const
		{
			return *inner;
		}

		pointer operator->() const
		{
			return &(*inner);
		}

		iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		iterator operator++(int)
		{
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		friend bool operator==(const iterator &a,
							   const iterator &b)
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}
		friend bool operator!=(const iterator &a,
							   const iterator &b)
		{
			return !(a == b);
		}

	private:
		//same logic as const_iterator but works on mutable map.
		void advance_until_valid()
		{
			while(parent && shardIdx < ShardCount)
			{
				std::lock_guard<std::mutex> lock(parent->shards[shardIdx].mtx);
				if(inner != parent->shards[shardIdx].map.end())
					return;

				++shardIdx;
				if(shardIdx < ShardCount)
					inner = parent->shards[shardIdx].map.begin();
			}
			parent = nullptr;
			shardIdx = ShardCount;
		}

		ConcurrentFastHashMap *parent = nullptr;
		std::size_t shardIdx = ShardCount;
		InnerIter inner;
	};

	iterator find(const K &key)
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		auto it = shards[idx].map.find(key);
		return (it == shards[idx].map.end())
			? end()
			: iterator(this, idx, it);
	}

	const_iterator find(const K &key) const
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		auto it = shards[idx].map.find(key);
		return (it == shards[idx].map.end())
			? end()
			: const_iterator(this, idx, it);
	}

	V &operator[](const K &key)
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map[key];
	}

	V &operator[](K &&key)
	{
		std::size_t idx = ShardIndex(key);
		std::lock_guard<std::mutex> lock(shards[idx].mtx);
		return shards[idx].map[std::forward<K_>(key)];
	}

	//obtains a scoped lock for the shard that will store key
	std::unique_lock<std::mutex> LockForKey(const K &key)
	{
		std::size_t idx = ShardIndex(key);
		return std::unique_lock<std::mutex>(shards[idx].mtx);
	}

	//const version of LockForKey
	std::unique_lock<std::mutex> LockForKey(const K &key) const
	{
		std::size_t idx = ShardIndex(key);
		return std::unique_lock<std::mutex>(shards[idx].mtx);
	}

	iterator begin()
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lock(shards[i].mtx);
			if(!shards[i].map.empty())
				return iterator(this, i, shards[i].map.begin());
		}
		return end();
	}

	const_iterator begin() const
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lock(shards[i].mtx);
			if(!shards[i].map.empty())
				return const_iterator(this, i, shards[i].map.begin());
		}
		return end();
	}

	iterator end()
	{
		return iterator();
	}

	const_iterator end() const
	{
		return const_iterator();
	}

protected:

	//TODO 24709: fix bugs
	//TODO 24709: only compute hash once and feed into flat_hash_map
	inline std::size_t ShardIndex(const K &key) const
	{
		std::size_t fullHash = hash(key);
		return fullHash & (ShardCount - 1);
	}

private:

	struct Shard
	{
		mutable std::mutex mtx;
		ska::flat_hash_map<K, V, H, E, A> map;
	};

	H hash;
	E equal;
	A alloc;

	std::array<Shard, ShardCount> shards;
};

#endif

//implements a map via a vector, where entries are looked up sequentially for brute force
//useful for very small hash maps (generally less than 30-40 entries) and for hash maps
//where entries are only found once
//note that, like other fast maps, iterators may be invalidated when the map is altered
template<typename K, typename V, typename E = std::equal_to<K>>
class SmallMap : public std::vector<std::pair<K, V>>
{
public:

	using key_type = K;
	using mapped_type = V;
	using value_type = std::pair<K, V>;

	//returns an iterator to deviation values that matches the nominal key
	inline auto find(K key)
	{
		return std::find_if(
			std::begin(*this),
			std::end(*this),
			[key](auto i)
			{	return E{}(i.first, key);	}
		);
	}

	//implement the map version of emplace, but allow for use of default constructor for the value
	template<class... Args>
	inline auto emplace(K key, Args&&... args)
	{
		if constexpr(sizeof...(Args) == 0)
			return this->emplace_back(key, V{});
		else
			return this->emplace_back(key, std::forward<Args>(args)...);
	}
};
