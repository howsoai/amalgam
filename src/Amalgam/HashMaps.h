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

//fastest hash maps
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
