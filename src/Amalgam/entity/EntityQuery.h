#pragma once

//project headers:
#include "EvaluableNode.h"
#include "GeneralizedDistance.h"

//system headers:
#include <utility>
#include <variant>
#include <vector>

//data structure used for an entity query
struct EntityQuery
{
	//initialize the tag and active alternative without zeroing inactive distance storage.
	EntityQuery() {}

	template<typename Parameters, typename... Args>
	EntityQuery(EvaluableNodeType type, std::in_place_type_t<Parameters>, Args &&... args)
		: queryType(type), payload(std::in_place_type<Parameters>, std::forward<Args>(args)...) {}

	EntityQuery(EntityQuery &&) noexcept = default;
	EntityQuery &operator=(EntityQuery &&) noexcept = default;
	EntityQuery(const EntityQuery &) = delete;
	EntityQuery &operator=(const EntityQuery &) = delete;

	struct Filter
	{
		//the interpreter roots evaluated query arguments until execution finishes
		EvaluableNode *node;
		StringInternPool::StringID label;
	};

	using EntityIds = std::vector<StringInternPool::StringID>;

	struct Aggregate
	{
		StringInternPool::StringID valueLabel;
		StringInternPool::StringID weightLabel = StringInternPool::NOT_A_STRING_ID;
	};

	struct Quantile : Aggregate
	{
		double percentage;
	};

	struct Mean : Aggregate
	{
		double power;
		double center;
		bool calculateMoment;
		bool absoluteValue;
	};

	struct Difference
	{
		StringInternPool::StringID valueLabel;
		double limit;
		bool includeZero;
	};

	struct Extremum
	{
		StringInternPool::StringID valueLabel;
		size_t count;
		EvaluableNodeImmediateValueType valueType;
	};

	struct Select
	{
		size_t count;
		size_t startOffset;
		bool hasStartOffset;
		bool hasRandomStream;
		RandomStream randomStream;
	};

	struct Sample
	{
		StringInternPool::StringID valueLabel;
		size_t count;
		bool hasRandomStream;
		RandomStream randomStream;
	};

	struct Distance
	{
		Distance()
		{
			distEvaluator.pValue = distEvaluator.inversePValue = 0.0;
			distEvaluator.computeSurprisal = distEvaluator.transformSurprisalToProb = false;
			distEvaluator.highAccuracyDistances = distEvaluator.recomputeAccurateDistances = false;
			distEvaluator.populateOmittedFeatureValues = false;
		}

		struct Neighbors
		{
			Neighbors() {}

			StringInternPool::StringID entityIdToExclude = StringInternPool::NOT_A_STRING_ID;
			double maxDistance = std::numeric_limits<double>::quiet_NaN();
			bool populateOmittedFeatureValues = false;
			std::vector<EvaluableNodeImmediateValueWithType> valuesToCompare;
		};

		struct Statistics
		{
			EntityIds entityIds;
			bool convictionOfRemoval = false;
		};

		struct Positions
		{
			//borrows the ordered children of a rooted query argument.
			EvaluableNode::OrderedType *nodes = nullptr;
		};

		struct Clusters
		{
			double minWeight = 0.0;
		};

		std::variant<Neighbors, Statistics, Positions, Clusters> target;

		bool DoesEntityMatchCondition(Entity *e, Neighbors &neighbors);
		double GetConditionDistanceMeasure(Entity *e, bool high_accuracy,
			std::vector<EvaluableNodeImmediateValueWithType> &position);
		EvaluableNodeReference GetMatchingEntities(EvaluableNodeType query_type, Entity *container,
			std::vector<Entity *> &matching_entities, bool from_all_entities, EvaluableNodeManager *enm);

		//the labels that comprise each dimension of the position
		std::vector<StringInternPool::StringID> positionLabels;

		//feature callback nodes borrow the interpreter's rooted query arguments
		GeneralizedDistanceEvaluator distEvaluator;

		//minimum number of entities to attempt to retrieve
		size_t minToRetrieve = std::numeric_limits<size_t>::max();

		//incremental probability where, if the next entity is below this threshold, don't retrieve more
		double numToRetrieveMinIncrementalProbability = 0.0;

		//number of entities to attempt to retrieve after any constraints
		size_t extraToRetrieve = 0;

		//distance weight exponent for distance queries (takes distance and raises it to the respective exponent) when returning distances
		//only applicable when computeSurprisal is false
		double distanceWeightExponent = 1.0;

		//indicates whether a compute result should be returned as a sorted list
		bool returnSortedList = false;

		//for ENT_QUERY_NEAREST_GENERALIZED_DISTANCE and ENT_QUERY_WITHIN_GENERALIZED_DISTANCE, if returnSortedList is true,
		// additionally return these labels if valid
		std::vector<StringInternPool::StringID> additionalSortedListLabels;

		//interpreter calling the query
		Interpreter *interpreter = nullptr;

		//containing entity
		Entity *entity = nullptr;

		//if true, use concurrency if applicable
		bool useConcurrency = false;

		//radius label
		StringInternPool::StringID singleLabel = StringInternPool::NOT_A_STRING_ID;

		//a label representing a weight label
		StringInternPool::StringID weightLabel = StringInternPool::NOT_A_STRING_ID;

		//maximum number of entities to attempt to retrieve (based on queryType)
		size_t maxToRetrieve = 0;

		RandomStream randomStream;
	};

	//ENT_NULL also marks invalid queries whose parsed payload is still present
	EvaluableNodeType queryType = ENT_NULL;
	std::variant<std::monostate, Filter, EntityIds, Aggregate, Quantile, Mean, Difference, Extremum, Select, Sample, Distance> payload;

	Aggregate GetAggregateLabels() const
	{
		return std::visit([](const auto &parameters) {
			Aggregate labels{StringInternPool::NOT_A_STRING_ID};

			if constexpr(requires { parameters.valueLabel; })
				labels.valueLabel = parameters.valueLabel;

			if constexpr(requires { parameters.weightLabel; })
				labels.weightLabel = parameters.weightLabel;

			return labels;
		}, payload);
	}
};
