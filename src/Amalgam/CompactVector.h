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

		vecMemory = AllocateRaw(n);
		T *d = GetDataPointer();

		for(size_type i = 0; i < n; ++i)
			new (d + i) T();

		SetSize(n);
		SetCapacity(n);
	}

	CompactVector(size_type n, const T &value)
	{
		if(n == 0)
			return;

		vecMemory = AllocateRaw(n);
		T *d = GetDataPointer();

		for(size_type i = 0; i < n; ++i)
			new (d + i) T(value);

		SetSize(n);
		SetCapacity(n);
	}

	//range constructor (input iterator)
	template<std::input_iterator InputIt>
	CompactVector(InputIt first, InputIt last)
	{
		size_type n = std::distance(first, last);
		if(n == 0)
			return;

		vecMemory = AllocateRaw(n);
		T *d = GetDataPointer();
		size_type i = 0;
		for(auto it = first; it != last; ++it, ++i)
			new (d + i) T(*it);

		SetSize(n);
		SetCapacity(n);
	}

	CompactVector(const CompactVector &other)
	{
		size_type n = other.size();
		if(n == 0)
			return;

		vecMemory = AllocateRaw(n);
		T *dst = GetDataPointer();
		const T *src = other.GetDataPointer();

		if constexpr(trivially_relocatable_v)
		{
			std::memcpy(dst, src, n * sizeof(T));
		}
		else
		{
			for(size_type i = 0; i < n; ++i)
				new (dst + i) T(src[i]);
		}

		SetSize(n);
		SetCapacity(n);
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
		DestroyAll();
	}

	CompactVector &operator=(const CompactVector &other)
	{
		if(this == &other)
			return *this;

		DestroyAll();
		if(other.size() == 0)
			return *this;

		vecMemory = AllocateRaw(other.size());
		T *dst = GetDataPointer();
		const T *src = other.GetDataPointer();

		if constexpr(trivially_relocatable_v)
		{
			std::memcpy(dst, src, other.size() * sizeof(T));
		}
		else
		{
			for(size_type i = 0; i < other.size(); ++i)
				new (dst + i) T(src[i]);
		}

		SetSize(other.size());
		SetCapacity(other.size());
		return *this;
	}

	CompactVector &operator=(CompactVector &&other) noexcept
	{
		if(this != &other)
		{
			DestroyAll();
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
		return GetDataPointer()[pos];
	}

	inline const_reference operator[](size_type pos) const noexcept
	{
		return GetDataPointer()[pos];
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
		return GetDataPointer();
	}

	inline const T *data() const noexcept
	{
		return GetDataPointer();
	}

	inline bool empty() const noexcept
	{
		return size() == 0;
	}

	inline size_type size() const noexcept
	{
		if(vecMemory == nullptr)
			return 0;

		return reinterpret_cast<Header *>(vecMemory)->size;
	}

	size_type capacity() const noexcept
	{
		if(vecMemory == nullptr)
			return 0;

		return reinterpret_cast<Header *>(vecMemory)->capacity;
	}

	void reserve(size_type new_capacity)
	{
		if(new_capacity <= capacity())
			return;

		void *new_vec_mem = AllocateRaw(new_capacity);
		T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_vec_mem) + sizeof(Header));

		//move existing elements
		T *old_data = GetDataPointer();
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
			DeallocateRaw(vecMemory);
		}

		vecMemory = new_vec_mem;
		SetSize(cur_size);
		SetCapacity(new_capacity);
	}

	void shrink_to_fit()
	{
		size_type cur_size = size();
		if(cur_size == capacity())
			return;

		if(size() == 0)
		{
			DestroyAll();
			return;
		}

		T *old_data = GetDataPointer();
		void *new_vec_mem = AllocateRaw(cur_size);
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
		DeallocateRaw(vecMemory);

		vecMemory = new_vec_mem;
		SetSize(cur_size);
		SetCapacity(cur_size);
	}

	//move‑or‑copy existing elements into a newly allocated buffer
	//new_size is the size for the new buffer
	void resize(size_type new_size, const T &init = T{})
	{
		size_type prev_size = size();
		if(new_size == prev_size)
			return;

		if(new_size > prev_size)
		{
			reserve(new_size);
			T *data = GetDataPointer();
			for(size_type i = prev_size; i < new_size; i++)
				new (data + i) T(init);
		}
		else
		{
			T *data = GetDataPointer();
			DestroyRange(data + new_size, data + prev_size);
		}

		SetSize(new_size);
	}

	void clear() noexcept
	{
		DestroyRange(GetDataPointer(), GetDataPointer() + size());
		SetSize(0);
	}

	template<class... Args>
	reference emplace_back(Args&&... args)
	{
		if(capacity() == 0 || size() == capacity())
		{
			size_type new_cap = NextIncreasedCapacity(capacity());
			reserve(new_cap);
		}

		T *p = GetDataPointer() + size();
		new (p) T(std::forward<Args>(args)...);
		SetSize(size() + 1);
		return *p;
	}

	inline void push_back(const T &value)
	{
		emplace_back(value);
	}

	inline void push_back(T &&value)
	{
		emplace_back(std::move(value));
	}

	void pop_back() noexcept
	{
		if(empty())
			return;

		T *p = GetDataPointer() + size() - 1;
		p->~T();
		SetSize(size() - 1);
	}

	template<class... Args>
	iterator emplace(const_iterator pos, Args&&... args)
	{
		size_type index = 0;

		if(capacity() == 0)
		{
			const size_type new_cap = NextIncreasedCapacity(capacity());
			reserve(new_cap);
		}
		else if(size() == capacity())
		{
			//get index before reallocation
			index = static_cast<size_type>(GetDataPointer() - cbegin());

			const size_type new_cap = NextIncreasedCapacity(capacity());
			reserve(new_cap);
		}

		T *data = GetDataPointer();
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

		SetSize(size() + 1);
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
				new_cap = NextIncreasedCapacity(new_cap);
			reserve(new_cap);
		}

		T *data = GetDataPointer();
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

		SetSize(new_size);
		return data + index;
	}

	template<std::input_iterator InputIt>
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
				new_cap = NextIncreasedCapacity(new_cap);
			reserve(new_cap);
		}

		T *data = GetDataPointer();
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

		SetSize(new_size);
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
				DestroyRange(GetDataPointer() + count, GetDataPointer() + size());

			//construct/assign the first `count` elements
			T *dst = GetDataPointer();
			for(size_type i = 0; i < count; i++)
			{
				//reuse or construct as appropriate
				if(i < size())
					dst[i] = value;
				else
					new (dst + i) T(value);
			}

			SetSize(count);
		}
		else //need a bigger buffer, so destroy and create anew
		{
			DestroyAll();

			void *new_mem = AllocateRaw(count);
			T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_mem) + sizeof(Header));

			for(size_type i = 0; i < count; ++i)
				new (new_data + i) T(value);

			vecMemory = new_mem;
			SetSize(count);
			SetCapacity(count);
		}
	}

	template<std::input_iterator InputIt>
	void assign(InputIt first, InputIt last)
	{
		const size_type new_size = static_cast<size_type>(std::distance(first, last));

		if(new_size <= capacity())
		{
			// destroy excess elements, then copy/assign into the existing buffer
			if(new_size < size())
				DestroyRange(GetDataPointer() + new_size, GetDataPointer() + size());

			T *dst = GetDataPointer();
			size_type i = 0;
			for(InputIt it = first; it != last; ++it, ++i)
			{
				if(i < size())
					dst[i] = *it;          // reuse already‑constructed slot
				else
					new (dst + i) T(*it);   // construct fresh slot
			}

			SetSize(new_size);
		}
		else //need a bigger buffer, so destroy and create anew
		{
			DestroyAll();

			void *new_mem = AllocateRaw(new_size);
			T *new_data = reinterpret_cast<T *>(static_cast<char *>(new_mem) + sizeof(Header));

			size_type i = 0;
			for(InputIt it = first; it != last; ++it, ++i)
				new (new_data + i) T(*it);

			vecMemory = new_mem;
			SetSize(new_size);
			SetCapacity(new_size);
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

		T *data = GetDataPointer();
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

		SetSize(size() - 1);
		return data + index;
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		const size_type first_index = static_cast<size_type>(first - cbegin());
		const size_type last_index = static_cast<size_type>(last - cbegin());

		if(first_index >= last_index)
			return const_cast<iterator>(first);

		T *data = GetDataPointer();

		//destroy the elements to be removed
		DestroyRange(data + first_index, data + last_index);

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

		SetSize(size() - shift_cnt);
		return data + first_index;
	}


	inline iterator begin() noexcept
	{
		return GetDataPointer();
	}

	inline const_iterator begin() const noexcept
	{
		return GetDataPointer();
	}

	inline const_iterator cbegin() const noexcept
	{
		return GetDataPointer();
	}

	inline iterator end() noexcept
	{
		return GetDataPointer() + size();
	}

	inline const_iterator end() const noexcept
	{
		return GetDataPointer() + size();
	}

	inline const_iterator cend() const noexcept
	{
		return GetDataPointer() + size();
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

	inline void SetSize(size_type n) noexcept
	{
		if(vecMemory != nullptr)
			reinterpret_cast<Header *>(vecMemory)->size = n;
	}

	inline void SetCapacity(size_type capacity) noexcept
	{
		if(vecMemory != nullptr)
			reinterpret_cast<Header *>(vecMemory)->capacity = capacity;
	}

	//pointer to the first element
	inline T *GetDataPointer() noexcept
	{
		if(vecMemory == nullptr)
			return nullptr;

		//data starts after the header
		return reinterpret_cast<T *>(static_cast<char *>(vecMemory) + sizeof(Header));
	}

	inline const T *GetDataPointer() const noexcept
	{
		return const_cast<CompactVector *>(this)->GetDataPointer();
	}

	inline static void *AllocateRaw(size_type cap)
	{
		size_type total = sizeof(Header) + cap * sizeof(T);

		size_type align = std::max(align_of_T, header_align);
		void *p = ::operator new(total, std::align_val_t{ align });
		return p;
	}

	inline static void DeallocateRaw(void *ptr) noexcept
	{
		if(ptr == nullptr)
			return;

		size_type align = std::max(align_of_T, sizeof(Header));
		::operator delete(ptr, std::align_val_t{ align });
	}

	//destroy elements in range [first, last)
	inline void DestroyRange(T *first, T *last) noexcept
	{
		if constexpr(!std::is_trivially_destructible_v<T>)
		{
			for(T *p = first; p != last; ++p)
				p->~T();
		}
	}

	inline void DestroyAll() noexcept
	{
		if(vecMemory == nullptr)
			return;

		T *p = GetDataPointer();
		DestroyRange(p, p + size());
		DeallocateRaw(vecMemory);

		vecMemory = nullptr;
	}

	inline size_type NextIncreasedCapacity(size_type cur_capacity)
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
