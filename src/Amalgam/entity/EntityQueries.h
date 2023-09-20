#pragma once

//project headers:
#include "DistanceReferencePair.h"
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
	double GetConditionDistanceMeasure(Entity *e);

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

	std::vector<StringInternPool::StringID> positionLabels;	//the labels that comprise each dimension of the position
	std::vector<EvaluableNodeImmediateValue> valueToCompare;//sometimes used for position values in conjunction with positionLabels

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

	//if ENT_QUERY_SELECT or ENT_QUERY_SAMPLE has a random stream
	bool hasRandomStream;

	//ENT_QUERY_SELECT's or ENT_QUERY_SAMPLE's random stream
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

	//for ENT_QUERY_NEAREST_GENERALIZED_DISTANCE and ENT_QUERY_WITHIN_GENERALIZED_DISTANCE, if returnSortedList is true, additionally return this label if valid
	StringInternPool::StringID additionalSortedListLabel;

	//if conviction_of_removal is true, then it will compute the conviction as if the entities were removed, if false, will compute added or included
	bool convictionOfRemoval;

	//if true, use concurrency if applicable
	bool useConcurrency;
};

void SortEntitiesByID(std::vector<Entity *> &entities);

//converts a set of DistanceReferencePair into the appropriate EvaluableNode structure
template<typename EntityReference, typename GetEntityFunction>
static inline EvaluableNodeReference ConvertResultsToEvaluableNodes(
	std::vector<DistanceReferencePair<EntityReference>> &results,
	EvaluableNodeManager *enm, bool as_sorted_list, StringInternPool::StringID additional_sorted_list_label,
	GetEntityFunction get_entity)
{
	if(as_sorted_list)
	{
		//build list of results
		EvaluableNode *query_return = enm->AllocNode(ENT_LIST);
		auto &qr_ocn = query_return->GetOrderedChildNodesReference();
		qr_ocn.resize(additional_sorted_list_label == string_intern_pool.NOT_A_STRING_ID ? 2 : 3);

		qr_ocn[0] = CreateListOfStringsIdsFromIteratorAndFunction(results, enm,
			[get_entity](auto &drp) {  return get_entity(drp.reference)->GetIdStringId(); });
		qr_ocn[1] = CreateListOfNumbersFromIteratorAndFunction(results, enm, [](auto drp) { return drp.distance; });

		//if adding on a label, retrieve the values from the entities
		if(additional_sorted_list_label != string_intern_pool.NOT_A_STRING_ID)
		{
			//make a copy of the value at additionalSortedListLabel for each entity
			EvaluableNode *list_of_values = enm->AllocNode(ENT_LIST);
			qr_ocn[2] = list_of_values;
			auto &list_ocn = list_of_values->GetOrderedChildNodes();
			list_ocn.resize(results.size());
			for(size_t i = 0; i < results.size(); i++)
			{
				Entity *entity = get_entity(results[i].reference);
				list_ocn[i] = entity->GetValueAtLabel(additional_sorted_list_label, enm, false);

				//update cycle checks and idempotency
				if(list_ocn[i] != nullptr)
				{
					if(list_ocn[i]->GetNeedCycleCheck())
						query_return->SetNeedCycleCheck(true);

					if(!list_ocn[i]->GetIsIdempotent())
						query_return->SetIsIdempotent(false);
				}
			}
		}

		return EvaluableNodeReference(query_return, true);
	}
	else //return as assoc
	{
		return CreateAssocOfNumbersFromIteratorAndFunctions(results,
			[get_entity](auto &drp) { return get_entity(drp.reference)->GetIdStringId(); },
			[](auto &drp) { return drp.distance; },
			enm
		);
	}
}
