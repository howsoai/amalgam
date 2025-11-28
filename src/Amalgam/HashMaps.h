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

//A hash map based on a custom version of ska::flat_hash_map that
// exposes methods that accept precomputed hashes of values.
//It allows consistent concurrent access for all access types, though
// iteration locks one shard at a time.
//This class mostly works like the std::unordered_map interface, however
//only one iterator may be used at a time due to each iterator containing a lock.
//Larger values of ShardCount require more memory but allow more concurrency.
//ShardCount must be a power of 2
template<
	typename K,
	typename V,
	typename H = std::hash<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<std::pair<const K, V>>,
	size_t ShardCount = 256
>
class ConcurrentFastHashMap
{
public:

	class iterator;
	class const_iterator;

	class iterator
	{
		using InnerIter = typename ska::flat_hash_map<K, V, H, E, A>::iterator;
		using InnerMap = typename ska::flat_hash_map<K, V, H, E, A>;
		friend class const_iterator;
		friend class ConcurrentFastHashMap<K, V, H, E, A>;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = std::pair<const K, V>;
		using difference_type = std::ptrdiff_t;
		using pointer = typename std::iterator_traits<InnerIter>::pointer;
		using reference = typename std::iterator_traits<InnerIter>::reference;

		iterator() = default;

		//disallow copying because it contains a lock
		iterator(const iterator &) = delete;
		iterator &operator=(const iterator &) = delete;

		//allow moving
		iterator(iterator &&) = default;
		iterator &operator=(iterator &&) = default;

		iterator(ConcurrentFastHashMap *parent,
				 size_t shardIdx,
				 InnerIter inner,
				 std::unique_lock<std::mutex> lk) noexcept
			: parent(parent), shardIdx(shardIdx), inner(inner), lock(std::move(lk))
		{}

		reference operator*() const noexcept
		{
			return *inner;
		}
		pointer operator->() const noexcept
		{
			return &(*inner);
		}

		iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		friend bool operator==(const iterator &a, const iterator &b) noexcept
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}
		friend bool operator!=(const iterator &a, const iterator &b) noexcept
		{
			return !(a == b);
		}

		//mixed‑type equality
		bool operator==(const const_iterator &b) const noexcept
		{
			return parent == b.parent &&
				shardIdx == b.shardIdx &&
				(shardIdx == ShardCount || inner == b.inner);
		}
		bool operator!=(const const_iterator &b) const noexcept
		{
			return parent != b.parent ||
				shardIdx != b.shardIdx ||
				!(shardIdx == ShardCount || inner == b.inner);
		}

	private:
		void advance_until_valid()
		{
			while(parent && shardIdx < ShardCount)
			{
				if(inner != parent->shards[shardIdx].map.end())
					return;

				//advance to next shard
				++shardIdx;
				if(shardIdx == ShardCount) break;

				lock = std::unique_lock<std::mutex>(parent->shards[shardIdx].mtx);
				inner = parent->shards[shardIdx].map.begin();

				if(inner != parent->shards[shardIdx].map.end())
					return;
			}

			//reached end, clear state
			parent = nullptr;
			shardIdx = ShardCount;
			if(lock.owns_lock())
				lock.unlock();
		}

		ConcurrentFastHashMap *parent = nullptr;
		size_t                     shardIdx = ShardCount;
		InnerIter                       inner;
		std::unique_lock<std::mutex>    lock;
	};

	class const_iterator
	{
		using InnerIter = typename ska::flat_hash_map<K, V, H, E, A>::const_iterator;
		friend class iterator;
		friend class ConcurrentFastHashMap<K, V, H, E, A>;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = std::pair<const K, V>;
		using difference_type = std::ptrdiff_t;
		using pointer = typename std::iterator_traits<InnerIter>::pointer;
		using reference = typename std::iterator_traits<InnerIter>::reference;

		const_iterator() = default;

		//disallow copying because it contains a lock
		const_iterator(const const_iterator &) = delete;
		const_iterator &operator=(const const_iterator &) = delete;

		//allow moving
		const_iterator(const_iterator &&) = default;
		const_iterator &operator=(const_iterator &&) = default;

		const_iterator(const ConcurrentFastHashMap *parent,
					   size_t shardIdx,
					   InnerIter inner,
					   std::unique_lock<std::mutex> lk) noexcept
			: parent(parent), shardIdx(shardIdx), inner(inner), lock(std::move(lk))
		{}

		reference operator*()  const noexcept
		{
			return *inner;
		}
		pointer   operator->() const noexcept
		{
			return &(*inner);
		}

		const_iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		friend bool operator==(const const_iterator &a,
							   const const_iterator &b) noexcept
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}
		friend bool operator!=(const const_iterator &a,
							   const const_iterator &b) noexcept
		{
			return !(a == b);
		}

		//mixed‑type equality
		bool operator==(const iterator &b) const noexcept
		{
			return parent == b.parent &&
				shardIdx == b.shardIdx &&
				(shardIdx == ShardCount || inner == b.inner);
		}
		bool operator!=(const iterator &b) const noexcept
		{
			return parent != b.parent ||
				shardIdx != b.shardIdx ||
				!(shardIdx == ShardCount || inner == b.inner);
		}

	private:
		void advance_until_valid()
		{
			while(parent && shardIdx < ShardCount)
			{
				if(inner != parent->shards[shardIdx].map.end())
					return;

				++shardIdx;
				if(shardIdx == ShardCount) break;

				lock = std::unique_lock<std::mutex>(parent->shards[shardIdx].mtx);
				inner = parent->shards[shardIdx].map.begin();

				if(inner != parent->shards[shardIdx].map.end())
					return;
			}

			//reached end, clear state
			parent = nullptr;
			shardIdx = ShardCount;
			lock.unlock();
		}

		const ConcurrentFastHashMap *parent = nullptr;
		size_t                     shardIdx = ShardCount;
		InnerIter                       inner;
		std::unique_lock<std::mutex>    lock;
	};

	using key_type = K;
	using mapped_type = V;
	using value_type = std::pair<const K, V>;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using hasher = H;
	using key_equal = E;
	using allocator_type = A;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = typename std::allocator_traits<A>::pointer;
	using const_pointer = typename std::allocator_traits<A>::const_pointer;
	using iterator = iterator;
	using const_iterator = const_iterator;

	friend class iterator;
	friend class const_iterator;

	ConcurrentFastHashMap() = default;

	explicit ConcurrentFastHashMap(
		size_t bucket_count,
		const H &hash = H(),
		const E &equal = E(),
		const A &alloc = A())
		: hash(hash), equal(equal), alloc(alloc)
	{
	}

	bool empty() const
	{
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards[i].mtx);
			if(!shards[i].map.empty())
				return false;
		}
		return true;
	}

	size_type size() const
	{
		size_type total = 0;
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards[i].mtx);
			total += shards[i].map.size();
		}
		return total;
	}

	void clear()
	{
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards[i].mtx);
			shards[i].map.clear();
		}
	}

	mapped_type &operator[](const key_type &key)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		return shards[shard_index].map[key];
	}

	mapped_type &operator[](key_type &&key)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		return shards[shard_index].map[std::move(key)];
	}

	mapped_type &at(const key_type &key)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		auto it = shards[shard_index].map.find_with_hash(key, full_hash);
		return it->second;
	}

	const mapped_type &at(const key_type &key) const
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		auto it = shards[shard_index].map.find_with_hash(key, full_hash);
		return it->second;
	}

	template<class InnerIter>
	static iterator make_iterator(ConcurrentFastHashMap *parent,
								  size_t shardIdx,
								  InnerIter inner,
								  std::unique_lock<std::mutex> lk) noexcept
	{
		return iterator(parent, shardIdx, inner, std::move(lk));
	}

	std::pair<iterator, bool> insert(const value_type &value)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		auto result = shards[shard_index].map.insert_with_hash(value, full_hash);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, idx, inner_it, std::move(lk)), inserted };
	}

	std::pair<iterator, bool> insert(value_type &&value)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//keep the pair in a temporary; the map *does* move‑construct the value,
		//but the iterator itself is just copied out
		auto result = shards[shard_index].map.insert_with_hash(std::move(value), full_hash);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	template<class KArg, class... Rest>
	std::pair<iterator, bool> emplace(KArg &&key, Rest&&... rest)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//store the whole pair first; no structured‑binding that mixes move
		auto result = shards[shard_index].map.emplace_with_hash(std::forward<KArg>(key), full_hash,
											  std::forward<Rest>(rest)...);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	template<class... Args>
	std::pair<iterator, bool> try_emplace(const key_type &key, Args&&... args)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//store the whole pair first; no structured‑binding that mixes move
		auto result = shards[shard_index].map.try_emplace_with_hash(key, full_hash,
												 std::forward<Args>(args)...);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	size_type erase(const key_type &key)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		return shards[shard_index].map.erase_with_hash(key, full_hash);
	}

	void erase(iterator &pos)
	{
		size_t shard_index = pos.shardIdx;
		shards[shard_index].map.erase(pos.inner);
	}

	void erase(const_iterator &pos)
	{
		size_t shard_index = pos.shardIdx;
		shards[shard_index].map.erase(pos.inner);
	}

	iterator find(const key_type &key)
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		auto it = shards[shard_index].map.find(key);
		if(it == shards[shard_index].map.end())
			return end();
		return iterator(this, shard_index, it, std::move(lk));
	}

	const_iterator find(const key_type &key) const
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		auto it = shards[shard_index].map.find(key);
		if(it == shards[shard_index].map.end())
			return end();
		return const_iterator(this, shard_index, it, std::move(lk));
	}

	size_type count(const key_type &key) const
	{
		auto [full_hash, shard_index] = get_hash_and_shard_index(key);
		std::lock_guard<std::mutex> lk(shards[shard_index].mtx);
		return shards[shard_index].map.count(key);
	}

	iterator begin()
	{
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::unique_lock<std::mutex> lk(shards[i].mtx);
			if(!shards[i].map.empty())
				return iterator(this, i, shards[i].map.begin(), std::move(lk));
		}
		//if nothing found return empty/end
		return iterator();
	}

	const_iterator begin() const
	{
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::unique_lock<std::mutex> lk(shards[i].mtx);
			if(!shards[i].map.empty())
				return const_iterator(this, i, shards[i].map.begin(), std::move(lk));
		}
		return const_iterator();
	}

	iterator end() noexcept
	{
		return iterator();
	}
	const_iterator end() const noexcept
	{
		return const_iterator();
	}

	bool operator==(const ConcurrentFastHashMap &other) const
	{
		if(size() != other.size()) return false;
		for(size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk1(shards[i].mtx);
			std::lock_guard<std::mutex> lk2(other.shards[i].mtx);
			if(shards[i].map != other.shards[i].map) return false;
		}
		return true;
	}

	bool operator!=(const ConcurrentFastHashMap &other) const
	{
		return !(*this == other);
	}

protected:
	std::pair<size_t, size_t> get_hash_and_shard_index(const key_type &key) const
	{
		size_t full_hash = hash(key);
		//ShardCount assumed to be power‑of‑2
		return std::make_pair(full_hash, full_hash & (ShardCount - 1));
	}

private:

	//a shard that can be locked independently
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
