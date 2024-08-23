#pragma once

//project headers:
#include "FastMath.h"
#include "DistanceReferencePair.h"
#include "HashMaps.h"
#include "Opcodes.h"
#include "StringInternPool.h"

//system headers:
#include <functional>

//Contains templated functions that compute statistical queries on data sets
//If weights are used and are zero, then a zero weight will take precedence over infinite or nan values
class EntityQueriesStatistics
{
public:

	//computes sum of values
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static double Sum(EntityIterator first, EntityIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight)
	{
		double sum = 0.0;

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
					sum += value;
			}
		}
		else
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 0.0;
					if(get_weight(i, weight_value))
					{
						//don't multiply if zero in case value is infinite
						if(weight_value != 0.0)
							sum += weight_value * value;
					}
					else
						sum += value;
				}
			}
		}

		return sum;
	}

	//computes mode of number values, and returns mode
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static double ModeNumber(EntityIterator first, EntityIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight)
	{
		FastHashMap<double, double, std::hash<double>, DoubleNanHashComparator> value_weights;

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					auto [inserted_value, inserted] = value_weights.insert(std::make_pair(value, 1.0));
					if(!inserted)
						inserted_value->second += 1.0;
				}
			}
		}
		else
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					auto [inserted_value, inserted] = value_weights.insert(std::make_pair(value, weight_value));
					if(!inserted)
						inserted_value->second += weight_value;
				}
			}
		}

		//find highest value
		double mode = std::numeric_limits<double>::quiet_NaN();
		double mode_weight = 0.0;
		for(auto &[value, weight] : value_weights)
		{
			if(weight > mode_weight)
			{
				mode = value;
				mode_weight = weight;
			}
		}

		return mode;
	}

	//computes mode of string ids, and returns a tuple of whether a mode has been found,
	// and if so, the mode
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static std::pair<bool, StringInternPool::StringID> ModeStringId(EntityIterator first, EntityIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight)
	{
		FastHashMap<StringInternPool::StringID, double> value_weights;

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				StringInternPool::StringID value = string_intern_pool.NOT_A_STRING_ID;
				if(get_value(i, value))
				{
					auto [inserted_value, inserted] = value_weights.insert(std::make_pair(value, 1.0));
					if(!inserted)
						inserted_value->second += 1.0;
				}
			}
		}
		else
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				StringInternPool::StringID value = string_intern_pool.NOT_A_STRING_ID;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);
					
					auto [inserted_value, inserted] = value_weights.insert(std::make_pair(value, weight_value));
					if(!inserted)
						inserted_value->second += weight_value;
				}
			}
		}

		//find highest value
		bool mode_found = false;
		StringInternPool::StringID mode = string_intern_pool.NOT_A_STRING_ID;
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

	//computes masses (weights) of each numeric value
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static
		FastHashMap<double, double, std::hash<double>, DoubleNanHashComparator>
		ValueMassesNumber(EntityIterator first, EntityIterator last, size_t estimated_num_unique_values,
			ValueFunction get_value, bool has_weight, WeightFunction get_weight)
	{
		FastHashMap<double, double, std::hash<double>, DoubleNanHashComparator> value_masses;
		value_masses.reserve(estimated_num_unique_values);

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					auto [inserted_value, inserted] = value_masses.insert(std::make_pair(value, 1.0));
					if(!inserted)
						inserted_value->second += 1.0;
				}
			}
		}
		else
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					auto [inserted_value, inserted] = value_masses.insert(std::make_pair(value, weight_value));
					if(!inserted)
						inserted_value->second += weight_value;
				}
			}
		}

		return value_masses;
	}

	//computes masses (weights) of each string value
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static
		FastHashMap<StringInternPool::StringID, double>
		ValueMassesStringId(EntityIterator first, EntityIterator last, size_t estimated_num_unique_values,
			ValueFunction get_value, bool has_weight, WeightFunction get_weight)
	{
		FastHashMap<StringInternPool::StringID, double> value_masses;
		value_masses.reserve(estimated_num_unique_values);

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				StringInternPool::StringID value;
				if(get_value(i, value))
				{
					auto [inserted_value, inserted] = value_masses.insert(std::make_pair(value, 1.0));
					if(!inserted)
						inserted_value->second += 1.0;
				}
			}
		}
		else
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				StringInternPool::StringID value;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);

					auto [inserted_value, inserted] = value_masses.insert(std::make_pair(value, weight_value));
					if(!inserted)
						inserted_value->second += weight_value;
				}
			}
		}

		return value_masses;
	}

	//computes the quantile of the values
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value. Otherwise, weight is 1.
	//q_percentage is the quantile percentage to calculate
	//values_buffer is a temporary buffer to hold data that can be reused
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static double Quantile(EntityIterator first, EntityIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight, double q_percentage,
		std::vector<std::pair<double,double>> &values_buffer)
	{
		//invalid range of quantile percentage
		if(FastIsNaN(q_percentage) || q_percentage < 0.0 || q_percentage > 1.0)
			return std::numeric_limits<double>::quiet_NaN();

		std::vector<std::pair<double,double>> &value_weights = values_buffer;
		value_weights.clear();
		double total_weight = 0.0;
		bool eq_or_no_weights = true;

		if(!has_weight)
		{
			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					value_weights.push_back(std::make_pair(value, 1.0));
					total_weight += 1.0;
				}
			}
		}
		else
		{
			double weight_check = std::numeric_limits<double>::quiet_NaN();

			for(EntityIterator i = first; i != last; ++i)
			{
				double value = 0.0;
				if(get_value(i, value))
				{
					double weight_value = 1.0;
					get_weight(i, weight_value);
					if(!FastIsNaN(weight_value))
					{
						value_weights.push_back(std::make_pair(value, weight_value));
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
		if(value_weights.size() == 0 || total_weight == 0.0)
			return std::numeric_limits<double>::quiet_NaN();

		//sorts on .first - value, not weight
		std::sort(std::begin(value_weights), std::end(value_weights));

		//early outs for edge cases
		if(value_weights.size() == 1 || q_percentage == 0.0)
			return value_weights.front().first;
		else if(q_percentage == 1.0)
			return value_weights.back().first;

		//search cumulative density for target quantile
		const double first_cdf_term = 0.5 * value_weights.front().second;
		const double last_cdf_term = total_weight - 0.5 * value_weights.front().second - 0.5 * value_weights.back().second;
		double accum_weight = 0.0;
		double cdf_term_prev = 0.0;
		for(size_t i = 0; i < value_weights.size(); ++i)
		{
			const auto &[curr_value, curr_weight] = value_weights[i];

			//calculate cdf term
			double cdf_term = 0.0;
			accum_weight += value_weights[i].second;
			cdf_term += accum_weight - 0.5 * value_weights[i].second;

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
				return value_weights[i - 1].first;
			else if(q_percentage == cdf_term)
				return curr_value;
			else if(cdf_term_prev < q_percentage && q_percentage < cdf_term)
			{
				const auto &prev_value = value_weights[i - 1].first;

				//linearly interpolate
				return prev_value + (curr_value - prev_value) * (q_percentage - cdf_term_prev) / (cdf_term - cdf_term_prev);
			}

			cdf_term_prev = cdf_term;
		}

		//if didn't find (quantile percentage larger than last cdf term), use last element
		return value_weights.back().first;
	}

	//computes the generalized mean of the values where p_value is the parameter for the generalized mean
	//center is the center the calculation is around, default is 0.0
	//if calculate_moment is true, the final calculation will not be raised to 1/p for p>=1
	//if absolute_value is true, the first order mean (p=1) will take the absolute value
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	//has separate paths for different values of p_value for efficiency
	template<typename EntityIterator, typename ValueFunction, typename WeightFunction>
	static double GeneralizedMean(EntityIterator first, EntityIterator last,
		ValueFunction get_value, bool has_weight, WeightFunction get_weight,
		double p_value, double center = 0.0, bool calculate_moment = false, bool absolute_value = false)
	{
		double mean = 0.0;

		if(!has_weight)
		{
			size_t num_elements = 0;

			if(p_value == 1.0) // arithmetic
			{
				for(EntityIterator i = first; i != last; ++i)
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
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
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
				mean = 1.0;
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						mean *= (value - center);
						num_elements++;
					}
				}

				if(!calculate_moment)
					mean = std::pow(mean, 1.0 / num_elements);
			}
			else if(p_value == -1.0) // harmonic
			{
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						mean += (1.0 / (value - center));
						num_elements++;
					}
				}

				mean /= num_elements;
				if(!calculate_moment)
					mean = (1.0 / mean);
			}
			else
			{
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						mean += std::pow(value - center, p_value);
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
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						double weight_value = 1.0;
						get_weight(i, weight_value);

						//don't multiply if zero in case value is infinite
						if(weight_value != 0.0)
						{
							mean += weight_value * (value - center);
							weights_sum += weight_value;
						}
					}
				}

				//can divide at the end because multiplication is associative and commutative
				mean /= weights_sum;
			}
			else if(p_value == 2.0) // root mean square (quadratic)
			{
				for(EntityIterator i = first; i != last; ++i)
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
				for(EntityIterator i = first; i != last; ++i)
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

				mean = 1.0;
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						double weight_value = 1.0;
						get_weight(i, weight_value);

						//don't multiply if zero in case value is infinite
						if(weight_value != 0.0)
							mean *= std::pow(value - center, weight_value);
					}
				}

				if(!calculate_moment)
					mean = std::pow(mean, 1.0 / weights_sum);
			}
			else if(p_value == -1.0) // harmonic
			{
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						double weight_value = 1.0;
						get_weight(i, weight_value);

						//don't multiply if zero in case value is infinite
						if(weight_value != 0.0)
						{
							mean += weight_value / (value - center);
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
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						double weight_value = 1.0;
						get_weight(i, weight_value);

						//don't multiply if zero in case value is infinite
						if(weight_value != 0.0)
						{
							mean += weight_value * std::pow(value - center, p_value);
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

	//computes the extreme value of the values
	//if select_min_value is true, will return the minimum, otherwise returns the maximum
	//max_distance is the maximum distance anything can be (infinity is a valid value)
	//if include_zero_distances is true, then it will include zero distance if that is the extreme value
	//iterates from first to last, calling get_value
	// if has_weight, then will use get_weight to obtain the weight of each value
	// values_buffer is a temporary buffer to hold data that can be reused
	template<typename EntityIterator, typename ValueFunction>
	static double ExtremeDifference(EntityIterator first, EntityIterator last,
		ValueFunction get_value,
		bool select_min_value, double max_distance, bool include_zero_distances,
		std::vector<double> &values_buffer)
	{
		std::vector<double> &values = values_buffer;
		values_buffer.clear();

		for(EntityIterator i = first; i != last; ++i)
		{
			double value = 0.0;
			if(get_value(i, value))
			{
				//don't compare nulls (nans) because they don't contribute to finding an extreme difference
				if(!FastIsNaN(value))
					values.push_back(value);
			}
		}

		//deal with edge cases
		//if no values, then don't have any gaps
		if(values.size() == 0)
			return std::numeric_limits<double>::quiet_NaN();

		//if have one value, then infinite gap
		if(values.size() == 1)
		{
			if(!FastIsNaN(max_distance))
				return std::numeric_limits<double>::infinity();
			else
				return max_distance;
		}

		std::sort(begin(values), end(values));

		double extreme_distance;
		if(select_min_value)
		{
			extreme_distance = std::numeric_limits<double>::infinity();
			for(size_t i = 0; i + 1 < values.size(); i++)
			{
				double delta = values[i + 1] - values[i];

				//skip zeros if applicable
				if(include_zero_distances && delta == 0)
					continue;

				if(delta < extreme_distance)
					extreme_distance = delta;
			}

			if(!FastIsNaN(max_distance))
			{
				double dist_between_ends = values[0] + std::max(0.0, max_distance - values[values.size() - 1]);
				if(dist_between_ends < extreme_distance)
					extreme_distance = dist_between_ends;
			}
		}
		else //max value
		{
			extreme_distance = 0.0;
			for(size_t i = 0; i + 1 < values.size(); i++)
			{
				double delta = values[i + 1] - values[i];
				if(delta > extreme_distance)
					extreme_distance = delta;
			}

			if(!FastIsNaN(max_distance))
			{
				double dist_between_ends = values[0] + std::max(0.0, max_distance - values[values.size() - 1]);
				if(dist_between_ends > extreme_distance)
					extreme_distance = dist_between_ends;
			}
		}

		return extreme_distance;
	}

	//holds parameters and transforms distances and surprisals
	//EntityReference is the type of reference to an entity, and entity_reference is the reference itself
	//index is the element number as sorted by smallest distance, where 0 is the entity with the smallest distance
	//if surprisal_to_probability is true, it will transform surprisal to probability
	//if surprisal_to_probability is false, distance_weight_exponent is the exponent each distance is raised to
	//has_weight, if set, will use get_weight, taking in a function of an entity reference and a reference to an output
	// double to set the weight, and should return true if the entity has a weight, false if not
	template<typename EntityReference>
	class DistanceTransform
	{
	public:
		constexpr DistanceTransform(bool surprisal_to_probability, double distance_weight_exponent,
			bool has_weight, std::function<bool(EntityReference, double &)> get_weight)
		{
			distanceWeightExponent = distance_weight_exponent;
			transformSuprisalToProb = surprisal_to_probability;
			hasWeight = has_weight;
			getEntityWeightFunction = get_weight;
		}

		//transforms distances with regard to distance weight exponents, harmonic series, and entity weights as specified by parameters,
		// transforming and updating the distances in entity_distance_pair_container in place
		//EntityDistancePairContainer is the container for the entity-distance pairs, and EntityReference is the reference to the entity
		//entity_distance_pair_container is the iterable container of the entity-distance pairs
		//distance_weight_exponent is the exponent each distance is raised to
		//has_weight, if set, will use get_weight, taking in a function of an entity reference and a reference to an output double to set the weight,
		// and should return true if the entity has a weight, false if not
		//sort_results, if set, will sort the results appropriately for the distance_weight_exponent,
		// from smallest to largest if distance_weight_exponent is positive, largest to smallest otherwise
		//get_entity returns the EntityReference for an iterator of EntityDistancePairContainer
		//get_distance_ref returns a reference as a pointer to the location of the distance in the EntityDistancePairContainer
		inline void TransformDistances(std::vector<DistanceReferencePair<EntityReference>> &entity_distance_pair_container, bool sort_results)
		{
			if(transformSuprisalToProb)
			{
				//convert to surprisal
				for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
					iter->distance = std::exp(-iter->distance);

				if(hasWeight)
				{
					//if weighted, need to weight by the logical OR of all probability masses
					// this is complex to compute if done as P(A or B) = P(A) + P(B) - P(A and B),
					//but is much more simple if computed as P(A or B) = 1 - ( (1 - P(A)) and (1 - P(B)))
					//the latter is a multiplication, lending itself to raising to the power of the weight
					//e.g., a weight of 2 is (1 - P(A))^2
					for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
					{
						double weight = 1.0;
						//if has a weight and not 1 (since 1 is fast)
						if(getEntityWeightFunction(iter->reference, weight) && weight != 1.0)
						{
							if(weight != 0.0)
							{
								double prob_not_same = 1.0 - iter->distance;
								double weighted_prob_not_same = std::pow(prob_not_same, weight);
								iter->distance = 1.0 - weighted_prob_not_same;
							}
							else //weight of 0.0
							{
								iter->distance = 0.0;
							}
						}
					}
				}
			}
			else //distance transform
			{
				if(distanceWeightExponent == -1)
				{
					for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
						iter->distance = 1.0 / iter->distance;
				}
				else if(distanceWeightExponent == 0)
				{
					for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
						iter->distance = 1.0;
				}
				else if(distanceWeightExponent != 1)
				{
					if(distanceWeightExponent >= 0)
					{
						for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
							iter->distance = std::pow(iter->distance, distanceWeightExponent);
					}
					else //need special handling for zero distances to prevent NaN
					{
						for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
						{
							if(iter->distance == 0.0)
								iter->distance = std::numeric_limits<double>::infinity();
							else
								iter->distance = std::pow(iter->distance, distanceWeightExponent);
						}
					}
				}
				//else distanceWeightExponent == 1, which means just leave it

				if(hasWeight)
				{
					for(auto iter = begin(entity_distance_pair_container); iter != end(entity_distance_pair_container); ++iter)
					{
						double weight = 1.0;
						if(getEntityWeightFunction(iter->reference, weight))
						{
							if(weight != 0.0)
								iter->distance *= weight;
							else
								iter->distance = 0.0;
						}
					}
				}
			}

			if(sort_results)
			{
				//if distance, sort by smallest first
				if(!transformSuprisalToProb && distanceWeightExponent > 0)
				{
					std::sort(begin(entity_distance_pair_container), end(entity_distance_pair_container),
						[](auto a, auto b) {return a.distance < b.distance; }
					);
				}
				else //inverse distance, sort by largest first
				{
					std::sort(begin(entity_distance_pair_container), end(entity_distance_pair_container),
						[](auto a, auto b) {return a.distance > b.distance; }
					);
				}
			}
		}

		//like TransformDistances but returns the appropriate expected value
		template<typename EntityDistancePairIterator>
		inline double TransformDistancesToExpectedValue(
			EntityDistancePairIterator entity_distance_pair_container_begin,
			EntityDistancePairIterator entity_distance_pair_container_end)
		{
			if(transformSuprisalToProb)
			{
				//need to weight by the logical OR of all probability masses
				// this is complex to compute if done as P(A or B) = P(A) + P(B) - P(A and B),
				//but is much more simple if computed as P(A or B) = 1 - ( (1 - P(A)) and (1 - P(B)))
				//the latter is a multiplication, additionally lending itself to raising to the power of the weight
				//e.g., a weight of 2 is (1 - P(A))^2
				double prob_none_same = 1.0;

				if(hasWeight)
				{
					//convert to surprisal
					for(auto iter = entity_distance_pair_container_begin; iter != entity_distance_pair_container_end; ++iter)
					{
						double prob_same = std::exp(-iter->distance);
						double prob_not_same = 1.0 - prob_same;

						double weight = 1.0;
						if(getEntityWeightFunction(iter->reference, weight) && weight != 1.0)
						{
							if(weight == 0.0)
								continue;

							prob_not_same = std::pow(prob_not_same, weight);
						}
						
						prob_none_same *= prob_not_same;
					}
				}
				else //!hasWeight
				{
					//convert to surprisal
					for(auto iter = entity_distance_pair_container_begin; iter != entity_distance_pair_container_end; ++iter)
					{
						double prob_same = std::exp(-iter->distance);
						double prob_not_same = 1.0 - prob_same;
						prob_none_same *= prob_not_same;
					}
				}

				double any_prob_same = 1 - prob_none_same;
				return -std::log(any_prob_same);
			}
			else //distance transform
			{
				if(hasWeight)
				{
					return GeneralizedMean<typename std::vector<DistanceReferencePair<EntityReference>>::iterator>(
						entity_distance_pair_container_begin,
						entity_distance_pair_container_end,
						[](typename std::vector<DistanceReferencePair<EntityReference>>::iterator iter, double &value)
						{ value = iter->distance; return true; },
						true,
						[this](typename std::vector<DistanceReferencePair<EntityReference>>::iterator iter, double &weight)
						{ return getEntityWeightFunction(iter->reference, weight); },
						distanceWeightExponent);
				}
				else
				{
					return GeneralizedMean<typename std::vector<DistanceReferencePair<EntityReference>>::iterator>(
						entity_distance_pair_container_begin,
						entity_distance_pair_container_end,
						[](typename std::vector<DistanceReferencePair<EntityReference>>::iterator iter, double &value)
						{ value = iter->distance; return true; },
						false,
						[](typename std::vector<DistanceReferencePair<EntityReference>>::iterator iter, double &weight)
						{ return false; },
						distanceWeightExponent);
				}
			}

		}

		//Computes the distance contribution as a type of generalized mean with special handling for distances of zero
		// entity is the entity that the distance contribution is being performed on, and entity_distance_pair_container are the distances to
		// its nearest entities
		// the functions get_entity and get_distance_ref return the entity and reference to the distance for an iterator of entity_distance_pair_container
		double ComputeDistanceContribution(std::vector<DistanceReferencePair<EntityReference>> &entity_distance_pair_container, EntityReference entity)
		{
			double distance_contribution = 0.0;
			//there's at least one entity in question
			size_t num_identical_entities = 1;

			auto entity_distance_begin = begin(entity_distance_pair_container);
			auto entity_distance_iter = entity_distance_begin;

			//if no weight, can do a more streamlined process
			if(!hasWeight)
			{
				//count the number of zero distances
				for(; entity_distance_iter != end(entity_distance_pair_container); ++entity_distance_iter)
				{
					if(entity_distance_iter->distance != 0.0)
						break;

					num_identical_entities++;
				}

				distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));

				//split the distance contribution among the identical entities
				return distance_contribution / num_identical_entities;
			}
			
			double weight_of_identical_entities = 0.0;

			//count the number of zero distances and get the associated weight,
			// since this weight isn't accounted for in the other distances
			for(; entity_distance_iter != end(entity_distance_pair_container); ++entity_distance_iter)
			{
				if(entity_distance_iter->distance != 0.0)
					break;

				double weight = 1.0;
				if(getEntityWeightFunction(entity_distance_iter->reference, weight))
					weight_of_identical_entities += weight;
				else
					weight_of_identical_entities += 1.0;
			}

			distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));

			//if no cases had any weight, distance contribution is 0
			if(FastIsNaN(distance_contribution))
				return 0.0;

			double entity_weight = 1.0;
			if(getEntityWeightFunction(entity, entity_weight))
			{
				if(entity_weight != 0)
					distance_contribution *= entity_weight;
				else
					return 0.0;
			}

			//split the distance contribution among the identical entities
			return distance_contribution * entity_weight / (weight_of_identical_entities + entity_weight);
		}

		//exponent by which to scale the distances
		//only applicable when transformSuprisalToProb is false
		double distanceWeightExponent;

		//if true, the values will be transformed from surprisal to probability; if false, will perform a distance transform
		bool transformSuprisalToProb;

		//if hasWeight is true, then will call getEntityWeightFunction and apply the respective entity weight to each distance
		bool hasWeight;
		std::function<bool(EntityReference, double &)> getEntityWeightFunction;
	};
};
