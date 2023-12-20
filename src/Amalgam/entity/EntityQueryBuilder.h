#pragma once

//project headers:
#include "EntityQueries.h"
#include "EvaluableNode.h"
#include "StringInternPool.h"

//Constructs a query engine query condition from Amalgam evaluable nodes
namespace EntityQueryBuilder
{
	//parameter indices for distance queries
	enum DistParamIndices : size_t
	{
		MAX_TO_FIND_OR_MAX_DISTANCE,
		POSITION_LABELS,
		POSITION,

		WEIGHTS,
		DISTANCE_TYPES,
		ATTRIBUTES,
		DEVIATIONS,

		//optional params
		MINKOWSKI_PARAMETER,
		DISTANCE_VALUE_TRANSFORM,
		ENTITY_WEIGHT_LABEL_NAME,
		RANDOM_SEED,
		RADIUS_LABEL,
		NUMERICAL_PRECISION,
		
		NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS //always last - do not add after this
	};

	constexpr bool DoesDistanceQueryUseEntitiesInsteadOfPosition(EvaluableNodeType type)
	{
		return (type == ENT_COMPUTE_ENTITY_CONVICTIONS
			|| type == ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE
			|| type == ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS
			|| type == ENT_COMPUTE_ENTITY_KL_DIVERGENCES);
	}

	//populates the features of dist_params based on either num_elements or element_names for each of the
	// four different attribute parameters based on its type (using num_elements if list or immediate, element_names if assoc)
	inline void PopulateDistanceFeatureParameters(GeneralizedDistance &dist_params,
		size_t num_elements, std::vector<StringInternPool::StringID> &element_names,
		EvaluableNode *weights_node, EvaluableNode *distance_types_node, EvaluableNode *attributes_node, EvaluableNode *deviations_node)
	{
		dist_params.featureParams.resize(num_elements);

		//get weights
		EvaluableNode::ConvertChildNodesAndStoreValue(weights_node, element_names, num_elements,
			[&dist_params](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_params.featureParams.size())
				{
					if(found)
						dist_params.featureParams[i].weight = EvaluableNode::ToNumber(en);
					else
						dist_params.featureParams[i].weight = 1.0;
				}
			});

		//get type
		EvaluableNode::ConvertChildNodesAndStoreValue(distance_types_node, element_names, num_elements,
			[&dist_params](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_params.featureParams.size())
				{
					auto feature_type = GeneralizedDistance::FDT_CONTINUOUS_NUMERIC;
					if(found)
					{
						StringInternPool::StringID feature_type_id = EvaluableNode::ToStringIDIfExists(en);
						switch(feature_type_id)
						{
						case ENBISI_nominal:								feature_type = GeneralizedDistance::FDT_NOMINAL;					break;
						case ENBISI_continuous:								feature_type = GeneralizedDistance::FDT_CONTINUOUS_NUMERIC;			break;
						case ENBISI_cyclic:									feature_type = GeneralizedDistance::FDT_CONTINUOUS_NUMERIC_CYCLIC;	break;
						case GetStringIdFromNodeTypeFromString(ENT_STRING): feature_type = GeneralizedDistance::FDT_CONTINUOUS_STRING;			break;	
						case ENBISI_code:									feature_type = GeneralizedDistance::FDT_CONTINUOUS_CODE;			break;
						default:											feature_type = GeneralizedDistance::FDT_CONTINUOUS_NUMERIC;			break;
						}
					}
					dist_params.featureParams[i].featureType = feature_type;
				}
			});

		//get attributes
		EvaluableNode::ConvertChildNodesAndStoreValue(attributes_node, element_names, num_elements,
			[&dist_params](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_params.featureParams.size())
				{
					dist_params.featureParams[i].unknownToUnknownDifference = std::numeric_limits<double>::quiet_NaN();
					dist_params.featureParams[i].knownToUnknownDifference = std::numeric_limits<double>::quiet_NaN();

					//get attributes based on feature type
					switch(dist_params.featureParams[i].featureType)
					{
					case GeneralizedDistance::FDT_NOMINAL:
						if(found && !EvaluableNode::IsNull(en))
						{
							if(en->EvaluableNode::IsOrderedArray())
							{
								auto &ocn = en->GetOrderedChildNodesReference();
								size_t ocn_size = ocn.size();
								if(ocn_size > 0)
									dist_params.featureParams[i].typeAttributes.nominalCount = EvaluableNode::ToNumber(ocn[0]);
								if(ocn_size > 1)
									dist_params.featureParams[i].knownToUnknownDifference = EvaluableNode::ToNumber(ocn[1]);
								if(ocn_size > 2)
									dist_params.featureParams[i].unknownToUnknownDifference = EvaluableNode::ToNumber(ocn[2]);
							}
							else //treat as singular value
							{
								dist_params.featureParams[i].typeAttributes.nominalCount = EvaluableNode::ToNumber(en);
							}
						}
						else
						{
							dist_params.featureParams[i].typeAttributes.nominalCount = 0.0;
						}
						break;

					case GeneralizedDistance::FDT_CONTINUOUS_NUMERIC_CYCLIC:
						if(found && !EvaluableNode::IsNull(en))
						{
							if(en->EvaluableNode::IsOrderedArray())
							{
								auto &ocn = en->GetOrderedChildNodesReference();
								size_t ocn_size = ocn.size();
								if(ocn_size > 0)
									dist_params.featureParams[i].typeAttributes.maxCyclicDifference = EvaluableNode::ToNumber(ocn[0]);
								if(ocn_size > 1)
									dist_params.featureParams[i].knownToUnknownDifference = EvaluableNode::ToNumber(ocn[1]);
								if(ocn_size > 2)
									dist_params.featureParams[i].unknownToUnknownDifference = EvaluableNode::ToNumber(ocn[2]);
							}
							else //treat as singular value
							{
								dist_params.featureParams[i].typeAttributes.maxCyclicDifference = EvaluableNode::ToNumber(en);
							}
						}
						else //can't be cyclic without a range
						{
							dist_params.featureParams[i].featureType = GeneralizedDistance::FDT_CONTINUOUS_NUMERIC;
						}
						break;

					case GeneralizedDistance::FDT_CONTINUOUS_NUMERIC:
					case GeneralizedDistance::FDT_CONTINUOUS_STRING:
					case GeneralizedDistance::FDT_CONTINUOUS_CODE:
						if(found && !EvaluableNode::IsNull(en))
						{
							if(en->EvaluableNode::IsOrderedArray())
							{
								auto &ocn = en->GetOrderedChildNodesReference();
								size_t ocn_size = ocn.size();
								if(ocn_size > 0)
									dist_params.featureParams[i].knownToUnknownDifference = EvaluableNode::ToNumber(ocn[0]);
								if(ocn_size > 1)
									dist_params.featureParams[i].unknownToUnknownDifference = EvaluableNode::ToNumber(ocn[1]);
							}
							else //treat as singular value
							{
								dist_params.featureParams[i].knownToUnknownDifference = EvaluableNode::ToNumber(en);
							}
						}
						break;
					}
				}
			});

		//get deviations
		EvaluableNode::ConvertChildNodesAndStoreValue(deviations_node, element_names, num_elements,
			[&dist_params](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_params.featureParams.size())
				{
					if(found)
						dist_params.featureParams[i].deviation = EvaluableNode::ToNumber(en);
					else
						dist_params.featureParams[i].deviation = 0.0;
				}
			});
	}


	//interpret evaluable node as a distance query
	inline void BuildDistanceCondition(EvaluableNode *cn, EntityQueryCondition &cond)
	{
		//cache ordered child nodes so don't need to keep fetching
		auto &ocn = cn->GetOrderedChildNodes();

		//need to at least have position, otherwise not valid query
		if(ocn.size() <= POSITION)
			return;

		//if ENT_QUERY_NEAREST_GENERALIZED_DISTANCE, see if excluding an entity in the previous query -- if so, exclude here
		//EntityQueryCondition *cur_condition = nullptr;
		if(cond.queryType == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
			//&& conditions.size() > 0 && conditions.back().queryType == ENT_QUERY_NOT_IN_ENTITY_LIST && conditions.back().existLabels.size() == 1
			)
		{
			cur_condition = &(conditions.back());
			cur_condition->exclusionLabel = cur_condition->existLabels[0];
			cur_condition->existLabels.clear();
		}
		else
		{
			//create a new condition for distance
			conditions.emplace_back();
			cur_condition = &(conditions.back());

			cur_condition->exclusionLabel = string_intern_pool.NOT_A_STRING_ID;
		}

		//set query condition type
		cond.useConcurrency = cn->GetConcurrency();

		//set maximum distance and max number of results (top_k) to find
		if(cond.queryType == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE) //maximum distance to search within
		{
			cond.maxToRetrieve = std::numeric_limits<double>::infinity();
			cond.maxDistance = EvaluableNode::ToNumber(ocn[MAX_TO_FIND_OR_MAX_DISTANCE]);
			if(FastIsNaN(cond.maxDistance))
				cond.maxDistance = 0;
		}
		else //infinite range query, use param as number to find (top_k)
		{
			cond.maxToRetrieve = EvaluableNode::ToNumber(ocn[MAX_TO_FIND_OR_MAX_DISTANCE]);
			if(FastIsNaN(cond.maxToRetrieve))
				cond.maxToRetrieve = 0;
			cond.maxDistance = std::numeric_limits<double>::infinity();
		}

		//set position labels
		EvaluableNode *position_labels = ocn[POSITION_LABELS];
		if(EvaluableNode::IsOrderedArray(position_labels))
		{
			cond.positionLabels.reserve(position_labels->GetOrderedChildNodes().size());
			for(auto &pl : position_labels->GetOrderedChildNodes())
			{
				StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(pl);
				if(Entity::IsLabelValidAndPublic(label_sid))
					cond.positionLabels.push_back(label_sid);
				else
					cond.queryType = ENT_NULL;
			}
		}

		//select based on type for position or entities
		if(DoesDistanceQueryUseEntitiesInsteadOfPosition(cond.queryType))
		{
			EvaluableNode *entities = ocn[POSITION];
			if(EvaluableNode::IsOrderedArray(entities))
			{
				auto &entities_ocn = entities->GetOrderedChildNodesReference();
				cond.existLabels.reserve(entities_ocn.size());
				for(auto &entity_en : entities_ocn)
					cond.existLabels.push_back(EvaluableNode::ToStringIDIfExists(entity_en));
			}
		}
		else
		{
			//set position
			EvaluableNode *position = ocn[POSITION];
			if(EvaluableNode::IsOrderedArray(position) && (position->GetNumChildNodes() == cond.positionLabels.size()))
			{
				auto &position_ocn = position->GetOrderedChildNodesReference();
				cond.valueToCompare.reserve(position_ocn.size());
				cond.valueTypes.reserve(position_ocn.size());
				for(auto &pos_en : position_ocn)
				{
					EvaluableNodeImmediateValue imm_val;
					auto value_type = imm_val.CopyValueFromEvaluableNode(pos_en);
					cond.valueTypes.push_back(value_type);
					cond.valueToCompare.push_back(imm_val);
				}
			}
			else // no positions given, default to nulls for each label
			{
				cond.valueToCompare.reserve(cond.positionLabels.size());
				cond.valueTypes.reserve(cond.positionLabels.size());
				for(size_t i = 0; i < cond.positionLabels.size(); i++)
				{
					cond.valueTypes.push_back(ENIVT_NULL);
					cond.valueToCompare.push_back(EvaluableNodeImmediateValue());
				}
			}
		}
		//else don't bother parsing this, it instead contains the cases to compute case conviction for

		size_t num_elements = cond.positionLabels.size();
		auto &dist_params = cond.distParams;

		EvaluableNode *weights_node = nullptr;
		if(ocn.size() > WEIGHTS)
			weights_node = ocn[WEIGHTS];

		EvaluableNode *distance_types_node = nullptr;
		if(ocn.size() > DISTANCE_TYPES)
			distance_types_node = ocn[DISTANCE_TYPES];

		EvaluableNode *attributes_node = nullptr;
		if(ocn.size() > ATTRIBUTES)
			attributes_node = ocn[ATTRIBUTES];

		EvaluableNode *deviations_node = nullptr;
		if(ocn.size() > DEVIATIONS)
			deviations_node = ocn[DEVIATIONS];

		PopulateDistanceFeatureParameters(dist_params, num_elements, cond.positionLabels,
			weights_node, distance_types_node, attributes_node, deviations_node);

		//set minkowski parameter; default to 2.0 for Euclidian distance
		double p_value = 2.0;
		if(ocn.size() > MINKOWSKI_PARAMETER)
		{
			p_value = EvaluableNode::ToNumber(ocn[MINKOWSKI_PARAMETER]);

			//make sure valid value, if not, fall back to 2
			if(FastIsNaN(p_value) || p_value < 0)
				p_value = 2;
		}
		cond.distParams.pValue = p_value;

		//value transforms for whatever is measured as "distance"
		cond.transformSuprisalToProb = false;
		cond.distanceWeightExponent = 1.0;
		if(ocn.size() > DISTANCE_VALUE_TRANSFORM)
		{
			EvaluableNode *dwe_param = ocn[DISTANCE_VALUE_TRANSFORM];
			if(!EvaluableNode::IsNull(dwe_param))
			{
				if(dwe_param->GetType() == ENT_STRING && dwe_param->GetStringIDReference() == ENBISI_surprisal_to_prob)
					cond.transformSuprisalToProb = true;
				else //try to convert to number
					cond.distanceWeightExponent = EvaluableNode::ToNumber(dwe_param, 1.0);
			}
		}

		cond.weightLabel = StringInternPool::NOT_A_STRING_ID;
		if(ocn.size() > ENTITY_WEIGHT_LABEL_NAME)
			cond.weightLabel = EvaluableNode::ToStringIDIfExists(ocn[ENTITY_WEIGHT_LABEL_NAME]);

		//set random seed
		std::string seed = "";
		if(ocn.size() > RANDOM_SEED)
			seed = EvaluableNode::ToString(ocn[RANDOM_SEED]);
		cond.randomStream.SetState(seed);

		//set radius label
		if(ocn.size() > RADIUS_LABEL)
			cond.singleLabel = EvaluableNode::ToStringIDIfExists(ocn[RADIUS_LABEL]);
		else
			cond.singleLabel = StringInternPool::NOT_A_STRING_ID;

		//set numerical precision
		cond.distParams.highAccuracy = false;
		cond.distParams.recomputeAccurateDistances = true;
		if(ocn.size() > NUMERICAL_PRECISION)
		{
			StringInternPool::StringID np_sid = EvaluableNode::ToStringIDIfExists(ocn[NUMERICAL_PRECISION]);
			if(np_sid == ENBISI_precise)
			{
				cond.distParams.highAccuracy = true;
				cond.distParams.recomputeAccurateDistances = false;
			}
			else if(np_sid == ENBISI_fast)
			{
				cond.distParams.highAccuracy = false;
				cond.distParams.recomputeAccurateDistances = false;
			}
			//don't need to do anything for np_sid == ENBISI_recompute_precise because it's default
		}
		
		cond.returnSortedList = false;
		cond.additionalSortedListLabels.clear();
		if(cond.queryType == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE || cond.queryType == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE || cond.queryType == ENT_COMPUTE_ENTITY_DISTANCE_CONTRIBUTIONS)
		{
			if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0)
			{
				EvaluableNode *list_param = ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0];
				cond.returnSortedList = EvaluableNode::IsTrue(list_param);
				if(!EvaluableNode::IsEmptyNode(list_param))
				{
					if(list_param->GetType() == ENT_STRING)
					{
						cond.additionalSortedListLabels.push_back(list_param->GetStringIDReference());
					}
					else
					{
						for(auto label_node : list_param->GetOrderedChildNodes())
							cond.additionalSortedListLabels.push_back(EvaluableNode::ToStringIDIfExists(label_node));
					}
				}
			}
		}
		else if(cond.queryType == ENT_COMPUTE_ENTITY_CONVICTIONS || cond.queryType == ENT_COMPUTE_ENTITY_GROUP_KL_DIVERGENCE || cond.queryType == ENT_COMPUTE_ENTITY_KL_DIVERGENCES)
		{
			cond.convictionOfRemoval = false;
			if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0)
				cond.convictionOfRemoval = EvaluableNode::IsTrue(ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0]);

			if(cond.queryType == ENT_COMPUTE_ENTITY_CONVICTIONS || cond.queryType == ENT_COMPUTE_ENTITY_KL_DIVERGENCES)
			{
				if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 1)
				{
					EvaluableNode *list_param = ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 1];
					cond.returnSortedList = EvaluableNode::IsTrue(list_param);
					if(!EvaluableNode::IsEmptyNode(list_param))
					{
						if(list_param->GetType() == ENT_STRING)
						{
							cond.additionalSortedListLabels.push_back(list_param->GetStringIDReference());
						}
						else
						{
							for(auto label_node : list_param->GetOrderedChildNodes())
								cond.additionalSortedListLabels.push_back(EvaluableNode::ToStringIDIfExists(label_node));
						}
					}
				}
			}
		}
	}

	//builds a query condition from cn
	inline void BuildNonDistanceCondition(EvaluableNode *cn, EntityQueryCondition &cond,
		EvaluableNodeManager &enm, RandomStream &rs)
	{
		auto &ocn = cn->GetOrderedChildNodes();

		//validate number of parameters
		switch(cond.queryType)
		{
			case ENT_QUERY_BETWEEN: //all double parameter query types
			case ENT_QUERY_NOT_BETWEEN:
				if(ocn.size() < 3)
					return;
				break;

			case ENT_QUERY_LESS_OR_EQUAL_TO:
			case ENT_QUERY_GREATER_OR_EQUAL_TO:
			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_EQUALS:
				if(ocn.size() < 2)
					return;
				break;

			case ENT_QUERY_MIN:
			case ENT_QUERY_MAX:
			case ENT_QUERY_VALUE_MASSES:
				if(ocn.size() < 1)
					return;
				break;

			default:;
		}

		//next, determine if a a new condition should be made, or reuse the current one
		bool requires_new_condition = true; //if true, create a new condition rather than using current_condition
		switch(cond.queryType)
		{
			case ENT_QUERY_NOT_EXISTS:
			case ENT_QUERY_EXISTS:
			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_EQUALS:
			case ENT_QUERY_NOT_BETWEEN:
				requires_new_condition = (conditions.size() == 0 || conditions.back().queryType != cond.queryType);
				break;

			case ENT_QUERY_BETWEEN:
			case ENT_QUERY_GREATER_OR_EQUAL_TO:
			case ENT_QUERY_LESS_OR_EQUAL_TO:
			{
				//these three are equivalent
				if(conditions.size() > 0)
				{
					EvaluableNodeType prev_type = conditions.back().queryType;
					if(prev_type == ENT_QUERY_BETWEEN || prev_type == ENT_QUERY_GREATER_OR_EQUAL_TO || prev_type == ENT_QUERY_LESS_OR_EQUAL_TO)
						requires_new_condition = false;
				}
				break;
			}

			default:;
		}

		//create a new condition if needed
		if(requires_new_condition)
		{
			//create new condition
			conditions.emplace_back();
			conditions.back().queryType = type;
		}

		auto cur_condition = &(conditions.back());
		cur_condition->singleLabel = 0;

		//get label sid and return if label is invalid
		StringInternPool::StringID label_sid = StringInternPool::NOT_A_STRING_ID;
		if(    cond.queryType == ENT_QUERY_NOT_EXISTS
			|| cond.queryType == ENT_QUERY_EXISTS
			|| cond.queryType == ENT_QUERY_MIN
			|| cond.queryType == ENT_QUERY_MAX
			|| cond.queryType == ENT_QUERY_SUM
			|| cond.queryType == ENT_QUERY_MODE
			|| cond.queryType == ENT_QUERY_QUANTILE
			|| cond.queryType == ENT_QUERY_GENERALIZED_MEAN
			|| cond.queryType == ENT_QUERY_MIN_DIFFERENCE
			|| cond.queryType == ENT_QUERY_MAX_DIFFERENCE
			|| cond.queryType == ENT_QUERY_VALUE_MASSES
			|| cond.queryType == ENT_QUERY_LESS_OR_EQUAL_TO
			|| cond.queryType == ENT_QUERY_GREATER_OR_EQUAL_TO
			|| cond.queryType == ENT_QUERY_NOT_EQUALS
			|| cond.queryType == ENT_QUERY_EQUALS
			|| cond.queryType == ENT_QUERY_BETWEEN
			|| cond.queryType == ENT_QUERY_NOT_BETWEEN
			|| cond.queryType == ENT_QUERY_AMONG
			|| cond.queryType == ENT_QUERY_NOT_AMONG)
		{
			if(ocn.size() >= 1)
				label_sid = EvaluableNode::ToStringIDIfExists(ocn[0]);

			if(!Entity::IsLabelValidAndPublic(label_sid))
			{
				cond.queryType = ENT_NULL;
				return;
			}
		}

		//actually populate the condition parameters from the evaluable nodes
		switch(cond.queryType)
		{
			case ENT_QUERY_SELECT:
			{
				cond.maxToRetrieve = (ocn.size() >= 1) ? EvaluableNode::ToNumber(ocn[0], 0.0) : 0;

				cond.hasStartOffset = (ocn.size() >= 2);
				cond.startOffset = cond.hasStartOffset ? static_cast<size_t>(EvaluableNode::ToNumber(ocn[1], 0.0)) : 0;

				cond.hasRandomStream = (ocn.size() >= 3 && !EvaluableNode::IsEmptyNode(ocn[2]));
				if(cond.hasRandomStream)
					cond.randomStream.SetState(EvaluableNode::ToString(ocn[2]));

				break;
			}
			case ENT_QUERY_SAMPLE:
			{
				cond.maxToRetrieve = (ocn.size() > 0) ? EvaluableNode::ToNumber(ocn[0], 0.0) : 1;
				cond.hasRandomStream = (ocn.size() > 1 && !EvaluableNode::IsEmptyNode(ocn[1]));
				if(cond.hasRandomStream)
					cond.randomStream.SetState(EvaluableNode::ToString(ocn[1]));
				else
					cond.randomStream = rs.CreateOtherStreamViaRand();
			    break;
			}
			case ENT_QUERY_WEIGHTED_SAMPLE:
			{
				cond.singleLabel = (ocn.size() > 0) ? EvaluableNode::ToStringIDIfExists(ocn[0]) : StringInternPool::NOT_A_STRING_ID;
				cond.maxToRetrieve = (ocn.size() > 1) ? EvaluableNode::ToNumber(ocn[1], 0.0) : 1;
				cond.hasRandomStream = (ocn.size() > 2 && !EvaluableNode::IsEmptyNode(ocn[2]));
				if(cond.hasRandomStream)
					cond.randomStream.SetState(EvaluableNode::ToString(ocn[2]));
				else
					cond.randomStream = rs.CreateOtherStreamViaRand();
				break;
			}
			case ENT_QUERY_IN_ENTITY_LIST:
			case ENT_QUERY_NOT_IN_ENTITY_LIST:
			{
				if(ocn.size() >= 1)
				{
					EvaluableNode *entity_sids = ocn[0];
					if(EvaluableNode::IsOrderedArray(entity_sids))
					{
						cond.existLabels.reserve(entity_sids->GetOrderedChildNodes().size());
						for(auto &esid : entity_sids->GetOrderedChildNodes())
						{
							StringInternPool::StringID entity_sid = EvaluableNode::ToStringIDIfExists(esid);
							cond.existLabels.push_back(entity_sid);
						}
					}
				}
				break;
			}
			case ENT_QUERY_BETWEEN:
			case ENT_QUERY_NOT_BETWEEN:
			{
				//number of parameters checked above
				EvaluableNode *low_value = ocn[1];
				EvaluableNode *high_value = ocn[2];

				//since types need to match, force both to the same type
				if(EvaluableNode::IsNativelyNumeric(low_value) || EvaluableNode::IsNativelyNumeric(high_value))
				{
					cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(
						EvaluableNode::ToNumber(low_value), EvaluableNode::ToNumber(high_value))));

					cond.valueTypes.push_back(ENIVT_NUMBER);
				}
				else
				{
					StringInternPool::StringID low_sid = EvaluableNode::ToStringIDIfExists(low_value);
					StringInternPool::StringID high_sid = EvaluableNode::ToStringIDIfExists(high_value);

					cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(low_sid, high_sid)));

					cond.valueTypes.push_back(ENIVT_STRING_ID);
				}

				break;
			}

			case ENT_QUERY_AMONG:
			case ENT_QUERY_NOT_AMONG:
			{
				cond.singleLabel = label_sid;

				//already checked for nullptr above
				auto &values_ocn = ocn[1]->GetOrderedChildNodes();
				for(auto value_node : values_ocn)
				{
					EvaluableNodeImmediateValue value;
					auto value_type = value.CopyValueFromEvaluableNode(value_node);
					cond.valueToCompare.push_back(value);
					cond.valueTypes.push_back(value_type);
				}

				break;
			}

			case ENT_QUERY_NOT_EXISTS:
			case ENT_QUERY_EXISTS:
			{
				//get label and append it if it is valid (otherwise don't match on anything)
				if(ocn.size() >= 1)
					cond.existLabels.push_back(label_sid);

				break;
			}

			case ENT_QUERY_MIN:
			case ENT_QUERY_MAX:
			{
				cond.singleLabel = label_sid;

				//default to retrieve 1
				cond.maxToRetrieve = 1;
				if(ocn.size() >= 2)
				{
					EvaluableNode *value = ocn[1];
					cond.maxToRetrieve = EvaluableNode::ToNumber(value);
				}

				if(ocn.size() <= 2 || EvaluableNode::IsTrue(ocn[2]))
					cond.singleLabelType = ENIVT_NUMBER;
				else
					cond.singleLabelType = ENIVT_STRING_ID;

				break;
			}

			case ENT_QUERY_LESS_OR_EQUAL_TO:
			case ENT_QUERY_GREATER_OR_EQUAL_TO:
			{
				//these query types will be transformed into a between query, including the appropriate infinite

				//number of parameters checked above
				EvaluableNode *compare_value = ocn[1];

				if(EvaluableNode::IsNativelyNumeric(compare_value))
				{
					if(cond.queryType == ENT_QUERY_LESS_OR_EQUAL_TO)
						cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(
							-std::numeric_limits<double>::infinity(), EvaluableNode::ToNumber(compare_value))));
					else
						cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(
							EvaluableNode::ToNumber(compare_value), std::numeric_limits<double>::infinity())));

					cond.valueTypes.push_back(ENIVT_NUMBER);
				}
				else
				{
					if(cond.queryType == ENT_QUERY_LESS_OR_EQUAL_TO)
						cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(
							string_intern_pool.NOT_A_STRING_ID, EvaluableNode::ToStringIDIfExists(compare_value))));
					else
						cond.pairedLabels.push_back(std::make_pair(label_sid, std::make_pair(
							EvaluableNode::ToStringIDIfExists(compare_value), string_intern_pool.NOT_A_STRING_ID)));

					cond.valueTypes.push_back(ENIVT_STRING_ID);
				}

				cond.queryType = ENT_QUERY_BETWEEN;
				break;
			}


			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_EQUALS:
			{				
				EvaluableNodeImmediateValue value;
				EvaluableNodeImmediateValueType value_type = value.CopyValueFromEvaluableNode(ocn[1]);

				cond.valueTypes.push_back(value_type);
				cond.singleLabels.emplace_back(std::make_pair(label_sid, value));

				break;
			}

			case ENT_QUERY_MIN_DIFFERENCE:
				cond.singleLabel = label_sid;

				cond.maxDistance = std::numeric_limits<double>::quiet_NaN();
				if(ocn.size() >= 2)
					cond.maxDistance = EvaluableNode::ToNumber(ocn[1]);

				cond.includeZeroDifferences = true;
				if(ocn.size() >= 3)
					cond.includeZeroDifferences = EvaluableNode::IsTrue(ocn[2]);
				break;

			case ENT_QUERY_MAX_DIFFERENCE:
				cond.singleLabel = label_sid;
				
				cond.maxDistance = std::numeric_limits<double>::quiet_NaN();
				if(ocn.size() >= 2)
					cond.maxDistance = EvaluableNode::ToNumber(ocn[1]);

				break;

			case ENT_QUERY_SUM:
			case ENT_QUERY_MODE:
			case ENT_QUERY_VALUE_MASSES:
			{
				cond.singleLabel = label_sid;

				cond.weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 2)
					cond.weightLabel = EvaluableNode::ToStringIDIfExists(ocn[1]);

				if(cond.queryType == ENT_QUERY_MODE || cond.queryType == ENT_QUERY_VALUE_MASSES)
				{
					if(ocn.size() <= 2 || EvaluableNode::IsTrue(ocn[2]))
						cond.singleLabelType = ENIVT_NUMBER;
					else
						cond.singleLabelType = ENIVT_STRING_ID;
				}

				break;
			}

			case ENT_QUERY_QUANTILE:
			{
				cond.singleLabel = label_sid;

				cond.qPercentage = 0.5;
				if(ocn.size() >= 2)
					cond.qPercentage = EvaluableNode::ToNumber(ocn[1]);

				cond.weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 3)
					cond.weightLabel = EvaluableNode::ToStringIDIfExists(ocn[2]);

				break;
			}

			case ENT_QUERY_GENERALIZED_MEAN:
			{
				cond.singleLabel = label_sid;

				cond.distParams.pValue = 1;
				if(ocn.size() >= 2)
					cond.distParams.pValue = EvaluableNode::ToNumber(ocn[1]);

				cond.weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 3)
					cond.weightLabel = EvaluableNode::ToStringIDIfExists(ocn[2]);

				cond.center = 0.0;
				if(ocn.size() >= 4)
					cond.center = EvaluableNode::ToNumber(ocn[3], 0.0);

				cond.calculateMoment = false;
				if(ocn.size() >= 5)
					cond.calculateMoment = EvaluableNode::IsTrue(ocn[4]);

				cond.absoluteValue = false;
				if(ocn.size() >= 6)
					cond.absoluteValue = EvaluableNode::IsTrue(ocn[5]);

				break;
			}

			default:;
		}//end switch
	}
};
