#pragma once
//system includes
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

//implements a map via a vector, where entries are looked up sequentially for brute force
//useful for standing in for hash maps when the data is very small (generally less than 20 entries)
// and for hash maps where entries are only iterated over or found once
//note that, like flat hash maps, iterators may be invalidated when the map is altered
//the order of the elements is as inserted, but when an element is deleted, it swaps it with the last element
template<typename K, typename V, typename KeyEqual = std::equal_to<K>>
class VectorMap
{
public:
	using key_type = K;
	using mapped_type = V;
	using value_type = std::pair<key_type, mapped_type>;
	using key_equal = KeyEqual;

	//standard naming for iterators
	using iterator = std::vector<std::pair<key_type, mapped_type>>::iterator;
	using const_iterator = std::vector<std::pair<key_type, mapped_type>>::const_iterator;

	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = mapped_type &;
	using const_reference = const mapped_type &;
	using iterator_category = std::random_access_iterator_tag;

	//--- Construction & Lifecycle ---

	inline VectorMap() = default;
	inline VectorMap(const VectorMap &other) = default;
	inline VectorMap(VectorMap &&other) noexcept = default;

	inline ~VectorMap() = default;

	//--- Iterators ---

	inline iterator begin() noexcept
	{
		return data.begin();
	}

	inline const_iterator cbegin() const noexcept
	{
		return data.cbegin();
	}

	inline iterator end() noexcept
	{
		return data.end();
	}

	inline const_iterator cend() const noexcept
	{
		return data.cend();
	}

	//--- Capacity & Size ---

	inline bool empty() const
	{
		return data.empty();
	}

	inline size_t size() const
	{
		return data.size();
	}

	inline size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max();
	}

	//--- Modifiers ---

	inline void reserve(size_t n)
	{
		data.reserve(n);
	}

	inline void clear()
	{
		data.clear();
	}

	inline void resize(size_t n)
	{
		data.resize(n);
	}

	template<class... Args> inline std::pair<iterator, bool> try_emplace(key_type &&key, Args &&...args)
	{
		auto it = find(key);
		if(it != end())
			return {it, false};

		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), mapped_type{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return {std::prev(data.end()), true};
	}

	template<class... Args> inline std::pair<iterator, bool> emplace(key_type key, Args &&...args)
	{
		auto it = find(key);
		if(it != data.end())
			return {it, false};

		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), mapped_type{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return {std::prev(data.end()), true};
	}

	//can only be called when it is known ahead of time that the key is not contained
	template<class... Args> inline iterator EmplaceUnique(key_type key, Args &&...args)
	{
		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), mapped_type{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return std::prev(data.end());
	}

	template<class... Args> iterator insert_or_assign(key_type &&key, Args &&...args)
	{
		auto it = find(key);
		if(it != end())
		{
			it->second = mapped_type{std::forward<Args>(args)...};
			return it;
		}
		else
		{
			if constexpr(sizeof...(Args) == 0)
				data.emplace_back(std::move(key), mapped_type{});
			else
				data.emplace_back(std::move(key), std::forward<Args>(args)...);

			return std::prev(data.end());
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

	void insert(std::initializer_list<value_type> init)
	{
		for(auto &item : init)
		{
			auto it = find(item.first);
			if(it != end())
				continue;

			data.emplace_back(std::move(item));
		}
	}

	inline iterator erase(iterator it)
	{
		//only swap if not the last element
		if(it != std::prev(data.end()))
			std::swap(*it, data.back());

		data.pop_back();
		return it;
	}

	size_t erase(const key_type &key)
	{
		auto it = find(key);
		if(it != end())
		{
			erase(it);
			return 1;
		}
		return 0;
	}

	inline void swap(VectorMap &other) noexcept
	{
		std::swap(this->data, other.data);
	}

	//--- Lookup ---

	inline mapped_type &at(const key_type &key)
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("VectorMap::at: key not found");

		return it->second;
	}

	const mapped_type &at(const key_type &key) const
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("VectorMap::at: key not found");

		return it->second;
	}

	inline mapped_type &operator[](const key_type &key)
	{
		auto it = find(key);
		if(it == end())
			it = try_emplace(key).first;
		return it->second;
	}

	inline size_type count(const key_type &key) const
	{
		return find(key) != cend() ? 1 : 0;
	}

	inline iterator find(const key_type &key)
	{
		return std::find_if(data.begin(), data.end(),
			[&](const std::pair<key_type, mapped_type> &p) { return key_equal{}(p.first, key); });
	}

	inline bool contains(const key_type &key) const
	{
		return find(key) != data.cend();
	}

	//used for more advanced manipulation
	std::vector<std::pair<key_type, mapped_type>> &GetVector()
	{
		return data;
	}

private:
	std::vector<std::pair<key_type, mapped_type>> data;
};

namespace
{
	template<typename K, typename V, typename E>
	inline auto begin(VectorMap<K, V, E> &m)
	{
		return m.begin();
	}

	template<typename K, typename V, typename E>
	inline auto cbegin(const VectorMap<K, V, E> &m)
	{
		return m.cbegin();
	}

	template<typename K, typename V, typename E>
	inline auto end(VectorMap<K, V, E> &m)
	{
		return m.end();
	}

	template<typename K, typename V, typename E>
	inline auto cend(const VectorMap<K, V, E> &m)
	{
		return m.cend();
	}
}
