#pragma once

#include <cmath>
#include <cstring>
#include <utility>

#include "fast_log/src/exp_table.h"

//code below from fast_log library with modifications for error checking and performance
//see fast_log directory for original code and details

inline double FastExp(double x)
{
	if(x != x)
		return std::numeric_limits<double>::quiet_NaN();

	int64_t offset = static_cast<int64_t>(x);
	//x now only contains the fractional part
	x -= offset;

	offset += 710;
	if(offset < 0)
		return 0;
	if(offset >= 1420)
		return std::numeric_limits<double>::infinity();
	
	// Use a 4-part polynomial to approximate exp(x);
	double c[] = { 0.28033708, 0.425302, 1.01273643, 1.00020947 };

	// Use Horner's method to evaluate the polynomial.
	double val = c[3] + x * (c[2] + x * (c[1] + x * (c[0])));
	return val * EXP_TABLE[offset];
}

template <class To, class From>
inline To bit_cast(const From &src)
{
	To dst;
	std::memcpy(&dst, &src, sizeof(To));
	return dst;
}

//returns the exponent and a normalized mantissa with the relationship:
//[a * 2^b] = x
inline std::pair<double, int> FastFrexp(double x)
{
	uint64_t bits = bit_cast<uint64_t, double>(x);
	if(bits == 0)
		return {0., 0};

	// See:
	// https://en.wikipedia.org/wiki/IEEE_754#Basic_and_interchange_formats

	// Extract the 52-bit mantissa field.
	uint64_t mantissa = bits & 0xFFFFFFFFFFFFF;
	bits >>= 52;

	// Extract the 11-bit exponent field, and add the bias.
	int exponent = int(bits & 0x7ff) - 1023;
	bits >>= 11;

	// Extract the sign bit.
	uint64_t sign = bits;
	bits >>= 1;

	// Construct the normalized double;
	uint64_t res = sign;
	res <<= 11;
	res |= 1023 - 1;
	res <<= 52;
	res |= mantissa;

	double frac = bit_cast<double, uint64_t>(res);
	return { frac, exponent + 1 };
}

inline double FastLog(double x)
{
	/// Extract the fraction, and the power-of-two exponent.

	auto a = FastFrexp(x);
	x = a.first;
	int pow2 = a.second;

	// Use a 4-part polynom to approximate log2(x);
	double c[] = { 1.33755322, -4.42852392, 6.30371424, -3.21430967 };
	double log2 = 0.6931471805599453;

	// Use Horner's method to evaluate the polynomial.
	double val = c[3] + x * (c[2] + x * (c[1] + x * (c[0])));

	// Compute log2(x), and convert the result to base-e.
	return log2 * (pow2 + val);
}
