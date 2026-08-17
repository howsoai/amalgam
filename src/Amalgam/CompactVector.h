#pragma once
#include <cstring>
#include <utility>
#include <iterator>
#include <type_traits>
#include <stdexcept>

template<class T>
class CompactVector
{
public:
	//type aliases
	using value_type = T;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = T *;
	using const_iterator = const T *;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	CompactVector() noexcept = default;

	explicit CompactVector(size_type n)
	{
		if(n == 0)
			return;

		vecMemory = allocate_raw(n);
		T *d = data_ptr();

		for(size_type i = 0; i < n; ++i)
			new (d + i) T();

		set_size(n);
		set_capacity(n);
	}

	CompactVector(size_type n, const T &value)
	{
		if(n == 0)
			return;

		vecMemory = allocate_raw(n);
		T *d = data_ptr();

		for(size_type i = 0; i < n; ++i)
			new (d + i) T(value);

		set_size(n);
		set_capacity(n);
	}

	//range constructor (input iterator)
	template<class InputIt,
		class = typename std::enable_if<
		std::is_base_of_v<std::input_iterator_tag,
		typename std::iterator_traits<InputIt>::iterator_category>
	>::type>
	CompactVector(InputIt first, InputIt last)
	{
		size_type n = std::distance(first, last);
		if(n == 0)
			return;

		vecMemory = allocate_raw(n, large);
		T *d = data_ptr();
		size_type i = 0;
		for(auto it = first; it != last; ++it, ++i)
			new (d + i) T(*it);

		set_size(n);
		set_capacity(n, large);
	}

	CompactVector(const CompactVector &other)
	{
		size_type n = other.size();
		if(n == 0)
			return;

		vecMemory = allocate_raw(n);
		T *dst = data_ptr();
		const T *src = other.data_ptr();

		if constexpr(trivially_relocatable_v)
		{
			std::memcpy(dst, src, n * sizeof(T));
		}
		else
		{
			for(size_type i = 0; i < n; ++i)
				new (dst + i) T(src[i]);
		}

		set_size(n);
		set_capacity(n);
	}

	CompactVector(CompactVector &&other) noexcept
	{
		vecMemory = other.vecMemory;
		other.vecMemory = nullptr;
	}

	inline CompactVector(std::initializer_list<T> init)
	{
		assign(init.begin(), init.end());
	}

	~CompactVector()
	{
		destroy_all();
	}

	CompactVector &operator=(const CompactVector &other)
	{
		if(this == &other)
			return *this;

		destroy_all();
		if(other.size() == 0)
			return *this;

		vecMemory = allocate_raw(other.size());
		T *dst = data_ptr();
		const T *src = other.data_ptr();

		if constexpr(trivially_relocatable_v)
		{
			std::memcpy(dst, src, other.size() * sizeof(T));
		}
		else
		{
			for(size_type i = 0; i < other.size(); ++i)
				new (dst + i) T(src[i]);
		}

		set_size(other.size());
		set_capacity(other.size());
		return *this;
	}

	CompactVector &operator=(CompactVector &&other) noexcept
	{
		if(this != &other)
		{
			destroy_all();
			vecMemory = other.vecMemory;
			other.vecMemory = nullptr;
		}

		return *this;
	}

	inline CompactVector &operator=(std::initializer_list<T> init)
	{
		assign(init.begin(), init.end());
	}

	inline reference operator[](size_type pos) noexcept
	{
		return data_ptr()[pos];
	}

	inline const_reference operator[](size_type pos) const noexcept
	{
		return data_ptr()[pos];
	}

	inline reference at(size_type pos)
	{
		if(pos >= size())
			throw std::out_of_range("CompactVector::at");

		return (*this)[pos];
	}

	inline const_reference at(size_type pos) const
	{
		if(pos >= size())
			throw std::out_of_range("CompactVector::at");

		return (*this)[pos];
	}

	inline reference front() noexcept
	{
		return (*this)[0];
	}

	inline const_reference front() const noexcept
	{
		return (*this)[0];
	}

	inline reference back() noexcept
	{
		return (*this)[size() - 1];
	}

	inline const_reference back() const noexcept
	{
		return (*this)[size() - 1];
	}

	inline T *data() noexcept
	{
		return data_ptr();
	}

	inline const T *data() const noexcept
	{
		return data_ptr();
	}

	inline bool empty() const noexcept
	{
		return size() == 0;
	}

	inline size_type size() const noexcept
	{
		if(!vecMemory)
			return 0;

		return reinterpret_cast<Header *>(vecMemory)->size;
	}

	size_type capacity() const noexcept
	{
		if(!vecMemory)
			return 0;

		return reinterpret_cast<Header *>(vecMemory)->capacity;
	}

	void reserve(size_type new_capacity)
	{
		if(new_capacity <= capacity())
			return;

		void *new_vec_mem = allocate_raw(new_capacity);
		T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_vec_mem) + sizeof(Header));

		//move existing elements
		T *old_data = data_ptr();
		size_type cur_size = size();

		if(old_data != nullptr)
		{
			if constexpr(trivially_relocatable_v)
			{
				std::memcpy(new_data, old_data, cur_size * sizeof(T));
			}
			else
			{
				for(size_type i = 0; i < cur_size; i++)
					new (new_data + i) T(std::move(old_data[i]));
			}
			deallocate_raw(vecMemory);
		}

		vecMemory = new_vec_mem;
		set_size(cur_size);
		set_capacity(new_capacity);
	}

	void shrink_to_fit()
	{
		size_type cur_size = size();
		if(cur_size == capacity())
			return;

		if(size() == 0)
		{
			destroy_all();
			return;
		}

		T *old_data = data_ptr();
		void *new_vec_mem = allocate_raw(cur_size);
		T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_vec_mem) + sizeof(Header));

		if constexpr(trivially_relocatable_v)
		{
			std::memcpy(new_data, old_data, cur_size * sizeof(T));
		}
		else
		{
			for(size_type i = 0; i < cur_size; i++)
				new (new_data + i) T(std::move(old_data[i]));
		}
		deallocate_raw(vecMemory);

		vecMemory = new_vec_mem;
		set_size(cur_size);
		set_capacity(cur_size);
	}

	//move‑or‑copy existing elements into a newly allocated buffer
	//new_size is the size for the new buffer
	void resize(size_type new_size, const T &init = T{})
	{
		size_type prev_size = size();
		if(new_size == prev_size)
			return;

		reserve(new_size);

		T *data = reinterpret_cast<T *>(static_cast<char *>(vecMemory) + sizeof(Header));

		if(new_size > prev_size)
		{
			for(size_type i = prev_size; i < new_size; i++)
				new (data + i) T(init);
		}
		else
		{
			if constexpr(!trivially_relocatable_v)
				destroy_range(data + new_size, data + prev_size);
		}

		set_size(new_size);
	}

	void clear() noexcept
	{
		destroy_range(data_ptr(), data_ptr() + size());
		set_size(0);
	}

	void push_back(const T &value)
	{
		if(capacity() == 0 || size() == capacity())
		{
			size_type new_cap = new_increased_capacity(capacity());
			reserve(new_cap);
		}

		new (data_ptr() + size()) T(value);
		set_size(size() + 1);
	}

	void push_back(T &&value)
	{
		if(capacity() == 0 || size() == capacity())
		{
			size_type new_cap = new_increased_capacity(capacity());
			reserve(new_cap);
		}

		new (data_ptr() + size()) T(std::move(value));
		set_size(size() + 1);
	}

	template<class... Args>
	reference emplace_back(Args&&... args)
	{
		if(capacity() == 0 || size() == capacity())
		{
			size_type new_cap = new_increased_capacity(capacity());
			reserve(new_cap);
		}

		T *p = data_ptr() + size();
		new (p) T(std::forward<Args>(args)...);
		set_size(size() + 1);
		return *p;
	}

	void pop_back() noexcept
	{
		if(empty())
			return;

		T *p = data_ptr() + size() - 1;
		p->~T();
		set_size(size() - 1);
	}

	template<class... Args>
	iterator emplace(const_iterator pos, Args&&... args)
	{
		size_type index = 0;

		if(capacity() == 0)
		{
			const size_type new_cap = new_increased_capacity(capacity());
			reserve(new_cap);
		}
		else if(size() == capacity())
		{
			//get index before reallocation
			index = static_cast<size_type>(data_ptr() - cbegin());

			const size_type new_cap = new_increased_capacity(capacity());
			reserve(new_cap);
		}

		T *data = data_ptr();
		T *insertion_point = data + index;

		if(index == size())
		{
			new (insertion_point) T(std::forward<Args>(args)...);
		}
		else
		{
			//construct the last entry as an empty value
			new (data + size()) T(std::move(data[size() - 1]));

			//shift everything above insertion_point one position to the right
			if constexpr(trivially_relocatable_v)
			{
				std::memmove(insertion_point + 1, insertion_point, (size() - index - 1) * sizeof(T));
			}
			else
			{
				for(size_type i = size() - 1; i > index; --i)
					data[i] = std::move(data[i - 1]);
			}

			//construct the new element in place
			insertion_point->~T();
			new (insertion_point) T(std::forward<Args>(args)...);
		}

		set_size(size() + 1);
		return data + index;
	}

	inline iterator insert(const_iterator pos, const T &value)
	{
		return emplace(pos, value);
	}

	inline iterator insert(const_iterator pos, T &&value)
	{
		return emplace(pos, std::move(value));
	}

	iterator insert(const_iterator pos, size_type count, const T &value)
	{
		if(count == 0)
			return const_cast<iterator>(pos);

		const size_type index = static_cast<size_type>(pos - cbegin());
		const size_type old_size = size();
		const size_type new_size = old_size + count;

		if(new_size > capacity())
		{
			size_type new_cap = capacity();
			while(new_cap < new_size)
				new_cap = new_increased_capacity(new_cap);
			reserve(new_cap);
		}

		T *data = data_ptr();
		T *insertion_point = data + index;

		//move the tail far enough to make space for the new range
		if(index < old_size)
		{
			if constexpr(trivially_relocatable_v)
			{
				std::memmove(insertion_point + count, insertion_point, (old_size - index) * sizeof(T));
			}
			else
			{
				for(size_type i = old_size; i > index; --i)
					new (data + i + count - 1) T(std::move(data[i - 1]));
			}
		}

		//construct the inserted copies
		for(size_type i = 0; i < count; ++i)
			new (insertion_point + i) T(value);

		set_size(new_size);
		return data + index;
	}

	template<class InputIt,
		class = typename std::enable_if<std::is_base_of_v<std::input_iterator_tag,
			typename std::iterator_traits<InputIt>::iterator_category>
		>::type>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		const size_type count = static_cast<size_type>(std::distance(first, last));
		if(count == 0)
			return const_cast<iterator>(pos);

		const size_type index = static_cast<size_type>(pos - cbegin());
		const size_type old_size = size();
		const size_type new_size = old_size + count;

		if(new_size > capacity())
		{
			size_type new_cap = capacity();
			while(new_cap < new_size)
				new_cap = new_increased_capacity(new_cap);
			reserve(new_cap);
		}

		T *data = data_ptr();
		T *insertion_point = data + index;

		//move existing tail rightwards
		if(index < old_size)
		{
			if constexpr(trivially_relocatable_v)
			{
				std::memmove(insertion_point + count, insertion_point, (old_size - index) * sizeof(T));
			}
			else
			{
				for(size_type i = old_size; i > index; --i)
					new (data + i + count - 1) T(std::move(data[i - 1]));
			}
		}

		//copy‑construct the new range
		T *dst = insertion_point;
		for(InputIt it = first; it != last; ++it, ++dst)
			new (dst) T(*it);

		set_size(new_size);
		return data + index;
	}

	iterator insert(const_iterator pos, std::initializer_list<T> init)
	{
		return insert(pos, init.begin(), init.end());
	}

	void assign(size_type count, const T &value)
	{
		if(count <= capacity())
		{
			//destroy any existing elements beyond the new size
			if(count < size())
				destroy_range(data_ptr() + count, data_ptr() + size());

			//construct/assign the first `count` elements
			T *dst = data_ptr();
			for(size_type i = 0; i < count; i++)
			{
				//reuse or construct as appropriate
				if(i < size())
					dst[i] = value;
				else
					new (dst + i) T(value);
			}

			set_size(count);
		}
		else //need a bigger buffer, so destroy and create anew
		{
			destroy_all();

			void *new_mem = allocate_raw(count);
			T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_mem) + sizeof(Header));

			for(size_type i = 0; i < count; ++i)
				new (new_data + i) T(value);

			vecMemory = new_mem;
			set_size(count);
			set_capacity(count);
		}
	}

	template<class InputIt,
		class = typename std::enable_if<std::is_base_of_v<std::input_iterator_tag,
			typename std::iterator_traits<InputIt>::iterator_category>
		>::type>
	void assign(InputIt first, InputIt last)
	{
		const size_type new_size = static_cast<size_type>(std::distance(first, last));

		if(new_size <= capacity())
		{
			// destroy excess elements, then copy/assign into the existing buffer
			if(new_size < size())
				destroy_range(data_ptr() + new_size, data_ptr() + size());

			T *dst = data_ptr();
			size_type i = 0;
			for(InputIt it = first; it != last; ++it, ++i)
			{
				if(i < size())
					dst[i] = *it;          // reuse already‑constructed slot
				else
					new (dst + i) T(*it);   // construct fresh slot
			}

			set_size(new_size);
		}
		else //need a bigger buffer, so destroy and create anew
		{
			destroy_all();

			void *new_mem = allocate_raw(new_size);
			T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_mem) + sizeof(Header));

			size_type i = 0;
			for(InputIt it = first; it != last; ++it, ++i)
				new (new_data + i) T(*it);

			vecMemory = new_mem;
			set_size(new_size);
			set_capacity(new_size);
		}
	}

	inline void assign(std::initializer_list<T> init)
	{
		assign(init.begin(), init.end());
	}

	iterator erase(const_iterator pos)
	{
		const size_type index = static_cast<size_type>(pos - cbegin());
		if(index >= size())
			return end();

		T *data = data_ptr();
		T *target = data + index;

		target->~T();

		//shift tail left
		if(index + 1 < size())
		{
			if constexpr(trivially_relocatable_v)
			{
				std::memmove(target, target + 1, (size() - index - 1) * sizeof(T));
			}
			else
			{
				for(size_type i = index; i + 1 < size(); ++i)
					new (data + i) T(std::move(data[i + 1]));
			}
		}

		set_size(size() - 1);
		return data + index;
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		const size_type first_index = static_cast<size_type>(first - cbegin());
		const size_type last_index = static_cast<size_type>(last - cbegin());

		if(first_index >= last_index)
			return const_cast<iterator>(first);

		T *data = data_ptr();

		//destroy the elements to be removed
		destroy_range(data + first_index, data + last_index);

		const size_type shift_cnt = last_index - first_index;
		const size_type tail_cnt = size() - last_index;

		//move tail leftward
		if(tail_cnt > 0)
		{
			if constexpr(trivially_relocatable_v)
			{
				std::memmove(data + first_index, data + last_index, tail_cnt * sizeof(T));
			}
			else
			{
				for(size_type i = 0; i < tail_cnt; ++i)
					new (data + first_index + i) T(std::move(data + last_index + i));
			}
		}

		set_size(size() - shift_cnt);
		return data + first_index;
	}


	inline iterator begin() noexcept
	{
		return data_ptr();
	}

	inline const_iterator begin() const noexcept
	{
		return data_ptr();
	}

	inline const_iterator cbegin() const noexcept
	{
		return data_ptr();
	}

	inline iterator end() noexcept
	{
		return data_ptr() + size();
	}

	inline const_iterator end() const noexcept
	{
		return data_ptr() + size();
	}

	inline const_iterator cend() const noexcept
	{
		return data_ptr() + size();
	}

	inline reverse_iterator rbegin() noexcept
	{
		return reverse_iterator(end());
	}

	inline const_reverse_iterator rbegin() const noexcept
	{
		return const_reverse_iterator(end());
	}

	inline const_reverse_iterator crbegin() const noexcept
	{
		return const_reverse_iterator(cend());
	}

	inline reverse_iterator rend() noexcept
	{
		return reverse_iterator(begin());
	}

	inline const_reverse_iterator rend() const noexcept
	{
		return const_reverse_iterator(begin());
	}

	inline const_reverse_iterator crend() const noexcept
	{
		return const_reverse_iterator(cbegin());
	}

	inline void swap(CompactVector &other) noexcept
	{
		std::swap(vecMemory, other.vecMemory);
	}

	inline bool operator==(const CompactVector &other) const noexcept
	{
		if(size() != other.size()) return false;
		return std::equal(begin(), end(), other.begin());
	}

	inline bool operator!=(const CompactVector &other) const noexcept
	{
		return !(*this == other);
	}

protected:
	static_assert(!std::is_reference_v<T>, "CompactVector cannot hold references");
	static_assert(!std::is_const_v<T>, "CompactVector cannot hold const types");

	//detect if a type can be memmove‑copied safely
	static constexpr bool trivially_relocatable_v =
		std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>;

	struct Header
	{
		size_type size;
		size_type capacity;
	};

	static constexpr size_type align_of_T = alignof(T);
	static constexpr size_type header_align = alignof(Header);

	inline void set_size(size_type n) noexcept
	{
		if(vecMemory != nullptr)
			reinterpret_cast<Header *>(vecMemory)->size = n;
	}

	inline void set_capacity(size_type capacity) noexcept
	{
		if(vecMemory != nullptr)
			reinterpret_cast<Header *>(vecMemory)->capacity = capacity;
	}

	//pointer to the first element
	inline T *data_ptr() noexcept
	{
		if(!vecMemory)
			return nullptr;

		//data starts after the header
		return reinterpret_cast<T *>(static_cast<char *>(vecMemory) + sizeof(Header));
	}

	inline const T *data_ptr() const noexcept
	{
		return const_cast<CompactVector *>(this)->data_ptr();
	}

	inline static void *allocate_raw(size_type cap)
	{
		size_type total = sizeof(Header) + cap * sizeof(T);

		size_type align = std::max(align_of_T, header_align);
		void *p = ::operator new(total, std::align_val_t{ align });
		return p;
	}

	inline static void deallocate_raw(void *ptr) noexcept
	{
		if(!ptr)
			return;

		size_type align = std::max(align_of_T, sizeof(Header));
		::operator delete(ptr, std::align_val_t{ align });
	}

	//destroy elements in range [first, last)
	inline void destroy_range(T *first, T *last) noexcept
	{
		if constexpr(!std::is_trivially_destructible_v<T>)
		{
			for(T *p = first; p != last; ++p)
				p->~T();
		}
	}

	inline void destroy_all() noexcept
	{
		if(vecMemory == nullptr)
			return;

		T *p = data_ptr();
		destroy_range(p, p + size());
		deallocate_raw(vecMemory);

		vecMemory = nullptr;
	}

	inline size_type new_increased_capacity(size_type cur_capacity)
	{
		//increase capacity by just over golden ratio, rounded up to next integer
		return cur_capacity == 0 ? 1
			: cur_capacity + (cur_capacity >> 1) + 1;
	}

	//pointer to memory with the header and vector, nullptr if empty
	void *vecMemory = nullptr;
};

template<class T>
inline auto begin(CompactVector<T> &c) -> typename CompactVector<T>::iterator
{
	return c.begin();
}

template<class T>
inline auto begin(const CompactVector<T> &c) -> typename CompactVector<T>::const_iterator
{
	return c.begin();
}

template<class T>
inline auto end(CompactVector<T> &c) -> typename CompactVector<T>::iterator
{
	return c.end();
}

template<class T>
inline auto end(const CompactVector<T> &c) -> typename CompactVector<T>::const_iterator
{
	return c.end();
}

template<class T>
inline auto cbegin(const CompactVector<T> &c) -> typename CompactVector<T>::const_iterator
{
	return c.cbegin();
}
template<class T>
inline auto cend(const CompactVector<T> &c) -> typename CompactVector<T>::const_iterator
{
	return c.cend();
}

template<class T>
inline auto rbegin(CompactVector<T> &c) -> typename CompactVector<T>::reverse_iterator
{
	return c.rbegin();
}

template<class T>
inline auto rbegin(const CompactVector<T> &c) -> typename CompactVector<T>::const_reverse_iterator
{
	return c.rbegin();
}

template<class T>
inline auto rend(CompactVector<T> &c) -> typename CompactVector<T>::reverse_iterator
{
	return c.rend();
}

template<class T>
inline auto rend(const CompactVector<T> &c) -> typename CompactVector<T>::const_reverse_iterator
{
	return c.rend();
}

template<class T>
inline auto crbegin(const CompactVector<T> &c) -> typename CompactVector<T>::const_reverse_iterator
{
	return c.crbegin();
}

template<class T>
inline auto crend(const CompactVector<T> &c) -> typename CompactVector<T>::const_reverse_iterator
{
	return c.crend();
}
