#pragma once

//project headers:
#include "HashMaps.h"

//system headers:
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

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

//Normalizes the vector; if any are infinity, it will equally uniformly normalize over just the infinite values
//p is the Lebesque order, where 1 is Manhattan / probability, 2 is Euclidean, etc.
template<typename ContainerType, typename GetterFunc, typename SetterFunc>
inline void NormalizeVector(ContainerType &vec, double p, GetterFunc getter, SetterFunc setter)
{
	//fast path for p = 1
	if(p == 1.0)
	{
		double total = 0.0;
		size_t inf_count = 0;
		for(auto &item : vec)
		{
			double v = getter(item);
			if(v == std::numeric_limits<double>::infinity())
				inf_count++;
			else
				total += std::abs(v);
		}

		if(inf_count > 0)
		{
			for(auto &item : vec)
			{
				double v = getter(item);
				if(v != std::numeric_limits<double>::infinity())
					setter(item, 0.0);
				else
					setter(item, 1.0);
			}
			double norm = static_cast<double>(inf_count);
			for(auto &item : vec)
			{
				double v = getter(item);
				setter(item, v / norm);
			}
		}
		else
		{
			if(total <= 0.0)
			{
				for(auto &item : vec)
					setter(item, 0.0);
			}
			else
			{
				for(auto &item : vec)
				{
					double v = getter(item);
					setter(item, v / total);
				}
			}
		}
		return;
	}

	//fast path for p = 2
	if(p == 2.0)
	{
		double total = 0.0;
		size_t inf_count = 0;
		for(auto &item : vec)
		{
			double v = getter(item);
			if(v == std::numeric_limits<double>::infinity())
				inf_count++;
			else
				total += v * v;
		}

		if(inf_count > 0)
		{
			for(auto &item : vec)
			{
				double v = getter(item);
				if(v != std::numeric_limits<double>::infinity())
					setter(item, 0.0);
				else
					setter(item, 1.0);
			}
			double norm = std::sqrt(static_cast<double>(inf_count));
			for(auto &item : vec)
			{
				double v = getter(item);
				setter(item, v / norm);
			}
		}
		else
		{
			if(total <= 0.0)
			{
				for(auto &item : vec)
					setter(item, 0.0);
			}
			else
			{
				double norm = std::sqrt(total);
				for(auto &item : vec)
				{
					double v = getter(item);
					setter(item, v / norm);
				}
			}
		}
		return;
	}

	//generic path for other p values
	double total = 0.0;
	size_t inf_count = 0;
	for(auto &item : vec)
	{
		double v = getter(item);
		if(v == std::numeric_limits<double>::infinity())
			inf_count++;
		else
			total += std::pow(std::abs(v), p);
	}

	if(inf_count > 0)
	{
		for(auto &item : vec)
		{
			double v = getter(item);
			if(v != std::numeric_limits<double>::infinity())
				setter(item, 0.0);
			else
				setter(item, 1.0);
		}
		double norm = std::pow(static_cast<double>(inf_count), 1.0 / p);
		for(auto &item : vec)
		{
			double v = getter(item);
			setter(item, v / norm);
		}
	}
	else
	{
		if(total <= 0.0)
		{
			for(auto &item : vec)
				setter(item, 0.0);
		}
		else
		{
			double norm = std::pow(total, 1.0 / p);
			for(auto &item : vec)
			{
				double v = getter(item);
				setter(item, v / norm);
			}
		}
	}
}

template<typename ContainerType>
inline void NormalizeVector(ContainerType &vec, double p)
{
	NormalizeVector(vec, p,
		[](auto &v) { return v; },
		[](auto &v, double new_val) { v = new_val; }
	);
}

template<typename MapType>
inline void NormalizeVectorAsMap(MapType &map, double p)
{
	NormalizeVector(map, p,
		[](const auto &pair) { return pair.second; },
		[](auto &pair, double new_val) { pair.second = new_val; }
	);
}

//computes mode of number values, and returns a tuple of whether a mode has been found,
// and if so, the mode
//iterates from first to last, calling get_value
// if has_weight, then will use get_weight to obtain the weight of each value
template<typename ValueIterator,
	typename ValueType, typename ValueHash, typename ValueEquality, typename ValueFunction, typename WeightFunction>
static std::pair<bool, ValueType> Mode(ValueIterator first, ValueIterator last,
	ValueFunction get_value, bool has_weight, WeightFunction get_weight, ValueType value_if_not_found)
{
	FastHashMap<ValueType, double, ValueHash, ValueEquality> value_weights;

	if(!has_weight)
	{
		for(ValueIterator i = first; i != last; ++i)
		{
			ValueType value = value_if_not_found;
			if(get_value(i, value))
			{
				auto [inserted_value, inserted] = value_weights.emplace(value, 1.0);
				if(!inserted)
					inserted_value->second += 1.0;
			}
		}
	}
	else
	{
		for(ValueIterator i = first; i != last; ++i)
		{
			ValueType value = value_if_not_found;
			if(get_value(i, value))
			{
				double weight_value = 1.0;
				bool has_weight = get_weight(i, weight_value);
				if(!has_weight)
					weight_value = 1.0;

				auto [inserted_value, inserted] = value_weights.emplace(value, weight_value);
				if(!inserted)
					inserted_value->second += weight_value;
			}
		}
	}

	//find highest value
	bool mode_found = false;
	ValueType mode = value_if_not_found;
	double mode_weight = 0.0;
	for(auto &[value, weight] : value_weights)
	{
		if(weight > mode_weight)
		{
			mode_found = true;
			mode = value;
			mode_weight = weight;
		}
	}

	return std::make_pair(mode_found, mode);
}

//specialization of Mode for std::string
template<typename ValueIterator, typename ValueFunction, typename WeightFunction>
inline static std::pair<bool, std::string> ModeString(ValueIterator first, ValueIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight)
{
	return Mode<ValueIterator, std::string,
		std::hash<std::string>, std::equal_to<std::string>>(first, last,
			get_value, has_weight, get_weight, std::string());
}

//computes the quantile of the values
//iterates from first to last, calling get_value
// if has_weight, then will use get_weight to obtain the weight of each value. Otherwise, weight is 1.
//q_percentage is the quantile percentage to calculate
//values_buffer is a temporary buffer to hold data that can be reused if specified
template<typename ValueIterator, typename ValueFunction, typename WeightFunction>
static double Quantile(ValueIterator first, ValueIterator last,
	ValueFunction get_value, bool has_weight, WeightFunction get_weight, double q_percentage,
	std::vector<std::pair<double, double>> *values_buffer = nullptr)
{
	//invalid range of quantile percentage
	if(FastIsNaN(q_percentage) || q_percentage < 0.0 || q_percentage > 1.0)
		return std::numeric_limits<double>::quiet_NaN();

	//reuse buffer if available, create local one if not
	std::vector<std::pair<double, double>> *value_weights = values_buffer;
	std::vector<std::pair<double, double>> value_weights_local_buffer;
	if(value_weights != nullptr)
		value_weights->clear();
	else
		value_weights = &value_weights_local_buffer;

	double total_weight = 0.0;
	bool eq_or_no_weights = true;

	if(!has_weight)
	{
		for(ValueIterator i = first; i != last; ++i)
		{
			double value = 0.0;
			if(get_value(i, value))
			{
				value_weights->emplace_back(value, 1.0);
				total_weight += 1.0;
			}
		}
	}
	else
	{
		double weight_check = std::numeric_limits<double>::quiet_NaN();

		for(ValueIterator i = first; i != last; ++i)
		{
			double value = 0.0;
			if(get_value(i, value))
			{
				double weight_value = 1.0;
				get_weight(i, weight_value);
				if(!FastIsNaN(weight_value))
				{
					value_weights->emplace_back(value, weight_value);
					total_weight += weight_value;

					//check to see if weights are different
					if(FastIsNaN(weight_check))
						weight_check = weight_value;
					else if(weight_check != weight_value)
						eq_or_no_weights = false;
				}
			}
		}
	}

	//make sure have valid values and weights
	if(value_weights->size() == 0 || total_weight == 0.0)
		return std::numeric_limits<double>::quiet_NaN();

	//sorts on .first - value, not weight
	std::sort(std::begin(*value_weights), std::end(*value_weights));

	//early outs for edge cases
	if(value_weights->size() == 1 || q_percentage == 0.0)
		return value_weights->front().first;
	else if(q_percentage == 1.0)
		return value_weights->back().first;

	//search cumulative density for target quantile
	const double first_cdf_term = 0.5 * value_weights->front().second;
	const double last_cdf_term = total_weight - 0.5 * value_weights->front().second - 0.5 * value_weights->back().second;
	double accum_weight = 0.0;
	double cdf_term_prev = 0.0;
	for(size_t i = 0; i < value_weights->size(); ++i)
	{
		const auto &[curr_value, curr_weight] = (*value_weights)[i];

		//calculate cdf term
		double cdf_term = 0.0;
		accum_weight += (*value_weights)[i].second;
		cdf_term += accum_weight - 0.5 * (*value_weights)[i].second;

		//there are different ways in which to shift and normalize each individual cdf term, all of which
		// produce mathematically correct quantiles (given a quantile is an interval, not a point). To be consistent
		// with popular math packages for equal or no weighting, the normalization is a shift and scale based on the
		// first and last cdf terms. For weighted samples, the standard normalization using total weight is used.
		if(eq_or_no_weights)
		{
			cdf_term -= first_cdf_term;
			cdf_term /= last_cdf_term;
		}
		else
		{
			cdf_term /= total_weight;
		}

		//edge case for setting initial cdf term and returning first
		// value if target quantile is smaller than cdf_term
		if(i == 0)
		{
			cdf_term_prev = cdf_term;
			if(q_percentage <= cdf_term)
				return curr_value;
		}

		//check for found quantile
		if(q_percentage == cdf_term_prev)
			return (*value_weights)[i - 1].first;
		else if(q_percentage == cdf_term)
			return curr_value;
		else if(cdf_term_prev < q_percentage && q_percentage < cdf_term)
		{
			const auto &prev_value = (*value_weights)[i - 1].first;

			//linearly interpolate
			return prev_value + (curr_value - prev_value) * (q_percentage - cdf_term_prev) / (cdf_term - cdf_term_prev);
		}

		cdf_term_prev = cdf_term;
	}

	//if didn't find (quantile percentage larger than last cdf term), use last element
	return value_weights->back().first;
}

//computes the generalized mean of the values where p_value is the parameter for the generalized mean
//center is the center the calculation is around, default is 0.0
//if calculate_moment is true, the final calculation will not be raised to 1/p for p>=1
//if absolute_value is true, the first order mean (p=1) will take the absolute value
//iterates from first to last, calling get_value
// if has_weight, then will use get_weight to obtain the weight of each value
//has separate paths for different values of p_value for efficiency
template<typename ValueIterator, typename ValueFunction, typename WeightFunction>
static double GeneralizedMean(ValueIterator first, ValueIterator last,
	ValueFunction get_value, bool has_weight, WeightFunction get_weight,
	double p_value, double center = 0.0, bool calculate_moment = false,
	bool absolute_value = false)
{
	//deal with edge case of no values
	if(first == last)
		return std::numeric_limits<double>::quiet_NaN();

	double mean = 0.0;

	if(!has_weight)
	{
		size_t num_elements = 0;

		if(p_value == 1.0) // arithmetic
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double diff = value - center;
					mean += (absolute_value ? std::abs(diff) : diff);
					num_elements++;
				}
			}

			mean /= num_elements;
		}
		else if(p_value == 2.0) // root mean square (quadratic)
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					//don't need to worry about absolute value because squaring it will make it positive
					double diff = value - center;
					mean += diff * diff;
					num_elements++;
				}
			}

			mean /= num_elements;
			if(!calculate_moment)
				mean = std::sqrt(mean);
		}
		else if(p_value == 0.0) // geometric
		{
			bool zero_found = false;
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double diff = value - center;
					if(absolute_value)
						diff = std::abs(diff);

					//ignore nonpositive values
					if(diff > 0.0)
					{
						mean += std::log(diff);
						num_elements++;
					}
					else if(diff == 0.0)
					{
						zero_found = true;
						break;
					}
					else
					{
						return std::numeric_limits<double>::quiet_NaN();
					}
				}
			}

			if(zero_found)
			{
				mean = 0.0;
			}
			else
			{
				if(!calculate_moment)
					mean /= num_elements;
				mean = std::exp(mean);
			}
		}
		else if(p_value == -1.0) // harmonic
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double diff = value - center;
					if(absolute_value)
						diff = std::abs(diff);

					mean += (1.0 / diff);
					num_elements++;
				}
			}

			mean /= num_elements;
			if(!calculate_moment)
				mean = (1.0 / mean);
		}
		else
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double diff = value - center;
					if(absolute_value)
						diff = std::abs(diff);

					mean += std::pow(diff, p_value);
					num_elements++;
				}
			}

			mean /= num_elements;
			if(!calculate_moment)
				mean = std::pow(mean, 1.0 / p_value);
		}
	}
	else //use weights
	{
		double weights_sum = 0.0;

		if(p_value == 1.0) // arithmetic
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
					{
						double diff = value - center;
						if(absolute_value)
							diff = std::abs(diff);

						mean += weight_value * diff;
						weights_sum += weight_value;
					}
				}
			}

			//can divide at the end because multiplication is associative and commutative
			mean /= weights_sum;
		}
		else if(p_value == 2.0) // root mean square (quadratic)
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
					{
						double diff = value - center;
						//don't need absolute value of diff because squaring will make it positive
						mean += weight_value * diff * diff;
						weights_sum += weight_value;
					}
				}
			}

			//can divide at the end because multiplication is associative and commutative
			mean /= weights_sum;
			if(!calculate_moment)
				mean = std::sqrt(mean);
		}
		else if(p_value == 0.0) // geometric
		{
			//collect weights total first
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
						weights_sum += weight_value;
				}
			}

			bool zero_found = false;
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
					{
						double diff = value - center;
						if(absolute_value)
							diff = std::abs(diff);

						//ignore nonpositive values
						if(diff > 0.0)
						{
							mean += weight_value * std::log(diff);
						}
						else if(diff == 0.0)
						{
							zero_found = true;
							break;
						}
						else
						{
							return std::numeric_limits<double>::quiet_NaN();
						}
					}
				}
			}

			if(zero_found)
			{
				mean = 0.0;
			}
			else
			{
				if(!calculate_moment)
					mean = std::exp(mean);
			}
		}
		else if(p_value == -1.0) // harmonic
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
					{
						double diff = value - center;
						if(absolute_value)
							diff = std::abs(diff);

						mean += weight_value / diff;
						weights_sum += weight_value;
					}
				}
			}

			//can divide at the end because multiplication is associative and commutative
			mean /= weights_sum;
			if(!calculate_moment)
				mean = (1.0 / mean);
		}
		else
		{
			for(ValueIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					//don't multiply if zero in case value is infinite
					if(weight_value != 0.0)
					{
						double diff = value - center;
						if(absolute_value)
							diff = std::abs(diff);

						mean += weight_value * std::pow(diff, p_value);
						weights_sum += weight_value;
					}
				}
			}

			//can divide at the end because multiplication is associative and commutative
			mean /= weights_sum;
			if(!calculate_moment)
				mean = std::pow(mean, 1.0 / p_value);
		}
	}

	return mean;
}
