#pragma once

//project headers:
#include "FastMath.h"
#include "RandomStream.h"

//system headers:
#include <limits>
#include <map>
#include <vector>

//default Functor for the default type for ProbabilityAsDoubleFunctor to transform probability values into doubles
class DoubleAsDouble
{
public:
	constexpr double operator()(double value)
	{
		return value;
	}
};

//Will return a random index, weighted by the values in probabilities based on the specified RandomStream
// if normalize is true, then it will normalize the probabilities in place
template<typename ContainerType>
size_t WeightedDiscreteRandomSample(ContainerType &probabilities, RandomStream &rs, bool normalize = false)
{
	if(normalize)
		NormalizeVector<ContainerType>(probabilities, 1.0);

	double r = rs.Rand();
	size_t selected_element = 0;
	double probability_mass = 0.0;

	for(; selected_element < probabilities.size(); selected_element++)
	{
		probability_mass += probabilities[selected_element];

		if(r <= probability_mass)
			return selected_element;
	}

	//should only make it here when the numerical precision is off (i.e., didn't add up to 1 exactly)
	//moved past the end, so return the one prior
	return selected_element - 1;
}

//Will return a random index, weighted by the values in probabilities based on the specified RandomStream
// if normalize is true, then it will normalize the probabilities in place
// requires that probabilities_map be non-empty
template<typename ContainerType, typename ValueType>
ValueType WeightedDiscreteRandomSampleMap(ContainerType &probabilities_map, RandomStream &rs, bool normalize = false)
{
	if(normalize)
		NormalizeVectorAsMap<ContainerType, ValueType>(probabilities_map, 1.0);

	double r = rs.Rand();
	ValueType selected_element = 0;
	double probability_mass = 0.0;

	for(auto &[key, prob] : probabilities_map)
	{
		selected_element = key;
		probability_mass += prob;

		if(r <= probability_mass)
			return key;
	}

	//should only make it here when the numerical precision is off (i.e., didn't add up to 1 exactly)
	//just grab the first available
	return selected_element;
}

//Class for creating a stream of random values (of type ValueType) based on weighted buckets of values specified by ValueType
//Implements the Alias method as described in
// Vose, Michael D. (September 1991). "A linear algorithm for generating random numbers with a given distribution" (PDF). IEEE Transactions on Software Engineering. 17 (9): 972–975.
template<typename ValueType, typename MapType = std::map<ValueType, double>, typename ProbabilityAsDoubleFunctor = DoubleAsDouble>
class WeightedDiscreteRandomStreamTransform
{
public:
	
	WeightedDiscreteRandomStreamTransform()
	{ }

	WeightedDiscreteRandomStreamTransform(const MapType &map, bool normalize = false)
	{
		Initialize(map, normalize);
	}

	//initializes like the constructor
	void Initialize(const MapType &map, bool normalize = false)
	{
		//pull out data from input
		std::vector<double> probabilities;
		probabilities.reserve(map.size());
		valueTable.reserve(map.size());

		//iterate over all input and grab values
		ProbabilityAsDoubleFunctor transform_to_double;
		for(auto &[key, prob] : map)
		{
			valueTable.push_back(key);
			probabilities.push_back(transform_to_double(prob));
		}

		InitializeAliasTable(probabilities, normalize);
	}

	WeightedDiscreteRandomStreamTransform(std::vector<ValueType> &values, std::vector<double> &probabilities, bool normalize = false)
	{
		valueTable = values;
		InitializeAliasTable(probabilities, normalize);
	}
	
	//pre-computes the alias tables given a probability distribution
	// if normalize is true, then it will sum all probabilities and divide by the sum such that they sum to 1.0
	// if num_elements is nonzero, then it will preallocate that number of elements
	void InitializeAliasTable(std::vector<double> &probabilities, bool normalize)
	{
		if(normalize)
			NormalizeVector(probabilities, 1.0);

		probabilityTable.resize(probabilities.size());
		aliasTable.resize(probabilities.size());

		//separate values into smaller and larger than what a uniform distribution would yield
		std::vector<size_t> small_probs;
		std::vector<size_t> large_probs;
		double uniform_probability = 1.0 / probabilities.size();
		for(size_t i = 0; i < probabilities.size(); i++)
		{
			if(probabilities[i] >= uniform_probability)
				large_probs.push_back(i);
			else
				small_probs.push_back(i);
		}

		//until have run out of probability
		while(!small_probs.empty() && !large_probs.empty())
		{
			size_t less = small_probs.back();
			small_probs.pop_back();
			size_t more = large_probs.back();
			large_probs.pop_back();

			//scale probabilities so that 1.0 is the value that would be given for a uniform distribution
			probabilityTable[less] = probabilities[less] * probabilities.size();
			aliasTable[less] = more;

			//adjust probabilities
			probabilities[more] = probabilities[more] + probabilities[less] - uniform_probability;

			//if excess probability, put it on the respective list
			if(probabilities[more] >= uniform_probability)
				large_probs.push_back(more);
			else
				small_probs.push_back(more);
		}

		///use any remaining probability mass
		while(!small_probs.empty())
		{
			probabilityTable[small_probs.back()] = 1.0;
			small_probs.pop_back();
		}

		while(!large_probs.empty())
		{
			probabilityTable[large_probs.back()] = 1.0;
			large_probs.pop_back();
		}
	}

	//returns true if initialized
	bool IsInitialized()
	{
		return aliasTable.size() > 0;
	}

	//returns a value based on the value's probability mass
	ValueType WeightedDiscreteRand(RandomStream &rs)
	{
		size_t bucket = (rs.RandUInt32() % probabilityTable.size());
		bool pick_alias = (rs.Rand() < probabilityTable[bucket]);

		size_t value_index = (pick_alias ? bucket : aliasTable[bucket]);
		return valueTable[value_index];
	}

private:
	//which element is aliased with the current position
	std::vector<size_t> aliasTable;

	//probability of each element
	std::vector<double> probabilityTable;

	//the value corresponding to each element in probabilityTable
	std::vector<ValueType> valueTable;
};
