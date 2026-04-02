#pragma once
#include <cstddef>
#include <new>
#include <type_traits>
#include <iterator>
#include <stdexcept>

//holds raw, correctly-aligned storage for N objects of type T
//does *not* invoke any constructors or destructors automatically
//provides a minimal std::array-like API (operator[], size(), begin(),
//end(), data())
template <class T, std::size_t N>
class UninitializedArray
{
public:
	UninitializedArray()
	{ }

	~UninitializedArray()
	{ }

	using value_type = T;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;

	static constexpr size_type size() noexcept
	{
		return N;
	}
	static constexpr bool      empty() noexcept
	{
		return N == 0;
	}

	constexpr reference operator[](size_type i) noexcept
	{
		return *std::launder(Location(storage, i));
	}

	constexpr const_reference operator[](size_type i) const noexcept
	{
		return *std::launder(Location(const_cast<std::byte *>(storage), i));
	}

	constexpr reference at(size_type i)
	{
		if(i >= N)
			throw std::out_of_range("UninitializedArray::at");
		return (*this)[i];
	}
	constexpr const_reference at(size_type i) const
	{
		if(i >= N)
			throw std::out_of_range("UninitializedArray::at");
		return (*this)[i];
	}

	constexpr pointer data() noexcept
	{
		return std::launder(Location(storage, 0));
	}

	constexpr const_pointer data() const noexcept
	{
		return std::launder(Location(const_cast<std::byte *>(storage), 0));
	}

	constexpr pointer begin() noexcept
	{
		return data();
	}
	constexpr const_pointer begin() const noexcept
	{
		return data();
	}
	constexpr const_pointer cbegin() const noexcept
	{
		return data();
	}

	constexpr pointer end() noexcept
	{
		return data() + N;
	}
	constexpr const_pointer end() const noexcept
	{
		return data() + N;
	}
	constexpr const_pointer cend() const noexcept
	{
		return data() + N;
	}

	template <class... Args>
	void Construct(size_type i, Args&&... args)
	{
		::new (static_cast<void *>(Location(storage, i)))
			T(std::forward<Args>(args)...);
	}

	void Destroy(size_type i) noexcept
	{
		(*this)[i].~T();
	}

	void DestroyAll() noexcept
	{
		if constexpr(!std::is_trivially_destructible_v<T>)
		{
			for(size_type i = 0; i < N; ++i)
				Destroy(i);
		}
	}

	UninitializedArray(const UninitializedArray &) = delete;
	UninitializedArray &operator=(const UninitializedArray &) = delete;
	UninitializedArray(UninitializedArray &&) = delete;
	UninitializedArray &operator=(UninitializedArray &&) = delete;

protected:

	static constexpr T *Location(std::byte *base, std::size_t i) noexcept
	{
		return reinterpret_cast<T *>(base + i * sizeof(T));
	}

	alignas(T) std::byte storage[N * sizeof(T)];
};
