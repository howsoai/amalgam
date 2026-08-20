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
template<typename K, typename V, typename E = std::equal_to<K>>
class VectorMap
{
public:
	using key_type = K;
	using mapped_type = V;
	using value_type = std::pair<K, V>;

	// Standard naming for iterators
	using iterator = std::vector<std::pair<K, V>>::iterator;
	using const_iterator = std::vector<std::pair<K, V>>::const_iterator;

	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = mapped_type &;
	using const_reference = const mapped_type &;
	using iterator_category = std::random_access_iterator_tag;

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

	inline size_t size() const
	{
		return data.size();
	}

	inline bool empty() const
	{
		return data.empty();
	}

	inline void reserve(size_t n)
	{
		data.reserve(n);
	}

	inline void resize(size_t n)
	{
		data.resize(n);
	}

	inline iterator find(const K &key)
	{
		return std::find_if(data.begin(), data.end(),
			[&](const std::pair<K, V> &p) { return E{}(p.first, key); });
	}

	inline bool contains(const K &key) const
	{
		return find(key) != data.cend();
	}

	inline mapped_type &operator[](const K &key)
	{
		auto it = find(key);
		if(it == end())
			it = try_emplace(key).first;
		return it->second;
	}

	inline mapped_type &at(const K &key)
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("VectorMap::at: key not found");

		return it->second;
	}

	const mapped_type &at(const K &key) const
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("VectorMap::at: key not found");

		return it->second;
	}

	inline iterator erase(iterator it)
	{
		return data.erase(it);
	}

	size_t erase(const K &key)
	{
		auto it = find(key);
		if(it != end())
		{
			data.erase(it);
			return 1;
		}
		return 0;
	}

	inline void clear()
	{
		data.clear();
	}

	inline void swap(VectorMap &other) noexcept
	{
		std::swap(this->data, other.data);
	}

	inline size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max();
	}

	inline size_type count(const K &key) const
	{
		return find(key) != cend() ? 1 : 0;
	}

	std::pair<iterator, bool> insert(const K &key, const V &value)
	{
		auto it = find(key);
		if(it != end())
			return { it, false };

		data.emplace_back(key, value);
		return { std::prev(data.end()), true};
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

	template<class... Args>
	iterator insert_or_assign(K &&key, Args&&... args)
	{
		auto it = find(key);
		if(it != end())
		{
			it->second = V{ std::forward<Args>(args)... };
			return it;
		}
		else
		{
			if constexpr(sizeof...(Args) == 0)
				data.emplace_back(std::move(key), V{});
			else
				data.emplace_back(std::move(key), std::forward<Args>(args)...);

			return std::prev(data.end());
		}
	}

	template<class... Args>
	inline std::pair<iterator, bool> try_emplace(K &&key, Args&&... args)
	{
		auto it = find(key);
		if(it != end())
			return { it, false };

		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), V{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return { std::prev(data.end()), true };
	}

	template<class... Args>
	inline std::pair<iterator, bool> emplace(K key, Args&&... args)
	{
		auto it = find(key);
		if(it != data.end())
			return { it, false };

		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), V{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return { std::prev(data.end()), true };
	}

	//can only be called when it is known ahead of time that the key is not contained
	template<class... Args>
	inline iterator EmplaceUnique(K key, Args&&... args)
	{
		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), V{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return std::prev(data.end());
	}

private:
	std::vector<std::pair<K, V>> data;
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
