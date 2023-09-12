#pragma once

//project headers:
#include "DistanceReferencePair.h"
#include "FastMath.h"

//system headers:
#include <cmath>
#include <vector>

//KL(P||Q) = Sum(p(i) * log( p(i) / q(i) ), natural base
inline double KullbackLeiblerDivergence(const std::vector<double> &p, const std::vector<double> &q)
{
	double sum = 0.0;
	for(size_t i = 0; i < p.size(); i++)
	{
		if(q[i] != 0 && !FastIsNaN(q[i]))
			sum += p[i] ? p[i] * std::log(p[i] / q[i]) : 0;
	}
	return sum;
}

//computes the KL divergence between p and q.distance only for features specified by the indices given by q.reference
//i.e. this will give equivelent value if calling normal KL on p and q if p and q are the same value at indices oother than those in q.reference
//note that there are two versions of this function with the DistanceReferencePair parameters flipped
inline double PartialKullbackLeiblerDivergenceFromIndices(const std::vector<double> &p, const std::vector<DistanceReferencePair<size_t>> &q)
{
	double sum = 0.0;
	for(const auto &changed_contrib : q)
	{
		const double q_i = changed_contrib.distance;
		const double p_i = p[changed_contrib.reference];
		if(q_i != 0 && !FastIsNaN(q_i))
			sum += p_i ? p_i * std::log(p_i / q_i) : 0;
	}
	return sum;
}

//computes the KL divergence between p.distance and q only for features specified by the indices given by p.reference
//i.e. this will give equivelent value if calling normal KL on p and q if p and q are the same value at indices other than those in p.reference
//note that there are two versions of this function with the DistanceReferencePair parameters flipped
inline double PartialKullbackLeiblerDivergenceFromIndices(const std::vector<DistanceReferencePair<size_t>> &p, const std::vector<double> &q)
{
	double sum = 0.0;
	for(const auto &changed_contrib : p)
	{
		const double p_i = changed_contrib.distance;
		const double q_i = q[changed_contrib.reference];
		if(q_i != 0 && !FastIsNaN(q_i))
			sum += p_i ? p_i * std::log(p_i / q_i) : 0;
	}
	return sum;
}
