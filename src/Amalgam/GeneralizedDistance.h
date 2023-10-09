#pragma once

//project headers:
#include "EvaluableNode.h"
#include "EvaluableNodeTreeManipulation.h"
#include "FastMath.h"

//system headers:
#include <limits>
#include <vector>

//If defined, will use the Laplace LK metric (default).  Otherwise will use Gaussian.
#define DISTANCE_USE_LAPLACE_LK_METRIC true

//general class of feature comparisons
// align at 64-bits in order to play nice with data alignment where it is used
enum FeatureDifferenceType : uint64_t
{
	FDT_NOMINAL,
	//continuous with or without cycles, but everything is always numeric and all numbers interned
	FDT_CONTINUOUS_UNIVERSALLY_NUMERIC_INTERNED,
	//continuous without cycles, but everything is always numeric
	FDT_CONTINUOUS_UNIVERSALLY_NUMERIC,
	//continuous without cycles, may contain nonnumeric data
	FDT_CONTINUOUS_NUMERIC,
	//continuous with or without cycles, but all numbers interned
	FDT_CONTINUOUS_NUMERIC_INTERNED,
	//like FDT_CONTINUOUS_NUMERIC, but has cycles
	FDT_CONTINUOUS_NUMERIC_CYCLIC,
	//edit distance between strings
	FDT_CONTINUOUS_STRING,
	//continuous measures of the number of nodes different between two sets of code
	FDT_CONTINUOUS_CODE,
};

//base data struct for holding distance parameters and metadata
//generalizes Minkowski distance, information theoretic surprisal as a distance, and Lukaszyk–Karmowski
class GeneralizedDistance
{
public:
	//initialization functions

	//dynamically precompute and cache nominal deltas and defaults everytime the pValue is set
	inline void SetAndConstrainParams()
	{
		inversePValue = 1.0 / pValue;

		ComputeNominalDistanceTerms();

		bool compute_approximate = NeedToPrecomputeApproximate();
		if(compute_approximate)
		{
			fastPowP = RepeatedFastPow(pValue);
			fastPowInverseP = RepeatedFastPow(inversePValue);
		}

		//default to the accuracy that should be used first
		if(recomputeAccurateDistances)
			SetHighAccuracy(false);
		else
			SetHighAccuracy(highAccuracy);
	}

	//update usingHighAccuracy and nominal defaults
	inline void SetHighAccuracy(bool high_accuracy)
	{
		//need to have asked for high_accuracy and have computed high accuracy
		// or just not have computed low accuracy at all
		if( (high_accuracy && NeedToPrecomputeAccurate()))
			defaultPrecision = ExactApproxValuePair::EXACT;
		else
			defaultPrecision = ExactApproxValuePair::APPROX;
	}

	//computes and sets unknownToUnknownDistanceTerm and knownToUnknownDistanceTerm based on
	// unknownToUnknownDifference and knownToUnknownDifference respectively
	inline void ComputeAndStoreUncertaintyDistanceTerms(size_t index)
	{
		bool compute_accurate = NeedToPrecomputeAccurate();
		bool compute_approximate = NeedToPrecomputeApproximate();

		auto &feature_params = featureParams[index];

		//compute unknownToUnknownDistanceTerm
		if(compute_accurate)
		{
			feature_params.unknownToUnknownDistanceTerm.SetValue(
					ComputeDistanceTermNonNull(feature_params.unknownToUnknownDifference,
						index, ExactApproxValuePair::EXACT),
					ExactApproxValuePair::EXACT);
		}

		if(compute_approximate)
		{
			feature_params.unknownToUnknownDistanceTerm.SetValue(
				ComputeDistanceTermNonNull(feature_params.unknownToUnknownDifference,
					index, ExactApproxValuePair::APPROX),
				ExactApproxValuePair::APPROX);
		}

		//if knownToUnknownDifference is same as unknownToUnknownDifference, can copy distance term instead of recomputing
		if(feature_params.knownToUnknownDifference == feature_params.unknownToUnknownDifference)
		{
			feature_params.knownToUnknownDistanceTerm = feature_params.unknownToUnknownDistanceTerm;
			return;
		}

		//compute knownToUnknownDistanceTerm
		if(compute_accurate)
		{
			feature_params.knownToUnknownDistanceTerm.SetValue(
				ComputeDistanceTermNonNull(feature_params.knownToUnknownDifference,
					index, ExactApproxValuePair::EXACT),
				ExactApproxValuePair::EXACT);
		}

		if(compute_approximate)
		{
			feature_params.knownToUnknownDistanceTerm.SetValue(
				ComputeDistanceTermNonNull(feature_params.knownToUnknownDifference,
					index, ExactApproxValuePair::APPROX),
				ExactApproxValuePair::APPROX);
		}
	}

	//for the feature index, computes and stores the distance terms as measured from value to each interned value
	inline void ComputeAndStoreInternedNumberValuesAndDistanceTerms(size_t index, double value, std::vector<double> *interned_values)
	{
		auto &feature_params = featureParams[index];

		feature_params.precomputedNumberInternDistanceTermsPrecision = defaultPrecision;

		if(interned_values == nullptr)
		{
			feature_params.internedNumberIndexToNumberValue = nullptr;
			feature_params.precomputedNumberInternDistanceTerms.clear();
			feature_params.precomputedNumberInternDistanceTermsPrecision = defaultPrecision;
			return;
		}

		feature_params.precomputedNumberInternDistanceTerms.resize(interned_values->size());
		for(size_t i = 0; i < feature_params.precomputedNumberInternDistanceTerms.size(); i++)
		{
			double difference = value - interned_values->at(i);
			feature_params.precomputedNumberInternDistanceTerms[i] = ComputeDifferenceTermNonNominal(difference, index);
		}
	}

	// 2/sqrt(pi) = 2.0 / std::sqrt(3.141592653589793238462643383279502884L);
	static constexpr double s_two_over_sqrt_pi = 1.12837916709551257390;

	//sqrt(2.0)
	static constexpr double s_sqrt_2 = 1.41421356237309504880;

	__forceinline static double ComputeDeviationPartLaplace(const double diff, const double deviation)
	{
		return std::exp(-diff / deviation) * (3 * deviation + diff) / 2;
	}

	__forceinline static double ComputeDeviationPartLaplaceApprox(const double diff, const double deviation)
	{
		return FastExp(-diff / deviation) * (3 * deviation + diff) / 2;
	}

	__forceinline static double ComputeDeviationPartGaussian(const double diff, const double deviation)
	{
		const double term = diff / (2.0 * deviation); //diff / (2*sigma)
		return s_two_over_sqrt_pi * deviation * std::exp(-term * term) - diff * std::erfc(term); //2*sigma*(e^(-1*(diff^2)/((2*simga)^2)))/sqrt(pi) - diff*erfc(diff/(2*sigma))
	}

	__forceinline static double ComputeDeviationPartGaussianApprox(const double diff, const double deviation)
	{
		const double term = diff / (2.0 * deviation); //diff / (2*sigma)
		return s_two_over_sqrt_pi * deviation * FastExp(-term * term) - diff * std::erfc(term); //2*sigma*(e^(-1*(diff^2)/((2*simga)^2)))/sqrt(pi) - diff*erfc(diff/(2*sigma))
	}

	//computes the Lukaszyk–Karmowski metric deviation component for the minkowski distance equation given the feature difference and feature deviation
	//assumes deviation is nonnegative
	__forceinline double ComputeDeviationPart(const double diff, const double deviation)
	{
		if(defaultPrecision == ExactApproxValuePair::EXACT)
		#ifdef DISTANCE_USE_LAPLACE_LK_METRIC
			return ComputeDeviationPartLaplace(diff, deviation);
		#else
			return ComputeDeviationPartGaussian(diff, deviation);
		#endif
		else
		#ifdef DISTANCE_USE_LAPLACE_LK_METRIC
			return ComputeDeviationPartLaplaceApprox(diff, deviation);
		#else
			return ComputeDeviationPartGaussianApprox(diff, deviation);
		#endif
	}

	//constrains the difference to the cycle length for cyclic distances
	__forceinline static double ConstrainDifferenceToCyclicDifference(double difference, double cycle_length)
	{
		//cyclics that are less than a cycle apart, the distance is the closer of: calculated distance or the looped distance of cycle length - calculated distance
		//for distances that are larger than a cycle, reduce it by taking the mod of it and do the same type of comparison
		if(difference > cycle_length)
			difference = std::fmod(difference, cycle_length);

		return std::min(difference, cycle_length - difference);
	}

protected:

	constexpr bool NeedToPrecomputeApproximate()
	{
		return (!highAccuracy || recomputeAccurateDistances);
	}

	constexpr bool NeedToPrecomputeAccurate()
	{
		return (highAccuracy || recomputeAccurateDistances);
	}

	//stores a pair of exact and approximate values
	// which can be referenced by getting the value at the corresponding offset
	//the values default to 0.0 on initialization
	class ExactApproxValuePair
	{
	public:
		//offset for each precision level
		static constexpr int APPROX = 0;
		static constexpr int EXACT = 1;

		__forceinline ExactApproxValuePair(double initial_value = 0.0)
		{
			exactApproxPair = { initial_value, initial_value };
		}

		constexpr double GetValue(int offset)
		{
			return exactApproxPair[offset];
		}

		__forceinline void SetValue(double value, int offset)
		{
			exactApproxPair[offset] = value;
		}

		std::array<double, 2> exactApproxPair;
	};

	//update cached nominal deltas based on highAccuracy and recomputeAccurateDistances, caching what is needed given those flags
	inline void ComputeNominalDistanceTerms()
	{
		bool compute_accurate = NeedToPrecomputeAccurate();
		bool compute_approximate = NeedToPrecomputeApproximate();

		//infinite pValue means take max or min, so just use 1 for computations below,
		// and term aggregation (outside of this function) will take care of the rest
		double effective_p_value = pValue;
		if(pValue == std::numeric_limits<double>::infinity() || pValue == -std::numeric_limits<double>::infinity())
			effective_p_value = 1;

		const size_t num_features = featureParams.size();

		//value of delta for nominal values when not using high accuracy, may be not exactly 1.0 due to using FastPow approximation for effective_p_values that aren't 1
		double nominal_approximate_diff = 1.0;
		if(compute_approximate)
			nominal_approximate_diff = ( (effective_p_value == 1) ? 1.0 : FastPowNonZeroExp(1.0, effective_p_value) );

		for(size_t i = 0; i < num_features; i++)
		{
			auto &feat_params = featureParams[i];
			if(feat_params.featureType != FDT_NOMINAL)
				continue;

			double weight = feat_params.weight;

			if(!DoesFeatureHaveDeviation(i))
			{
				if(compute_accurate)
				{
					feat_params.nominalMatchDistanceTerm.SetValue(0.0, ExactApproxValuePair::EXACT);

					if(pValue != 0)
						feat_params.nominalNonMatchDistanceTerm.SetValue(weight, ExactApproxValuePair::EXACT);
					else //1.0 to any power is still 1.0 when computed exactly
						feat_params.nominalNonMatchDistanceTerm.SetValue(1.0, ExactApproxValuePair::EXACT);
				}

				if(compute_approximate)
				{
					feat_params.nominalMatchDistanceTerm.SetValue(0.0, ExactApproxValuePair::APPROX);

					if(effective_p_value != 0)
						feat_params.nominalNonMatchDistanceTerm.SetValue(weight * nominal_approximate_diff, ExactApproxValuePair::APPROX);
					else //pValue == 0
						feat_params.nominalNonMatchDistanceTerm.SetValue(FastPow(1.0, weight), ExactApproxValuePair::APPROX);
				}
			}
			else //has deviations
			{
				double deviation = feat_params.deviation;
				double nominal_count = feat_params.typeAttributes.nominalCount;

				// n = number of nominal classes
				// match: deviation ^ p * weight
				// non match: (deviation + (1 - deviation) / (n - 1)) ^ p * weight
				//if there is only one nominal class, the smallest delta value it could be is the specified smallest delta, otherwise it's 1.0
				constexpr double smallest_delta = 1e-100;
				if(nominal_count == 1 && deviation < smallest_delta)
					deviation = smallest_delta;

				double mismatch_deviation = 1.0;
				if(nominal_count > 1)
					mismatch_deviation = (deviation + (1 - deviation) / (nominal_count - 1));

				if(compute_accurate)
				{
					if(effective_p_value == 1)
					{
						feat_params.nominalMatchDistanceTerm.SetValue(deviation * weight, ExactApproxValuePair::EXACT);
						feat_params.nominalNonMatchDistanceTerm.SetValue(mismatch_deviation * weight, ExactApproxValuePair::EXACT);
					}
					else if(effective_p_value != 0)
					{
						feat_params.nominalMatchDistanceTerm.SetValue(std::pow(deviation, effective_p_value) * weight, ExactApproxValuePair::EXACT);
						feat_params.nominalNonMatchDistanceTerm.SetValue(std::pow(mismatch_deviation, effective_p_value) * weight, ExactApproxValuePair::EXACT);
					}
					else //pValue == 0
					{
						feat_params.nominalMatchDistanceTerm.SetValue(std::pow(deviation, weight), ExactApproxValuePair::EXACT);
						feat_params.nominalNonMatchDistanceTerm.SetValue(std::pow(mismatch_deviation, weight), ExactApproxValuePair::EXACT);
					}
				}

				if(compute_approximate)
				{
					if(effective_p_value == 1)
					{
						feat_params.nominalMatchDistanceTerm.SetValue(deviation * weight, ExactApproxValuePair::APPROX);
						feat_params.nominalNonMatchDistanceTerm.SetValue(mismatch_deviation * weight, ExactApproxValuePair::APPROX);
					}
					else if(effective_p_value != 0)
					{
						feat_params.nominalMatchDistanceTerm.SetValue(FastPow(deviation, effective_p_value) * weight, ExactApproxValuePair::APPROX);
						feat_params.nominalNonMatchDistanceTerm.SetValue(FastPow(mismatch_deviation, effective_p_value) * weight, ExactApproxValuePair::APPROX);
					}
					else //pValue == 0
					{
						feat_params.nominalMatchDistanceTerm.SetValue(FastPow(deviation, weight), ExactApproxValuePair::APPROX);
						feat_params.nominalNonMatchDistanceTerm.SetValue(FastPow(mismatch_deviation, weight), ExactApproxValuePair::APPROX);
					}
				}
			}
		}
	}

public:

	//query functions

	//returns true if the feature has a nonzero weight
	__forceinline bool IsFeatureEnabled(size_t feature_index)
	{
		return (featureParams[feature_index].weight > 0.0);
	}

	//returns true if the feature is nominal
	__forceinline bool IsFeatureNominal(size_t feature_index)
	{
		return (featureParams[feature_index].featureType == FDT_NOMINAL);
	}

	//returns true if the feature is cyclic
	__forceinline bool IsFeatureCyclic(size_t feature_index)
	{
		return (featureParams[feature_index].featureType == FDT_CONTINUOUS_NUMERIC_CYCLIC);
	}

	//returns true if the feature has a deviation
	__forceinline bool DoesFeatureHaveDeviation(size_t feature_index)
	{
		return (featureParams[feature_index].deviation > 0);
	}

	//returns true if a known to unknown distance term would be less than or same as an exact match
	// based on the difference versus deviation
	__forceinline bool IsKnownToUnknownDistanceLessThanOrEqualToExactMatch(size_t feature_index)
	{
		auto &feature_params = featureParams[feature_index];
		return (feature_params.knownToUnknownDifference <= feature_params.deviation);
	}

	//computes the exponentiation of d to 1/p
	__forceinline double InverseExponentiateDistance(double d)
	{
		if(pValue == 1)
			return d;

		if(pValue == 0.5)
			return d * d;

		if(defaultPrecision == ExactApproxValuePair::EXACT)
			return std::pow(d, inversePValue);
		else
			return fastPowInverseP.FastPow(d);
	}

	//computes the exponentiation of d to p given precision being from ExactApproxValuePair
	__forceinline double ExponentiateDifferenceTerm(double d, int precision)
	{
		if(pValue == 1)
			return d;

		if(pValue == 2)
			return d * d;

		if(precision == ExactApproxValuePair::EXACT)
			return std::pow(d, pValue);
		else
			return fastPowP.FastPow(d);
	}

	//returns the maximum difference
	inline double GetMaximumDifference(size_t index)
	{
		auto &feature_params = featureParams[index];
		switch(feature_params.featureType)
		{
		case FDT_NOMINAL:
			return 1.0;

		case FDT_CONTINUOUS_NUMERIC_CYCLIC:
			return feature_params.typeAttributes.maxCyclicDifference / 2;

		default:
			if(feature_params.weight > 0)
				return std::numeric_limits<double>::infinity();
			else
				return -std::numeric_limits<double>::infinity();
		}
	}
	//computes the distance term for a nominal when two nominals are equal
	__forceinline double ComputeDistanceTermNominalExactMatch(size_t index)
	{
		return featureParams[index].nominalMatchDistanceTerm.GetValue(defaultPrecision);
	}

	//computes the distance term for a nominal when two nominals are not equal
	__forceinline double ComputeDistanceTermNominalNonMatch(size_t index)
	{
		return featureParams[index].nominalNonMatchDistanceTerm.GetValue(defaultPrecision);
	}

	//computes the distance term for an unknown-unknown
	__forceinline double ComputeDistanceTermUnknownToUnknown(size_t index)
	{
		return featureParams[index].unknownToUnknownDistanceTerm.GetValue(defaultPrecision);
	}

	//computes the distance term for an known-unknown
	__forceinline double ComputeDistanceTermKnownToUnknown(size_t index)
	{
		return featureParams[index].knownToUnknownDistanceTerm.GetValue(defaultPrecision);
	}

	//computes the inner term for a non-nominal with an exact match of values
	__forceinline double ComputeDistanceTermNonNominalExactMatch(size_t index)
	{
		if(!DoesFeatureHaveDeviation(index))
			return 0.0;

		//apply deviations
		double diff = ComputeDeviationPart(0.0, featureParams[index].deviation);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//computes the difference between two values non-nominal (e.g., continuous)
	__forceinline double ComputeDifferenceTermNonNominal(double diff, size_t index)
	{
		//compute absolute value
		diff = std::abs(diff);

		//apply cyclic wrapping
		if(IsFeatureCyclic(index))
			diff = ConstrainDifferenceToCyclicDifference(diff, featureParams[index].typeAttributes.maxCyclicDifference);

		//apply deviations
		if(DoesFeatureHaveDeviation(index))
			diff += ComputeDeviationPart(diff, featureParams[index].deviation);

		return diff;
	}

	//computes the difference between two values non-nominal (e.g., continuous) that isn't cyclic
	__forceinline double ComputeDifferenceTermNonNominalNonCyclic(double diff, size_t index)
	{
		//compute absolute value
		diff = std::abs(diff);

		//apply deviations
		if(DoesFeatureHaveDeviation(index))
			diff += ComputeDeviationPart(diff, featureParams[index].deviation);

		return diff;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite with no nulls
	// diff can be negative
	__forceinline double ComputeDistanceTermNonNominalNonNullRegular(double diff, size_t index)
	{
		diff = ComputeDifferenceTermNonNominal(diff, index);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite with max of one null
	// diff can be negative
	__forceinline double ComputeDistanceTermNonNominalOneNonNullRegular(double diff, size_t index)
	{
		diff = ComputeDifferenceTermNonNominal(diff, index);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite that isn't cyclic with no nulls
	// diff can be negative
	__forceinline double ComputeDistanceTermNonNominalNonCyclicNonNullRegular(double diff, size_t index)
	{
		diff = ComputeDifferenceTermNonNominalNonCyclic(diff, index);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite that isn't cyclic with max of one null
	// diff can be negative
	__forceinline double ComputeDistanceTermNonNominalNonCyclicOneNonNullRegular(double diff, size_t index)
	{
		if(FastIsNaN(diff))
			return ComputeDistanceTermKnownToUnknown(index);

		diff = ComputeDifferenceTermNonNominalNonCyclic(diff, index);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//computes the inner term of the Minkowski norm summation for a single index for p=0
	__forceinline double ComputeDistanceTermP0(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index)
	{
		double diff = ComputeDifference(a, b, a_type, b_type, featureParams[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index);

		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return (diff == 0.0) ? ComputeDistanceTermNominalExactMatch(index) : ComputeDistanceTermNominalNonMatch(index);

		diff = ComputeDifferenceTermNonNominal(diff, index);

		return std::pow(diff, featureParams[index].weight);
	}

	//computes the inner term of the Minkowski norm summation for a single index for p=infinity or -infinity
	__forceinline double ComputeDistanceTermPInf(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index)
	{
		double diff = ComputeDifference(a, b, a_type, b_type, featureParams[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index);

		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return (diff == 0.0) ? ComputeDistanceTermNominalExactMatch(index) : ComputeDistanceTermNominalNonMatch(index);

		diff = ComputeDifferenceTermNonNominal(diff, index);

		return diff * featureParams[index].weight;
	}

	//computes the inner term of the Minkowski norm summation for a single index regardless of pValue
	__forceinline double ComputeDistanceTermNonNull(double diff, size_t index, int precision)
	{
		if(!IsFeatureNominal(index))
			diff = ComputeDifferenceTermNonNominal(diff, index);

		if(pValue == 0.0)
			return std::pow(diff, featureParams[index].weight);
		else if(pValue == std::numeric_limits<double>::infinity()
				|| pValue == -std::numeric_limits<double>::infinity())
			return diff * featureParams[index].weight;
		else
			return ExponentiateDifferenceTerm(diff, precision) * featureParams[index].weight;
	}

	//computes the inner term of the Minkowski norm summation for a single index for p non-zero and non-infinite
	//where at least one of the values is non-null
	__forceinline double ComputeDistanceTermRegularOneNonNull(double diff, size_t index)
	{
		if(FastIsNaN(diff))
			return ComputeDistanceTermKnownToUnknown(index);

		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return (diff == 0.0) ? ComputeDistanceTermNominalExactMatch(index) : ComputeDistanceTermNominalNonMatch(index);

		return ComputeDistanceTermNonNominalNonNullRegular(diff, index);
	}

	//computes the inner term of the Minkowski norm summation for a single index for p non-zero and non-infinite
	__forceinline double ComputeDistanceTermRegular(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index)
	{
		double diff = ComputeDifference(a, b, a_type, b_type, featureParams[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index);;

		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return (diff == 0.0) ? ComputeDistanceTermNominalExactMatch(index) : ComputeDistanceTermNominalNonMatch(index);

		return ComputeDistanceTermNonNominalNonNullRegular(diff, index);
	}

	//computes the inner term of the Minkowski norm summation for a single index that isn't null,
	//but computes only from the distance (does not take into account feature measurement type)
	__forceinline double ComputeDistanceTermFromNonNullDifferenceOnly(double diff, size_t index)
	{
		if(pValue == 0.0)
			return std::pow(diff, featureParams[index].weight);
		else if(pValue == std::numeric_limits<double>::infinity()
				|| pValue == -std::numeric_limits<double>::infinity())
			return diff * featureParams[index].weight;
		else
			return ExponentiateDifferenceTerm(diff, defaultPrecision) * featureParams[index].weight;
	}

	//returns the distance term for the either one or two unknown values
	__forceinline double LookupNullDistanceTerm(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index)
	{
		bool a_unknown = (a_type == ENIVT_NULL || (a_type == ENIVT_NUMBER && FastIsNaN(a.number)));
		bool b_unknown = (b_type == ENIVT_NULL || (b_type == ENIVT_NUMBER && FastIsNaN(b.number)));
		if(a_unknown && b_unknown)
			return ComputeDistanceTermUnknownToUnknown(index);
		if(a_unknown || b_unknown)
			return ComputeDistanceTermKnownToUnknown(index);

		//incompatible types, use whichever is further
		return std::max(ComputeDistanceTermUnknownToUnknown(index), ComputeDistanceTermKnownToUnknown(index));
	}

	//computes the difference between a and b given their types and the distance_type and the feature difference type
	__forceinline static double ComputeDifference(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, FeatureDifferenceType feature_type)
	{
		if(feature_type == FDT_CONTINUOUS_UNIVERSALLY_NUMERIC_INTERNED || feature_type == FDT_CONTINUOUS_UNIVERSALLY_NUMERIC
			|| feature_type == FDT_CONTINUOUS_NUMERIC || feature_type == FDT_CONTINUOUS_NUMERIC_INTERNED
			|| feature_type == FDT_CONTINUOUS_NUMERIC_CYCLIC)
		{
			if(a_type == ENIVT_NUMBER && b_type == ENIVT_NUMBER)
				return a.number - b.number;

			if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
				return (a.stringID == b.stringID ? 0.0 : 1.0);

			return std::numeric_limits<double>::quiet_NaN();
		}

		if(a_type == ENIVT_NULL || b_type == ENIVT_NULL)
			return std::numeric_limits<double>::quiet_NaN();

		if(feature_type == FDT_NOMINAL)
		{
			if(a_type == ENIVT_NUMBER && b_type == ENIVT_NUMBER)
				return (a.number == b.number ? 0.0 : 1.0);

			if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
				return (a.stringID == b.stringID ? 0.0 : 1.0);

			if(a_type == ENIVT_CODE && b_type == ENIVT_CODE)
				return (EvaluableNode::AreDeepEqual(a.code, b.code) ? 0.0 : 1.0);

			//don't match
			return 1.0;
		}

		if(feature_type == FDT_CONTINUOUS_STRING)
		{
			if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
			{
				auto &a_str = string_intern_pool.GetStringFromID(a.stringID);
				auto &b_str = string_intern_pool.GetStringFromID(b.stringID);
				return static_cast<double>(EvaluableNodeTreeManipulation::EditDistance(a_str, b_str));
			}

			return std::numeric_limits<double>::quiet_NaN();
		}

		//everything below is for feature_type == FDT_CONTINUOUS_CODE

		if(a_type == ENIVT_NUMBER && b_type == ENIVT_NUMBER)
			return 1.0 - EvaluableNodeTreeManipulation::CommonalityBetweenNumbers(a.number, b.number);

		if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
			return (a.stringID == b.stringID ? 0.0 : 1.0);

		if(a_type == ENIVT_CODE || b_type == ENIVT_CODE)
		{
			//if one isn't code, then just return the size of the other, or at least 1
			if(a_type != ENIVT_CODE)
				return std::max(1.0, static_cast<double>(EvaluableNode::GetDeepSize(b.code)));
			if(b_type != ENIVT_CODE)
				return std::max(1.0, static_cast<double>(EvaluableNode::GetDeepSize(a.code)));
			
			return EvaluableNodeTreeManipulation::EditDistance(a.code, b.code);
		}

		//different immediate types
		return 1.0;
	}

	//computes the Minkowski distance between vectors a and b, and respective types a_types and b_types, with Minkowski parameter p
	// calling the fastest version that will work with the data provided
	//a, a_types, b, and b_types must be the same length
	//if weights.size() == 0, no weights are used, else weights.size() must == a.size() == b.size()
	//	-a multiplicative weight is applied to each a and b dimensional differences
	//if nominal_dimensions.size() == 0, no nominal features are used, else nominal_features.size() must == a.size() == b.size()
	//	-uses featured nominal dimensions: if a dimension is marked in nominal_dimensions as true, partial distance is binary: 0.0 if equal, 1.0 otherwise
	//if deviations.size() == 0, no deviations are used, else deviations.size() must == a.size() == b.size()
	//	-uses per-feature deviations: per-feature deviation is added after the distance between ai and bi is computed
	__forceinline double ComputeMinkowskiDistance(std::vector<EvaluableNodeImmediateValue> &a, std::vector<EvaluableNodeImmediateValueType> &a_types,
		std::vector<EvaluableNodeImmediateValue> &b, std::vector<EvaluableNodeImmediateValueType> &b_types)
	{
		if(a.size() != b.size())
			return std::numeric_limits<double>::quiet_NaN();

		if(pValue == 0.0)
		{
			double dist_accum = 1.0;
			for(size_t i = 0; i < a.size(); i++)
				dist_accum *= ComputeDistanceTermP0(a[i], b[i], a_types[i], b_types[i], i);

			return dist_accum;
		}
		else if(pValue == std::numeric_limits<double>::infinity())
		{
			double max_term = -std::numeric_limits<double>::infinity();

			for(size_t i = 0; i < a.size(); i++)
			{
				double term = ComputeDistanceTermPInf(a[i], b[i], a_types[i], b_types[i], i);

				if(term > max_term)
					max_term = term;
			}

			return max_term;
		}
		else if(pValue == -std::numeric_limits<double>::infinity())
		{
			double min_term = std::numeric_limits<double>::infinity();

			for(size_t i = 0; i < a.size(); i++)
			{
				double term = ComputeDistanceTermPInf(a[i], b[i], a_types[i], b_types[i], i);

				if(term < min_term)
					min_term = term;
			}

			return min_term;
		}
		else //non-extreme p-value
		{
			double dist_accum = 0.0;
			for(size_t i = 0; i < a.size(); i++)
				dist_accum += ComputeDistanceTermRegular(a[i], b[i], a_types[i], b_types[i], i);

			return InverseExponentiateDistance(dist_accum);
		}
	}

	class FeatureParams
	{
	public:
		inline FeatureParams()
			: featureType(FDT_CONTINUOUS_NUMERIC), weight(1.0),
			internedNumberIndexToNumberValue(nullptr), deviation(0.0),
			unknownToUnknownDistanceTerm(std::numeric_limits<double>::quiet_NaN()),
			knownToUnknownDistanceTerm(std::numeric_limits<double>::quiet_NaN()),
			unknownToUnknownDifference(std::numeric_limits<double>::quiet_NaN()),
			knownToUnknownDifference(std::numeric_limits<double>::quiet_NaN())
		{
			typeAttributes.maxCyclicDifference = std::numeric_limits<double>::quiet_NaN();
		}

		//the type of comparison for each feature
		// this type is 64-bit aligned to make sure the whole structure is aligned
		FeatureDifferenceType featureType;

		//weight of the feature
		double weight;

		//distance terms for nominals
		ExactApproxValuePair nominalMatchDistanceTerm;
		ExactApproxValuePair nominalNonMatchDistanceTerm;

		//pointer to a lookup table of indices to values if the feature is an interned number
		std::vector<double> *internedNumberIndexToNumberValue;
		int precomputedNumberInternDistanceTermsPrecision;
		std::vector<double> precomputedNumberInternDistanceTerms;

		//type attributes dependent on featureType
		union
		{
			//number of relevant nominal values
			double nominalCount;

			//maximum difference value of the feature for cyclic features (NaN if unknown)
			double maxCyclicDifference;

		} typeAttributes;
		
		//uncertainty of each value
		double deviation;

		//distance term to use if both values being compared are unknown
		ExactApproxValuePair unknownToUnknownDistanceTerm;

		//distance term to use if one value is known and the other is unknown
		ExactApproxValuePair knownToUnknownDistanceTerm;

		//difference between two values if both are unknown (NaN if unknown)
		double unknownToUnknownDifference;

		//difference between two values if one is known and the other is unknown (NaN if unknown)
		double knownToUnknownDifference;
	};

	std::vector<FeatureParams> featureParams;

	//precached ways to compute FastPow
	RepeatedFastPow fastPowP;
	RepeatedFastPow fastPowInverseP;

	//parameter of the Lebesgue space and Minkowski distance parameter
	double pValue;
	//computed inverse of pValue
	double inversePValue;

	//the current precision for exact vs approximate terms
	int defaultPrecision;

	//if true, then all computations should be performed with high accuracy
	bool highAccuracy;
	//if true, then estimates should be computed with low accuracy, but final results with high accuracy
	// if false, will reuse accuracy from estimates
	bool recomputeAccurateDistances;
};
