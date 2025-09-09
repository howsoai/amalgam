#pragma once

//project headers:
#include "EntityQueries.h"
#include "EvaluableNode.h"
#include "PlatformSpecific.h"
#include "StringInternPool.h"

//system headers:
#include <type_traits>

//Constructs a query engine query condition from Amalgam evaluable nodes
namespace EntityQueryBuilder
{
	//parameter indices for distance queries
	enum DistParamIndices : size_t
	{
		MAX_TO_FIND_OR_MAX_DISTANCE,
		POSITION_LABELS,
		POSITION,

		//optional params
		MINKOWSKI_PARAMETER,
		WEIGHTS,
		DISTANCE_TYPES,
		ATTRIBUTES,
		DEVIATIONS,
		WEIGHTS_SELECTION_FEATURE,
		DISTANCE_VALUE_TRANSFORM,
		ENTITY_WEIGHT_LABEL_NAME,
		RANDOM_SEED,
		RADIUS_LABEL,
		NUMERICAL_PRECISION,
		
		NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS //always last - do not add after this
	};

	//returns true if it is a distance
	constexpr static bool IsEvaluableNodeTypeDistanceQuery(EvaluableNodeType t)
	{
		return (t == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE || t == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
			|| t == ENT_QUERY_DISTANCE_CONTRIBUTIONS || t == ENT_QUERY_ENTITY_CONVICTIONS
			|| t == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE || t == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| t == ENT_QUERY_ENTITY_KL_DIVERGENCES
			);
	}

	//populates deviation data for a given nominal value
	//assumes that value_deviation_assoc is a valid pointer to an assoc
	template<typename NominalDeviationValuesType>
	inline void PopulateFeatureDeviationNominalValueAssocData(
		NominalDeviationValuesType &ndd, EvaluableNode *value_deviation_assoc)
	{
		auto &mcn = value_deviation_assoc->GetMappedChildNodesReference();
		ndd.reserve(mcn.size());
		for(auto &cn : mcn)
		{
			if constexpr(std::is_same<typename NominalDeviationValuesType::key_type, double>::value)
			{
				double value = std::numeric_limits<double>::quiet_NaN();
				if(cn.first != string_intern_pool.emptyStringId)
					value = Parser::ParseNumberFromKeyStringId(cn.first);

				ndd.emplace(value, EvaluableNode::ToNumber(cn.second));
			}
			else
			{
				ndd.emplace(cn.first, EvaluableNode::ToNumber(cn.second));
			}
		}
	}

	//populates deviation data for a given nominal value
	template<typename NominalDeviationValuesType>
	inline void PopulateFeatureDeviationNominalValueData(
		NominalDeviationValuesType &ndd, EvaluableNode *value_deviation_node)
	{
		if(EvaluableNode::IsNull(value_deviation_node))
			return;

		auto vdn_type = value_deviation_node->GetType();

		//if it's an assoc, just populate, otherwise parse list with assoc in it
		if(vdn_type == ENT_ASSOC)
		{
			PopulateFeatureDeviationNominalValueAssocData<NominalDeviationValuesType>(ndd, value_deviation_node);
		}
		else if(vdn_type == ENT_LIST)
		{
			//a list indicates that it is a pair of a sparse deviation assoc followed by a default deviation
			//the default being for when one of the values is found, but not the other
			auto &ocn = value_deviation_node->GetOrderedChildNodesReference();
			size_t ocn_size = ocn.size();

			if(ocn_size > 0
					&& !EvaluableNode::IsNull(ocn[0])
					&& ocn[0]->GetType() == ENT_ASSOC)
				PopulateFeatureDeviationNominalValueAssocData<NominalDeviationValuesType>(ndd, ocn[0]);

			if(ocn_size > 1)
				ndd.defaultDeviation = EvaluableNode::ToNumber(ocn[1]);
		}
		else if(vdn_type == ENT_NUMBER)
		{
			ndd.defaultDeviation = EvaluableNode::ToNumber(value_deviation_node);
		}
	}

	//populates deviation data for feature_attribs from deviation_node given that deviation_node is known to be an ENT_ASSOC
	inline void PopulateFeatureDeviationNominalValuesMatrixData(GeneralizedDistanceEvaluator::FeatureAttributes &feature_attribs, EvaluableNode *deviation_node)
	{
		auto &number_sdm = feature_attribs.nominalNumberSparseDeviationMatrix;
		auto &string_sdm = feature_attribs.nominalStringSparseDeviationMatrix;
		number_sdm.clear();
		string_sdm.clear();

		auto &mcn = deviation_node->GetMappedChildNodesReference();
		if(feature_attribs.featureType == GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER)
		{
			number_sdm.reserve(mcn.size());
			for(auto &cn : mcn)
			{
				double value = std::numeric_limits<double>::quiet_NaN();
				if(cn.first != string_intern_pool.emptyStringId)
					value = Parser::ParseNumberFromKeyStringId(cn.first);

				number_sdm.emplace(value);
				PopulateFeatureDeviationNominalValueData(number_sdm.back().second, cn.second);
			}
		}
		else if(feature_attribs.featureType == GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING
			|| feature_attribs.featureType == GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE)
		{
			string_sdm.reserve(mcn.size());
			for(auto &cn : deviation_node->GetMappedChildNodes())
			{
				string_sdm.emplace(cn.first);
				PopulateFeatureDeviationNominalValueData(string_sdm.back().second, cn.second);
			}
		}
	}

	//populates deviation data for feature_attribs from deviation_node
	inline void PopulateFeatureDeviationNominalValuesData(GeneralizedDistanceEvaluator::FeatureAttributes &feature_attribs, EvaluableNode *deviation_node)
	{
		feature_attribs.deviation = std::numeric_limits<double>::quiet_NaN();

		if(deviation_node == nullptr)
			return;

		auto dnt = deviation_node->GetType();
		if(dnt == ENT_ASSOC)
		{
			PopulateFeatureDeviationNominalValuesMatrixData(feature_attribs, deviation_node);
		}
		else if(dnt == ENT_LIST)
		{
			//a list indicates that it is a pair of a sparse deviation matrix followed by a default deviation
			//the default being for when the first value being compared is not found
			auto &ocn = deviation_node->GetOrderedChildNodesReference();
			if(ocn.size() > 0)
				PopulateFeatureDeviationNominalValuesMatrixData(feature_attribs, ocn[0]);

			if(ocn.size() > 1)
				feature_attribs.deviation = EvaluableNode::ToNumber(ocn[1]);
		}
		else
		{
			feature_attribs.deviation = EvaluableNode::ToNumber(deviation_node, 0);
		}
	}

	//populates the weight attribute for the corresponding features in dist_eval
	//requires that weights_node is an assoc
	//distributes the 
	inline void PopulateWeightsFromSelectionFeature(GeneralizedDistanceEvaluator &dist_eval, EvaluableNode *weights_node,
		size_t num_elements, std::vector<StringInternPool::StringID> &element_names,
		StringInternPool::StringID weights_selection_feature)
	{
		auto &weights_matrix = weights_node->GetMappedChildNodesReference();
		auto weights_for_feature_node_entry = weights_matrix.find(weights_selection_feature);

		//if entry not found or only one feature, just default to 1/n
		if(weights_for_feature_node_entry == end(weights_matrix)
			|| dist_eval.featureAttribs.size() == 1)
		{
			double even_weight = 1.0 / dist_eval.featureAttribs.size();
			for(auto &feat : dist_eval.featureAttribs)
				feat.weight = even_weight;
			return;
		}

		EvaluableNode *weights_for_feature_node = weights_for_feature_node_entry->second;
		//if not an assoc, accumulate normally
		if(weights_for_feature_node == nullptr || !weights_for_feature_node->IsAssociativeArray())
		{
			//populate weights the normal way from the particular feature's data
			EvaluableNode::ConvertChildNodesAndStoreValue(weights_node, element_names, num_elements,
				[&dist_eval](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_eval.featureAttribs.size())
				{
					if(found)
						dist_eval.featureAttribs[i].weight = EvaluableNode::ToNumber(en, 0.0);
					else
						dist_eval.featureAttribs[i].weight = 0.0;
				}
			});

			return;
		}

		auto &weights_for_feature_node_mcn = weights_for_feature_node->GetMappedChildNodesReference();

		//collect all weights that contribute to this feature, but leave weights_selection_feature out
		FastHashMap<StringInternPool::StringID, double> unused_weights_by_name;
		double total_probability_mass = 0.0;
		for(auto &[sid, weight_node] : weights_for_feature_node_mcn)
		{
			if(sid != weights_selection_feature)
			{
				double weight = EvaluableNode::ToNumber(weight_node, 0.0);
				if(weight > 0.0)
				{
					unused_weights_by_name.emplace(sid, weight);
					total_probability_mass += weight;
				}
			}
		}

		//populate weights the normal way from the particular feature's data
		//and remove used features
		for(size_t i = 0; i < element_names.size(); i++)
		{
			EvaluableNode *value_en = nullptr;
			bool found = false;
			auto found_node = weights_for_feature_node_mcn.find(element_names[i]);
			if(found_node != end(weights_for_feature_node_mcn))
			{
				value_en = found_node->second;
				found = true;
			}

			double weight = 0.0;
			if(found)
			{
				weight = EvaluableNode::ToNumber(value_en, 0.0);
				//normalize
				weight /= total_probability_mass;
			}

			dist_eval.featureAttribs[i].weight = weight;
			if(weight > 0.0)
				unused_weights_by_name.erase(element_names[i]);
		}

		//compute and accumulate probability masses from unused features into their corresponding features
		for(auto &[unused_feature_sid, unused_feature_weight] : unused_weights_by_name)
		{
			//normalize unused weights	
			unused_feature_weight /= total_probability_mass;

			//get the entry in the matrix
			auto unused_weights_for_feature_entry = weights_matrix.find(unused_feature_sid);
			if(unused_weights_for_feature_entry == end(weights_matrix))
				continue;
			auto unused_weights_for_feature_node = unused_weights_for_feature_entry->second;
			if(unused_weights_for_feature_node == nullptr || !unused_weights_for_feature_node->IsAssociativeArray())
				continue;
			auto &unused_weights_for_feature_mcn = unused_weights_for_feature_node->GetMappedChildNodesReference();

			//get total probability mass to normalize this feature
			double total_probability_mass_for_feature = 0.0;
			for(size_t i = 0; i < element_names.size(); i++)
			{
				//don't count the selecting feature
				if(element_names[i] == weights_selection_feature)
					continue;

				auto unused_element_entry = unused_weights_for_feature_mcn.find(element_names[i]);
				if(unused_element_entry == end(unused_weights_for_feature_mcn))
					continue;

				total_probability_mass_for_feature += EvaluableNode::ToNumber(unused_element_entry->second, 0.0);
			}

			//accumulate the normalized probability of this feature influencing the unused feature and accumulate
			//that probability mass back into the corresponding feature that will be used
			for(size_t i = 0; i < element_names.size(); i++)
			{
				//don't count the selecting feature
				if(element_names[i] == weights_selection_feature)
					continue;

				auto unused_element_entry = unused_weights_for_feature_mcn.find(element_names[i]);
				if(unused_element_entry == end(unused_weights_for_feature_mcn))
					continue;

				double unused_weight = EvaluableNode::ToNumber(unused_element_entry->second, 0.0);
				dist_eval.featureAttribs[i].weight += unused_weight * (unused_feature_weight / total_probability_mass_for_feature);
			}
		}

		//do a final normalization pass on feature weights
		total_probability_mass = 0.0;
		for(auto &feat : dist_eval.featureAttribs)
			total_probability_mass += feat.weight;

		for(auto &feat : dist_eval.featureAttribs)
			feat.weight /= total_probability_mass;
	}

	//populates the features of dist_eval based on either num_elements or element_names for each of the
	// four different attribute parameters based on its type (using num_elements if list or immediate, element_names if assoc)
	inline void PopulateDistanceFeatureParameters(GeneralizedDistanceEvaluator &dist_eval,
		size_t num_elements, std::vector<StringInternPool::StringID> &element_names,
		EvaluableNode *weights_node, StringInternPool::StringID weights_selection_feature,
		EvaluableNode *distance_types_node, EvaluableNode *attributes_node, EvaluableNode *deviations_node)
	{
		dist_eval.featureAttribs.resize(num_elements);

		if(weights_selection_feature != string_intern_pool.NOT_A_STRING_ID && weights_node != nullptr
			&& weights_node->IsAssociativeArray())
		{
			PopulateWeightsFromSelectionFeature(dist_eval, weights_node, num_elements, element_names, weights_selection_feature);
		}
		else
		{
			//get weights
			EvaluableNode::ConvertChildNodesAndStoreValue(weights_node, element_names, num_elements,
				[&dist_eval](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_eval.featureAttribs.size())
				{
					if(found)
						dist_eval.featureAttribs[i].weight = EvaluableNode::ToNumber(en, 1.0);
					else
						dist_eval.featureAttribs[i].weight = 1.0;
				}
			});
		}

		//get type
		EvaluableNode::ConvertChildNodesAndStoreValue(distance_types_node, element_names, num_elements,
			[&dist_eval](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_eval.featureAttribs.size())
				{
					auto feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER;
					if(found)
					{
						StringInternPool::StringID feature_type_id = EvaluableNode::ToStringIDIfExists(en);
						if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_nominal_number))					feature_type = GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_nominal_string))				feature_type = GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_nominal_code))					feature_type = GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_continuous_number))			feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_continuous_number_cyclic))		feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER_CYCLIC;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_continuous_string))			feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_STRING;
						else if(feature_type_id == GetStringIdFromBuiltInStringId(ENBISI_continuous_code))				feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_CODE;
						else																							feature_type = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER;
					}
					dist_eval.featureAttribs[i].featureType = feature_type;
				}
			});

		//get attributes
		EvaluableNode::ConvertChildNodesAndStoreValue(attributes_node, element_names, num_elements,
			[&dist_eval](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_eval.featureAttribs.size())
				{
					//get attributes based on feature type
					switch(dist_eval.featureAttribs[i].featureType)
					{
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER:
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING:
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE:
						if(found && !EvaluableNode::IsNull(en))
							dist_eval.featureAttribs[i].typeAttributes.nominalCount = EvaluableNode::ToNumber(en);
						break;

					case GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER_CYCLIC:
						if(found && !EvaluableNode::IsNull(en))
							dist_eval.featureAttribs[i].typeAttributes.maxCyclicDifference = EvaluableNode::ToNumber(en);
						else //can't be cyclic without a range
							dist_eval.featureAttribs[i].featureType = GeneralizedDistanceEvaluator::FDT_CONTINUOUS_NUMBER;
						break;

					default:
						break;
					}
				}
			});

		//get deviations
		EvaluableNode::ConvertChildNodesAndStoreValue(deviations_node, element_names, num_elements,
			[&dist_eval](size_t i, bool found, EvaluableNode *en) {
				if(i < dist_eval.featureAttribs.size())
				{
					dist_eval.featureAttribs[i].deviation = 0.0;
					dist_eval.featureAttribs[i].unknownToUnknownDistanceTerm.deviation = std::numeric_limits<double>::quiet_NaN();
					dist_eval.featureAttribs[i].knownToUnknownDistanceTerm.deviation = std::numeric_limits<double>::quiet_NaN();

					//get deviations based on feature type
					switch(dist_eval.featureAttribs[i].featureType)
					{
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_NUMBER:
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_STRING:
					case GeneralizedDistanceEvaluator::FDT_NOMINAL_CODE:
						if(found && !EvaluableNode::IsNull(en))
						{
							if(en->EvaluableNode::IsOrderedArray())
							{
								auto &ocn = en->GetOrderedChildNodesReference();
								size_t ocn_size = ocn.size();

								if(ocn_size > 0)
									PopulateFeatureDeviationNominalValuesData(dist_eval.featureAttribs[i], ocn[0]);

								if(ocn_size > 1)
									dist_eval.featureAttribs[i].knownToUnknownDistanceTerm.deviation = EvaluableNode::ToNumber(ocn[1]);

								if(ocn_size > 2)
									dist_eval.featureAttribs[i].unknownToUnknownDistanceTerm.deviation = EvaluableNode::ToNumber(ocn[2]);
							}
							else //treat as singular value
							{
								PopulateFeatureDeviationNominalValuesData(dist_eval.featureAttribs[i], en);
							}
						}
						break;

					default:
						if(found && !EvaluableNode::IsNull(en))
						{
							if(en->EvaluableNode::IsOrderedArray())
							{
								auto &ocn = en->GetOrderedChildNodesReference();
								size_t ocn_size = ocn.size();
								if(ocn_size > 0)
									dist_eval.featureAttribs[i].deviation = EvaluableNode::ToNumber(ocn[0]);
								if(ocn_size > 1)
									dist_eval.featureAttribs[i].knownToUnknownDistanceTerm.deviation = EvaluableNode::ToNumber(ocn[1]);
								if(ocn_size > 2)
									dist_eval.featureAttribs[i].unknownToUnknownDistanceTerm.deviation = EvaluableNode::ToNumber(ocn[2]);
							}
							else //treat as singular value
							{
								dist_eval.featureAttribs[i].deviation = EvaluableNode::ToNumber(en);
							}
						}
						break;
					}
				}
			});
	}

	//interpret evaluable node as a distance query
	inline void BuildDistanceCondition(EvaluableNode *cn, EvaluableNodeType condition_type,
		std::vector<EntityQueryCondition> &conditions, RandomStream &rs)
	{
		//cache ordered child nodes so don't need to keep fetching
		auto &ocn = cn->GetOrderedChildNodes();

		//need to at least have position, otherwise not valid query
		if(ocn.size() <= POSITION)
			return;

		//if ENT_QUERY_NEAREST_GENERALIZED_DISTANCE, see if excluding an entity in the previous query -- if so, exclude here
		EntityQueryCondition *cur_condition = nullptr;
		if(condition_type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE && conditions.size() > 0
			&& conditions.back().queryType == ENT_QUERY_NOT_IN_ENTITY_LIST && conditions.back().existLabels.size() == 1)
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
		cur_condition->queryType = condition_type;
		cur_condition->useConcurrency = cn->GetConcurrency();

		//set maximum distance and max number of results (top_k) to find
		cur_condition->maxToRetrieve = std::numeric_limits<size_t>::max();
		cur_condition->minToRetrieve = std::numeric_limits<size_t>::max();
		cur_condition->numToRetrieveMinIncrementalProbability = 0.0;
		cur_condition->extraToRetrieve = 0;
		if(condition_type == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE) //maximum distance to search within
		{
			cur_condition->maxDistance = EvaluableNode::ToNumber(ocn[MAX_TO_FIND_OR_MAX_DISTANCE]);
			if(FastIsNaN(cur_condition->maxDistance))
				cur_condition->maxDistance = 0;
		}
		else //infinite range query, use param as number to find (top_k)
		{
			EvaluableNode *top_k_node = ocn[MAX_TO_FIND_OR_MAX_DISTANCE];
			if(EvaluableNode::IsOrderedArray(top_k_node))
			{
				auto &top_k_ocn = top_k_node->GetOrderedChildNodesReference();
				size_t num_params = top_k_ocn.size();
				//retrieve all the parameters from the list, clamping as appropriate
				if(num_params >= 1)
				{
					double min_inc_prob = EvaluableNode::ToNumber(top_k_ocn[0], 0.0);
					cur_condition->numToRetrieveMinIncrementalProbability = std::max(0.0, min_inc_prob);

					if(num_params >= 2)
					{
						double min_to_retrieve = EvaluableNode::ToNumber(top_k_ocn[1], std::numeric_limits<double>::infinity());
						min_to_retrieve = std::max(0.0, min_to_retrieve);
						if(min_to_retrieve < static_cast<double>(std::numeric_limits<size_t>::max()))
							cur_condition->minToRetrieve = static_cast<size_t>(min_to_retrieve);

						if(num_params >= 3)
						{
							double max_to_retrieve = EvaluableNode::ToNumber(top_k_ocn[2], std::numeric_limits<double>::infinity());
							max_to_retrieve = std::max(0.0, max_to_retrieve);
							if(max_to_retrieve < static_cast<double>(std::numeric_limits<size_t>::max()))
								cur_condition->maxToRetrieve = static_cast<size_t>(max_to_retrieve);

							if(num_params >= 4)
							{
								double extra_to_retrieve = EvaluableNode::ToNumber(top_k_ocn[3], 0.0);
								extra_to_retrieve = std::max(0.0, extra_to_retrieve);
								if(extra_to_retrieve < static_cast<double>(std::numeric_limits<size_t>::max()))
									cur_condition->extraToRetrieve = static_cast<size_t>(extra_to_retrieve);
							}
						}
					}
				}
			}
			else //single value for k
			{
				cur_condition->maxToRetrieve = static_cast<size_t>(EvaluableNode::ToNumber(top_k_node, 1));
				cur_condition->maxDistance = std::numeric_limits<double>::infinity();
			}
		}

		//set position labels
		EvaluableNode *position_labels = ocn[POSITION_LABELS];
		if(EvaluableNode::IsOrderedArray(position_labels))
		{
			cur_condition->positionLabels.reserve(position_labels->GetOrderedChildNodes().size());
			for(auto &pl : position_labels->GetOrderedChildNodes())
			{
				StringInternPool::StringID label_sid = EvaluableNode::ToStringIDIfExists(pl);
				if(Entity::IsLabelValidAndPublic(label_sid))
					cur_condition->positionLabels.push_back(label_sid);
				else
					cur_condition->queryType = ENT_NULL;
			}
		}

		//select based on type for position or entities
		if(condition_type == ENT_QUERY_ENTITY_CONVICTIONS
			|| condition_type == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
			|| condition_type == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS
			|| condition_type == ENT_QUERY_ENTITY_KL_DIVERGENCES)
		{
			EvaluableNode *entities = ocn[POSITION];
			if(EvaluableNode::IsOrderedArray(entities))
			{
				auto &entities_ocn = entities->GetOrderedChildNodesReference();
				cur_condition->existLabels.reserve(entities_ocn.size());
				for(auto &entity_en : entities_ocn)
					cur_condition->existLabels.push_back(EvaluableNode::ToStringIDIfExists(entity_en));
			}
		}
		else if(condition_type == ENT_QUERY_DISTANCE_CONTRIBUTIONS)
		{
			EvaluableNode *positions = ocn[POSITION];
			if(!EvaluableNode::IsOrderedArray(positions))
			{
				cur_condition->queryType = ENT_NULL;
				return;
			}
			cur_condition->positionsToCompare = &positions->GetOrderedChildNodesReference();
		}
		else
		{
			//set position
			EvaluableNode *position = ocn[POSITION];
			if(EvaluableNode::IsOrderedArray(position) && (position->GetNumChildNodes() == cur_condition->positionLabels.size()))
			{
				CopyOrderedChildNodesToImmediateValuesAndTypes(position->GetOrderedChildNodesReference(),
					cur_condition->valueToCompare, cur_condition->valueTypes);
			}
			else // no positions given, default to nulls for each label
			{
				cur_condition->valueToCompare.reserve(cur_condition->positionLabels.size());
				cur_condition->valueTypes.reserve(cur_condition->positionLabels.size());
				for(size_t i = 0; i < cur_condition->positionLabels.size(); i++)
				{
					cur_condition->valueTypes.push_back(ENIVT_NULL);
					cur_condition->valueToCompare.push_back(EvaluableNodeImmediateValue());
				}
			}
		}

		//set minkowski parameter; default to 1.0 for L1 distance
		cur_condition->distEvaluator.pValue = 1.0;
		if(ocn.size() > MINKOWSKI_PARAMETER)
		{
			cur_condition->distEvaluator.pValue = EvaluableNode::ToNumber(ocn[MINKOWSKI_PARAMETER]);

			//make sure valid value, if not, fall back to 2
			if(FastIsNaN(cur_condition->distEvaluator.pValue) || cur_condition->distEvaluator.pValue < 0)
				cur_condition->distEvaluator.pValue = 1.0;
		}

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

		StringInternPool::StringID weights_selection_feature = string_intern_pool.NOT_A_STRING_ID;
		if(ocn.size() > WEIGHTS_SELECTION_FEATURE)
			weights_selection_feature = EvaluableNode::ToStringIDIfExists(ocn[WEIGHTS_SELECTION_FEATURE]);

		PopulateDistanceFeatureParameters(cur_condition->distEvaluator,
			cur_condition->positionLabels.size(), cur_condition->positionLabels,
			weights_node, weights_selection_feature, distance_types_node, attributes_node, deviations_node);
		
		//value transforms for whatever is measured as "distance"
		cur_condition->distanceWeightExponent = 1.0;
		cur_condition->distEvaluator.computeSurprisal = false;
		cur_condition->distEvaluator.transformSurprisalToProb = false;
		if(ocn.size() > DISTANCE_VALUE_TRANSFORM)
		{
			EvaluableNode *dwe_param = ocn[DISTANCE_VALUE_TRANSFORM];
			if(!EvaluableNode::IsNull(dwe_param))
			{
				if(dwe_param->GetType() == ENT_STRING
					&& dwe_param->GetStringIDReference() == GetStringIdFromBuiltInStringId(ENBISI_surprisal_to_prob))
				{
					cur_condition->distEvaluator.computeSurprisal = true;
					cur_condition->distEvaluator.transformSurprisalToProb = true;
				}
				else if(dwe_param->GetType() == ENT_STRING
					&& dwe_param->GetStringIDReference() == GetStringIdFromBuiltInStringId(ENBISI_surprisal))
				{
					cur_condition->distEvaluator.computeSurprisal = true;
				}
				else //try to convert to number
				{
					cur_condition->distanceWeightExponent = EvaluableNode::ToNumber(dwe_param, 1.0);
				}
			}
		}

		cur_condition->weightLabel = StringInternPool::NOT_A_STRING_ID;
		if(ocn.size() > ENTITY_WEIGHT_LABEL_NAME)
			cur_condition->weightLabel = EvaluableNode::ToStringIDIfExists(ocn[ENTITY_WEIGHT_LABEL_NAME]);

		//set random seed
		cur_condition->hasRandomStream = (ocn.size() > RANDOM_SEED && !EvaluableNode::IsNull(ocn[RANDOM_SEED]));
		if(cur_condition->hasRandomStream)
			cur_condition->randomStream.SetState(EvaluableNode::ToString(ocn[RANDOM_SEED]));
		else
			cur_condition->randomStream = rs.CreateOtherStreamViaRand();

		//set radius label
		if(ocn.size() > RADIUS_LABEL)
			cur_condition->singleLabel = EvaluableNode::ToStringIDIfExists(ocn[RADIUS_LABEL]);
		else
			cur_condition->singleLabel = StringInternPool::NOT_A_STRING_ID;

		//set numerical precision
		cur_condition->distEvaluator.highAccuracyDistances = false;
		cur_condition->distEvaluator.recomputeAccurateDistances = true;
		if(ocn.size() > NUMERICAL_PRECISION)
		{
			StringInternPool::StringID np_sid = EvaluableNode::ToStringIDIfExists(ocn[NUMERICAL_PRECISION]);
			if(np_sid == GetStringIdFromBuiltInStringId(ENBISI_precise))
			{
				cur_condition->distEvaluator.highAccuracyDistances = true;
				cur_condition->distEvaluator.recomputeAccurateDistances = false;
			}
			else if(np_sid == GetStringIdFromBuiltInStringId(ENBISI_fast))
			{
				cur_condition->distEvaluator.highAccuracyDistances = false;
				cur_condition->distEvaluator.recomputeAccurateDistances = false;
			}
			//don't need to do anything for np_sid == ENBISI_recompute_precise because it's default
		}

		cur_condition->returnSortedList = false;
		cur_condition->additionalSortedListLabels.clear();
		if(condition_type == ENT_QUERY_WITHIN_GENERALIZED_DISTANCE
			|| condition_type == ENT_QUERY_NEAREST_GENERALIZED_DISTANCE
			|| condition_type == ENT_QUERY_DISTANCE_CONTRIBUTIONS
			|| condition_type == ENT_QUERY_ENTITY_DISTANCE_CONTRIBUTIONS)
		{
			if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0)
			{
				EvaluableNode *list_param = ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0];
				cur_condition->returnSortedList = EvaluableNode::IsTrue(list_param);
				if(!EvaluableNode::IsNull(list_param))
				{
					if(list_param->GetType() == ENT_STRING)
					{
						cur_condition->additionalSortedListLabels.push_back(list_param->GetStringIDReference());
					}
					else
					{
						for(auto label_node : list_param->GetOrderedChildNodes())
							cur_condition->additionalSortedListLabels.push_back(EvaluableNode::ToStringIDIfExists(label_node));
					}
				}
			}
		}
		else if(condition_type == ENT_QUERY_ENTITY_CONVICTIONS
			|| condition_type == ENT_QUERY_ENTITY_GROUP_KL_DIVERGENCE
			|| condition_type == ENT_QUERY_ENTITY_KL_DIVERGENCES)
		{
			cur_condition->convictionOfRemoval = false;
			if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0)
				cur_condition->convictionOfRemoval = EvaluableNode::IsTrue(ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 0]);

			if(condition_type == ENT_QUERY_ENTITY_CONVICTIONS || condition_type == ENT_QUERY_ENTITY_KL_DIVERGENCES)
			{
				if(ocn.size() > NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 1)
				{
					EvaluableNode *list_param = ocn[NUM_MINKOWSKI_DISTANCE_QUERY_PARAMETERS + 1];
					cur_condition->returnSortedList = EvaluableNode::IsTrue(list_param);
					if(!EvaluableNode::IsNull(list_param))
					{
						if(list_param->GetType() == ENT_STRING)
						{
							cur_condition->additionalSortedListLabels.push_back(list_param->GetStringIDReference());
						}
						else
						{
							for(auto label_node : list_param->GetOrderedChildNodes())
								cur_condition->additionalSortedListLabels.push_back(EvaluableNode::ToStringIDIfExists(label_node));
						}
					}
				}
			}
		}
	}

	//builds a query condition from cn
	inline void BuildNonDistanceCondition(EvaluableNode *cn, EvaluableNodeType type,
		std::vector<EntityQueryCondition> &conditions, RandomStream &rs)
	{
		auto &ocn = cn->GetOrderedChildNodes();

		//validate number of parameters
		switch(type)
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
		switch(type)
		{
			case ENT_QUERY_NOT_EXISTS:
			case ENT_QUERY_EXISTS:
			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_EQUALS:
			case ENT_QUERY_NOT_BETWEEN:
				requires_new_condition = (conditions.size() == 0 || conditions.back().queryType != type);
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
		if(	   type == ENT_QUERY_NOT_EXISTS
			|| type == ENT_QUERY_EXISTS
			|| type == ENT_QUERY_MIN
			|| type == ENT_QUERY_MAX
			|| type == ENT_QUERY_SUM
			|| type == ENT_QUERY_MODE
			|| type == ENT_QUERY_QUANTILE
			|| type == ENT_QUERY_GENERALIZED_MEAN
			|| type == ENT_QUERY_MIN_DIFFERENCE
			|| type == ENT_QUERY_MAX_DIFFERENCE
			|| type == ENT_QUERY_VALUE_MASSES
			|| type == ENT_QUERY_LESS_OR_EQUAL_TO
			|| type == ENT_QUERY_GREATER_OR_EQUAL_TO
			|| type == ENT_QUERY_NOT_EQUALS
			|| type == ENT_QUERY_EQUALS
			|| type == ENT_QUERY_BETWEEN
			|| type == ENT_QUERY_NOT_BETWEEN
			|| type == ENT_QUERY_AMONG
			|| type == ENT_QUERY_NOT_AMONG)
		{
			if(ocn.size() >= 1)
				label_sid = EvaluableNode::ToStringIDIfExists(ocn[0]);

			if(!Entity::IsLabelValidAndPublic(label_sid))
			{
				cur_condition->queryType = ENT_NULL;
				return;
			}
		}

		//actually populate the condition parameters from the evaluable nodes
		switch(type)
		{
			case ENT_QUERY_SELECT:
			{
				cur_condition->maxToRetrieve = (ocn.size() > 0) ? static_cast<size_t>(EvaluableNode::ToNumber(ocn[0], 1)) : 0;

				cur_condition->hasStartOffset = (ocn.size() > 1);
				cur_condition->startOffset = cur_condition->hasStartOffset ? static_cast<size_t>(EvaluableNode::ToNumber(ocn[1], 1)) : 0;

				cur_condition->hasRandomStream = (ocn.size() > 2 && !EvaluableNode::IsNull(ocn[2]));
				if(cur_condition->hasRandomStream)
					cur_condition->randomStream.SetState(EvaluableNode::ToString(ocn[2]));
				else
					cur_condition->randomStream = rs.CreateOtherStreamViaRand();

				break;
			}
			case ENT_QUERY_SAMPLE:
			{
				cur_condition->maxToRetrieve = (ocn.size() > 0) ? static_cast<size_t>(EvaluableNode::ToNumber(ocn[0], 1)) : 1;
				cur_condition->singleLabel = (ocn.size() > 1) ? EvaluableNode::ToStringIDIfExists(ocn[1]) : StringInternPool::NOT_A_STRING_ID;

				cur_condition->hasRandomStream = (ocn.size() > 2 && !EvaluableNode::IsNull(ocn[2]));
				if(cur_condition->hasRandomStream)
					cur_condition->randomStream.SetState(EvaluableNode::ToString(ocn[2]));
				else
					cur_condition->randomStream = rs.CreateOtherStreamViaRand();
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
						cur_condition->existLabels.reserve(entity_sids->GetOrderedChildNodes().size());
						for(auto &esid : entity_sids->GetOrderedChildNodes())
						{
							StringInternPool::StringID entity_sid = EvaluableNode::ToStringIDIfExists(esid);
							cur_condition->existLabels.push_back(entity_sid);
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
				if(EvaluableNode::IsNumericOrNull(low_value) || EvaluableNode::IsNumericOrNull(high_value))
				{
					cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(
						EvaluableNode::ToNumber(low_value), EvaluableNode::ToNumber(high_value)));

					cur_condition->valueTypes.push_back(ENIVT_NUMBER);
				}
				else
				{
					StringInternPool::StringID low_sid = EvaluableNode::ToStringIDIfExists(low_value);
					StringInternPool::StringID high_sid = EvaluableNode::ToStringIDIfExists(high_value);

					cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(low_sid, high_sid));

					cur_condition->valueTypes.push_back(ENIVT_STRING_ID);
				}

				break;
			}

			case ENT_QUERY_AMONG:
			case ENT_QUERY_NOT_AMONG:
			{
				cur_condition->singleLabel = label_sid;

				//already checked for nullptr above
				CopyOrderedChildNodesToImmediateValuesAndTypes(ocn[1]->GetOrderedChildNodes(),
					cur_condition->valueToCompare, cur_condition->valueTypes);
				break;
			}

			case ENT_QUERY_NOT_EXISTS:
			case ENT_QUERY_EXISTS:
			{
				//get label and append it if it is valid (otherwise don't match on anything)
				if(ocn.size() >= 1)
					cur_condition->existLabels.push_back(label_sid);

				break;
			}

			case ENT_QUERY_MIN:
			case ENT_QUERY_MAX:
			{
				cur_condition->singleLabel = label_sid;

				//default to retrieve 1
				cur_condition->maxToRetrieve = 1;
				if(ocn.size() >= 2)
				{
					EvaluableNode *value = ocn[1];
					cur_condition->maxToRetrieve = static_cast<size_t>(EvaluableNode::ToNumber(value, 1));
				}

				if(ocn.size() <= 2 || EvaluableNode::IsTrue(ocn[2]))
					cur_condition->singleLabelType = ENIVT_NUMBER;
				else
					cur_condition->singleLabelType = ENIVT_STRING_ID;

				break;
			}

			case ENT_QUERY_LESS_OR_EQUAL_TO:
			case ENT_QUERY_GREATER_OR_EQUAL_TO:
			{
				//these query types will be transformed into a between query, including the appropriate infinite

				//number of parameters checked above
				EvaluableNode *compare_value = ocn[1];

				if(EvaluableNode::IsNumericOrNull(compare_value))
				{
					if(type == ENT_QUERY_LESS_OR_EQUAL_TO)
						cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(
							-std::numeric_limits<double>::infinity(), EvaluableNode::ToNumber(compare_value)));
					else
						cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(
							EvaluableNode::ToNumber(compare_value), std::numeric_limits<double>::infinity()));

					cur_condition->valueTypes.push_back(ENIVT_NUMBER);
				}
				else
				{
					if(type == ENT_QUERY_LESS_OR_EQUAL_TO)
						cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(
							string_intern_pool.NOT_A_STRING_ID, EvaluableNode::ToStringIDIfExists(compare_value)));
					else
						cur_condition->pairedLabels.emplace_back(label_sid, std::make_pair(
							EvaluableNode::ToStringIDIfExists(compare_value), string_intern_pool.NOT_A_STRING_ID));

					cur_condition->valueTypes.push_back(ENIVT_STRING_ID);
				}

				cur_condition->queryType = ENT_QUERY_BETWEEN;
				break;
			}


			case ENT_QUERY_NOT_EQUALS:
			case ENT_QUERY_EQUALS:
			{				
				EvaluableNodeImmediateValue value;
				EvaluableNodeImmediateValueType value_type = value.CopyValueFromEvaluableNode(ocn[1]);

				cur_condition->valueTypes.push_back(value_type);
				cur_condition->singleLabels.emplace_back(std::make_pair(label_sid, value));

				break;
			}

			case ENT_QUERY_MIN_DIFFERENCE:
				cur_condition->singleLabel = label_sid;
				//weightLabel is used in common paths, so make sure it is initialized
				cur_condition->weightLabel = string_intern_pool.NOT_A_STRING_ID;

				cur_condition->maxDistance = std::numeric_limits<double>::quiet_NaN();
				if(ocn.size() >= 2)
					cur_condition->maxDistance = EvaluableNode::ToNumber(ocn[1]);

				cur_condition->includeZeroDifferences = true;
				if(ocn.size() >= 3)
					cur_condition->includeZeroDifferences = EvaluableNode::IsTrue(ocn[2]);
				break;

			case ENT_QUERY_MAX_DIFFERENCE:
				cur_condition->singleLabel = label_sid;
				//weightLabel is used in common paths, so make sure it is initialized
				cur_condition->weightLabel = string_intern_pool.NOT_A_STRING_ID;
				
				cur_condition->maxDistance = std::numeric_limits<double>::quiet_NaN();
				if(ocn.size() >= 2)
					cur_condition->maxDistance = EvaluableNode::ToNumber(ocn[1]);

				break;

			case ENT_QUERY_SUM:
			case ENT_QUERY_MODE:
			case ENT_QUERY_VALUE_MASSES:
			{
				cur_condition->singleLabel = label_sid;

				cur_condition->weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 2)
					cur_condition->weightLabel = EvaluableNode::ToStringIDIfExists(ocn[1]);

				break;
			}

			case ENT_QUERY_QUANTILE:
			{
				cur_condition->singleLabel = label_sid;

				cur_condition->qPercentage = 0.5;
				if(ocn.size() >= 2)
					cur_condition->qPercentage = EvaluableNode::ToNumber(ocn[1]);

				cur_condition->weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 3)
					cur_condition->weightLabel = EvaluableNode::ToStringIDIfExists(ocn[2]);

				break;
			}

			case ENT_QUERY_GENERALIZED_MEAN:
			{
				cur_condition->singleLabel = label_sid;

				cur_condition->distEvaluator.pValue = 1;
				if(ocn.size() >= 2)
					cur_condition->distEvaluator.pValue = EvaluableNode::ToNumber(ocn[1]);

				cur_condition->weightLabel = StringInternPool::NOT_A_STRING_ID;
				if(ocn.size() >= 3)
					cur_condition->weightLabel = EvaluableNode::ToStringIDIfExists(ocn[2]);

				cur_condition->center = 0.0;
				if(ocn.size() >= 4)
					cur_condition->center = EvaluableNode::ToNumber(ocn[3], 0.0);

				cur_condition->calculateMoment = false;
				if(ocn.size() >= 5)
					cur_condition->calculateMoment = EvaluableNode::IsTrue(ocn[4]);

				cur_condition->absoluteValue = false;
				if(ocn.size() >= 6)
					cur_condition->absoluteValue = EvaluableNode::IsTrue(ocn[5]);

				break;
			}

			default:;
		}//end switch
	}
};
