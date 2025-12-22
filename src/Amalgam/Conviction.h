#pragma once

//project headers:
#include "EntityQueriesStatistics.h"
#include "KnnCache.h"
#include "SeparableBoxFilterDataStore.h"

//system headers:
#include <numeric>

//KL(P||Q) = Sum(p(i) * log( p(i) / q(i) ), natural base
inline double KullbackLeiblerDivergence(const std::vector<double> &p, const std::vector<double> &q)
{
	double sum = 0.0;
	for(size_t i = 0; i < p.size(); i++)
	{
		if(q[i] != 0 && !FastIsNaN(q[i]))
			sum += p[i] ? p[i] * std::log(p[i] / q[i]) : 0;
	}
	return sum;
}

//computes the KL divergence between p and q.distance only for features specified by the indices given by q.reference
//i.e. this will give equivelent value if calling normal KL on p and q if p and q are the same value at indices oother than those in q.reference
//note that there are two versions of this function with the DistanceReferencePair parameters flipped
inline double PartialKullbackLeiblerDivergenceFromIndices(const std::vector<double> &p, const std::vector<DistanceReferencePair<size_t>> &q)
{
	double sum = 0.0;
	for(const auto &changed_contrib : q)
	{
		const double q_i = changed_contrib.distance;
		const double p_i = p[changed_contrib.reference];
		if(q_i != 0 && !FastIsNaN(q_i))
			sum += p_i ? p_i * std::log(p_i / q_i) : 0;
	}
	return sum;
}

//computes the KL divergence between p.distance and q only for features specified by the indices given by p.reference
//i.e. this will give equivelent value if calling normal KL on p and q if p and q are the same value at indices other than those in p.reference
//note that there are two versions of this function with the DistanceReferencePair parameters flipped
inline double PartialKullbackLeiblerDivergenceFromIndices(const std::vector<DistanceReferencePair<size_t>> &p, const std::vector<double> &q)
{
	double sum = 0.0;
	for(const auto &changed_contrib : p)
	{
		const double p_i = changed_contrib.distance;
		const double q_i = q[changed_contrib.reference];
		if(q_i != 0 && !FastIsNaN(q_i))
			sum += p_i ? p_i * std::log(p_i / q_i) : 0;
	}
	return sum;
}

//manages all types of processing related to conviction
class ConvictionProcessor
{
	using EntityReference = size_t;
	using EntityReferenceSet = BitArrayIntegerSet;
public:
	//buffers to be reused for less memory churn
	struct ConvictionProcessorBuffers
	{
		std::vector<DistanceReferencePair<EntityReference>> neighbors;
		std::vector<DistanceReferencePair<size_t>> updatedDistanceContribs;
		std::vector<double> baseDistanceContributions;
		std::vector<double> baseDistanceProbabilities;
		std::vector<EvaluableNodeImmediateValueType> positionValueTypes;
		std::vector<EvaluableNodeImmediateValue> positionValues;
	};

#ifdef MULTITHREAD_SUPPORT
	ConvictionProcessor(KnnCache &cache,
		EntityQueriesStatistics::DistanceTransform<EntityReference> &distance_transform, size_t num_nearest_neighbors,
		StringInternPool::StringID radius_label, bool run_concurrently)
#else
	ConvictionProcessor(KnnCache &cache,
		EntityQueriesStatistics::DistanceTransform<EntityReference> &distance_transform, size_t num_nearest_neighbors,
		StringInternPool::StringID radius_label)
#endif
	{
		knnCache = &cache;
		distanceTransform = &distance_transform;
		numNearestNeighbors = num_nearest_neighbors;
		radiusLabel = radius_label;

#ifdef MULTITHREAD_SUPPORT
		runConcurrently = run_concurrently;
#endif
	}

	//Computes distance contribution for entity_reference
	// if additional_holdout_reference is specified, then it will ignore that case
	inline double ComputeDistanceContribution(EntityReference entity_reference,
		EntityReference additional_holdout_reference = DistanceReferencePair<EntityReference>::InvalidReference())
	{
		//fetch the knn results from the cache
		buffers.neighbors.clear();
		knnCache->GetKnn(entity_reference, numNearestNeighbors, true,
			buffers.neighbors, additional_holdout_reference);

		double entity_weight = 0.0;
		distanceTransform->getEntityWeightFunction(entity_reference, entity_weight);
		return distanceTransform->ComputeDistanceContribution(buffers.neighbors, entity_weight);
	}

	//Like the other ComputeDistanceContribution, but only includes included_entities
	inline double ComputeDistanceContribution(EntityReference entity_reference, EntityReferenceSet &included_entities)
	{
		//fetch the knn results from the cache
		buffers.neighbors.clear();
		knnCache->GetKnn(entity_reference, numNearestNeighbors, true, buffers.neighbors, included_entities);

		double entity_weight = 0.0;
		distanceTransform->getEntityWeightFunction(entity_reference, entity_weight);
		return distanceTransform->ComputeDistanceContribution(buffers.neighbors, entity_weight);
	}

	//Computes the Distance Contributions for each entity specified in entities_to_compute
	//if entities_to_compute is specified (i.e., not nullptr), any entity not in entities_to_compute will be omitted
	//sets contribs_out to the respective distance contributions of each entity in entities_to_compute, or, for all entities in the cache
	//sets contribs_sum_out to the sum of all distance contributions of entities in entities_to_compute and also, if not nullptr, in the cache.
	inline void ComputeDistanceContributions(EntityReferenceSet *entities_to_compute, std::vector<double> &contribs_out, double &contribs_sum_out)
	{
		if(entities_to_compute == nullptr)
			entities_to_compute = knnCache->GetRelevantEntities();
	
		//compute distance contribution for each entity in entities_to_compute
		contribs_out.resize(entities_to_compute->size());
		IterateOverConcurrentlyIfPossible(*entities_to_compute,
			[this, &contribs_out](auto index, auto entity)
			{
				contribs_out[index] = ComputeDistanceContribution(entity);
			}
		#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
		#endif
		);

		contribs_sum_out = std::accumulate(begin(contribs_out), end(contribs_out), 0.0);
	}

	//computes distance contributions without caching over entities_to_compute
	inline void ComputeDistanceContributionsWithoutCache(EntityReferenceSet *entities_to_compute, std::vector<double> &contribs_out)
	{
		if(entities_to_compute == nullptr)
			entities_to_compute = knnCache->GetRelevantEntities();

		contribs_out.resize(entities_to_compute->size());
		IterateOverConcurrentlyIfPossible(*entities_to_compute,
			[this, &contribs_out](auto index, auto entity)
			{
				knnCache->GetKnnWithoutCache(entity, numNearestNeighbors, true, buffers.neighbors);

				double entity_weight = 0.0;
				distanceTransform->getEntityWeightFunction(entity, entity_weight);
				contribs_out[index] = distanceTransform->ComputeDistanceContribution(buffers.neighbors, entity_weight);
			}
		#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
		#endif
		);
	}

	inline void ComputeDistanceContributionsOnPositions(std::vector<EvaluableNode *> &positions_to_compare, std::vector<double> &contribs_out)
	{
		contribs_out.resize(positions_to_compare.size());
		IterateOverConcurrentlyIfPossible(positions_to_compare,
			[this, &contribs_out](auto index, auto position)
		{
			if(!EvaluableNode::IsOrderedArray(position))
			{
				contribs_out[index] = std::numeric_limits<double>::quiet_NaN();
				return;
			}
			
			CopyOrderedChildNodesToImmediateValuesAndTypes(position->GetOrderedChildNodesReference(),
					buffers.positionValues, buffers.positionValueTypes);

			knnCache->GetKnnWithoutCache(buffers.positionValues, buffers.positionValueTypes,
				numNearestNeighbors, true, buffers.neighbors);
			contribs_out[index] = distanceTransform->ComputeDistanceContribution(buffers.neighbors, 1.0);
		}
	#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
	#endif
		);
	}

#ifdef MULTITHREAD_SUPPORT
	//placeholder function to implement C++20 functionality around std::atomic_ref<double>
	//TODO: remove this when migrating to C++20
	static inline double fetch_add_double(double &obj,
								   double arg,
								   std::memory_order order = std::memory_order_seq_cst)
	{
		//reinterpret as atomic 64-bit integer to support C++17 limitations
		auto *atomic_u64 = reinterpret_cast<std::atomic<std::uint64_t>*>(&obj);

		std::uint64_t expected = atomic_u64->load(order);
		for(;;)
		{
			double cur_val = *reinterpret_cast<double *>(&expected);
			double new_val = cur_val + arg;
			std::uint64_t desired = *reinterpret_cast<std::uint64_t *>(&new_val);

			//try to replace the old bit pattern with the new one.
			if(atomic_u64->compare_exchange_weak(expected, desired, order, std::memory_order_relaxed))
				return *reinterpret_cast<double *>(&expected);

			//on failure expected has the new value
		}
	}
#else
	static inline double fetch_add_double(double &obj,
								   double arg)
	{
		obj += arg;
	}
#endif

	inline void ComputeNeighborWeightsForEntities(EntityReferenceSet *entities_to_compute, std::vector<DistanceReferencePair<size_t>> &neighbors_with_weights)
	{
		if(entities_to_compute == nullptr)
			entities_to_compute = knnCache->GetRelevantEntities();

		neighbors_with_weights.clear();

		if(knnCache->GetNumRelevantEntities() == 0)
			return;

		size_t end_entity_index = knnCache->GetEndEntityIndex();

		auto &entity_probabilities = buffers.baseDistanceProbabilities;
		entity_probabilities.clear();
		entity_probabilities.resize(end_entity_index, 0.0);

		IterateOverConcurrentlyIfPossible(*entities_to_compute,
			[this, &entity_probabilities](auto index, auto entity)
		{
			knnCache->GetKnnWithoutCache(entity, numNearestNeighbors, false, buffers.neighbors);

			distanceTransform->TransformDistances(buffers.neighbors, false);

			double total_prob = 0.0;
			for(auto &n : buffers.neighbors)
				total_prob += n.distance;

			//modulate total probability based on this entity's weight
			//and change operation into a multiply since it's faster than a divide
			double entity_weight = 0.0;
			distanceTransform->getEntityWeightFunction(entity, entity_weight);
			double weight_multiplier = entity_weight / total_prob;

			//accumulate neighbor weights
			for(auto &n : buffers.neighbors)
				fetch_add_double(entity_probabilities[n.reference], n.distance * weight_multiplier);
		}
	#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
	#endif
		);

		//pull together all neighbors with nonzero weight
		for(size_t i = 0; i < entity_probabilities.size(); i++)
		{
			if(entity_probabilities[i] > 0.0)
				neighbors_with_weights.push_back(DistanceReferencePair(entity_probabilities[i], i));
		}
	}

	inline void ComputeNeighborWeightsOnPositions(std::vector<EvaluableNode *> &positions_to_compare, std::vector<DistanceReferencePair<size_t>> &neighbors_with_weights)
	{
		neighbors_with_weights.clear();
		if(knnCache->GetNumRelevantEntities() == 0)
			return;

		size_t end_entity_index = knnCache->GetEndEntityIndex();

		auto &entity_probabilities = buffers.baseDistanceProbabilities;
		entity_probabilities.clear();
		entity_probabilities.resize(end_entity_index, 0.0);

		IterateOverConcurrentlyIfPossible(positions_to_compare,
			[this, &entity_probabilities](auto index, auto position)
		{
			if(!EvaluableNode::IsOrderedArray(position))
				return;

			CopyOrderedChildNodesToImmediateValuesAndTypes(position->GetOrderedChildNodesReference(),
					buffers.positionValues, buffers.positionValueTypes);

			knnCache->GetKnnWithoutCache(buffers.positionValues, buffers.positionValueTypes,
				numNearestNeighbors, false, buffers.neighbors);

			distanceTransform->TransformDistances(buffers.neighbors, false);

			double total_prob = 0.0;
			for(auto &n : buffers.neighbors)
				total_prob += n.distance;

			//change operation into a multiply since it's faster than a divide
			double weight_multiplier = 1.0 / total_prob;

			//accumulate neighbor weights
			for(auto &n : buffers.neighbors)
				fetch_add_double(entity_probabilities[n.reference], n.distance * weight_multiplier);
		}
	#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
	#endif
		);

		//pull together all neighbors with nonzero weight
		for(size_t i = 0; i < entity_probabilities.size(); i++)
		{
			if(entity_probabilities[i] > 0.0)
				neighbors_with_weights.push_back(DistanceReferencePair(entity_probabilities[i], i));
		}
	}


	//Like ComputeDistanceContributions, but will populate contribs_out with a value for each of the included_entities, and will set any relevant entity
	// in the cache but only in included_entities to excluded_entity_distance_contribution_value, which will not be included in the contribs_sum_out
	inline void ComputeDistanceContributionsFromEntities(EntityReferenceSet &included_entities, double excluded_entity_distance_contribution_value,
		std::vector<double> &contribs_out, double &contribs_sum_out)
	{
		//compute distance contribution for each entity in entities_to_compute
		contribs_out.resize(knnCache->GetNumRelevantEntities());
		IterateOverConcurrentlyIfPossible(*knnCache->GetRelevantEntities(),
			[this, &contribs_out, &included_entities](auto index, auto entity)
			{
				//skip entities not specified in included_entities
				if(!included_entities.contains(entity))
					contribs_out[index] = std::numeric_limits<double>::quiet_NaN();
				else
					contribs_out[index] = ComputeDistanceContribution(entity, included_entities);
			}
		#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
		#endif
		);

		contribs_sum_out = 0.0;
		for(auto &contrib : contribs_out)
		{
			if(FastIsNaN(contrib))
				contrib = excluded_entity_distance_contribution_value;
			else
				contribs_sum_out += contrib;
		}
	}

	//Computes the distance contributions for each relevant entity in the cache with the entity at holdout_entity removed from the model
	//populates updated_contribs_out only with the set of entities that have different distance contributions than their base contributions
	//sets updated_contribs_sum_out to the sum of all distance contributions of entities
	inline void UpdateDistanceContributionsWithHoldout(EntityReference holdout_entity, double holdout_replacement_value,
		const std::vector<double> &dist_contribs, const double base_dist_contrib_sum,
		std::vector<DistanceReferencePair<size_t>> &updated_contribs_out, double &updated_contribs_sum_out)
	{
		updated_contribs_sum_out = base_dist_contrib_sum;

		int64_t distance_contribs_index = -1;
		updated_contribs_out.reserve(knnCache->GetNumRelevantEntities());
		for(auto entity_reference : *knnCache->GetRelevantEntities())
		{
			distance_contribs_index++;

			//if holdout reference, replace with expected value
			if(entity_reference == holdout_entity)
			{
				//remove the old distance contribution from the sum
				updated_contribs_sum_out -= dist_contribs[distance_contribs_index];

				//write out whatever the replacement value is for this element
				updated_contribs_out.emplace_back(holdout_replacement_value, distance_contribs_index);
				continue;
			}

			//if the nearest neighbors don't include holdout_entity, the distance contribution will be unchanged
			if(!knnCache->DoesCachedKnnContainEntity(entity_reference, holdout_entity, numNearestNeighbors))
				continue;

			double distance_contribution = ComputeDistanceContribution(entity_reference, holdout_entity);

			//reduce later workload by culling data here - do not mark dc as scaled/different if it actually isn't
			if(dist_contribs[distance_contribs_index] == distance_contribution)
				continue;

			//"replace" the distance contribution in the sum by subtracting the base contribution and adding the scaled contribution
			updated_contribs_sum_out -= dist_contribs[distance_contribs_index];
			updated_contribs_sum_out += distance_contribution;

			//add scaled distance contribution for this element to output
			updated_contribs_out.emplace_back(distance_contribution, distance_contribs_index);
		}

		//if all the distance contributions are zero, (which can happen if all the cases have mismatching nan values and can't be compared to each other in sparse datasets)
		//set the distance contributions to be 1/n for all of them since they are all the 'same'
		if(updated_contribs_sum_out == 0.0)
		{
			double avg_dc = 1.0 / (knnCache->GetNumRelevantEntities());
			for(size_t i = 0; i < updated_contribs_out.size(); i++)
				updated_contribs_out[i].distance = avg_dc;

			updated_contribs_sum_out = updated_contribs_out.size() * avg_dc;
		}
	}

	//Computes the case KL divergence or conviction for each case in entities_to_compute
	//if normalize_convictions is false, it will return the kl divergences, if true, it will return the convictions
	//if conviction_of_removal is true, then it will compute the conviction as if the entities not in base_group_entities were removed,
	// if false, then will compute the conviction as if those entities were added or included
	inline void ComputeCaseKLDivergences(EntityReferenceSet &entities_to_compute, std::vector<double> &convictions_out, bool normalize_convictions, bool conviction_of_removal)
	{
		//prime the cache
	#ifdef MULTITHREAD_SUPPORT
		knnCache->PreCacheKnn(nullptr, numNearestNeighbors + 1, true, runConcurrently);
	#else
		knnCache->PreCacheKnn(nullptr, numNearestNeighbors + 1, true);
	#endif

		//find base distance contributions
		double contrib_sum = 0.0;
		auto &base_dist_contribs = buffers.baseDistanceContributions;
		base_dist_contribs.clear();
		ComputeDistanceContributions(nullptr, base_dist_contribs, contrib_sum);

		//convert base distance contributions to probabilities
		auto &base_dist_probs = buffers.baseDistanceProbabilities;
		base_dist_probs.clear();
		base_dist_probs.reserve(base_dist_contribs.size());

		if(contrib_sum != 0)
		{
			for(const double &contrib : base_dist_contribs)
				base_dist_probs.push_back(contrib / contrib_sum);
		}
		else //if contrib_sum == 0, then each contrib must be 0
		{
			base_dist_probs.resize(base_dist_contribs.size(), 0.0);
		}

		//cache constants for expected values
		const size_t num_relevant_entities = knnCache->GetNumRelevantEntities();
		const double probability_mass_of_non_holdouts = (1.0 - 1.0 / num_relevant_entities);
		// the reciprocal of the ratio of num cases without to num cases with times the contrib_sum; cached for scaling below
		// using the reciprocal here (instead of the more intuitive flip) saves a negation in the loop
		const double updated_contrib_to_contrib_scale_inverse = num_relevant_entities / (contrib_sum * (num_relevant_entities - 1));

		//for measuring kl divergence, only need to measure those entities that have a value that is different
		convictions_out.clear();
		convictions_out.resize(entities_to_compute.size());

		//compute the scaled distance contributions and sums when any 1 case is removed from the model
		//note that the kl_divergence for every non-scaled set is 0, so the sum will not change except for when a case is actually removed from the model
		IterateOverConcurrentlyIfPossible(entities_to_compute,
			[this, &convictions_out, &num_relevant_entities, &contrib_sum, &probability_mass_of_non_holdouts,
				&updated_contrib_to_contrib_scale_inverse, &conviction_of_removal, &base_dist_contribs, &base_dist_probs]
			(auto convictions_out_index, auto entity_reference)
			{
				//compute distance contributions of the entities whose dcs will be changed by the removal of entity_reference
				auto &updated_distance_contribs = buffers.updatedDistanceContribs;
				updated_distance_contribs.clear();
				double updated_contrib_sum = 0.0;
				buffers.neighbors.clear();
				UpdateDistanceContributionsWithHoldout(entity_reference, 1.0 / num_relevant_entities, base_dist_contribs, contrib_sum,
					buffers.updatedDistanceContribs, updated_contrib_sum);

				//convert updated_distance_contribs to probabilities
				//convert via the updated contribution sum and multiply by the probability mass of everything that isn't the holdout
				//multiplying a non-held out distance contribution by this value converts it into a probability
				double updated_dc_to_probability = probability_mass_of_non_holdouts / updated_contrib_sum;

				//convert updated distance contribution into a probability as appropriate
				for(auto &dc : updated_distance_contribs)
				{
					//the knockout case was already already assigned the probability
					if(dc.reference != convictions_out_index)
						dc.distance *= updated_dc_to_probability;
				}

				//compute KL divergence for the values which have different neighbor lists

				//need to compute the KL divergence for the cases that don't have different neighbor lists but are only scaled
				//for conviction_of_removal, this can be computed as
				//d_KL = sum_i -base_distance_probabilities[i] * log( base_distance_probabilities[i] / new_probabilities[i])
				//but because we know new_probabilities[i] = base_distance_probabilities[i] * dc_update_scale we can rewrite this as:
				// d_KL = sum_i -base_distance_probabilities[i] * log( 1 / dc_update_scale) )
				//the logarithm doesn't change and can be pulled out of the sum (pulling out the reciprocal as -1) to be:
				// d_KL = log( dc_update_scale) ) * sum_i base_distance_probabilities[i]
				//but because we've already computed the kl divergence for the updated_distance_contribs (changed neighbor sets),
				// we only want to compute d_KL for those that just need to be scaled
				//for the opposite, the conviction of adding the case, we just flip p and q in the kl divergence:
				// d_KL = sum_i -new_probabilities[i] * log( new_probabilities[i] / base_distance_probabilities[i] )
				//thus (note the negative sign due to the reciprocal of dc_update_scale):
				// d_KL = sum_i -new_probabilities[i] * log( dc_update_scale) )
				double dc_update_scale = updated_contrib_sum * updated_contrib_to_contrib_scale_inverse;

				double kld_updated;
				double kld_scaled;
				if(conviction_of_removal)
				{
					kld_updated = PartialKullbackLeiblerDivergenceFromIndices(base_dist_probs, updated_distance_contribs);

					//need to find unchanged distance contribution relative to the total in order to find the total probability mass
					double total_distance_contribution_unchanged = contrib_sum;
					for(auto &dc : updated_distance_contribs)
						total_distance_contribution_unchanged -= base_dist_contribs[dc.reference];

					double total_probability_mass_changed = (total_distance_contribution_unchanged / contrib_sum);

					kld_scaled = total_probability_mass_changed * std::log(dc_update_scale);
				}
				else
				{
					kld_updated = PartialKullbackLeiblerDivergenceFromIndices(updated_distance_contribs, base_dist_probs);

					//since the updated distance contribs have already been converted to probabilities, can just use them directly
					double total_updated_probability_mass_changed = 1.0;
					for(auto &dc : updated_distance_contribs)
						total_updated_probability_mass_changed -= dc.distance;

					//negative sign due to the reciprocal of dc_update_scale
					kld_scaled = -total_updated_probability_mass_changed * std::log(dc_update_scale);
				}

				double kld_total = kld_updated + kld_scaled;
				//only store values greater than zero
				if(kld_total >= 0.0)
					convictions_out[convictions_out_index] = kld_total;
			}
		#ifdef MULTITHREAD_SUPPORT
			, runConcurrently
		#endif
		);

		bool has_zero_kl = false;
		double kl_sum = 0.0;
		for(auto &conviction : convictions_out)
		{
			//can't be negative, so clamp to zero
			if(conviction > 0.0)
				kl_sum += conviction;
			else
				has_zero_kl = true;
		}

		//average
		const double kl_avg = kl_sum / convictions_out.size();

		//compute convictions
		if(kl_avg == 0.0) //if avg is zero, every conviction becomes one (even 0/0 as the kl denominator dominates)
		{
			convictions_out.clear();
			convictions_out.resize(entities_to_compute.size(), 1.0);
		}
		else if(normalize_convictions)
		{
			if(has_zero_kl)
			{
				for(auto &kl : convictions_out)
				{
					if(kl != 0.0)
						kl = (kl_avg / kl);
				}
			}
			else
			{
				for(auto &kl : convictions_out)
					kl = (kl_avg / kl);
			}
		}
	}

	//Computes the KL divergence for adding a group of cases (a new model) to an existing model.
	//This assumes that the current model is the combined model that already has the 
	//new cases in it with the indices of those original cases specified as base_group_entities
	//if conviction_of_removal is true, then it will compute the conviction as if the entities not in base_group_entities were removed,
	// if false, then will compute the conviction as if those entities were added or included
	inline double ComputeCaseGroupKLDivergence(EntityReferenceSet &base_group_entities, bool conviction_of_removal)
	{
		//prime cache; get double the number of numNearestNeighbors in attempt to reduce the number of queries needed
		// other heuristics other than 2x may be considered, and the effectiveness of the heuristic entirely will depend on the overlap between the two case groups
	#ifdef MULTITHREAD_SUPPORT
		knnCache->PreCacheKnn(nullptr, numNearestNeighbors * 2, true, runConcurrently);
	#else
		knnCache->PreCacheKnn(nullptr, numNearestNeighbors * 2, true);
	#endif

		//compute the resulting combined model distance contributions (reuse buffer)
		std::vector<double> &combined_model_distance_contribs = buffers.baseDistanceContributions;
		combined_model_distance_contribs.clear();
		double contrib_sum = 0.0;
		ComputeDistanceContributions(nullptr, combined_model_distance_contribs, contrib_sum);

		//compute scaled distance contributions of only the base model (only from base_group_entities) (reuse buffer)
		std::vector<double> &scaled_base_distance_contribs = buffers.baseDistanceProbabilities;
		scaled_base_distance_contribs.clear();
		double scaled_base_contrib_sum;

		//compute scaled distance contributions for the base cases in but setting the remaining entities to the probability of 1/n
		ComputeDistanceContributionsFromEntities(base_group_entities, 1.0 / knnCache->GetNumRelevantEntities(),
			scaled_base_distance_contribs, scaled_base_contrib_sum);

		//normalize the combined model distance contributions to convert them into probabilities
		double base_scalar = 1.0 / contrib_sum;
		for(auto &c : combined_model_distance_contribs)
			c *= base_scalar;

		//normalize each scaled distance contribution to convert them into probabilities 
		double prob_scalar = static_cast<double>(base_group_entities.size()) / knnCache->GetNumRelevantEntities();
		prob_scalar /= scaled_base_contrib_sum;

		//for each element that doesn't belong to base_group_entities, scale the probabilities so that the sum is 1.0
		// while leaving existing cases not in base_group_entities as previously set to the proper probability
		size_t distance_contribution_index = 0;
		for(auto entity_reference : *knnCache->GetRelevantEntities())
		{
			if(base_group_entities.contains(entity_reference))
				scaled_base_distance_contribs[distance_contribution_index] *= prob_scalar;

			distance_contribution_index++;
		}

		//compute KL divergence
		if(conviction_of_removal)
			return KullbackLeiblerDivergence(combined_model_distance_contribs, scaled_base_distance_contribs);
		else
			return KullbackLeiblerDivergence(scaled_base_distance_contribs, combined_model_distance_contribs);
	}

	protected:

		KnnCache *knnCache;
		EntityQueriesStatistics::DistanceTransform<EntityReference> *distanceTransform;

		//number of nearest neighbors
		size_t numNearestNeighbors;

		//radius label if applicable
		StringInternPool::StringID radiusLabel;

	#ifdef MULTITHREAD_SUPPORT
		//if true, attempt to run with concurrency
		bool runConcurrently;
	#endif

		//for multithreading, there should be one of these per thread
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
		thread_local
	#endif
			//buffers that can be used for less memory churn (per-thread if multithreaded)
			static ConvictionProcessorBuffers buffers;
};
