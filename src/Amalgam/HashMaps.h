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
	std::size_t ShardCount = 256   // must be a power‑of‑2 (multiple of 16)
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

		// disallow copying
		iterator(const iterator &) = delete;
		iterator &operator=(const iterator &) = delete;

		// allow moving (defaulted is fine)
		iterator(iterator &&) = default;
		iterator &operator=(iterator &&) = default;

		iterator(ConcurrentFastHashMap *parent,
				 std::size_t shardIdx,
				 InnerIter inner,
				 std::unique_lock<std::mutex> lk) noexcept
			: parent_(parent), shardIdx_(shardIdx), inner_(inner), lock_(std::move(lk))
		{}

		reference operator*()  const noexcept
		{
			return *inner_;
		}
		pointer   operator->() const noexcept
		{
			return &(*inner_);
		}

		// pre‑increment
		iterator &operator++()
		{
			++inner_;
			advance_until_valid();
			return *this;
		}

		// post‑increment
		iterator operator++(int)
		{
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		friend bool operator==(const iterator &a, const iterator &b) noexcept
		{
			return a.parent_ == b.parent_ &&
				a.shardIdx_ == b.shardIdx_ &&
				(a.shardIdx_ == ShardCount || a.inner_ == b.inner_);
		}
		friend bool operator!=(const iterator &a, const iterator &b) noexcept
		{
			return !(a == b);
		}

		// mixed‑type equality as a member
		bool operator==(const const_iterator &b) const noexcept
		{
			return parent_ == b.parent_ &&
				shardIdx_ == b.shardIdx_ &&
				(shardIdx_ == ShardCount || inner_ == b.inner_);
		}
		bool operator!=(const const_iterator &b) const noexcept
		{
			return parent_ != b.parent_ ||
				shardIdx_ != b.shardIdx_ ||
				!(shardIdx_ == ShardCount || inner_ == b.inner_);
		}

	private:
		void advance_until_valid()
		{
			while(parent_ && shardIdx_ < ShardCount)
			{
				if(inner_ != parent_->shards_[shardIdx_].map.end())
					return;                     // still inside a valid element

				// move to next shard
				++shardIdx_;
				if(shardIdx_ == ShardCount) break;

				lock_ = std::unique_lock<std::mutex>(parent_->shards_[shardIdx_].mtx);
				inner_ = parent_->shards_[shardIdx_].map.begin();

				if(inner_ != parent_->shards_[shardIdx_].map.end())
					return;
			}
			// reached end – clear state
			parent_ = nullptr;
			shardIdx_ = ShardCount;
			lock_.unlock();
		}

		ConcurrentFastHashMap *parent_ = nullptr;
		std::size_t                     shardIdx_ = ShardCount;
		InnerIter                       inner_;
		std::unique_lock<std::mutex>    lock_;
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

		// disallow copying
		const_iterator(const const_iterator &) = delete;
		const_iterator &operator=(const const_iterator &) = delete;

		// allow moving
		const_iterator(const_iterator &&) = default;
		const_iterator &operator=(const_iterator &&) = default;

		const_iterator(const ConcurrentFastHashMap *parent,
					   std::size_t shardIdx,
					   InnerIter inner,
					   std::unique_lock<std::mutex> lk) noexcept
			: parent_(parent), shardIdx_(shardIdx), inner_(inner), lock_(std::move(lk))
		{}

		reference operator*()  const noexcept
		{
			return *inner_;
		}
		pointer   operator->() const noexcept
		{
			return &(*inner_);
		}

		const_iterator &operator++()
		{
			++inner_;
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
							   const const_iterator &b) noexcept
		{
			return a.parent_ == b.parent_ &&
				a.shardIdx_ == b.shardIdx_ &&
				(a.shardIdx_ == ShardCount || a.inner_ == b.inner_);
		}
		friend bool operator!=(const const_iterator &a,
							   const const_iterator &b) noexcept
		{
			return !(a == b);
		}

		// mixed‑type equality as a member
		bool operator==(const iterator &b) const noexcept
		{
			return parent_ == b.parent_ &&
				shardIdx_ == b.shardIdx_ &&
				(shardIdx_ == ShardCount || inner_ == b.inner_);
		}
		bool operator!=(const iterator &b) const noexcept
		{
			return parent_ != b.parent_ ||
				shardIdx_ != b.shardIdx_ ||
				!(shardIdx_ == ShardCount || inner_ == b.inner_);
		}

	private:
		void advance_until_valid()
		{
			while(parent_ && shardIdx_ < ShardCount)
			{
				if(inner_ != parent_->shards_[shardIdx_].map.end())
					return;

				++shardIdx_;
				if(shardIdx_ == ShardCount) break;

				lock_ = std::unique_lock<std::mutex>(parent_->shards_[shardIdx_].mtx);
				inner_ = parent_->shards_[shardIdx_].map.begin();

				if(inner_ != parent_->shards_[shardIdx_].map.end())
					return;
			}
			parent_ = nullptr;
			shardIdx_ = ShardCount;
			lock_.unlock();
		}

		const ConcurrentFastHashMap *parent_ = nullptr;
		std::size_t                     shardIdx_ = ShardCount;
		InnerIter                       inner_;
		std::unique_lock<std::mutex>    lock_;
	};

	using key_type = K;
	using mapped_type = V;
	using value_type = std::pair<const K, V>;
	using size_type = std::size_t;
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
		std::size_t bucket_count,
		const H &hash = H(),
		const E &equal = E(),
		const A &alloc = A())
		: hash_(hash), equal_(equal), alloc_(alloc)
	{
		(void)bucket_count; // bucket count is ignored – shards are fixed
	}

	bool empty() const
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards_[i].mtx);
			if(!shards_[i].map.empty())
				return false;
		}
		return true;
	}

	size_type size() const
	{
		size_type total = 0;
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards_[i].mtx);
			total += shards_[i].map.size();
		}
		return total;
	}

	void clear()
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk(shards_[i].mtx);
			shards_[i].map.clear();
		}
	}

	mapped_type &operator[](const key_type &key)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		return shards_[idx].map[key];
	}

	mapped_type &operator[](key_type &&key)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		return shards_[idx].map[std::move(key)];
	}

	mapped_type &at(const key_type &key)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto it = shards_[idx].map.find(key);
		if(it == shards_[idx].map.end())
			throw std::out_of_range("ConcurrentFastHashMap::at");
		return it->second;
	}

	const mapped_type &at(const key_type &key) const
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto it = shards_[idx].map.find(key);
		if(it == shards_[idx].map.end())
			throw std::out_of_range("ConcurrentFastHashMap::at");
		return it->second;
	}

	std::pair<iterator, bool> insert(const value_type &value)
	{
		std::size_t idx = shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto [innerIt, inserted] = shards_[idx].map.insert(value);
		iterator wrapped(this, idx, innerIt, std::move(lk));
		return std::pair<iterator, bool>(std::move(wrapped), inserted);
	}

	std::pair<iterator, bool> insert(value_type &&value)
	{
		std::size_t idx = shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto [innerIt, inserted] = shards_[idx].map.insert(std::move(value));
		iterator wrapped(this, idx, innerIt, std::move(lk));
		return std::pair<iterator, bool>(std::move(wrapped), inserted);
	}

	template<class KArg, class... Rest>
	std::pair<iterator, bool> emplace(KArg &&key, Rest&&... rest)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto [innerIt, inserted] =
			shards_[idx].map.emplace(std::forward<KArg>(key),
									 std::forward<Rest>(rest)...);
		return { iterator(this, idx, innerIt, std::move(lk)), inserted };
	}

	template<class... Args>
	std::pair<iterator, bool> try_emplace(const key_type &key, Args&&... args)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto [innerIt, inserted] = shards_[idx].map.try_emplace(key, std::forward<Args>(args)...);
		iterator wrapped(this, idx, innerIt, std::move(lk));
		return std::pair<iterator, bool>(std::move(wrapped), inserted);
	}

	size_type erase(const key_type &key)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		return shards_[idx].map.erase(key);
	}

	// erase that moves the iterator (no copy)
	iterator erase(iterator &&pos)               // by r‑value reference
	{
		std::size_t idx = pos.shardIdx_;
		auto nextInner = shards_[idx].map.erase(pos.inner_);
		return iterator(this, idx, nextInner, std::move(pos.lock_));
	}

	// overload for const_iterator (also moves)
	iterator erase(const_iterator &&pos)
	{
		std::size_t idx = pos.shardIdx_;
		auto nextInner = shards_[idx].map.erase(pos.inner_);
		return iterator(this, idx, nextInner, std::move(pos.lock_));
	}

	iterator erase(iterator &pos)          // l‑value reference
	{
		// reuse the r‑value implementation
		return erase(std::move(pos));
	}

	iterator erase(const_iterator &pos)    // l‑value reference
	{
		return erase(std::move(pos));
	}

	iterator find(const key_type &key)
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto it = shards_[idx].map.find(key);
		if(it == shards_[idx].map.end())
			return end();
		return iterator(this, idx, it, std::move(lk));
	}

	const_iterator find(const key_type &key) const
	{
		std::size_t idx = shard_index(key);
		std::unique_lock<std::mutex> lk(shards_[idx].mtx);
		auto it = shards_[idx].map.find(key);
		if(it == shards_[idx].map.end())
			return end();
		return const_iterator(this, idx, it, std::move(lk));
	}

	size_type count(const key_type &key) const
	{
		std::size_t idx = shard_index(key);
		std::lock_guard<std::mutex> lk(shards_[idx].mtx);
		return shards_[idx].map.count(key);
	}

	iterator begin()
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::unique_lock<std::mutex> lk(shards_[i].mtx);
			if(!shards_[i].map.empty())
			{
				return iterator(this, i, shards_[i].map.begin(), std::move(lk));
			}
		}
		return iterator(); // empty iterator (null state)
	}

	const_iterator begin() const
	{
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::unique_lock<std::mutex> lk(shards_[i].mtx);
			if(!shards_[i].map.empty())
			{
				return const_iterator(this, i, shards_[i].map.begin(), std::move(lk));
			}
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
		for(std::size_t i = 0; i < ShardCount; ++i)
		{
			std::lock_guard<std::mutex> lk1(shards_[i].mtx);
			std::lock_guard<std::mutex> lk2(other.shards_[i].mtx);
			if(shards_[i].map != other.shards_[i].map) return false;
		}
		return true;
	}

	bool operator!=(const ConcurrentFastHashMap &other) const
	{
		return !(*this == other);
	}

protected:
	std::size_t shard_index(const key_type &key) const
	{
		std::size_t fullHash = hash_(key);
		return fullHash & (ShardCount - 1);   // ShardCount must be power‑of‑2
	}

private:
	struct Shard
	{
		mutable std::mutex                     mtx;
		ska::flat_hash_map<K, V, H, E, A>      map;
	};

	H                                          hash_;
	E                                          equal_;
	A                                          alloc_;
	std::array<Shard, ShardCount>              shards_;
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
