//system includes
#include <vector>

//implements a map via a vector, where entries are looked up sequentially for brute force
//useful for standing in for hash maps when the data is very small (generally less than 20 entries)
// and for hash maps where entries are only iterated over or found once
//note that, like flat hash maps, iterators may be invalidated when the map is altered
template<typename K, typename V, typename E = std::equal_to<K>>
class SmallMap
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

	inline auto begin()
	{
		return data.begin();
	}

	inline auto end()
	{
		return data.end();
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

	inline const_iterator find(const K &key) const
	{
		return std::find_if(data.begin(), data.end(),
			[&](const std::pair<K, V> &p) { return E{}(p.first, key); });
	}

	inline iterator find(const K &key)
	{
		return std::find_if(data.begin(), data.end(),
			[&](const std::pair<K, V> &p) { return E{}(p.first, key); });
	}

	inline bool contains(const K &key) const
	{
		return find(key) != data.end();
	}

	inline mapped_type &operator[](const K &key)
	{
		auto it = find(key);
		if(it == end())
		{
			emplace(key);
			return data.back().second;
		}
		return it->second;
	}

	inline mapped_type &at(const K &key)
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("SmallMap::at: key not found");

		return it->second;
	}

	const mapped_type &at(const K &key) const
	{
		auto it = find(key);
		if(it == end())
			throw std::out_of_range("SmallMap::at: key not found");

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

	std::pair<iterator, iterator> equal_range(const K &key)
	{
		auto it = find(key);
		if(it == end())
			return { end(), end() };

		return { it, it };
	}

	inline void clear()
	{
		data.clear();
	}

	inline void swap(SmallMap &other) noexcept
	{
		std::swap(this->data, other.data);
	}

	inline size_type max_size() const noexcept
	{
		return std::numeric_limits<size_type>::max();
	}

	inline size_type bucket_count() const noexcept
	{
		return 1;
	}

	inline size_t bucket(iterator it) const noexcept
	{
		return 0;
	}

	inline float load_factor() const noexcept
	{
		return 1.0f;
	}

	inline float max_load_factor() const noexcept
	{
		return 1.0f;
	}

	template<typename M>
	auto merge(M &&other)
	{
		for(auto &pair : other)
		{
			auto it = find(pair.first);
			if(it == end())
				this->push_back(pair);
			else
				it->second = pair.second;
		}
		return *this;
	}

	inline size_t count(const K &key) const
	{
		return find(key) != end() ? 1 : 0;
	}

	std::pair<iterator, bool> insert(const K &key, const V &value)
	{
		auto it = find(key);
		if(it != end())
			return { it, false };

		auto result = this->emplace_back(key, value);
		return { result.first, true };
	}

	std::pair<iterator, bool> insert(std::initializer_list<value_type> init)
	{
		for(auto &item : init)
		{
			auto it = find(item.first);
			if(it != end())
				continue;

			data.push_back(std::move(item));
		}

		return { data.empty() ? end() : data.end() - 1, true };
	}

	template<class... Args>
	iterator insert_or_assign(K key, Args&&... args)
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
				this->push_back(std::move(key), V{});
			else
				this->push_back(std::move(key), std::forward<Args>(args)...);

			return this->end() - 1;
		}
	}

	template<class... Args>
	inline std::pair<iterator, bool> try_emplace(K &&key, Args&&... args)
	{
		auto it = find(key);
		if(it != end())
			return { it, false };

		if constexpr(sizeof...(Args) == 0)
			this->push_back(std::move(key), V{});
		else
			this->push_back(std::move(key), std::forward<Args>(args)...);

		return { this->end() - 1, true };
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

		return { data.end() - 1, true };
	}

	//can only be called when it is known ahead of time that the key is not contained
	template<class... Args>
	inline iterator EmplaceUnique(K key, Args&&... args)
	{
		if constexpr(sizeof...(Args) == 0)
			data.emplace_back(std::move(key), V{});
		else
			data.emplace_back(std::move(key), std::forward<Args>(args)...);

		return data.end() - 1;
	}

private:
	std::vector<std::pair<K, V>> data;
};

namespace
{
	template<typename K, typename V, typename E>
	inline auto begin(SmallMap<K, V, E> &m)
	{
		return m.begin();
	}

	template<typename K, typename V, typename E>
	inline auto begin(const SmallMap<K, V, E> &m)
	{
		return m.begin();
	}

	template<typename K, typename V, typename E>
	inline auto end(SmallMap<K, V, E> &m)
	{
		return m.end();
	}

	template<typename K, typename V, typename E>
	inline auto end(const SmallMap<K, V, E> &m)
	{
		return m.end();
	}
}
