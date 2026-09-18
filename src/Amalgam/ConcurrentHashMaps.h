#pragma once

#if defined(MULTITHREAD_SUPPORT)

#include <mutex>
#include <shared_mutex>

template<
	typename FastHashType,
	size_t ShardCount
>
class ConcurrentFastHashBase
{
public:
	using key_type = FastHashType::key_type;
	using value_type = FastHashType::value_type;
	using size_type = FastHashType::size_type;
	using difference_type = std::ptrdiff_t;
	using hasher = FastHashType::hasher;
	using key_equal = FastHashType::key_equal;
	using allocator_type = FastHashType::allocator_type;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = FastHashType::pointer;
	using const_pointer = FastHashType::const_pointer;

	class iterator;
	class const_iterator;
	friend class iterator;
	friend class const_iterator;

	class iterator
	{
		using InnerIter = typename FastHashType::iterator;
		friend class const_iterator;
		friend class ConcurrentFastHashBase<FastHashType, ShardCount>;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = FastHashType::value_type;
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

		inline iterator(ConcurrentFastHashBase *parent,
				 size_t shardIdx,
				 InnerIter inner,
				 std::unique_lock<std::mutex> lk) noexcept
			: parent(parent), shardIdx(shardIdx), inner(inner), lock(std::move(lk))
		{}

		inline reference operator*() const noexcept
		{
			return *inner;
		}

		inline pointer operator->() const noexcept
		{
			return &(*inner);
		}

		inline iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		inline friend bool operator==(const iterator &a, const iterator &b) noexcept
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}
		inline friend bool operator!=(const iterator &a, const iterator &b) noexcept
		{
			return !(a == b);
		}

		//mixed‑type equality
		inline bool operator==(const const_iterator &b) const noexcept
		{
			return parent == b.parent &&
				shardIdx == b.shardIdx &&
				(shardIdx == ShardCount || inner == b.inner);
		}
		inline bool operator!=(const const_iterator &b) const noexcept
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

		ConcurrentFastHashBase *parent = nullptr;
		size_t shardIdx = ShardCount;
		InnerIter inner;
		std::unique_lock<std::mutex> lock;
	};

	class const_iterator
	{
		using InnerIter = typename FastHashType::const_iterator;
		friend class iterator;
		friend class ConcurrentFastHashBase<FastHashType, ShardCount>;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = FastHashType::value_type;
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

		inline const_iterator(const ConcurrentFastHashBase *parent,
					   size_t shardIdx,
					   InnerIter inner,
					   std::unique_lock<std::mutex> lk) noexcept
			: parent(parent), shardIdx(shardIdx), inner(inner), lock(std::move(lk))
		{}

		inline reference operator*()  const noexcept
		{
			return *inner;
		}

		inline pointer operator->() const noexcept
		{
			return &(*inner);
		}

		inline const_iterator &operator++()
		{
			++inner;
			advance_until_valid();
			return *this;
		}

		inline friend bool operator==(const const_iterator &a,
							   const const_iterator &b) noexcept
		{
			return a.parent == b.parent &&
				a.shardIdx == b.shardIdx &&
				(a.shardIdx == ShardCount || a.inner == b.inner);
		}

		inline friend bool operator!=(const const_iterator &a,
							   const const_iterator &b) noexcept
		{
			return !(a == b);
		}

		//mixed‑type equality
		inline bool operator==(const iterator &b) const noexcept
		{
			return parent == b.parent &&
				shardIdx == b.shardIdx &&
				(shardIdx == ShardCount || inner == b.inner);
		}

		inline bool operator!=(const iterator &b) const noexcept
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
			if(lock.owns_lock())
				lock.unlock();
		}

		const ConcurrentFastHashBase *parent = nullptr;
		size_t shardIdx = ShardCount;
		InnerIter inner;
		std::unique_lock<std::mutex> lock;
	};

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

	template<class InnerIter>
	inline static iterator make_iterator(ConcurrentFastHashBase *parent,
								  size_t shardIdx,
								  InnerIter inner,
								  std::unique_lock<std::mutex> lk) noexcept
	{
		return iterator(parent, shardIdx, inner, std::move(lk));
	}

	inline std::pair<iterator, bool> insert(const value_type &value)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		auto result = shards[shard_index].map.insert_with_hash(value, full_hash);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	inline std::pair<iterator, bool> insert(value_type &&value)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(value.first);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//keep the pair in a temporary; the map *does* move‑construct the value,
		//but the iterator itself is just copied out
		auto result = shards[shard_index].map.insert_with_hash(std::move(value), full_hash);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	template<class KArg, class... Rest>
	inline std::pair<iterator, bool> emplace(KArg &&key, Rest&&... rest)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//store the whole pair first; no structured‑binding that mixes move
		auto result = shards[shard_index].map.emplace_with_hash(std::forward<KArg>(key), full_hash,
											  std::forward<Rest>(rest)...);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	template<class... Args>
	inline std::pair<iterator, bool> try_emplace(const key_type &key, Args&&... args)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);

		//store the whole pair first; no structured‑binding that mixes move
		auto result = shards[shard_index].map.try_emplace_with_hash(key, full_hash,
												 std::forward<Args>(args)...);
		auto inner_it = result.first;
		bool inserted = result.second;

		return { make_iterator(this, shard_index, inner_it, std::move(lk)), inserted };
	}

	inline size_type erase(const key_type &key)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(shards[shard_index].mtx);
		return shards[shard_index].map.erase_with_hash(key, full_hash);
	}

	inline void erase(iterator &pos)
	{
		size_t shard_index = pos.shardIdx;
		shards[shard_index].map.erase(pos.inner);
	}

	inline void erase(const_iterator &pos)
	{
		size_t shard_index = pos.shardIdx;
		shards[shard_index].map.erase(pos.inner);
	}

	size_type count(const key_type &key) const
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
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

	inline iterator end() noexcept
	{
		return iterator();
	}

	inline const_iterator end() const noexcept
	{
		return const_iterator();
	}

	inline bool operator==(const ConcurrentFastHashBase &other) const
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

	inline bool operator!=(const ConcurrentFastHashBase &other) const
	{
		return !(*this == other);
	}

protected:
	ConcurrentFastHashBase(const hasher &h, const key_equal &e, const allocator_type &a)
		: hash(h), equal(e), alloc(a)
	{}

	inline std::pair<size_t, size_t> get_hash_and_shard_index(const key_type &key) const
	{
		size_t full_hash = hash(key);

		//SplitMix64 scramble for shard selection
		std::size_t x = full_hash + 0x9e3779b97f4a7c15ULL;
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
		std::size_t scrambled = x ^ (x >> 31);

		//ShardCount must be a power of 2
		std::size_t shard = scrambled & (ShardCount - 1);
		return { full_hash, shard };
	}

	//a shard that can be locked independently
	struct Shard
	{
		mutable std::mutex mtx;
		FastHashType map;
	};

	std::array<Shard, ShardCount> shards;

	hasher hash;
	key_equal equal;
	allocator_type alloc;
};

//A hash set based on a custom version of FastHashMapWithHashInserts that uses an extended variant of the
// std::unordered_set interface that exposes methods that accept precomputed hashes of values via insert_with_hash.
//It allows consistent concurrent access for all access types, though iteration locks one shard at a time.
//This class mostly works like the std::unordered_map interface, however
//only one iterator may be used at a time due to each iterator containing a lock.
//Larger values of ShardCount require more memory but allow more concurrency.
//ShardCount must be a power of 2
template<
	typename K,
	typename H = FastHasher<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<const K>,

	//if USE_STL_HASH_MAPS, then it's debugging, want a single unordered map to make it easy to debug
#ifdef USE_STL_HASH_MAPS
	size_t ShardCount = 1
#else
	size_t ShardCount = 256
#endif
>
class ConcurrentFastHashSet : public ConcurrentFastHashBase<FastHashSetWithHashInserts<K, H, E, A>, ShardCount>
{
public:

	using InnerHash = FastHashSetWithHashInserts<K, H, E, A>;
	using BaseClass = ConcurrentFastHashBase<InnerHash, ShardCount>;

	using key_type = K;
	using value_type = K;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using hasher = H;
	using key_equal = E;
	using allocator_type = A;
	using reference = value_type &;
	using const_reference = const value_type &;
	using pointer = typename std::allocator_traits<A>::pointer;
	using const_pointer = typename std::allocator_traits<A>::const_pointer;

	using iterator = BaseClass::iterator;
	using const_iterator = BaseClass::const_iterator;

	inline ConcurrentFastHashSet(
		const H &hash = H(),
		const E &equal = E(),
		const A &alloc = A())
		: ConcurrentFastHashBase<FastHashSetWithHashInserts<K, H, E, A>, ShardCount>(hash, equal, alloc)
	{}
};

//A hash map based on a custom version of FastHashMapWithHashInserts that uses an extended variant of the
// std::unordered_map interface that exposes methods that accept precomputed hashes of values via insert_with_hash.
//It allows consistent concurrent access for all access types, though iteration locks one shard at a time.
//This class mostly works like the std::unordered_map interface, however
//only one iterator may be used at a time due to each iterator containing a lock.
//Larger values of ShardCount require more memory but allow more concurrency.
//ShardCount must be a power of 2
template<
	typename K,
	typename V,
	typename H = FastHasher<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<std::pair<const K, V>>,

	//if USE_STL_HASH_MAPS, then it's debugging, want a single unordered map to make it easy to debug
#ifdef USE_STL_HASH_MAPS
	size_t ShardCount = 1
#else
	size_t ShardCount = 256
#endif
>
class ConcurrentFastHashMap : public ConcurrentFastHashBase<FastHashMapWithHashInserts<K, V, H, E, A>, ShardCount>
{
public:

	using InnerHash = FastHashMapWithHashInserts<K, V, H, E, A>;
	using BaseClass = ConcurrentFastHashBase<InnerHash, ShardCount>;

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

	using iterator = BaseClass::iterator;
	using const_iterator = BaseClass::const_iterator;

	inline ConcurrentFastHashMap(
		const H &hash = H(),
		const E &equal = E(),
		const A &alloc = A())
		: ConcurrentFastHashBase<FastHashMapWithHashInserts<K, V, H, E, A>, ShardCount>(hash, equal, alloc)
	{}

	inline mapped_type &operator[](const key_type &key)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		return this->shards[shard_index].map[key];
	}

	inline mapped_type &operator[](key_type &&key)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		return this->shards[shard_index].map[std::move(key)];
	}

	inline mapped_type &at(const key_type &key)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		auto it = this->shards[shard_index].map.find_with_hash(key, full_hash);
		return it->second;
	}

	inline const mapped_type &at(const key_type &key) const
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		auto it = this->shards[shard_index].map.find_with_hash(key, full_hash);
		return it->second;
	}

	inline iterator find(const key_type &key)
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		auto it = this->shards[shard_index].map.find(key);
		if(it == this->shards[shard_index].map.end())
			return this->end();
		return iterator(this, shard_index, it, std::move(lk));
	}

	inline const_iterator find(const key_type &key) const
	{
		auto [full_hash, shard_index] = this->get_hash_and_shard_index(key);
		std::unique_lock<std::mutex> lk(this->shards[shard_index].mtx);
		auto it = this->shards[shard_index].map.find(key);
		if(it == this->shards[shard_index].map.end())
			return this->end();
		return const_iterator(this, shard_index, it, std::move(lk));
	}
};

#endif
