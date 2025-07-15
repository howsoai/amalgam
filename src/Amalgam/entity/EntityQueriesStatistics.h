#pragma once

//project headers:
#include "FastMath.h"
#include "DistanceReferencePair.h"
#include "HashMaps.h"
#include "StringInternPool.h"

//system headers:
#include <functional>

//TODO 24016: uncomment only one of these at a time
//TODO 24016: remove these and accompanying code after figuring out what is correct
// #define DIST_CONTRIBS_HARMONIC_MEAN
#define DIST_CONTRIBS_GEOMETRIC_MEAN
// #define DIST_CONTRIBS_ARITHMETIC_MEAN
//#define DIST_CONTRIBS_PROBABILITY_MEAN
//this can be enabled or disabled independent of the others
#define BANDWIDTH_SELECTION_INVERSE_SURPRISAL

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
					auto [inserted_value, inserted] = value_weights.emplace(value, 1.0);
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

					auto [inserted_value, inserted] = value_weights.emplace(value, weight_value);
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
					auto [inserted_value, inserted] = value_weights.emplace(value, 1.0);
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
					
					auto [inserted_value, inserted] = value_weights.emplace(value, weight_value);
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
					auto [inserted_value, inserted] = value_masses.emplace(value, 1.0);
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

					auto [inserted_value, inserted] = value_masses.emplace(value, weight_value);
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
					auto [inserted_value, inserted] = value_masses.emplace(value, 1.0);
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

					auto [inserted_value, inserted] = value_masses.emplace(value, weight_value);
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
		//deal with edge case of no values
		if(first == last)
			return std::numeric_limits<double>::quiet_NaN();

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
				bool zero_found = false;
				for(EntityIterator i = first; i != last; ++i)
				{
					double value = 0.0;
					if(get_value(i, value))
					{
						double diff = value - center;
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

				bool zero_found = false;
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
	//if compute_surprisal is true, it will transform via surprisal,
	// and if surprisal_to_probability is true, it will convert to probability
	//if compute_surprisal  is false, distance_weight_exponent is the exponent each distance is raised to
	//uses min_to_retrieve and max_to_retrieve to determine how many entities to keep, stopping when the first
	// entity's marginal probability falls below the num_to_retrieve_min_increment_prob threshold
	//has_weight, if set, will use the function get_weight to deterimne the given entity's weight
	// double to set the weight, and should return true if the entity has a weight, false if not
	template<typename EntityReference>
	class DistanceTransform
	{
	public:
		constexpr DistanceTransform(bool compute_surprisal, bool surprisal_to_probability,
			double distance_weight_exponent,
			size_t min_to_retrieve, size_t max_to_retrieve,
			double num_to_retrieve_min_increment_prob,
			size_t extra_to_retrieve,
			bool has_weight, double min_weight, std::function<double(EntityReference)> get_weight)
		{
			distanceWeightExponent = distance_weight_exponent;
			computeSurprisal = compute_surprisal;
			surprisalToProbability = surprisal_to_probability;

			minToRetrieve = min_to_retrieve;
			maxToRetrieve = max_to_retrieve;
			numToRetrieveMinIncrementalProbability = num_to_retrieve_min_increment_prob;
			extraToRetrieve = extra_to_retrieve;

			//if all percentages are the same, that will yield the most number of entities kept
			//so round up the reciprocal of this number to find the maximum number of entities that can be kept
			double smallest_possible_prob_mass = std::min(1.0, min_weight) * numToRetrieveMinIncrementalProbability;
			double max_by_prob = std::ceil(1 / smallest_possible_prob_mass);
			//need to compare to valid values in floating point because some compilers treat static casts differently
			if(max_by_prob > 0.0 && max_by_prob <= static_cast<double>(std::numeric_limits<size_t>::max()))
			{
				size_t max_by_prob_int = static_cast<size_t>(max_by_prob);
				if(max_by_prob_int < maxToRetrieve)
					maxToRetrieve = max_by_prob_int;
			}

			if(maxToRetrieve < minToRetrieve)
				minToRetrieve = maxToRetrieve;

			hasWeight = has_weight;
			getEntityWeightFunction = get_weight;
		}

		__forceinline static double ConvertProbabilityToSurprisal(double prob)
		{
			return -std::log(prob);
		}

		__forceinline static double ConvertSurprisalToProbability(double surprisal)
		{
			return std::exp(-surprisal);
		}

		//returns the number of entities to retrieve to satisfy requirements for the parameters
		inline size_t GetNumToRetrieve()
		{
			return maxToRetrieve + extraToRetrieve;
		}

	protected:
		//transforms distances given transform_func, which should return a tuple of the following
		// values: resulting weighted value, resulting unweighted value, probability of being the same,
		// probability mass of value, and weight of entity
		//selects the bandwidth from the transformed values and returns the number of entities to keep,
		// which may be less than the total
		//calls result_func for each iteration, which accepts the following parameters:
		// the iterator, the weighted resulting value, the unweighted resulting value,
		// the probability mass of the result, and the weight of entity
		// so that this method can be used flexibly for map and reduce purposes,
		// writing out transformations or accumulating the results
		template<typename EntityDistancePairIterator, typename TransformFunc, typename ResultFunc>
		__forceinline size_t SelectBandwidthFromDistanceTransforms(
			EntityDistancePairIterator entity_distance_pair_container_begin,
			EntityDistancePairIterator entity_distance_pair_container_end,
			TransformFunc transform_func, ResultFunc result_func)
		{
			size_t max_k = std::distance(entity_distance_pair_container_begin, entity_distance_pair_container_end);

			if(minToRetrieve < maxToRetrieve || numToRetrieveMinIncrementalProbability > 0.0)
			{
				//if no elements, just return zero
				if(entity_distance_pair_container_begin == entity_distance_pair_container_end)
					return 0;
				
				auto [first_weighted_value, first_unweighted_value, first_prob_same, first_prob_mass, first_weight]
					= transform_func(entity_distance_pair_container_begin);
				result_func(entity_distance_pair_container_begin, first_weighted_value, first_unweighted_value, first_prob_mass, first_weight);

				double total_prob = first_prob_mass;
				size_t main_k = 1;
				for(; main_k < max_k; main_k++)
				{
					auto [weighted_value, unweighted_value, prob_same, prob_mass, weight]
						= transform_func(entity_distance_pair_container_begin + main_k);

					//stop if have enough entities and below probability threshold
					if(main_k >= minToRetrieve && prob_same / total_prob < numToRetrieveMinIncrementalProbability)
						break;

					total_prob += prob_mass;

					result_func(entity_distance_pair_container_begin + main_k, weighted_value, unweighted_value, prob_mass, weight);
				}

				//pull on any extra cases
				size_t extra_k = 0;
				for(; extra_k < extraToRetrieve && main_k + extra_k < max_k; extra_k++)
				{
					auto [weighted_value, unweighted_value, prob_same, prob_mass, weight]
						= transform_func(entity_distance_pair_container_begin + main_k + extra_k);

					total_prob += prob_mass;

					result_func(entity_distance_pair_container_begin + main_k + extra_k, weighted_value, unweighted_value, prob_mass, weight);
				}

				return main_k + extra_k;
			}
			else //just transform all of the elements
			{
				for(auto iter = entity_distance_pair_container_begin; iter != entity_distance_pair_container_end; ++iter)
				{
					auto [weighted_value, unweighted_value, prob_same, prob_mass, weight] = transform_func(iter);
					result_func(iter, weighted_value, unweighted_value, prob_mass, weight);
				}

				return max_k;
			}
		}

		//transforms distances based on how this object has been parameterized
		//calls result_func for each iteration, which accepts three parameters:
		// the weighted resulting value, the unweighted resulting value, and the weight of entity
		// so that this method can be used flexibly for map and reduce purposes
		// writing out transformations or accumulating the results
		//selects the bandwidth from the transformed values and returns the number of entities to keep,
		// which may be less than the total
		template<typename EntityDistancePairIterator, typename ResultFunc>
		__forceinline size_t TransformDistancesWithBandwidthSelectionAndResultFunction(
			EntityDistancePairIterator entity_distance_pair_container_begin,
			EntityDistancePairIterator entity_distance_pair_container_end,
			ResultFunc result_func)
		{
			size_t num_to_keep = 0;
			if(computeSurprisal)
			{
				if(surprisalToProbability)
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
						#ifdef BANDWIDTH_SELECTION_INVERSE_SURPRISAL
							double prob = 1.0 / iter->distance;
						#else
							double prob = ConvertSurprisalToProbability(iter->distance);
						#endif
							if(!hasWeight)
								return std::make_tuple(prob, prob, prob, prob, 1.0);

							double weight = getEntityWeightFunction(iter->reference);
							double weighted_prob = prob * weight;

							return std::make_tuple(weighted_prob, prob, prob, weighted_prob, weight);
						}, result_func);
				}
				else //keep in surprisal space
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
							double surprisal = iter->distance;
						#ifdef BANDWIDTH_SELECTION_INVERSE_SURPRISAL
							double prob = 1.0 / surprisal;
						#else
							double prob = ConvertSurprisalToProbability(surprisal);
						#endif
							if(!hasWeight)
								return std::make_tuple(surprisal, surprisal, prob, prob, 1.0);

							double weight = getEntityWeightFunction(iter->reference);

							return std::make_tuple(surprisal, surprisal, prob, prob * weight, weight);
						}, result_func);
				}
			}
			else //distance transform
			{
				if(distanceWeightExponent == -1)
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
							double prob = 1.0 / iter->distance;
							if(!hasWeight)
								return std::make_tuple(prob, prob, prob, prob, 1.0);

							double weight = getEntityWeightFunction(iter->reference);
							double weighted_prob = prob * weight;

							return std::make_tuple(weighted_prob, prob, prob, weighted_prob, weight);
						}, result_func);
				}
				else if(distanceWeightExponent == 1)
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
					{
						//positive distanceWeightExponent values still need to compute the corresponding reciprocal
						// in order to assess statistical bandwidth
						double prob = 1.0 / iter->distance;
						if(!hasWeight)
							return std::make_tuple(iter->distance, iter->distance, prob, prob, 1.0);

						double weight = getEntityWeightFunction(iter->reference);

						return std::make_tuple(iter->distance, iter->distance, prob, weight * prob, weight);
					}, result_func);
				}
				else if(distanceWeightExponent == 0)
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
							if(!hasWeight)
								return std::make_tuple(1.0, 1.0, 1.0, 1.0, 1.0);

							double weight = getEntityWeightFunction(iter->reference);

							return std::make_tuple(weight, 1.0, 1.0, weight, weight);
						}, result_func);
				}
				else if(distanceWeightExponent > 0)
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
							//positive distanceWeightExponent values still need to compute the corresponding reciprocal
							// in order to assess statistical bandwidth
							double prob = (iter->distance == 0 ? std::numeric_limits<double>::infinity()
								: std::pow(iter->distance, -distanceWeightExponent));

							if(!hasWeight)
								return std::make_tuple(iter->distance, iter->distance, prob, prob, 1.0);

							double weight = getEntityWeightFunction(iter->reference);

							return std::make_tuple(iter->distance, iter->distance, prob, weight * prob, weight);
						}, result_func);
				}
				else //distanceWeightExponent < 0
				{
					num_to_keep = SelectBandwidthFromDistanceTransforms(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[this](auto iter)
						{
							double prob = (iter->distance == 0 ? std::numeric_limits<double>::infinity()
								: std::pow(iter->distance, distanceWeightExponent));

							if(!hasWeight)
								return std::make_tuple(iter->distance, iter->distance, prob, prob, 1.0);

							double weight = getEntityWeightFunction(iter->reference);
							double weighted_prob = prob * weight;

							return std::make_tuple(weighted_prob, prob, prob, weighted_prob, weight);
						}, result_func);
				}
			}

			return num_to_keep;
		}

	public:
		//transforms distances based on how this object has been parameterized, modifying the values in place
		//selects the bandwidth from the transformed values and returns the number of entities to keep,
		// which may be less than the total
		template<typename EntityDistancePairIterator>
		inline size_t TransformDistances(EntityDistancePairIterator entity_distance_pair_container_begin,
			EntityDistancePairIterator entity_distance_pair_container_end, bool sort_results)
		{
			size_t num_kept = TransformDistancesWithBandwidthSelectionAndResultFunction(
				entity_distance_pair_container_begin, entity_distance_pair_container_end,
				[](auto ed_pair, double weighted_value, double unweighted_value, double prob_mass, double weight)
				{
					ed_pair->distance = weighted_value;
				});

			if(sort_results)
			{
				//some compilers' interpretations of std::sort require a copy of the iterator that can be modified
				auto begin_iter = entity_distance_pair_container_begin;
				//if probability values or inverse distance, sort largest first
				if((computeSurprisal && surprisalToProbability)
					|| distanceWeightExponent <= 0)
				{
					std::sort(begin_iter, begin_iter + num_kept,
						[](auto a, auto b) {return a.distance > b.distance; }
					);
				}
				else //surprisal or regular distance, sort by smallest first
				{
					std::sort(begin_iter, begin_iter + num_kept,
						[](auto a, auto b) {return a.distance < b.distance; }
					);
				}
			}

			return num_kept;
		}

		//like TransformDistances but returns the appropriate expected value
		template<typename EntityDistancePairIterator>
		inline double TransformDistancesToExpectedValue(
			EntityDistancePairIterator entity_distance_pair_container_begin,
			EntityDistancePairIterator entity_distance_pair_container_end)
		{
			if(computeSurprisal)
			{
				double total_entity_weight = 0.0;
				double total_probability = 0.0;
				double accumulated_value = 0.0;
				//collect smallest value in case of numeric underflow; can approximate by using the smallest value
				double min_value = std::numeric_limits<double>::infinity();
				
				TransformDistancesWithBandwidthSelectionAndResultFunction(
					entity_distance_pair_container_begin, entity_distance_pair_container_end,
					[&total_entity_weight, &total_probability, &accumulated_value, &min_value](auto ed_pair,
						double weighted_value, double unweighted_value, double prob_mass, double weight)
					{
						if(weight != 0.0)
						{
							total_entity_weight += weight;

							//in information theory, zero weights cancel out infinities, so skip if zero
							if(prob_mass != 0)
							{
								total_probability += prob_mass;
								accumulated_value += prob_mass * unweighted_value;
							}

							//compare the unweighted value in case of underflow
							if(unweighted_value < min_value)
								min_value = unweighted_value;
						}
					});

				//if no evidence, no information, thus infinite surprisal
				if(total_entity_weight == 0.0)
				{
					if(surprisalToProbability)
						return 0.0;
					else
						return std::numeric_limits<double>::infinity();
				}	

				//evidence, but underflow, use minimum surprisal, since the others have underflowed
				if(total_probability == 0.0)
				{
					if(surprisalToProbability)
						return 0.0;
					else
						return min_value;
				}

				return accumulated_value / total_probability;
			}
			else //distance transform
			{
				if(distanceWeightExponent != 0.0)
				{
					double total_probability = 0.0;
					double accumulated_value = 0.0;

					TransformDistancesWithBandwidthSelectionAndResultFunction(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[&total_probability, &accumulated_value](auto ed_pair,
							double weighted_value, double unweighted_value, double prob_mass, double weight)
						{
							total_probability += weight;
						#ifdef DIST_CONTRIBS_PROBABILITY_MEAN
							accumulated_value += weight * std::exp(-unweighted_value);
						#else
							accumulated_value += weight * unweighted_value;
						#endif
						});

					//normalize
					double ave = accumulated_value / total_probability;
					if(distanceWeightExponent == 1)
					#ifdef DIST_CONTRIBS_PROBABILITY_MEAN
						return -std::log(ave);
					#else
						return ave;
					#endif

					if(distanceWeightExponent == -1)
						return 1 / ave;

					return std::pow(ave, 1 / distanceWeightExponent);
				}
				else //distanceWeightExponent == 0.0
				{
					double total_probability = 0.0;
					double accumulated_value = 0.0;

					//temporarily set to 1 to get the values back, then reset
					distanceWeightExponent = 1.0;

					TransformDistancesWithBandwidthSelectionAndResultFunction(
						entity_distance_pair_container_begin, entity_distance_pair_container_end,
						[&total_probability, &accumulated_value](auto ed_pair,
							double weighted_value, double unweighted_value, double prob_mass, double weight)
						{
							total_probability += weight;
							accumulated_value += weight * std::log(unweighted_value);
						});

					//normalize
					double ave_log = accumulated_value / total_probability;

					distanceWeightExponent = 0.0;

					return std::exp(ave_log);
				}
			}
		}

		//Computes the distance contribution as a type of generalized mean with special handling for distances of zero
		// entity_distance_pair_container are the distances to its nearest entities,
		// and entity_weight is the weight of the entity for which this distance contribution is being computed
		// the functions get_entity and get_distance_ref return the entity and reference to the distance for an iterator of entity_distance_pair_container
		double ComputeDistanceContribution(std::vector<DistanceReferencePair<EntityReference>> &entity_distance_pair_container, double entity_weight)
		{
			if(entity_weight == 0.0)
				return 0.0;

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

				//TODO 24016: remove these hacks and update

			#ifdef DIST_CONTRIBS_HARMONIC_MEAN
				computeSurprisal = false;
				distanceWeightExponent = -1;
				distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));
				computeSurprisal = true;
			#elif defined(DIST_CONTRIBS_GEOMETRIC_MEAN)
				computeSurprisal = false;
				distanceWeightExponent = 0;
				distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));
				computeSurprisal = true;
			#elif defined(DIST_CONTRIBS_ARITHMETIC_MEAN) || defined(DIST_CONTRIBS_PROBABILITY_MEAN)
				//both use the same code here, but DIST_CONTRIBS_PROBABILITY_MEAN changes code elsewhere
				computeSurprisal = false;
				distanceWeightExponent = 1;
				distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));
				computeSurprisal = true;
			#else
				distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));
			#endif

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

				weight_of_identical_entities += getEntityWeightFunction(entity_distance_iter->reference);
			}

			//TODO 24016: remove this hack
			computeSurprisal = false;
			distanceWeightExponent = -1;
			distance_contribution = TransformDistancesToExpectedValue(entity_distance_iter, end(entity_distance_pair_container));
			computeSurprisal = true;

			//if no cases had any weight, distance contribution is 0
			if(FastIsNaN(distance_contribution))
				return 0.0;

			//split the distance contribution among the identical entities
			double fraction_per_identical_entity = entity_weight / (weight_of_identical_entities + entity_weight);

			//return the distance contribution modified by weights and identical entities
			return entity_weight * distance_contribution * fraction_per_identical_entity;
		}

		//return the entity weight for the entity reference if it exists, 1.0 if it does not
		std::function<double(EntityReference)> getEntityWeightFunction;

protected:

		//exponent by which to scale the distances
		//only applicable when computeSurprisal is false
		double distanceWeightExponent;

		//if true, the values will be calculated as surprisals, if false, will perform a distance transform
		bool computeSurprisal;

		//if true and computeSurprisal is true, the results will be transformed from surprisal to probability
		bool surprisalToProbability;

		//maximum number of entities to attempt to retrieve (based on queryType),
		size_t maxToRetrieve;

		//minimum number of entities to attempt to retrieve
		size_t minToRetrieve;

		//incremental probability where, if the next entity is below this threshold, don't retrieve more,
		double numToRetrieveMinIncrementalProbability;

		//number of entities to attempt to retrieve after any constraints
		size_t extraToRetrieve;

		//if hasWeight is true, then will call getEntityWeightFunction and apply the respective entity weight to each distance
		bool hasWeight;
	};
};
