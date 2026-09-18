#pragma once

//project headers:
#include "Concurrency.h"
#include "VectorMap.h"

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

//a fast, deterministic hash object
//note that any types that have padding or are more complex with equality will need their own hash implementation
template <typename T>
struct FastHasher {
	std::size_t operator()(const T& val) const noexcept {
		return std::hash<T>{}(val);
	}
};

//overload for pairs
template <typename T1, typename T2>
struct FastHasher<std::pair<T1, T2>> {
	std::size_t operator()(const std::pair<T1, T2>& val) const noexcept {
		auto h1 = std::hash<T1>{}(val.first);
		auto h2 = std::hash<T2>{}(val.second);
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL);
	}
};


template<typename T, typename H = FastHasher<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using FastHashSet = std::unordered_set<T, H, E, A>;

template<typename K, typename V, typename H = FastHasher<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using FastHashMap = std::unordered_map<K, V, H, E, A>;

template<typename T, typename H = FastHasher<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using CompactHashSet = std::unordered_set<T, H, E, A>;

template<typename K, typename V, typename H = FastHasher<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using CompactHashMap = std::unordered_map<K, V, H, E, A>;

//wrapper that includes method specializations of _with_hash to enable the use of std::unordered_set with ConcurrentFastHashSet
template<
	typename K,
	typename H = std::hash<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<const K>>
class FastHashSetWithHashInserts : public std::unordered_set<K, H, E, A>
{
	using Base = std::unordered_set<K, H, E, A>;

public:
	using Base::Base;

	bool erase_with_hash(const K &key, std::size_t /*key_hash*/)
	{
		return this->erase(key) != 0;    // returns true if something was erased
	}

	std::pair<typename Base::iterator, bool> insert_with_hash(const K &value, std::size_t /*key_hash*/)
	{
		return this->insert(value);
	}

	template<class... Args>
	std::pair<typename Base::iterator, bool> emplace_with_hash(const K &key, std::size_t /*key_hash*/, Args&&... args)
	{
		// Forward to the normal emplace; the hash argument is discarded.
		return this->emplace(std::piecewise_construct,
							 std::forward_as_tuple(key),
							 std::forward_as_tuple(std::forward<Args>(args)...));
	}
};

//wrapper that includes method specializations of _with_hash to enable the use of std::unordered_set with ConcurrentFastHashMap
template<
	typename K,
	typename V,
	typename H = std::hash<K>,
	typename E = std::equal_to<K>,
	typename A = std::allocator<std::pair<const K, V>>>
class FastHashMapWithHashInserts : public std::unordered_map<K, V, H, E, A>
{
	using Base = std::unordered_map<K, V, H, E, A>;

public:
	using Base::Base;

	typename Base::iterator find_with_hash(const K &key, std::size_t /*key_hash*/)
	{
		return this->find(key);
	}

	typename Base::const_iterator find_with_hash(const K &key, std::size_t /*key_hash*/) const
	{
		return this->find(key);
	}

	bool erase_with_hash(const K &key, std::size_t /*key_hash*/)
	{
		return this->erase(key) != 0;    // returns true if something was erased
	}

	std::pair<typename Base::iterator, bool> insert_with_hash(const std::pair<const K, V> &value,
						 std::size_t /*key_hash*/)
	{
		return this->insert(value);
	}

	template<class... Args>
	std::pair<typename Base::iterator, bool> emplace_with_hash(const K &key, std::size_t /*key_hash*/, Args&&... args)
	{
		// Forward to the normal emplace; the hash argument is discarded.
		return this->emplace(std::piecewise_construct,
							 std::forward_as_tuple(key),
							 std::forward_as_tuple(std::forward<Args>(args)...));
	}
};

#else

#include <array>
#include <cstddef>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include "rapidhash/rapidhash.h"
#include "skarupke_maps/bytell_hash_map.hpp"
#include "skarupke_maps/flat_hash_map.hpp"


//traits to detect if a type is a std::pair
template <typename T>
struct is_pair : std::false_type
{};

template <typename T, typename U>
struct is_pair<std::pair<T, U>> : std::true_type
{};

template <typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

//traits to check if a type has .data() and .size() methods
template <typename T, typename = void>
struct has_data_and_size : std::false_type
{};

template <typename T>
struct has_data_and_size<T, std::void_t<
	decltype(std::declval<const T &>().data()),
	decltype(std::declval<const T &>().size())
	>> : std::true_type {};

template <typename T>
inline constexpr bool has_data_and_size_v = has_data_and_size<T>::value;

inline constexpr uint64_t rapid_hasher_rand_seed = 13766731;

//a fast, deterministic hash object
//note that any types that have padding or are more complex with equality will need their own hash implementation
template <typename T, typename = void>
struct FastHasher
{
	std::size_t operator()(const T &val) const noexcept
	{
		return static_cast<std::size_t>(rapidhash_withSeed(&val, sizeof(T), rapid_hasher_rand_seed));
	}
};

//container types like std::string, std::vector
template <typename T>
struct FastHasher<T, std::enable_if_t<has_data_and_size_v<T>>>
{
	std::size_t operator()(const T &val) const noexcept
	{
		return static_cast<std::size_t>(rapidhash_withSeed(val.data(), val.size() * sizeof(*val.data()), rapid_hasher_rand_seed));
	}
};

//or raw objects, but ignore pairs to prevent ambiguity
template <typename T>
struct FastHasher<T, std::enable_if_t<std::is_trivially_copyable_v<T> && !has_data_and_size_v<T> && !is_pair_v<T>>>
{
	std::size_t operator()(const T &val) const noexcept
	{
		return static_cast<std::size_t>(rapidhash_withSeed(&val, sizeof(T), rapid_hasher_rand_seed));
	}
};

//pair of pointers
template<typename T>
struct FastHasher<std::pair<T *, T *>, void>
{
	inline size_t operator()(std::pair<T *, T *> const &pointer_pair) const noexcept
	{
		return static_cast<size_t>(rapidhash_withSeed(&pointer_pair, sizeof(pointer_pair), rapid_hasher_rand_seed));
	}
};

template<typename T, typename H = FastHasher<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using FastHashSet = ska::flat_hash_set<T, H, E, A>;

template<typename K, typename V, typename H = FastHasher<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using FastHashMap = ska::flat_hash_map<K, V, H, E, A>;

template<typename T, typename H = FastHasher<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using FastHashSetWithHashInserts = ska::flat_hash_set<T, H, E, A>;

template<typename K, typename V, typename H = FastHasher<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using FastHashMapWithHashInserts = ska::flat_hash_map<K, V, H, E, A>;

template<typename T, typename H = FastHasher<T>, typename E = std::equal_to<T>, typename A = std::allocator<T> >
using CompactHashSet = ska::bytell_hash_set<T, H, E, A>;

template<typename K, typename V, typename H = FastHasher<K>, typename E = std::equal_to<K>, typename A = std::allocator<std::pair<const K, V> > >
using CompactHashMap = ska::bytell_hash_map<K, V, H, E, A>;

#endif

#if defined(MULTITHREAD_SUPPORT)
#include "ConcurrentHashMaps.h"
#endif

#include "OrderedHashMap.h"
