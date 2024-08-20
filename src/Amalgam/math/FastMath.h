#pragma once

//system headers:
#include <limits>
#include <cstdint>
#include <cstdlib>

#if !defined(_MSC_VER)
#define __forceinline __attribute__((always_inline)) inline
#endif

#include "FastEMath.h"

//On some platforms, std::isnan creates a costly function call.  This is correct and at least as fast or faster.
template<typename T>
constexpr bool FastIsNaN(const T n)
{
	return n != n;
}

//returns true if both are equal, also counting both being NaN
template<typename T>
constexpr bool EqualIncludingNaN(const T a, const T b)
{
	return (a == b) || (FastIsNaN(a) && FastIsNaN(b));
}

//like EqualIncludingNaN, but for containers that require an object
class DoubleNanHashComparator
{
public:
	constexpr bool operator()(const double a, double b) const
	{
		return EqualIncludingNaN(a, b);
	}
};

//raises base to a nonnegative integer exponent
__forceinline double FastPowIntegerNonNegativeExp(double base, int64_t exponent)
{
	double r = 1.0;
	while(exponent != 0)
	{
		if((exponent & 1) != 0)
			r *= base;

		base *= base;
		exponent >>= 1;
	}
	return r;
}

__forceinline double FastPowApplyFractionalPartOfExponent(double value_raised_to_integer_power, double base, double fraction_part_of_exponent)
{
	int64_t base_as_raw_int = *(reinterpret_cast<int64_t *>(&base));
	int64_t result_as_raw_int = static_cast<int64_t>((fraction_part_of_exponent * (base_as_raw_int - 4606921280493453312LL)) + 4606921280493453312LL);
	return value_raised_to_integer_power * (*reinterpret_cast<double *>(&result_as_raw_int));
}

//Same as FastPow() but assumes the exponent is not zero 
// note: no need to check if exponent==0 since we don't use FastPow in p=0 flow, and we never allow negative base since we always pass in the abs diff
inline double FastPowNonZeroExp(double base, double exponent)
{
	if(base == 0.0)
		return 0;
	
	if(exponent >= 0)
	{
		//find the fraction of the exponent
		int64_t abs_int_exp = static_cast<int64_t>(exponent);
		double fraction_part_of_exponent = exponent - abs_int_exp;

		double r = FastPowIntegerNonNegativeExp(base, abs_int_exp);
		if(fraction_part_of_exponent == 0.0)
			return r;

		return FastPowApplyFractionalPartOfExponent(r, base, fraction_part_of_exponent);
	}
	else //negative exponent
	{
		//not a common value, so only check if we already know the exponent is negative
		if(exponent == -std::numeric_limits<double>::infinity())
			return 0;

		exponent = -exponent;

		//find the fraction of the exponent
		int64_t abs_int_exp = static_cast<int64_t>(exponent);
		double fraction_part_of_exponent = exponent - abs_int_exp;

		double r = FastPowIntegerNonNegativeExp(base, abs_int_exp);
		if(fraction_part_of_exponent != 0.0)
			r = FastPowApplyFractionalPartOfExponent(r, base, fraction_part_of_exponent);

		return 1.0 / r;
	}
}

//faster but less accurate replacement for std::pow
// based on the algorithm outlined by Martin Ankerl on his blog posts here:
// https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/
// and https://martin.ankerl.com/2007/02/11/optimized-exponential-functions-for-java/
// which are based on the exponential approximation in the paper
// Schraudolph, Nicol N. "A fast, compact approximation of the exponential function." Neural Computation 11.4 (1999): 853-862.
// where pow is found by solving and optimizing the ln and exp functions in the paper via a^b = e^(ln(a^b)) = e^(ln(a) * b)
// also improves the approximation (at a cost of speed) by using exponentiation by squaring
// because the results appear to be monotonic and relatively close for a wide range of values, including small and larg exponents
// this seems to be acceptable for many calculations
inline double FastPow(double base, double exponent)
{
	if(base == 0.0)
		return 0.0;
	if(exponent == 0.0)
		return 1.0;
	if(base < 0 && std::abs(exponent) < 1)
		return std::numeric_limits<double>::quiet_NaN();

	return FastPowNonZeroExp(base, exponent);
}

//fast replacement for std::pow, optimized for raising many numbers
//to the same exponent
class RepeatedFastPow
{
public:
	inline RepeatedFastPow()
	{
		SetExponent(1.0);
	}

	inline RepeatedFastPow(double _exponent)
	{
		SetExponent(_exponent);
	}

	inline void SetExponent(double _exponent)
	{
		exponent = _exponent;

		double abs_exponent = std::abs(exponent);
		
		absoluteIntegerExponent = static_cast<int64_t>(abs_exponent);
		fractionPartOfExponent = abs_exponent - absoluteIntegerExponent;
	}

	inline double FastPow(double base)
	{
		if(base == 0.0)
			return 0.0;
		if(exponent == 0.0)
			return 1.0;
		if(base < 0 && std::abs(exponent) < 1)
			return std::numeric_limits<double>::quiet_NaN();

		return FastPowNonZeroExpNonzeroBase(base);
	}

	//FastPow but when the exponent is known to be nonzero and the base is nonnegative
	inline double FastPowNonZeroExpNonnegativeBase(double base)
	{
		if(base == 0.0)
			return 0.0;

		return FastPowNonZeroExpNonzeroBase(base);
	}

protected:

	inline double FastPowNonZeroExpNonzeroBase(double base)
	{
		if(exponent >= 0)
		{
			double r = FastPowIntegerNonNegativeExp(base, absoluteIntegerExponent);
			if(fractionPartOfExponent == 0.0)
				return r;

			return FastPowApplyFractionalPartOfExponent(r, base, fractionPartOfExponent);
		}
		else //negative exponent
		{
			//not a common value, so only check if we already know the exponent is negative
			if(exponent == -std::numeric_limits<double>::infinity())
				return 0;

			double r = FastPowIntegerNonNegativeExp(base, absoluteIntegerExponent);
			if(fractionPartOfExponent != 0.0)
				r = FastPowApplyFractionalPartOfExponent(r, base, fractionPartOfExponent);

			return 1.0 / r;
		}
	}

	double exponent;
	int64_t absoluteIntegerExponent;
	double fractionPartOfExponent;
};
