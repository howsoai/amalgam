#pragma once

//project headers:
#include "DistanceReferencePair.h"
#include "Entity.h"
#include "EvaluableNode.h"
#include "GeneralizedDistance.h"

//system headers:
#include <algorithm>
#include <functional>
#include <list>
#include <string>
#include <utility>
#include <vector>

//forward declarations:
class Entity;

//if set to false, will not allow use of the SBF datastore
extern bool _enable_SBF_datastore;

class EntityQueryCondition
{
public:
	EntityQueryCondition()
		: queryType(ENT_NULL)
	{	}

	//returns true if the entity matches the condition
	bool DoesEntityMatchCondition(Entity *e);

	//computes the distance measure of the condition
	// returns NaN if invalid
	double GetConditionDistanceMeasure(Entity *e, bool high_accuracy);

	EvaluableNodeReference GetMatchingEntities(Entity *container, std::vector<Entity *> &matching_entities,
		bool from_all_entities, EvaluableNodeManager *enm);

	EvaluableNodeType queryType;

	//label vector used for existence queries
	//**also aliased and used for the list of entity IDs to compute conviction for when type is ENT_COMPUTE_ENTITY_CONVICTIONS
	std::vector<StringInternPool::StringID> existLabels;

	//vector used to describe the types of each label or value
	std::vector<EvaluableNodeImmediateValueType> valueTypes;

	//pairs of ids and values
	std::vector<std::pair<StringInternPool::StringID, EvaluableNodeImmediateValue>> singleLabels;

	//pairs of ids and pairs of values
	std::vector<std::pair<StringInternPool::StringID, std::pair<EvaluableNodeImmediateValue, EvaluableNodeImmediateValue>>> pairedLabels;

	//the labels that comprise each dimension of the position
	std::vector<StringInternPool::StringID> positionLabels;

	//the labels corresponding to positionLabels when appropriate
	std::vector<EvaluableNodeImmediateValue> valueToCompare;

	GeneralizedDistance distParams;

	//a single standalone label in the query
	StringInternPool::StringID singleLabel;

	//when requesting a single type
	EvaluableNodeImmediateValueType singleLabelType;

	//a label of an id to exclude
	StringInternPool::StringID exclusionLabel;

	//a label representing a weight label
	StringInternPool::StringID weightLabel;

	//maximum distance between valueToCompare and the entity
	double maxDistance;

	//maximum number of entities to retrieve (based on queryType)
	double maxToRetrieve;

	//distance weight exponent for distance queries (takes distance and raises it to the respective exponent) when returning distances
	//only applicable when transformSuprisalToProb is false
	double distanceWeightExponent;

	//if true, the values will be transformed from surprisal to probability; if false, will perform a distance transform
	bool transformSuprisalToProb;

	//if ENT_QUERY_SELECT has a start offset
	bool hasStartOffset;

	//ENT_QUERY_SELECT's value of the start offset
	size_t startOffset;

	//whether ENT_QUERY_SELECT or ENT_QUERY_SAMPLE has a random stream; if not, it will use consistent order
	bool hasRandomStream;

	//the random stream for queries that use it
	RandomStream randomStream;

	//includes zero as a valid difference for ENT_QUERY_MIN_DIFFERENCE
	bool includeZeroDifferences;

	//quantile percentage, for ENT_QUERY_QUANTILE
	double qPercentage;

	//for ENT_QUERY_GENERALIZED_MEAN
	double center;
	bool calculateMoment;
	bool absoluteValue;

	//indicates whether a compute result should be returned as a sorted list
	bool returnSortedList;

	//for ENT_QUERY_NEAREST_GENERALIZED_DISTANCE and ENT_QUERY_WITHIN_GENERALIZED_DISTANCE, if returnSortedList is true,
	// additionally return these labels if valid
	std::vector<StringInternPool::StringID> additionalSortedListLabels;

	//if conviction_of_removal is true, then it will compute the conviction as if the entities were removed, if false,
	// will compute added or included
	bool convictionOfRemoval;

	//if true, use concurrency if applicable
	bool useConcurrency;
};
