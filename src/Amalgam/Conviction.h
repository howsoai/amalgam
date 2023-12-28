#pragma once

//project headers:
#include "ConvictionUtil.h"
#include "EntityQueriesStatistics.h"
#include "KnnCache.h"
#include "SeparableBoxFilterDataStore.h"

//system headers:
#include <numeric>

//manages all types of processing related to conviction
template<typename KnnCache, typename EntityReference, typename EntityReferenceSet>
class ConvictionProcessor
{
public:
	//buffers to be reused for less memory churn
	struct ConvictionProcessorBuffers
	{
		std::vector<DistanceReferencePair<EntityReference>> neighbors;
		std::vector<DistanceReferencePair<size_t>> updatedDistanceContribs;
		std::vector<double> baseDistanceContributions;
		std::vector<double> baseDistanceProbabilities;
	};

#ifdef MULTITHREAD_SUPPORT
	ConvictionProcessor(ConvictionProcessorBuffers &_buffers, KnnCache &cache,
		EntityQueriesStatistics::DistanceTransform<EntityReference> &distance_transform, size_t num_nearest_neighbors,
		StringInternPool::StringID radius_label, bool run_concurrently)
#else
	ConvictionProcessor(ConvictionProcessorBuffers &_buffers, KnnCache &cache,
		EntityQueriesStatistics::DistanceTransform<EntityReference> &distance_transform, size_t num_nearest_neighbors)
#endif
	{
		buffers = &_buffers;
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
		buffers->neighbors.clear();
		knnCache->GetKnn(entity_reference, numNearestNeighbors, buffers->neighbors, additional_holdout_reference);

		return distanceTransform->ComputeDistanceContribution(buffers->neighbors, entity_reference);
	}

	//Like the other ComputeDistanceContribution, but only includes included_entities
	inline double ComputeDistanceContribution(EntityReference entity_reference, EntityReferenceSet &included_entities)
	{
		//fetch the knn results from the cache
		buffers->neighbors.clear();
		knnCache->GetKnn(entity_reference, numNearestNeighbors, buffers->neighbors, included_entities);

		return distanceTransform->ComputeDistanceContribution(buffers->neighbors, entity_reference);
	}

	//Computes the Distance Contributions for each entity specified in entities_to_compute
	//if entities_to_compute is specified (i.e., not nullptr), any entity not in entities_to_compute will be ommitted
	//sets contribs_out to the respective distance contributions of each entity in entities_to_compute, or, for all entities in the cache
	//sets contribs_sum_out to the sum of all distance contributions of entities in entities_to_compute and also, if not nullptr, in the cache.
	inline void ComputeDistanceContributions(EntityReferenceSet *entities_to_compute, std::vector<double> &contribs_out, double &contribs_sum_out)
	{
		buffers->neighbors.reserve(numNearestNeighbors + 1);
		contribs_sum_out = 0.0;
	
		if(entities_to_compute == nullptr)
			entities_to_compute = knnCache->GetRelevantEntities();
	
		//compute distance contribution for each entity in entities_to_compute
		contribs_out.resize(entities_to_compute->size());
		size_t out_index = 0;
		for(auto entity_reference : *entities_to_compute)
		{
			double contrib = ComputeDistanceContribution(entity_reference);
	
			//push back distance contribution, add sub sum to global sum
			contribs_sum_out += contrib;
			contribs_out[out_index++] = contrib;
		}
	}

	//like ComputeDistanceContributions, but doesn't use contribs_sum_out and will run in parallel if applicable
	inline void ComputeDistanceContributions(EntityReferenceSet *entities_to_compute, std::vector<double> &contribs_out)
	{
	#ifdef MULTITHREAD_SUPPORT
		//only cache concurrently if computing for all entities
		if(runConcurrently && (entities_to_compute == nullptr || entities_to_compute->size() == knnCache->GetNumRelevantEntities()))
			knnCache->PreCacheAllKnn(numNearestNeighbors, true);
	#endif

		double contribs_sum_out = 0.0;
		ComputeDistanceContributions(entities_to_compute, contribs_out, contribs_sum_out);
	}

	//Like ComputeDistanceContributions, but will populate contribs_out with a value for each of the included_entities, and will set any releavant entity
	// in the cache but only in included_entities to excluded_entity_distance_contribution_value, which will not be included in the contribs_sum_out
	inline void ComputeDistanceContributionsFromEntities(EntityReferenceSet &included_entities, double excluded_entity_distance_contribution_value,
		std::vector<double> &contribs_out, double &contribs_sum_out)
	{
		buffers->neighbors.reserve(numNearestNeighbors + 1);
		contribs_sum_out = 0.0;

		//compute distance contribution for each entity in entities_to_compute
		contribs_out.resize(knnCache->GetNumRelevantEntities());
		size_t out_index = 0;
		for(auto entity_reference : *knnCache->GetRelevantEntities())
		{
			//skip entities not specified in included_entities, instead store the expected probability value of 1/n
			if(!included_entities.contains(entity_reference))
			{
				contribs_out[out_index++] = excluded_entity_distance_contribution_value;
				//continue to the next entity without updating the contributions sum
				continue;
			}

			double contrib = ComputeDistanceContribution(entity_reference, included_entities);

			//push back distance contribution, add sub sum to global sum
			contribs_sum_out += contrib;
			contribs_out[out_index++] = contrib;
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
				updated_contribs_out.push_back(DistanceReferencePair<size_t>(holdout_replacement_value, distance_contribs_index));
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
			updated_contribs_out.push_back(DistanceReferencePair<size_t>(distance_contribution, distance_contribs_index));
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

	//populates probabilities_out with the distance probabilities given the distance contributions
	static inline void ConvertDistanceContributionsToProbabilities(const std::vector<double> &contributions, const double contribution_sum, std::vector<double> &probabilities_out)
	{
		probabilities_out.reserve(contributions.size());
		if(contribution_sum != 0)
		{
			for(const double &contrib : contributions)
				probabilities_out.push_back(contrib / contribution_sum);
		}
		else //if contrib_sum == 0, then each contrib must be 0
		{
			probabilities_out.resize(contributions.size(), 0.0);
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
		knnCache->PreCacheAllKnn(numNearestNeighbors + 1, runConcurrently);
	#else
		knnCache->PreCacheAllKnn(numNearestNeighbors + 1);
	#endif

		//find base distance contributions
		double contrib_sum = 0.0;
		buffers->baseDistanceContributions.clear();
		ComputeDistanceContributions(nullptr, buffers->baseDistanceContributions, contrib_sum);

		//convert base distance contributions to probabilities
		buffers->baseDistanceProbabilities.clear();
		ConvertDistanceContributionsToProbabilities(buffers->baseDistanceContributions, contrib_sum, buffers->baseDistanceProbabilities);

		//cache constants for expected values
		const size_t num_relevant_entities = knnCache->GetNumRelevantEntities();
		const double probability_mass_of_non_holdouts = (1.0 - 1.0 / num_relevant_entities);
		// the reciprocal of the ratio of num cases without to num cases with times the contrib_sum; cached for scaling below
		// using the reciprocal here (instead of the more intuitive flip) saves a negation in the loop
		const double updated_contrib_to_contrib_scale_inverse = num_relevant_entities / (contrib_sum * (num_relevant_entities - 1));

		//for measuring kl divergence, only need to measure those entities that have a value that is different
		auto &updated_distance_contribs = buffers->updatedDistanceContribs;
		convictions_out.clear();
		convictions_out.reserve(num_relevant_entities);
		double kl_sum = 0.0;
		bool has_zero_kl = false; //flag will be set to true if there are any convictions that are 0, used later to prevent division by 0

		//compute the scaled distance contributions and sums when any 1 case is removed from the model
		//note that the kl_divergence for every non-scaled set is 0, so the sum will not change except for when a case is actually removed from the model
		size_t distance_contribution_index = 0;
		for(auto entity_reference : entities_to_compute)
		{
			//compute distance contributions of the entities whose dcs will be changed by the removal of entity_reference
			updated_distance_contribs.clear();
			double updated_contrib_sum = 0.0;
			buffers->neighbors.clear();
			UpdateDistanceContributionsWithHoldout(entity_reference, 1.0 / num_relevant_entities, buffers->baseDistanceContributions, contrib_sum,
				buffers->updatedDistanceContribs, updated_contrib_sum);

			//convert updated_distance_contribs to probabilities
			//convert via the updated contribution sum and multiply by the probability mass of everything that isn't the holdout
			//multiplying a non-held out distance contribution by this value converts it into a probability
			double updated_dc_to_probability = probability_mass_of_non_holdouts / updated_contrib_sum;

			//convert updated distance contribution into a probability as appropriate
			for(auto &dc : updated_distance_contribs)
			{
				//the knockout case was already already assigned the probability
				if(dc.reference != distance_contribution_index)
					dc.distance *= updated_dc_to_probability;
			}

			//compute KL divergence for the values which have different neighbor lists

			//need to compute the KL divergence for the cases that don't have different neighbor lists but are only scaled
			//for conviction_of_removal, this can be computed as
			//d_KL = sum_i -base_distance_probabilities[i] * log( base_distance_probabilities[i] / new_probabilities[i])
			//but because we know new_probabilities[i] = base_distance_probabilities[i] * dc_update_scale we can rewrite this as:
			// d_KL = sum_i -base_distance_probabilities[i] * log( 1 / dc_update_scale) )
			//the logarithm doesn't change and can be pulled out of the sum (pulling out the reciprocol as -1) to be:
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
				kld_updated = PartialKullbackLeiblerDivergenceFromIndices(buffers->baseDistanceProbabilities, updated_distance_contribs);

				//need to find unchanged distance contribution relative to the total in order to find the total probability mass
				double total_distance_contribution_unchanged = contrib_sum;
				for(auto &dc : updated_distance_contribs)
					total_distance_contribution_unchanged -= buffers->baseDistanceContributions[dc.reference];

				double total_probability_mass_changed = (total_distance_contribution_unchanged / contrib_sum);

				kld_scaled = total_probability_mass_changed * std::log(dc_update_scale);
			}
			else
			{
				kld_updated = PartialKullbackLeiblerDivergenceFromIndices(updated_distance_contribs, buffers->baseDistanceProbabilities);

				//since the updated distance contribs have already been converted to probabilities, can just use them directly
				double total_updated_probability_mass_changed = 1.0;
				for(auto &dc : updated_distance_contribs)
					total_updated_probability_mass_changed -= dc.distance;

				//negative sign due to the reciprocal of dc_update_scale
				kld_scaled = -total_updated_probability_mass_changed * std::log(dc_update_scale);
			}

			double kld_total = kld_updated + kld_scaled;

			//can't be negative, so clamp to zero
			if(kld_total >= 0.0)
				kl_sum += kld_total;
			else
			{
				kld_total = 0.0;
				has_zero_kl = true;
			}

			convictions_out.push_back(kld_total);
			distance_contribution_index++;
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
		// other heuristics other than 2x may be considered, and the effectiness of the heuristic entirely will depend on the overlap between the two case groups
	#ifdef MULTITHREAD_SUPPORT
		knnCache->PreCacheAllKnn(numNearestNeighbors * 2, runConcurrently);
	#else
		knnCache->PreCacheAllKnn(numNearestNeighbors * 2);
	#endif

		//compute the resulting combined model distance contributions (reuse buffer)
		std::vector<double> &combined_model_distance_contribs = buffers->baseDistanceContributions;
		combined_model_distance_contribs.clear();
		double contrib_sum = 0.0;
		ComputeDistanceContributions(nullptr, combined_model_distance_contribs, contrib_sum);

		//compute scaled distance contributions of only the base model (only from base_group_entities) (reuse buffer)
		std::vector<double> &scaled_base_distance_contribs = buffers->baseDistanceProbabilities;
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

		//reusable memory buffers
		ConvictionProcessorBuffers *buffers;

	#ifdef MULTITHREAD_SUPPORT
		//if true, attempt to run with concurrency
		bool runConcurrently;
	#endif
};
