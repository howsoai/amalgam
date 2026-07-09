#pragma once
#include <type_traits>

//framework for enabling class enums to work with bitmasks

template<typename T> struct IsBitmaskEnum : std::false_type
{};

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T> operator|(T lhs, T rhs)
{
	using UT = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<UT>(lhs) | static_cast<UT>(rhs));
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T &> operator|=(T &lhs, T rhs)
{
	lhs = lhs | rhs;
	return lhs;
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T> operator&(T lhs, T rhs)
{
	using UT = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<UT>(lhs) & static_cast<UT>(rhs));
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T &> operator&=(T &lhs, T rhs)
{
	lhs = lhs & rhs;
	return lhs;
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T> operator^(T lhs, T rhs)
{
	using UT = std::underlying_type_t<T>;
	return static_cast<T>(static_cast<UT>(lhs) ^ static_cast<UT>(rhs));
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T &> operator^=(T &lhs, T rhs)
{
	lhs = lhs ^ rhs;
	return lhs;
}

template<typename T> std::enable_if_t<IsBitmaskEnum<T>::value, T> operator~(T val)
{
	using UT = std::underlying_type_t<T>;
	return static_cast<T>(~static_cast<UT>(val));
}

template<typename T>
constexpr bool IsSet(T value, T mask)
{
	using UT = std::underlying_type_t<T>;
	return (static_cast<UT>(value) & static_cast<UT>(mask)) != 0;
}
