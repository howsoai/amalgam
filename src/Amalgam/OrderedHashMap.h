#pragma once
//project headers
#include "VectorMap.h"

//system headers
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

//implements a hash map specified by HashMapType that also keeps track of
//elements' order, governed by insertion order but with a pop-and-swap delete
//it leverages a secondary VectorMap to preserve order and fast iteration
//it is designed to interoperate with VectorMap and allow for custom
//hash maps and hash sets
//note that, like flat hash maps, iterators may be invalidated when the map is altered
template<typename KeyType, typename ValueType,
	typename Hasher = FastHasher<KeyType>, typename Equality = std::equal_to<KeyType>,
	template<typename, typename, typename, typename> typename HashMapType = CompactHashMap,
	template<typename, typename, typename> typename HashSetType = CompactHashSet>
class OrderedHashMap
{
public:

	//--- Type Definitions ---

	using key_type = KeyType;
	using mapped_type = ValueType;
	using value_type = std::pair<key_type, mapped_type>;
	using size_type = std::size_t;

	using HashMap = HashMapType<key_type, size_t, Hasher, Equality>;
	using HashSet = HashSetType<key_type, Hasher, Equality>;

	using key_equal = HashMap::key_equal;
	using VecMap = VectorMap<key_type, mapped_type, key_equal>;
	using iterator = typename VecMap::iterator;
	using const_iterator = typename VecMap::const_iterator;

	//--- Construction & Lifecycle ---

	inline OrderedHashMap() = default;
	inline OrderedHashMap(const OrderedHashMap &other) = default;
	inline OrderedHashMap(OrderedHashMap &&other) noexcept = default;

	inline OrderedHashMap(size_type initial_capacity)
	{
		vectorMap.reserve(initial_capacity);
		hashMap.reserve(initial_capacity);
	}

	inline OrderedHashMap(const VecMap &other)
	{
		vectorMap = other;
		BuildHashMapFromVectorMap();
	}

	inline OrderedHashMap(VecMap &&other)
	{
		vectorMap = std::move(other);
		BuildHashMapFromVectorMap();
	}

	inline ~OrderedHashMap() = default;

	inline OrderedHashMap &operator=(const OrderedHashMap &other)
	{
		if(this != &other)
		{
			vectorMap = other.vectorMap;
			hashMap = other.hashMap;
		}
		return *this;
	}

	inline OrderedHashMap &operator=(OrderedHashMap &&other) noexcept
	{
		if(this != &other)
		{
			vectorMap = std::move(other.vectorMap);
			hashMap = std::move(other.hashMap);
		}
		return *this;
	}

	//assign from a VecMap
	inline OrderedHashMap &operator=(const VecMap &other)
	{
		vectorMap = other;
		BuildHashMapFromVectorMap();
		return *this;
	}

	//move from a VecMap
	inline OrderedHashMap &operator=(VecMap &&other) noexcept
	{
		vectorMap = std::move(other);
		BuildHashMapFromVectorMap();
		return *this;
	}

	//allow it to be used in context of VecMap
	inline operator VecMap &()
	{
		return vectorMap;
	}

	inline operator const VecMap &() const
	{
		return vectorMap;
	}

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

	inline void reserve(size_type n)
	{
		vectorMap.reserve(n);
		hashMap.reserve(n);
	}

	inline void clear()
	{
		vectorMap.clear();
		hashMap.clear();
	}

	template<class... Args>
	inline std::pair<iterator, bool> try_emplace(const key_type &key, Args&&... args)
	{
		//if all 3rd party libraries are C++20 compliant, can change emplace to a try_emplace
		auto [map_it, inserted] = hashMap.emplace(key, vectorMap.size());

		auto &vec = vectorMap.GetVector();
		if(inserted)
		{
			vec.emplace_back(key, mapped_type{ std::forward<Args>(args)... });
			std::size_t new_index = vec.size() - 1;
			map_it->second = new_index;
			return { vectorMap.begin() + new_index, true };
		}

		return { vectorMap.begin() + map_it->second, false };
	}

	template<class... Args>
	inline std::pair<iterator, bool> emplace(Args&&... args)
	{
		using tuple_t = std::tuple<Args&&...>;
		tuple_t tup{ std::forward<Args>(args)... };

		auto &&key = std::get<0>(tup);

		//if all 3rd party libraries are C++20 compliant, can change emplace to a try_emplace
		auto [map_it, inserted] = hashMap.emplace(key, vectorMap.size());

		auto &vec = vectorMap.GetVector();
		if(inserted)
		{
			//construct the value in place, forwarding remaining arguments
			constexpr std::size_t tuple_size = std::tuple_size_v<tuple_t>;
			auto construct_value = [&](auto&&... tail) {
				vec.emplace_back(std::forward<decltype(key)>(key),
					mapped_type{ std::forward<decltype(tail)>(tail)... });
				};

			//actually construct the value
			[&] <std::size_t... I>(std::index_sequence<I...>)
			{
				construct_value(std::get<I + 1>(tup)...);
			}(std::make_index_sequence<tuple_size - 1>{});

			std::size_t new_index = vec.size() - 1;
			map_it->second = new_index;
			return { vectorMap.begin() + new_index, true };
		}

		// Key already present – return iterator to existing element.
		return { vectorMap.begin() + map_it->second, false };
	}

	//can only be called when it is known ahead of time that the key is not contained
	template<class... Args>
	inline iterator EmplaceUnique(key_type key, Args &&...args)
	{
		hashMap.emplace(key, vectorMap.size());
		return vectorMap.EmplaceUnique(key, std::forward<Args>(args)...);
	}

	inline std::pair<iterator, bool> insert_or_assign(const key_type &key, mapped_type &&value)
	{
		//if all 3rd party libraries are C++20 compliant, can change emplace to a try_emplace
		auto [map_it, inserted] = hashMap.emplace(key, vectorMap.size());

		if(inserted)
		{
			auto &vec = vectorMap.GetVector();
			vec.emplace_back(key, std::move(value));
			size_type new_index = vec.size() - 1;
			map_it->second = new_index;
			return { vectorMap.begin() + new_index, true };
		}
		else
		{
			auto &vec = vectorMap.GetVector();
			vec[map_it->second].second = std::move(value);
			return { vectorMap.begin() + map_it->second, false };
		}
	}

	inline std::pair<iterator, bool> insert(const value_type &value)
	{
		return emplace(value.first, value.second);
	}

	inline std::pair<iterator, bool> insert(const key_type &key, const mapped_type &value)
	{
		return emplace(key, value);
	}

	size_t erase(const key_type &key)
	{
		auto it = hashMap.find(key);
		if(it == hashMap.end())
			return 0;

		auto &vec = vectorMap.GetVector();
		size_type index_to_remove = it->second;
		size_type last_index = vec.size() - 1;

		//move the last element into the slot of the element being removed
		if(index_to_remove != last_index)
		{
			vec[index_to_remove] = std::move(vec[last_index]);
			hashMap[vec[index_to_remove].first] = index_to_remove;
		}

		vec.pop_back();
		hashMap.erase(key);

		return 1;
	}

	iterator erase(iterator pos)
	{
		if(pos == vectorMap.end())
			return vectorMap.end();

		auto hash_map_it = hashMap.find(pos->first);
		if(hash_map_it == hashMap.end())
			return vectorMap.end();

		//swap with last
		auto &vec = vectorMap.GetVector();
		size_type index_to_remove = hash_map_it->second;
		size_type last_index = vec.size() - 1;
		if(index_to_remove != last_index)
		{
			vec[index_to_remove] = std::move(vec[last_index]);
			hashMap[vec[index_to_remove].first] = index_to_remove;
		}

		vec.pop_back();
		hashMap.erase(hash_map_it);

		return (index_to_remove == vec.size()) ? vectorMap.end() : vectorMap.begin() + index_to_remove;
	}

	void EraseBatch(const std::vector<key_type> &keys_to_remove)
	{
		HashSet to_remove(keys_to_remove.begin(), keys_to_remove.end());

		auto &vec = vectorMap.GetVector();
		std::size_t write = 0;
		for(std::size_t read = 0; read < vec.size(); read++)
		{
			const key_type &k = vec[read].first;
			if(to_remove.find(k) == to_remove.end())
			{
				if(write != read)
				{
					vec[write] = std::move(vec[read]);
					hashMap[vec[write].first] = write;
				}
				++write;
			}
			else
			{
				hashMap.erase(k);
			}
		}
		vec.resize(write);   // shrink to the number of kept elements
	}

	inline void swap(OrderedHashMap &other) noexcept
	{
		std::swap(vectorMap, other.vectorMap);
		std::swap(hashMap, other.hashMap);
	}

	//--- Lookup ---

	inline mapped_type &at(const key_type &key)
	{
		size_type index = hashMap.at(key);
		auto &vec = vectorMap.GetVector();
		return vec[index].second;
	}

	mapped_type &operator[](const key_type &key)
	{
		//if all 3rd party libraries are C++20 compliant, can change emplace to a try_emplace
		auto [map_it, inserted] = hashMap.emplace(key, vectorMap.size());

		auto &vec = vectorMap.GetVector();
		if(inserted)
		{
			vec.emplace_back(key, mapped_type{});
			size_type new_index = vec.size() - 1;
			map_it->second = new_index;
			return vec[new_index].second;
		}
		else
		{
			return vec[map_it->second].second;
		}
	}

	inline size_type count(const key_type &key) const
	{
		return hashMap.count(key);
	}

	inline iterator find(const key_type &key)
	{
		auto it = hashMap.find(key);
		if(it != hashMap.end())
		{
			return vectorMap.begin() + it->second;
		}
		return vectorMap.end();
	}

	inline bool contains(const key_type &key) const
	{
		return hashMap.find(key) != hashMap.end();
	}

	//gives access to the internal vectorMap
	inline VecMap &GetVectorMap()
	{
		return vectorMap;
	}

	//moves the vector map out and clears the data structure
	inline VecMap &&ExtractVectorMap()
	{
		hashMap.clear();
		return std::move(vectorMap);
	}

	//used for more advanced manipulation
	inline std::vector<std::pair<key_type, mapped_type>> &GetVector()
	{
		return vectorMap.GetVector();
	}

private:

	inline void BuildHashMapFromVectorMap()
	{
		hashMap.clear();
		hashMap.reserve(vectorMap.size());
		size_t index = 0;
		for(auto key : vectorMap | std::views::keys)
			hashMap.emplace(key, index++);
	}

	VecMap vectorMap;
	HashMap hashMap;
};

namespace
{
	template<typename... Args>
	inline auto begin(OrderedHashMap<Args...> &m)
	{
		return m.begin();
	}

	template<typename... Args>
	inline auto cbegin(const OrderedHashMap<Args...> &m)
	{
		return m.cbegin();
	}

	template<typename... Args>
	inline auto end(OrderedHashMap<Args...> &m)
	{
		return m.end();
	}

	template<typename... Args>
	inline auto cend(const OrderedHashMap<Args...> &m)
	{
		return m.cend();
	}

	template<typename... Args>
	inline auto swap(OrderedHashMap<Args...> &a, OrderedHashMap<Args...> &b)
	{
		return a.swap(b);
	}
}
