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

class GeneralizedDistanceEvaluator
{
public:
	//general class of feature comparisons
	// align at 32-bits in order to play nice with data alignment where it is used
	enum FeatureDifferenceType : uint32_t
	{
		//nominal based on numeric equivalence
		FDT_NOMINAL_NUMERIC,
		//nominal based on string equivalence
		FDT_NOMINAL_STRING,
		//nominal based on code equivalence
		FDT_NOMINAL_CODE,
		//continuous without cycles, may contain nonnumeric data
		FDT_CONTINUOUS_NUMERIC,
		//like FDT_CONTINUOUS_NUMERIC, but has cycles
		FDT_CONTINUOUS_NUMERIC_CYCLIC,
		//edit distance between strings
		FDT_CONTINUOUS_STRING,
		//continuous measures of the number of nodes different between two sets of code
		FDT_CONTINUOUS_CODE,
	};

	//stores the computed exact and approximate distance terms
	// which can be referenced by getting the value at the corresponding offset
	//the values default to 0.0 on initialization
	class DistanceTerms
	{
	public:
		//offset for each precision level
		static constexpr int APPROX = 0;
		static constexpr int EXACT = 1;

		__forceinline DistanceTerms(double initial_value = 0.0)
		{
			distanceTerm = { initial_value, initial_value };
		}

		constexpr double GetValue(bool high_accuracy)
		{
			return distanceTerm[high_accuracy ? EXACT : APPROX];
		}

		constexpr double GetValue(int offset)
		{
			return distanceTerm[offset];
		}

		__forceinline void SetValue(double value, int offset)
		{
			distanceTerm[offset] = value;
		}

		__forceinline void SetValue(double value, bool high_accuracy)
		{
			distanceTerm[high_accuracy ? EXACT : APPROX] = value;
		}

		std::array<double, 2> distanceTerm;
	};

	//stores the computed exact and approximate distance terms, as well as the deviation
	//the values default to 0.0 on initialization
	class DistanceTermsWithDeviation
		: public DistanceTerms
	{
	public:
		__forceinline DistanceTermsWithDeviation(double initial_value = 0.0)
			: DistanceTerms(initial_value)
		{
			deviation = initial_value;
		}

		double deviation;
	};

	class FeatureAttributes
	{
	public:
		inline FeatureAttributes()
			: featureType(FDT_CONTINUOUS_NUMERIC),
			featureIndex(std::numeric_limits<size_t>::max()), weight(1.0), deviation(0.0), deviationReciprocal(0.0),
			unknownToUnknownDistanceTerm(std::numeric_limits<double>::quiet_NaN()),
			knownToUnknownDistanceTerm(std::numeric_limits<double>::quiet_NaN())
		{
			typeAttributes.maxCyclicDifference = std::numeric_limits<double>::quiet_NaN();
		}

		//returns true if the feature is nominal
		__forceinline bool IsFeatureNominal()
		{
			return (featureType <= GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE);
		}

		//returns true if the feature is nominal
		__forceinline bool IsFeatureContinuous()
		{
			return (featureType >= GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC);
		}

		//returns true if the feature is cyclic
		__forceinline bool IsFeatureCyclic()
		{
			return (featureType == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC_CYCLIC);
		}

		//returns true if the feature has a deviation
		__forceinline bool DoesFeatureHaveDeviation()
		{
			return (deviation > 0);
		}

		//returns true if the feature is a nominal that only has one difference value for match and one for nonmatch
		__forceinline bool IsFeatureSymmetricNominal()
		{
			if(!IsFeatureNominal())
				return false;

			return (nominalNumberSparseDeviationMatrix.size() == 0
				&& nominalStringSparseDeviationMatrix.size() == 0);
		}

		//the type of comparison for each feature
		// this type is 32-bit aligned to make sure the whole structure is aligned
		FeatureDifferenceType featureType;

		//index of the in an external location
		size_t featureIndex;

		//weight of the feature
		double weight;

		//distance terms for nominals
		DistanceTerms nominalSymmetricMatchDistanceTerm;
		DistanceTerms nominalSymmetricNonMatchDistanceTerm;

		//type attributes dependent on featureType
		union
		{
			//number of relevant nominal values
			double nominalCount;

			//maximum difference value of the feature for cyclic features (NaN if unknown)
			double maxCyclicDifference;

		} typeAttributes;

		//mean absolute error of predicting the value
		//if sparse deviation values are specified, this is the average value
		double deviation;
		//cached reciprocal for speed
		double deviationReciprocal;

		//contains the deviations for a given nominal value for each other nominal value
		//if the nominal value is not found, then the attribute defaultDeviation should be used
		template<typename NominalValueType, typename EqualComparison = std::equal_to<NominalValueType>>
		class SparseNominalDeviationValues : public SmallMap<NominalValueType, double, EqualComparison>
		{
		public:
			inline SparseNominalDeviationValues()
				: defaultDeviation(0.0)
			{	}

			double defaultDeviation;
		};

		//sparse deviation matrix if the nominal is a string
		//store as a vector of pairs instead of a map because either only one value will be looked up once,
		//in which case there's no advantage to having a map, or many distance term values will be looked up
		//repeatedly, which is handled by a RepeatedGeneralizedDistanceEvaluator, which uses a map
		SmallMap<StringInternPool::StringID, SparseNominalDeviationValues<StringInternPool::StringID>> nominalStringSparseDeviationMatrix;

		//sparse deviation matrix if the nominal is a number
		//store as a vector of pairs instead of a map because either only one value will be looked up once,
		//in which case there's no advantage to having a map, or many distance term values will be looked up
		//repeatedly, which is handled by a RepeatedGeneralizedDistanceEvaluator, which uses a map
		SmallMap<double, SparseNominalDeviationValues<double, DoubleNanHashComparator>, DoubleNanHashComparator> nominalNumberSparseDeviationMatrix;

		//distance term to use if both values being compared are unknown
		//the difference will be NaN if unknown
		DistanceTermsWithDeviation unknownToUnknownDistanceTerm;

		//distance term to use if one value is known and the other is unknown
		//the difference will be NaN if unknown
		DistanceTermsWithDeviation knownToUnknownDistanceTerm;
	};

	//initializes and precomputes relevant data including featureAttribs
	//this should be called after all relevant attributes have been populated
	inline void InitializeParametersAndFeatureParams()
	{
		inversePValue = 1.0 / pValue;

		if(NeedToPrecomputeApproximate())
		{
			fastPowP = RepeatedFastPow(pValue);
			fastPowInverseP = RepeatedFastPow(inversePValue);
		}

		ComputeAndStoreCommonDistanceTerms();
	}

	// 2/sqrt(pi) = 2.0 / std::sqrt(3.141592653589793238462643383279502884L);
	static constexpr double s_two_over_sqrt_pi = 1.12837916709551257390;

	//sqrt(2.0)
	static constexpr double s_sqrt_2 = 1.41421356237309504880;

	//surprisal in nats of each of the different distributions given the appropriate uncertainty
	//this is equal to the nats of entropy of the distribution plus the entropy of the uncertainty
	//in the case of Laplace, the Laplace distribution is one nat, and the mean absolute deviation is half of that,
	//therefore the value is 1.5
	//these values can be computed via ComputeDeviationPartLaplace(0.0, 1) for each of the corresponding methods
	//deviations other than 1 can be used, but then the result should be divided by that deviation, yielding the same value
	static constexpr double s_surprisal_of_laplace = 1.5;
	static constexpr double s_surprisal_of_laplace_approx = 1.500314205;
	static constexpr double s_surprisal_of_gaussian = 1.1283791670955126;
	static constexpr double s_surprisal_of_gaussian_approx = 1.128615528679644;

	//computes the Lukaszyk�Karmowski metric deviation component for the minkowski distance equation given the feature difference and feature deviation
	// and adds the deviation to diff. assumes deviation is nonnegative
	//if surprisal_transform is true, then it will transform the result into surprisal space and remove the appropriate assumption of uncertainty
	// for Laplace, the Laplace distribution has 1 nat worth of information, but additionally, there is a 50/50 chance that the
	// difference is within the mean absolute error, yielding an overcounting of an additional 1/2 nat.  So the total reduction is 1.5 nats
	__forceinline double ComputeDifferenceWithDeviation(double diff, size_t feature_index, bool surprisal_transform, bool high_accuracy)
	{
		auto &feature_attribs = featureAttribs[feature_index];
		double deviation = feature_attribs.deviation;
	#ifdef DISTANCE_USE_LAPLACE_LK_METRIC
		if(high_accuracy)
		{
			diff += std::exp(-diff / deviation) * (3 * deviation + diff) * 0.5;
			if(!surprisal_transform)
				return diff;
			else
				return (diff / deviation) - s_surprisal_of_laplace;
		}
		else //!high_accuracy
		{
			//multiplying by the reciprocal is lower accuracy due to rounding differences but faster
			//cast to float before taking the exponent since it's faster than a double, and because if the
			//difference divided by the deviation exceeds the single precision floating point range,
			//it will just set the term to zero, which is appropriate
			double deviation_reciprocal = feature_attribs.deviationReciprocal;
			diff += std::exp(static_cast<float>(-diff * deviation_reciprocal)) * (3 * deviation + diff) * 0.5;
			if(!surprisal_transform)
				return diff;
			else
				return (diff * deviation_reciprocal) - s_surprisal_of_laplace_approx;
		}
	#else
		const double term = diff / (2.0 * deviation); //diff / (2*sigma)
		if(high_accuracy)
		{
			//2*sigma*(e^(-1*(diff^2)/((2*simga)^2)))/sqrt(pi) - diff*erfc(diff/(2*sigma))
			diff += s_two_over_sqrt_pi * deviation * std::exp(-term * term) - diff * std::erfc(term);
			if(!surprisal_transform)
				return diff;
			else
				return (diff / deviation) - s_surprisal_of_gaussian;
		}
		else //!high_accuracy
		{
			//multiplying by the reciprocal is lower accuracy due to rounding differences but faster
			//cast to float before taking the exponent since it's faster than a double, and because if the
			//difference divided by the deviation exceeds the single precision floating point range,
			//it will just set the term to zero, which is appropriate
			//2*sigma*(e^(-1*(diff^2)/((2*simga)^2)))/sqrt(pi) - diff*erfc(diff/(2*sigma))
			diff += s_two_over_sqrt_pi * deviation * std::exp(static_cast<float>(-term * term)) - diff * std::erfc(term);
			if(!surprisal_transform)
				return diff;
			else
				//multiplying by the reciprocal is lower accuracy due to rounding differences but faster
				return (diff * feature_attribs.deviationReciprocal) - s_surprisal_of_gaussian_approx;
		}
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

	//returns true if the feature is nominal
	__forceinline bool IsFeatureNominal(size_t feature_index)
	{
		return featureAttribs[feature_index].IsFeatureNominal();
	}

	//returns true if the feature is nominal
	__forceinline bool IsFeatureContinuous(size_t feature_index)
	{
		return featureAttribs[feature_index].IsFeatureContinuous();
	}

	//returns true if the feature is cyclic
	__forceinline bool IsFeatureCyclic(size_t feature_index)
	{
		return featureAttribs[feature_index].IsFeatureCyclic();
	}

	//returns true if the feature has a deviation
	__forceinline bool DoesFeatureHaveDeviation(size_t feature_index)
	{
		return featureAttribs[feature_index].DoesFeatureHaveDeviation();
	}

	//returns true if the feature is a nominal that only has one difference value for match and one for nonmatch
	__forceinline bool IsFeatureSymmetricNominal(size_t feature_index)
	{
		return featureAttribs[feature_index].IsFeatureSymmetricNominal();
	}

	//returns true if a known to unknown distance term would be less than or same as an exact match
	// based on the difference versus deviation
	__forceinline bool IsKnownToUnknownDistanceLessThanOrEqualToExactMatch(size_t feature_index)
	{
		auto &feature_attribs = featureAttribs[feature_index];
		return (feature_attribs.knownToUnknownDistanceTerm.deviation <= feature_attribs.deviation);
	}

	//computes the exponentiation of d to 1/p
	__forceinline double InverseExponentiateDistance(double d, bool high_accuracy)
	{
		if(pValue == 1)
			return d;

		if(pValue == 0.5)
			return d * d;

		if(high_accuracy)
			return std::pow(d, inversePValue);
		else
			return fastPowInverseP.FastPowNonZeroExpNonnegativeBase(d);
	}

	//computes the exponentiation of d to p
	__forceinline double ExponentiateDifferenceTerm(double d, bool high_accuracy)
	{
		if(pValue == 1)
			return d;

		if(pValue == 2)
			return d * d;

		if(high_accuracy)
			return std::pow(d, pValue);
		else
			return fastPowP.FastPowNonZeroExpNonnegativeBase(d);
	}

	//returns the distance term given that it is nominal
	__forceinline double ComputeDistanceTermNominal(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index, bool high_accuracy)
	{
		bool a_is_null = EvaluableNodeImmediateValue::IsNull(a_type, a);
		bool b_is_null = EvaluableNodeImmediateValue::IsNull(b_type, b);
		if(a_is_null && b_is_null)
			return ComputeDistanceTermUnknownToUnknown(index, high_accuracy);

		bool are_equal = EvaluableNodeImmediateValue::AreEqual(a_type, a, b_type, b);

		auto &feature_attribs = featureAttribs[index];
		if(IsFeatureSymmetricNominal(index))
		{
			//if both were null, that was caught above, so one must be known
			if(a_is_null || b_is_null)
				return ComputeDistanceTermKnownToUnknown(index, high_accuracy);
			
			return are_equal ? feature_attribs.nominalSymmetricMatchDistanceTerm.GetValue(high_accuracy)
				: feature_attribs.nominalSymmetricNonMatchDistanceTerm.GetValue(high_accuracy);
		}

		//assume one nonmatching class in existence if not specified
		double nonmatching_classes = 1;
		if(featureAttribs[index].typeAttributes.nominalCount > 1)
			nonmatching_classes = featureAttribs[index].typeAttributes.nominalCount - 1;

		double prob_class_given_match = std::numeric_limits<double>::quiet_NaN();
		double prob_class_given_nonmatch = std::numeric_limits<double>::quiet_NaN();
		if(a_type == ENIVT_NUMBER && feature_attribs.nominalNumberSparseDeviationMatrix.size() > 0)
		{
			auto a_deviations_it = feature_attribs.nominalNumberSparseDeviationMatrix.find(a.number);
			if(a_deviations_it != std::end(feature_attribs.nominalNumberSparseDeviationMatrix))
			{
				auto &ndd = a_deviations_it->second;

				auto match_deviation_it = ndd.find(a.number);
				if(match_deviation_it != end(ndd))
					prob_class_given_match = 1 - match_deviation_it->second;
				else
					prob_class_given_match = 1 - ndd.defaultDeviation;					

				auto nonmatch_deviation_it = ndd.find(b.number);
				if(nonmatch_deviation_it != end(ndd))
					prob_class_given_nonmatch = 1 - nonmatch_deviation_it->second;
				else
					prob_class_given_nonmatch = (1 - ndd.defaultDeviation) / nonmatching_classes;
			}
		}
		else if(a_type == ENIVT_STRING_ID && feature_attribs.nominalStringSparseDeviationMatrix.size() > 0)
		{
			auto a_deviations_it = feature_attribs.nominalStringSparseDeviationMatrix.find(a.stringID);
			if(a_deviations_it != std::end(feature_attribs.nominalStringSparseDeviationMatrix))
			{
				auto &ndd = a_deviations_it->second;

				auto match_deviation_it = ndd.find(a.stringID);
				if(match_deviation_it != end(ndd))
					prob_class_given_match = 1 - match_deviation_it->second;
				else
					prob_class_given_match = 1 - ndd.defaultDeviation;

				auto nonmatch_deviation_it = ndd.find(b.stringID);
				if(nonmatch_deviation_it != end(ndd))
					prob_class_given_nonmatch = 1 - nonmatch_deviation_it->second;
				else
					prob_class_given_nonmatch = (1 - ndd.defaultDeviation) / nonmatching_classes;
			}
		}

		if(!FastIsNaN(prob_class_given_match))
		{
			if(are_equal)
				return ComputeDistanceTermNominalMatchFromMatchProbabilities(
					index, prob_class_given_match, high_accuracy);
			else if(!FastIsNaN(prob_class_given_nonmatch))
				return ComputeDistanceTermNominalNonmatchFromMatchProbabilities(
					index, prob_class_given_match, prob_class_given_nonmatch, high_accuracy);
		}

		//if both were null, that was caught above, so one must be known
		if(a_is_null || b_is_null)
			return ComputeDistanceTermKnownToUnknown(index, high_accuracy);

		//need to compute because didn't match any above
		if(are_equal)
			return ComputeDistanceTermNominalUniversallySymmetricExactMatch(index, high_accuracy);
		else
			return ComputeDistanceTermNominalUniversallySymmetricNonMatch(index, high_accuracy);
	}

	//exponentiats and weights the difference term contextually based on pValue
	//note that it has extra logic to account for extreme values like infinity, negative infinity, and 0
	__forceinline double ContextuallyExponentiateAndWeightDifferenceTerm(double dist_term, size_t index, bool high_accuracy)
	{
		if(dist_term == 0.0)
			return 0.0;

		double weight = featureAttribs[index].weight;
		if(pValue == 0)
		{
			if(high_accuracy)
				return std::pow(dist_term, weight);
			else
				return FastPow(dist_term, weight);
		}
		else if(pValue == std::numeric_limits<double>::infinity()
			|| pValue == -std::numeric_limits<double>::infinity())
		{
			//infinite pValues are treated the same as 1 for distance terms,
			//and are the same value regardless of high_accuracy
			return dist_term * weight;
		}
		else
		{
			return ExponentiateDifferenceTerm(dist_term, high_accuracy) * weight;
		}
	}

	//returns the maximum difference
	inline double GetMaximumDifference(size_t index)
	{
		if(IsFeatureNominal(index))
			return 1.0;

		if(IsFeatureCyclic(index))
			return featureAttribs[index].typeAttributes.maxCyclicDifference / 2;

		if(featureAttribs[index].weight > 0)
			return std::numeric_limits<double>::infinity();
		else
			return -std::numeric_limits<double>::infinity();
	}

	//returns the base of the distance term for nominal comparisons for a match
	//given the probablility of the class being observed given that it is a match
	__forceinline double ComputeDistanceTermNominalMatchFromMatchProbabilities(size_t index,
		double prob_class_given_match, bool high_accuracy)
	{
		double dist_term_base = 0.0;
		if(!computeSurprisal)
			dist_term_base = 1 - prob_class_given_match;

		return ContextuallyExponentiateAndWeightDifferenceTerm(dist_term_base, index, high_accuracy);
	}

	//computes the distance term
	// for a given prob_class_given_match, which is the probability that the classes compared should have been a match,
	// and prob_class_given_nonmatch, the probability that the particular comparison class does not match
	__forceinline double ComputeDistanceTermNominalNonmatchFromMatchProbabilities(size_t index,
		double prob_class_given_match, double prob_class_given_nonmatch, bool high_accuracy)
	{
		double dist_term_base = 0.0;
		if(computeSurprisal)
		{
			if(prob_class_given_match >= prob_class_given_nonmatch)
			{
				double surprisal_class_given_match = -std::log(prob_class_given_match);
				double surprisal_class_given_nonmatch = -std::log(prob_class_given_nonmatch);

				//the surprisal of the class matching on a different value is the difference between
				//how surprised it would be given a nonmatch but without the surprisal given a match
				dist_term_base = surprisal_class_given_nonmatch - surprisal_class_given_match;
			}
		}
		else
		{
			dist_term_base = 1.0 - prob_class_given_nonmatch;
		}

		return ContextuallyExponentiateAndWeightDifferenceTerm(dist_term_base, index, high_accuracy);
	}

	//computes the distance term for a nominal when two universally symmetric nominals are equal
	__forceinline double ComputeDistanceTermNominalUniversallySymmetricExactMatch(size_t index, bool high_accuracy)
	{
		double prob_class_given_match = 1;
		if(DoesFeatureHaveDeviation(index))
			prob_class_given_match = 1 - featureAttribs[index].deviation;

		double dist_term = ComputeDistanceTermNominalMatchFromMatchProbabilities(index, prob_class_given_match, high_accuracy);
		return ContextuallyExponentiateAndWeightDifferenceTerm(dist_term, index, high_accuracy);
	}

	//computes the distance term for a nominal when two universally symmetric nominals are not equal
	__forceinline double ComputeDistanceTermNominalUniversallySymmetricNonMatch(size_t index, bool high_accuracy)
	{
		auto &feature_attribs = featureAttribs[index];

		//assume one nonmatching class in existence if not specified
		double nonmatching_classes = 1;
		if(featureAttribs[index].typeAttributes.nominalCount > 1)
			nonmatching_classes = featureAttribs[index].typeAttributes.nominalCount - 1;

		double deviation = 0.0;
		if(DoesFeatureHaveDeviation(index))
			deviation = feature_attribs.deviation;

		//find probability that the correct class was selected
		double prob_class_given_match = 1 - deviation;

		//find the probability that any other class besides the correct class was selected
		//divide the probability among the other classes
		double prob_class_given_nonmatch = deviation / nonmatching_classes;

		return ComputeDistanceTermNominalNonmatchFromMatchProbabilities(index, prob_class_given_match, prob_class_given_nonmatch, high_accuracy);
	}

	//computes the distance term for an unknown-unknown
	__forceinline double ComputeDistanceTermUnknownToUnknown(size_t index, bool high_accuracy)
	{
		return featureAttribs[index].unknownToUnknownDistanceTerm.GetValue(high_accuracy);
	}

	//computes the distance term for an known-unknown
	__forceinline double ComputeDistanceTermKnownToUnknown(size_t index, bool high_accuracy)
	{
		return featureAttribs[index].knownToUnknownDistanceTerm.GetValue(high_accuracy);
	}

	//computes the inner term for a non-nominal with an exact match of values
	__forceinline double ComputeDistanceTermContinuousExactMatch(size_t index, bool high_accuracy)
	{
		if(!DoesFeatureHaveDeviation(index) || computeSurprisal)
			return 0.0;

		//apply deviations -- if computeSurprisal, will be caught above and always return 0.0
		double diff = ComputeDifferenceWithDeviation(0.0, index, false, high_accuracy);
		
		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, high_accuracy) * featureAttribs[index].weight;
	}

	//computes the base of the difference between two continuous values without exponentiation
	__forceinline double ComputeDifferenceTermBaseContinuous(double diff, size_t index, bool high_accuracy)
	{
		//compute absolute value
		diff = std::abs(diff);

		//apply cyclic wrapping
		if(IsFeatureCyclic(index))
			diff = ConstrainDifferenceToCyclicDifference(diff, featureAttribs[index].typeAttributes.maxCyclicDifference);

		//apply deviations
		if(DoesFeatureHaveDeviation(index))
			return ComputeDifferenceWithDeviation(diff, index, computeSurprisal, high_accuracy);
		else
			return diff;
	}

	//computes the base of the difference between two values non-nominal (e.g., continuous) that isn't cyclic
	__forceinline double ComputeDifferenceTermBaseContinuousNonCyclic(double diff, size_t index, bool high_accuracy)
	{
		//compute absolute value
		diff = std::abs(diff);

		//apply deviations
		if(DoesFeatureHaveDeviation(index))
			return ComputeDifferenceWithDeviation(diff, index, computeSurprisal, high_accuracy);
		else
			return diff;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite with no nulls
	// diff can be negative
	__forceinline double ComputeDistanceTermContinuousNonNullRegular(double diff, size_t index, bool high_accuracy)
	{
		diff = ComputeDifferenceTermBaseContinuous(diff, index, high_accuracy);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, high_accuracy) * featureAttribs[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite with max of one null
	// diff can be negative
	__forceinline double ComputeDistanceTermContinuousOneNonNullRegular(double diff, size_t index, bool high_accuracy)
	{
		diff = ComputeDifferenceTermBaseContinuous(diff, index, high_accuracy);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, high_accuracy) * featureAttribs[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite that isn't cyclic with no nulls
	// diff can be negative
	__forceinline double ComputeDistanceTermContinuousNonCyclicNonNullRegular(double diff, size_t index, bool high_accuracy)
	{
		diff = ComputeDifferenceTermBaseContinuousNonCyclic(diff, index, high_accuracy);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, high_accuracy) * featureAttribs[index].weight;
	}

	//computes the distance term for a non-nominal (e.g., continuous) for p non-zero and non-infinite that isn't cyclic with max of one null
	// diff can be negative
	__forceinline double ComputeDistanceTermContinuousNonCyclicOneNonNullRegular(double diff, size_t index, bool high_accuracy)
	{
		if(FastIsNaN(diff))
			return ComputeDistanceTermKnownToUnknown(index, high_accuracy);

		diff = ComputeDifferenceTermBaseContinuousNonCyclic(diff, index, high_accuracy);

		//exponentiate and return with weight
		return ExponentiateDifferenceTerm(diff, high_accuracy) * featureAttribs[index].weight;
	}

	//computes the inner term of the Minkowski norm summation for a single index for p=0
	__forceinline double ComputeDistanceTermP0(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index, bool high_accuracy)
	{
		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return ComputeDistanceTermNominal(a, b, a_type, b_type, index, high_accuracy);

		double diff = ComputeDifference(a, b, a_type, b_type, featureAttribs[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index, high_accuracy);

		diff = ComputeDifferenceTermBaseContinuous(diff, index, high_accuracy);

		return ContextuallyExponentiateAndWeightDifferenceTerm(diff, index, high_accuracy);
	}

	//computes the inner term of the Minkowski norm summation for a single index for p=infinity or -infinity
	__forceinline double ComputeDistanceTermPInf(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index, bool high_accuracy)
	{
		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return ComputeDistanceTermNominal(a, b, a_type, b_type, index, high_accuracy);

		double diff = ComputeDifference(a, b, a_type, b_type, featureAttribs[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index, high_accuracy);

		diff = ComputeDifferenceTermBaseContinuous(diff, index, high_accuracy);

		return ContextuallyExponentiateAndWeightDifferenceTerm(diff, index, high_accuracy);
	}

	//computes the inner term of the Minkowski norm when a term matches a null value
	//for a given deviation with regard to the null
	__forceinline double ComputeDistanceTermMatchOnNull(size_t index, double deviation, bool high_accuracy)
	{
		double diff = 0;
		if(IsFeatureNominal(index))
		{
			if(computeSurprisal)
			{
				//find probability that the correct class was selected
				double prob_class_given_match = 1 - deviation;
				diff = -std::log(prob_class_given_match);
			}
			else //nonsurprisal nominals just use the deviation as provided
			{
				diff = deviation;
			}
		}
		else
		{
			diff = ComputeDifferenceTermBaseContinuous(deviation, index, high_accuracy);
		}

		return ContextuallyExponentiateAndWeightDifferenceTerm(diff, index, high_accuracy);
	}

	//computes the inner term of the Minkowski norm summation for a single index for p non-zero and non-infinite
	__forceinline double ComputeDistanceTermRegular(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index, bool high_accuracy)
	{
		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(IsFeatureNominal(index))
			return ComputeDistanceTermNominal(a, b, a_type, b_type, index, high_accuracy);

		double diff = ComputeDifference(a, b, a_type, b_type, featureAttribs[index].featureType);
		if(FastIsNaN(diff))
			return LookupNullDistanceTerm(a, b, a_type, b_type, index, high_accuracy);

		return ComputeDistanceTermContinuousNonNullRegular(diff, index, high_accuracy);
	}

	//returns the distance term for the either one or two unknown values
	__forceinline double LookupNullDistanceTerm(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, size_t index, bool high_accuracy)
	{
		bool a_unknown = EvaluableNodeImmediateValue::IsNull(a_type, a);
		bool b_unknown = EvaluableNodeImmediateValue::IsNull(b_type, b);
		if(a_unknown && b_unknown)
			return ComputeDistanceTermUnknownToUnknown(index, high_accuracy);
		if(a_unknown || b_unknown)
			return ComputeDistanceTermKnownToUnknown(index, high_accuracy);

		//incompatible types, use whichever is further
		return std::max(ComputeDistanceTermUnknownToUnknown(index, high_accuracy), ComputeDistanceTermKnownToUnknown(index, high_accuracy));
	}

	//computes the difference between a and b given their types and the distance_type and the feature difference type
	__forceinline static double ComputeDifference(EvaluableNodeImmediateValue a, EvaluableNodeImmediateValue b,
		EvaluableNodeImmediateValueType a_type, EvaluableNodeImmediateValueType b_type, GeneralizedDistanceEvaluator::FeatureDifferenceType feature_type)
	{
		if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC
			|| feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMERIC_CYCLIC)
		{
			if(a_type == ENIVT_NUMBER && b_type == ENIVT_NUMBER)
				return a.number - b.number;

			if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
				return (a.stringID == b.stringID ? 0.0 : 1.0);

			return std::numeric_limits<double>::quiet_NaN();
		}

		if(a_type == ENIVT_NULL || b_type == ENIVT_NULL)
			return std::numeric_limits<double>::quiet_NaN();

		if(feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMERIC
			|| feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING
			|| feature_type == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE)
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

		if(feature_type == GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING)
		{
			if(a_type == ENIVT_STRING_ID && b_type == ENIVT_STRING_ID)
			{
				auto a_str = string_intern_pool.GetStringFromID(a.stringID);
				auto b_str = string_intern_pool.GetStringFromID(b.stringID);
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
		std::vector<EvaluableNodeImmediateValue> &b, std::vector<EvaluableNodeImmediateValueType> &b_types, bool high_accuracy)
	{
		if(a.size() != b.size())
			return std::numeric_limits<double>::quiet_NaN();

		if(pValue == 0.0)
		{
			double dist_accum = 1.0;
			for(size_t i = 0; i < a.size(); i++)
				dist_accum *= ComputeDistanceTermP0(a[i], b[i], a_types[i], b_types[i], i, high_accuracy);

			return dist_accum;
		}
		else if(pValue == std::numeric_limits<double>::infinity())
		{
			double max_term = -std::numeric_limits<double>::infinity();

			for(size_t i = 0; i < a.size(); i++)
			{
				double term = ComputeDistanceTermPInf(a[i], b[i], a_types[i], b_types[i], i, high_accuracy);

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
				double term = ComputeDistanceTermPInf(a[i], b[i], a_types[i], b_types[i], i, high_accuracy);

				if(term < min_term)
					min_term = term;
			}

			return min_term;
		}
		else //non-extreme p-value
		{
			double dist_accum = 0.0;
			for(size_t i = 0; i < a.size(); i++)
				dist_accum += ComputeDistanceTermRegular(a[i], b[i], a_types[i], b_types[i], i, high_accuracy);

			return InverseExponentiateDistance(dist_accum, high_accuracy);
		}
	}

	constexpr bool NeedToPrecomputeApproximate()
	{
		return (!highAccuracyDistances || recomputeAccurateDistances);
	}

	constexpr bool NeedToPrecomputeAccurate()
	{
		return (highAccuracyDistances || recomputeAccurateDistances);
	}

protected:

	//computes and caches symmetric nominal and uncertainty distance terms
	inline void ComputeAndStoreCommonDistanceTerms()
	{
		bool compute_accurate = NeedToPrecomputeAccurate();
		bool compute_approximate = NeedToPrecomputeApproximate();

		for(size_t i = 0; i < featureAttribs.size(); i++)
		{
			auto &feature_attribs = featureAttribs[i];
			if(feature_attribs.IsFeatureNominal())
			{
				if(computeSurprisal && !DoesFeatureHaveDeviation(i))
					feature_attribs.deviation = feature_attribs.unknownToUnknownDistanceTerm.deviation;

				//ensure if a feature has deviations they're not too small to underflow
				if(DoesFeatureHaveDeviation(i))
				{
					constexpr double smallest_delta = 1e-100;
					if(feature_attribs.typeAttributes.nominalCount == 1 && feature_attribs.deviation < smallest_delta)
						feature_attribs.deviation = smallest_delta;
				}

				if(compute_accurate)
				{
					feature_attribs.nominalSymmetricMatchDistanceTerm.SetValue(ComputeDistanceTermNominalUniversallySymmetricExactMatch(i, true), true);
					feature_attribs.nominalSymmetricNonMatchDistanceTerm.SetValue(ComputeDistanceTermNominalUniversallySymmetricNonMatch(i, true), true);
				}

				if(compute_approximate)
				{
					feature_attribs.nominalSymmetricMatchDistanceTerm.SetValue(ComputeDistanceTermNominalUniversallySymmetricExactMatch(i, false), false);
					feature_attribs.nominalSymmetricNonMatchDistanceTerm.SetValue(ComputeDistanceTermNominalUniversallySymmetricNonMatch(i, false), false);
				}
			}

			if(DoesFeatureHaveDeviation(i))
				feature_attribs.deviationReciprocal = 1.0 / feature_attribs.deviation;

			//compute unknownToUnknownDistanceTerm
			if(compute_accurate)
			{
				feature_attribs.unknownToUnknownDistanceTerm.SetValue(
					ComputeDistanceTermMatchOnNull(i, feature_attribs.unknownToUnknownDistanceTerm.deviation, true), true);
			}

			if(compute_approximate)
			{
				feature_attribs.unknownToUnknownDistanceTerm.SetValue(
					ComputeDistanceTermMatchOnNull(i, feature_attribs.unknownToUnknownDistanceTerm.deviation, false), false);
			}

			//if knownToUnknownDifference is same as unknownToUnknownDifference, can copy distance term instead of recomputing
			if(feature_attribs.knownToUnknownDistanceTerm.deviation == feature_attribs.unknownToUnknownDistanceTerm.deviation)
			{
				feature_attribs.knownToUnknownDistanceTerm = feature_attribs.unknownToUnknownDistanceTerm;
			}
			else
			{
				//compute knownToUnknownDistanceTerm
				if(compute_accurate)
				{
					feature_attribs.knownToUnknownDistanceTerm.SetValue(
						ComputeDistanceTermMatchOnNull(i, feature_attribs.knownToUnknownDistanceTerm.deviation, true), true);
				}

				if(compute_approximate)
				{
					feature_attribs.knownToUnknownDistanceTerm.SetValue(
						ComputeDistanceTermMatchOnNull(i, feature_attribs.knownToUnknownDistanceTerm.deviation, false), false);
				}
			}
		}
	}

public:

	std::vector<FeatureAttributes> featureAttribs;

	//precached ways to compute FastPow
	RepeatedFastPow fastPowP;
	RepeatedFastPow fastPowInverseP;

	//parameter of the Lebesgue space and Minkowski distance parameter
	double pValue;
	//computed inverse of pValue
	double inversePValue;

	//if true, it will perform computations resulting in surprisal before
	//the exponentiation
	bool computeSurprisal;

	//if true, then all computations should be performed with high accuracy
	bool highAccuracyDistances;
	//if true, then estimates should be computed with low accuracy, but final results with high accuracy
	// if false, will reuse accuracy from estimates
	bool recomputeAccurateDistances;
};

//base data struct for holding distance parameters and metadata
//generalizes Minkowski distance, information theoretic surprisal as a distance, and Lukaszyk�Karmowski
class RepeatedGeneralizedDistanceEvaluator
{
public:

	//an extension of values of GeneralizedDistanceEvaluator::FeatureDifferenceType
	//with differentiation on how the values can be computed
	enum EffectiveFeatureDifferenceType : uint32_t
	{
		//everything that isn't initially populated shares the same value
		//represented by precomputedRemainingIdenticalDistanceTerm
		EFDT_REMAINING_IDENTICAL_PRECOMPUTED,
		//everything is precomputed from interned values that are looked up
		EFDT_UNIVERSALLY_INTERNED_PRECOMPUTED,
		//continuous without cycles, but everything is always numeric
		EFDT_CONTINUOUS_UNIVERSALLY_NUMERIC,
		//continuous without cycles, may contain nonnumeric data
		EFDT_CONTINUOUS_NUMERIC,
		//like FDT_CONTINUOUS_NUMERIC, but has cycles
		EFDT_CONTINUOUS_NUMERIC_CYCLIC,
		//continuous precomputed (cyclic or not), may contain nonnumeric data
		EFDT_CONTINUOUS_NUMERIC_PRECOMPUTED,
		//nominal compared to a string value where nominals may not be symmetric
		EFDT_NOMINAL_STRING,
		//nominal compared to a number value where nominals may not be symmetric
		EFDT_NOMINAL_NUMERIC,
		//nominal based on code equivalence
		EFDT_NOMINAL_CODE,
		//edit distance between strings
		EFDT_CONTINUOUS_STRING,
		//continuous measures of the number of nodes different between two sets of code
		EFDT_CONTINUOUS_CODE,
	};

	RepeatedGeneralizedDistanceEvaluator()
		: distEvaluator(nullptr)
	{	}

	inline RepeatedGeneralizedDistanceEvaluator(GeneralizedDistanceEvaluator *dist_evaluator)
		: distEvaluator(dist_evaluator)
	{	}

	//for the feature index, computes and stores the distance terms for nominal values
	inline void ComputeAndStoreNominalDistanceTerms(size_t index)
	{
		bool high_accuracy = !distEvaluator->NeedToPrecomputeApproximate();

		//make sure there's room for the interned index
		if(featureData.size() <= index)
			featureData.resize(index + 1);

		auto &feature_attributes = distEvaluator->featureAttribs[index];
		auto &feature_data = featureData[index];

		//assume one nonmatching class in existence if not specified
		double nonmatching_classes = 1;
		if(feature_attributes.typeAttributes.nominalCount > 1)
			nonmatching_classes = feature_attributes.typeAttributes.nominalCount - 1;

		//since most of the computations will be using approximate if it is needed,
		//only set to high accuracy if not using approximate
		feature_data.precomputedNominalDistanceTermsHighAccuracy = high_accuracy;

		if(feature_data.targetValue.nodeType == ENIVT_NUMBER)
		{
			auto &sdm = feature_attributes.nominalNumberSparseDeviationMatrix;
			auto target_value = feature_data.targetValue.nodeValue.number;

			auto deviations_for_value = sdm.find(target_value);
			if(deviations_for_value != end(sdm))
			{
				auto &deviations = deviations_for_value->second;
				for(auto &[value, deviation] : deviations)
				{
					double dist_term = distEvaluator->ComputeDistanceTermNominal(target_value,
						value, ENIVT_NUMBER, ENIVT_NUMBER, index, high_accuracy);
					feature_data.nominalNumberDistanceTerms.emplace(value, dist_term);
				}

				double default_deviation = deviations_for_value->second.defaultDeviation;
				if(FastIsNaN(default_deviation))
				{
					deviations.defaultDeviation
						= distEvaluator->ComputeDistanceTermNominalUniversallySymmetricNonMatch(index, high_accuracy);
				}
				else
				{
					//find probability that the correct class was selected
					double prob_class_given_match = 1 - default_deviation;

					//find the probability that any other class besides the correct class was selected
					//divide the probability among the other classes
					double prob_class_given_nonmatch = default_deviation / nonmatching_classes;

					deviations.defaultDeviation
						= distEvaluator->ComputeDistanceTermNominalNonmatchFromMatchProbabilities(
							index, prob_class_given_match, prob_class_given_nonmatch, high_accuracy);
				}
			}
		}
		else if(feature_data.targetValue.nodeType == ENIVT_STRING_ID)
		{
			auto &sdm = feature_attributes.nominalStringSparseDeviationMatrix;
			auto target_sid = feature_data.targetValue.nodeValue.stringID;

			auto deviations_for_sid = sdm.find(target_sid);
			if(deviations_for_sid != end(sdm))
			{
				auto &deviations = deviations_for_sid->second;
				for(auto &[sid, deviation] : deviations)
				{
					double dist_term = distEvaluator->ComputeDistanceTermNominal(target_sid,
						sid, ENIVT_NUMBER, ENIVT_NUMBER, index, high_accuracy);
					feature_data.nominalStringDistanceTerms.emplace(sid, dist_term);
				}

				double default_deviation = deviations_for_sid->second.defaultDeviation;
				if(FastIsNaN(default_deviation))
				{
					deviations.defaultDeviation
						= distEvaluator->ComputeDistanceTermNominalUniversallySymmetricNonMatch(index, high_accuracy);
				}
				else
				{
					//find probability that the correct class was selected
					double prob_class_given_match = 1 - default_deviation;

					//find the probability that any other class besides the correct class was selected
					//divide the probability among the other classes
					double prob_class_given_nonmatch = default_deviation / nonmatching_classes;

					deviations.defaultDeviation
						= distEvaluator->ComputeDistanceTermNominalNonmatchFromMatchProbabilities(
							index, prob_class_given_match, prob_class_given_nonmatch, high_accuracy);
				}
			}
		}
	}

	//for the feature index, computes and stores the distance terms as measured from value to each interned value
	inline void ComputeAndStoreInternedNumberValuesAndDistanceTerms(size_t index, std::vector<double> *interned_values)
	{
		bool compute_accurate = distEvaluator->NeedToPrecomputeAccurate();
		bool compute_approximate = distEvaluator->NeedToPrecomputeApproximate();

		//make sure there's room for the interned index
		if(featureData.size() <= index)
			featureData.resize(index + 1);

		auto &feature_data = featureData[index];
		feature_data.internedNumberIndexToNumberValue = interned_values;

		if(interned_values == nullptr)
		{
			feature_data.internedDistanceTerms.clear();
			return;
		}

		feature_data.internedDistanceTerms.resize(interned_values->size());

		auto &feature_attribs = distEvaluator->featureAttribs[index];

		double value = feature_data.targetValue.GetValueAsNumber();
		if(FastIsNaN(value))
		{
			//first entry is unknown-unknown distance
			feature_data.internedDistanceTerms[0] = feature_attribs.unknownToUnknownDistanceTerm;
			
			auto k_to_unk = feature_attribs.knownToUnknownDistanceTerm;
			for(size_t i = 1; i < feature_data.internedDistanceTerms.size(); i++)
				feature_data.internedDistanceTerms[i] = k_to_unk;
		}
		else
		{
			//first entry is known-unknown distance
			feature_data.internedDistanceTerms[0] = feature_attribs.knownToUnknownDistanceTerm;

			for(size_t i = 1; i < feature_data.internedDistanceTerms.size(); i++)
			{
				double difference = value - (*interned_values)[i];
				if(compute_accurate)
					feature_data.internedDistanceTerms[i].SetValue(distEvaluator->ComputeDistanceTermContinuousNonNullRegular(difference, index, true), true);
				if(compute_approximate)
					feature_data.internedDistanceTerms[i].SetValue(distEvaluator->ComputeDistanceTermContinuousNonNullRegular(difference, index, false), false);
			}
		}
	}

	//returns true if the feature at index has interned number values
	__forceinline bool HasNumberInternValues(size_t index)
	{
		return featureData[index].internedNumberIndexToNumberValue != nullptr;
	}

	//returns the precomputed distance term for the interned value with intern_value_index
	__forceinline double ComputeDistanceTermInternedPrecomputed(size_t intern_value_index, size_t index, bool high_accuracy)
	{
		return featureData[index].internedDistanceTerms[intern_value_index].GetValue(high_accuracy);
	}

	//returns true if the nominal feature has a specific distance term when compared with unknown values
	__forceinline bool HasNominalSpecificKnownToUnknownDistanceTerm(size_t index)
	{
		auto &feature_data = featureData[index];
		return
			(	feature_data.nominalNumberDistanceTerms.find(std::numeric_limits<double>::quiet_NaN())
					!= end(feature_data.nominalNumberDistanceTerms)
				|| feature_data.nominalStringDistanceTerms.find(string_intern_pool.NOT_A_STRING_ID)
					!= end(feature_data.nominalStringDistanceTerms) );
	}

	//returns the distance term given that it is nominal
	__forceinline double ComputeDistanceTermNominal(EvaluableNodeImmediateValue other_value,
		EvaluableNodeImmediateValueType other_type, size_t index, bool high_accuracy)
	{
		auto &feature_data = featureData[index];
		if(feature_data.precomputedNominalDistanceTermsHighAccuracy == high_accuracy)
		{
			if(other_type == ENIVT_NUMBER)
			{
				auto dist_term_entry = feature_data.nominalNumberDistanceTerms.find(other_value.number);
				if(dist_term_entry != end(feature_data.nominalNumberDistanceTerms))
					return dist_term_entry->second;

				if(other_value.number == feature_data.targetValue.GetValueAsNumber())
					return distEvaluator->ComputeDistanceTermNominalUniversallySymmetricExactMatch(index, high_accuracy);
			}
			else if(other_type == ENIVT_STRING_ID)
			{
				auto dist_term_entry = feature_data.nominalStringDistanceTerms.find(other_value.stringID);
				if(dist_term_entry != end(feature_data.nominalStringDistanceTerms))
					return dist_term_entry->second;

				if(other_value.stringID == feature_data.targetValue.GetValueAsStringIDIfExists())
					return distEvaluator->ComputeDistanceTermNominalUniversallySymmetricExactMatch(index, high_accuracy);
			}

			if(EvaluableNodeImmediateValue::IsNull(other_type, other_value))
			{
				if(feature_data.targetValue.IsNull())
					return distEvaluator->ComputeDistanceTermUnknownToUnknown(index, high_accuracy);
				else
					return distEvaluator->ComputeDistanceTermKnownToUnknown(index, high_accuracy);
			}
			else
			{
				return distEvaluator->ComputeDistanceTermNominalUniversallySymmetricNonMatch(index, high_accuracy);
			}
		}
		else
		{
			return distEvaluator->ComputeDistanceTermNominal(feature_data.targetValue.nodeValue, other_value,
				feature_data.targetValue.nodeType, other_type, index, high_accuracy);
		}
	}

	//for all nominal distance term values that equal dist_term for the given high_accuracy,
	//it will call func passing in the numeric value
	template<typename Func>
	__forceinline void IterateOverNominalValuesWithLessOrEqualDistanceTermsNumeric(double dist_term, size_t index, bool high_accuracy,
		Func func)
	{
		auto &feature_data = featureData[index];
		for(auto &entry : feature_data.nominalNumberDistanceTerms)
		{
			if(entry.second <= dist_term)
				func(entry.first);
		}
	}

	//for all nominal distance term values that equal dist_term for the given high_accuracy,
	//it will call func passing in the string id value
	template<typename Func>
	__forceinline void IterateOverNominalValuesWithLessOrEqualDistanceTermsString(double dist_term, size_t index, bool high_accuracy,
		Func func)
	{
		auto &feature_data = featureData[index];
		for(auto &entry : feature_data.nominalStringDistanceTerms)
		{
			if(entry.second <= dist_term)
				func(entry.first);
		}
	}

	//returns the smallest distance term larger than compared_dist_term
	__forceinline double ComputeDistanceTermNonNullNominalNextSmallest(double compared_dist_term, size_t index, bool high_accuracy)
	{
		double next_smallest_dist_term = std::numeric_limits<double>::infinity();
		
		auto &feature_data = featureData[index];
		for(auto &entry : feature_data.nominalStringDistanceTerms)
		{
			if(entry.second > compared_dist_term)
			{
				if(entry.second < next_smallest_dist_term)
					next_smallest_dist_term  = entry.second;
			}
		}

		for(auto &entry : feature_data.nominalNumberDistanceTerms)
		{
			if(entry.second > compared_dist_term)
			{
				if(entry.second < next_smallest_dist_term)
					next_smallest_dist_term = entry.second;
			}
		}

		return distEvaluator->ComputeDistanceTermNominalUniversallySymmetricNonMatch(index, high_accuracy);
	}

	//returns the smallest nonmatching distance term regardless of value
	__forceinline double ComputeDistanceTermNominalNonNullSmallestNonmatch(size_t index, bool high_accuracy)
	{
		double match_dist_term = distEvaluator->ComputeDistanceTermNominalUniversallySymmetricExactMatch(index, high_accuracy);
		double smallest_nonmatch = ComputeDistanceTermNonNullNominalNextSmallest(match_dist_term, index, high_accuracy);

		//if there is no such value, return infinite
		if(smallest_nonmatch == match_dist_term)
			return std::numeric_limits<double>::infinity();

		return smallest_nonmatch;
	}

	//computes the inner term of the Minkowski norm summation
	__forceinline double ComputeDistanceTerm(EvaluableNodeImmediateValue other_value,
		EvaluableNodeImmediateValueType other_type, size_t index, bool high_accuracy)
	{
		auto &feature_data = featureData[index];

		//if nominal, don't need to compute absolute value of diff because just need to compare to 0
		if(distEvaluator->IsFeatureNominal(index))
			return ComputeDistanceTermNominal(other_value, other_type, index, high_accuracy);

		double diff = distEvaluator->ComputeDifference(feature_data.targetValue.nodeValue, other_value,
			feature_data.targetValue.nodeType, other_type, distEvaluator->featureAttribs[index].featureType);

		if(FastIsNaN(diff))
			return distEvaluator->LookupNullDistanceTerm(feature_data.targetValue.nodeValue, other_value,
				feature_data.targetValue.nodeType, other_type, index, high_accuracy);

		return distEvaluator->ComputeDistanceTermContinuousNonNullRegular(diff, index, high_accuracy);
	}

	//pointer to a valid, populated GeneralizedDistanceEvaluator
	GeneralizedDistanceEvaluator *distEvaluator;

	class FeatureData
	{
	public:

		FeatureData()
			: effectiveFeatureType(EFDT_CONTINUOUS_NUMERIC),
			internedNumberIndexToNumberValue(nullptr)
		{	}

		//clears all the feature data
		void Clear()
		{
			effectiveFeatureType = EFDT_CONTINUOUS_NUMERIC;
			precomputedRemainingIdenticalDistanceTerm = 0.0;
			internedNumberIndexToNumberValue = nullptr;
			internedDistanceTerms.clear();
			nominalStringDistanceTerms.clear();
			nominalNumberDistanceTerms.clear();
			precomputedNominalDistanceTermsHighAccuracy = false;
		}

		//sets the value for a precomputed distance term that will apply to the rest of the distance
		//evaluations and changes the feature type appropriately
		inline void SetPrecomputedRemainingIdenticalDistanceTerm(double dist_term)
		{
			effectiveFeatureType = EFDT_REMAINING_IDENTICAL_PRECOMPUTED;
			precomputedRemainingIdenticalDistanceTerm = dist_term;
		}

		//the effective comparison for the feature type, specialized for performance
		// this type is 32-bit aligned to make sure the whole structure is aligned
		EffectiveFeatureDifferenceType effectiveFeatureType;

		//target that the distance will be computed to
		EvaluableNodeImmediateValueWithType targetValue;

		//the distance term for EFDT_REMAINING_IDENTICAL_PRECOMPUTED
		double precomputedRemainingIdenticalDistanceTerm;

		std::vector<double> *internedNumberIndexToNumberValue;
		std::vector<GeneralizedDistanceEvaluator::DistanceTerms> internedDistanceTerms;

		//used to store distance terms for the respective targetValue for the sparse deviation matrix
		FastHashMap<StringInternPool::StringID, double> nominalStringDistanceTerms;
		FastHashMap<double, double> nominalNumberDistanceTerms;

		//if true, then nominalStringDistanceTerms and nominalNumberDistanceTerms are high accuracy, otherwise approximate
		bool precomputedNominalDistanceTermsHighAccuracy;
	};

	//for each feature, precomputed distance terms for each interned value looked up by intern index
	std::vector<FeatureData> featureData;
};
