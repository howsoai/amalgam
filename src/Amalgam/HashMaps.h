#pragma once

////////////////////
// Defines hash set types in a generic way so they can be easily changed
// * * * Profile and choose whichever works fastest and with least memory  * * *
// Notes about the hashes:
// std::unordered is second best for maximizing debugability (due to IDE support) but not as easy as std::map, but is slow
// ska::flat_hash is best for performance, but eats a bit of memory
// ska::bytell_hash is good for compact memory and almost as fast as ska::flat_hash (should be used for things that need to be fairly fast but are not accessed as frequently, where minimizing memory is more important)

#ifdef USE_STL_HASH_MAPS

#include <unordered_map>
#include <unordered_set>

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
