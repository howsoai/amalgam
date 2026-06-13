//project headers:
#include "EntityQueriesDensityFunctions.h"

#define HDBSCAN

#ifdef HDBSCAN

void EntityQueriesDensityProcessor::BuildMutualReachabilityMST(std::vector<double> &core_distances, std::vector<size_t> &order,
	std::vector<double> &edge_distances, std::vector<size_t> &parent_entities)
{
	size_t num_entity_ids = core_distances.size();
	// Clear and resize containers to the appropriate size based on the internal ID system
	edge_distances.assign(num_entity_ids, std::numeric_limits<double>::infinity());
	parent_entities.assign(num_entity_ids, std::numeric_limits<size_t>::max());

	// The first item in 'order' (highest core distance) is the root of the MST.
	size_t root = order[0];
	parent_entities[root] = root;
	edge_distances[root] = 0.0;

	for(size_t order_index = 1; order_index < order.size(); ++order_index)
	{
		size_t cur_entity_index = order[order_index];
		size_t best_parent = std::numeric_limits<size_t>::max();
		double best_dist = std::numeric_limits<double>::max();

		auto &neighbors = knnCache->GetKnnCache(cur_entity_index);
		for(auto &nb : neighbors)
		{
			size_t neighbor_idx = nb.reference;
			// Mutual Reachability Distance: max(core_dist_i, core_dist_j, dist_ij)
			// In your surprisal space, this ensures that the distance is "stretched" 
			// by the density of both points before we calculate the inverse for 'density'.
			double mutual_reach = std::max({ core_distances[cur_entity_index],
											  core_distances[neighbor_idx], nb.distance });

			if(mutual_reach < best_dist)
			{
				best_dist = mutual_reach;
				best_parent = neighbor_idx;
			}
		}

		// Only fall back to root if the k-nearest neighborhood provides no viable connection.
		// This minimizes "jumps" that create fragmented clusters in high-value space.
		if(best_parent == std::numeric_limits<size_t>::max())
		{
			best_dist = std::max(core_distances[cur_entity_index], core_distances[root]);
			best_parent = root;
		}

		parent_entities[cur_entity_index] = best_parent;
		edge_distances[cur_entity_index] = best_dist;
	}
}

void EntityQueriesDensityProcessor::ExtractClustersFromMST(EntityReferenceSet &entities_to_compute,
	std::vector<double> &core_distances, std::vector<double> &edge_distances,
	std::vector<size_t> &parent_entities, std::vector<size_t> &order, double minimum_cluster_weight,
	std::vector<size_t> &cluster_ids, std::vector<double> &stabilities)
{
	size_t num_entity_ids = edge_distances.size();

	// Density is 1 / mutual reachability distance.
	// Note: Since your distances are in surprisal space, we ensure a small epsilon 
	// to prevent division-by-zero/infinity errors without altering the underlying values.
	std::vector<double> densities(num_entity_ids, 0.0);
	for(auto entity_index : entities_to_compute)
	{
		if(edge_distances[entity_index] > 1e-9)
			densities[entity_index] = 1.0 / edge_distances[entity_index];
	}

	size_t root_index = order.front();
	if(core_distances[root_index] > 1e-9)
	{
		densities[root_index] = 1.0 / core_distances[root_index];
	}

	// Build an adjacency list of children to convert the O(N^2) lookup into O(N).
	std::vector<std::vector<size_t>> children_list(num_entity_ids);
	for(size_t i = 0; i < num_entity_ids; ++i)
	{
		if(parent_entities[i] != i && parent_entities[i] != std::numeric_limits<size_t>::max())
		{
			children_list[parent_entities[i]].push_back(i);
		}
	}

	std::vector<double> subtree_cumulative_weights(num_entity_ids, 0.0);
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;
		size_t parent_idx = parent_entities[entity_index];

		double w = 1.0;
		distanceTransform->getEntityWeightFunction(entity_index, w);
		subtree_cumulative_weights[entity_index] += w;

		if(parent_idx != entity_index && parent_idx != std::numeric_limits<size_t>::max())
		{
			subtree_cumulative_weights[parent_idx] += subtree_cumulative_weights[entity_index];
		}
	}

	stabilities.assign(num_entity_ids, 0.0);
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;
		// Optimization: Only iterate over the children of the current node.
		for(size_t child_idx : children_list[entity_index])
		{
			double delta_density = densities[child_idx] - densities[entity_index];
			if(delta_density < 0.0) delta_density = 0.0;
			stabilities[entity_index] += delta_density * subtree_cumulative_weights[child_idx];
		}
	}

	cluster_ids.assign(num_entity_ids, 0);
	size_t next_cluster_id = 1;
	constexpr double stability_eps = 1e-12;

	std::vector<size_t> search_stack;
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;

		if(cluster_ids[entity_index] != 0) continue;

		// Criteria for starting a cluster: 
		// 1. Stability is above the floating point noise floor.
		// 2. The subtree has enough weight to satisfy your minimum_cluster_weight.
		if(stabilities[entity_index] < stability_eps ||
		   subtree_cumulative_weights[entity_index] < minimum_cluster_weight)
		{
			continue;
		}

		// Ancestor Check: Ensure we aren't starting a new cluster inside an already identified one.
		bool ancestor_clustered = false;
		size_t current_anc = parent_entities[entity_index];
		while(current_anc != entity_index)
		{
			if(cluster_ids[current_anc] != 0)
			{
				ancestor_clustered = true;
				break;
			}
			if(current_anc == parent_entities[current_anc]) break;
			current_anc = parent_entities[current_anc];
		}
		if(ancestor_clustered) continue;

		// Successfully found a cluster root. Assign ID and label all connected descendants.
		cluster_ids[entity_index] = next_cluster_id;
		search_stack.clear();
		search_stack.push_back(entity_index);

		while(!search_stack.empty())
		{
			size_t curr = search_stack.back();
			search_stack.pop_back();
			cluster_ids[curr] = next_cluster_id;

			for(size_t child : children_list[curr])
			{
				if(cluster_ids[child] == 0)
				{
					search_stack.push_back(child);
				}
			}
		}

		// Update the weights of parents as this weight is now "consumed" by a cluster.
		double consumed = subtree_cumulative_weights[entity_index];
		size_t up = parent_entities[entity_index];
		while(up != entity_index && up != std::numeric_limits<size_t>::max())
		{
			subtree_cumulative_weights[up] -= consumed;
			if(up == parent_entities[up]) break;
			up = parent_entities[up];
		}

		next_cluster_id++;
	}
}

void EntityQueriesDensityProcessor::ComputeCaseClusters(EntityReferenceSet &entities_to_compute,
		std::vector<double> &clusters_out, double minimum_cluster_weight)
{
	//prime the cache
#ifdef MULTITHREAD_SUPPORT
	knnCache->PreCacheKnn(&entities_to_compute, numNearestNeighbors, true, runConcurrently);
#else
	knnCache->PreCacheKnn(&entities_to_compute, numNearestNeighbors, true);
#endif

	//find distance contributions to use as core weights
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();
	auto &core_distances = buffers.baseDistanceContributions;
	core_distances.clear();
	core_distances.resize(num_entity_indices, std::numeric_limits<double>::infinity());

	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[this, &core_distances](auto /*unused index*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

		double entity_weight = 1.0;
		distanceTransform->getEntityWeightFunction(entity, entity_weight);

		//TODO 24886: evaluate core distance, DC is too small, current algorithm is too large and stabilities go to zero
		//core_distances[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);
		size_t num_neighbors_by_bandwidth = distanceTransform->TransformDistances(neighbors, false);
		core_distances[entity] = neighbors[num_neighbors_by_bandwidth - 1].distance;
	}
#ifdef MULTITHREAD_SUPPORT
		, runConcurrently
#endif
	);

	//entity indices, sorted descending by core distance
	std::vector<size_t> order;
	order.reserve(num_entities);
	for(auto entity : entities_to_compute)
		order.push_back(entity);
	std::stable_sort(order.begin(), order.end(),
		[&](size_t i, size_t j) { return core_distances[i] > core_distances[j]; });

	//reuse baseDistanceProbabilities, but because clustering is not typically done repeatedly,
	//don't reuse any of the other buffers
	auto &edge_distances = buffers.baseDistanceProbabilities;
	std::vector<size_t> parent_entities;
	BuildMutualReachabilityMST(core_distances, order, edge_distances, parent_entities);

	std::vector<size_t> cluster_ids_tmp;
	std::vector<double> node_stabilities;
	ExtractClustersFromMST(entities_to_compute, core_distances, edge_distances, parent_entities, order,
		minimum_cluster_weight, cluster_ids_tmp, node_stabilities);

	//convert integer ids to double
	clusters_out.clear();
	clusters_out.reserve(num_entities);
	for(auto entity_id : entities_to_compute)
		clusters_out.emplace_back(static_cast<double>(cluster_ids_tmp[entity_id]));
}
#else

template <typename T>
static inline void RemoveDuplicates(std::vector<T> &v)
{
	std::sort(v.begin(), v.end());
	auto last = std::unique(v.begin(), v.end());
	v.erase(last, v.end());
}

static inline void PruneAndCompactClusterIds(EntityQueriesDensityProcessor::EntityReferenceSet &entities_to_compute,
	std::vector<size_t> &cluster_ids,
	EntityQueriesStatistics::DistanceTransform<EntityQueriesDensityProcessor::EntityReference> &distance_transform,
	double min_weight, std::vector<double> &clusters_out)
{
	FastHashMap<size_t, size_t> cluster_id_to_compact_cluster_id;
	FastHashMap<size_t, double> total_weights_map;
	for(auto entity_index : entities_to_compute)
	{
		size_t cluster_id = cluster_ids[entity_index];
		if(cluster_id == 0)
			continue;

		double entity_weight = 1.0;
		distance_transform.getEntityWeightFunction(entity_index, entity_weight);

		cluster_id_to_compact_cluster_id.emplace(cluster_id, 0);
		total_weights_map[cluster_id] += entity_weight;
	}

	//determine cluster ids to erase
	FastHashSet<size_t> cluster_ids_to_erase;
	for(auto &[id, w] : total_weights_map)
	{
		if(w < min_weight)
		{
			cluster_ids_to_erase.insert(id);
			cluster_id_to_compact_cluster_id.erase(id);
		}
	}

	//remove clusters that are too small
	for(auto &id : cluster_ids)
	{
		if(cluster_ids_to_erase.find(id) != end(cluster_ids_to_erase))
			id = 0;
	}

	//build compact mapping for surviving ids
	size_t next_id = 1;
	for(auto &cluster_mapping : cluster_id_to_compact_cluster_id)
		cluster_mapping.second = next_id++;

	//map ids and convert to double
	clusters_out.clear();
	clusters_out.reserve(entities_to_compute.size());
	for(auto entity_index : entities_to_compute)
	{
		double id = 0.0;
		if(cluster_ids[entity_index] != 0)
		{
			size_t cluster_id = cluster_ids[entity_index];
			id = static_cast<double>(cluster_id_to_compact_cluster_id[cluster_id]);
		}
		clusters_out.emplace_back(id);
	}
}

void EntityQueriesDensityProcessor::ComputeCaseClusters(EntityReferenceSet &entities_to_compute,
	std::vector<double> &clusters_out, double minimum_cluster_weight)
{
	//prime the cache, grabbing many extra cases in case the distance contributions
	//occasionally push past what is needed
#ifdef MULTITHREAD_SUPPORT
	knnCache->PreCacheKnn(&entities_to_compute, 3 * numNearestNeighbors, true, runConcurrently);
#else
	knnCache->PreCacheKnn(&entities_to_compute, 3 * numNearestNeighbors, true);
#endif

	//find distance contributions to use as core weights
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();
	auto &distance_contributions = buffers.baseDistanceContributions;
	distance_contributions.clear();
	distance_contributions.resize(num_entity_indices, std::numeric_limits<double>::infinity());
	std::vector<double> entity_bandwidth_case_weight(num_entity_indices, 0.0);
	std::vector<size_t> entity_bandwidth_case_count(num_entity_indices, 0);

	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[this, &distance_contributions, &entity_bandwidth_case_weight, &entity_bandwidth_case_count](auto /*unused index*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

		double entity_weight = 1.0;
		distanceTransform->getEntityWeightFunction(entity, entity_weight);
		distance_contributions[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);

		size_t num_kept = distanceTransform->TransformDistances(neighbors, false, false);
		for(size_t i = 0; i < num_kept; i++)
		{
			double neighbor_weight = 1.0;
			distanceTransform->getEntityWeightFunction(neighbors[i].reference, neighbor_weight);
			entity_bandwidth_case_weight[entity] += neighbor_weight;
		}
		entity_bandwidth_case_count[entity] = num_kept;

		if(num_kept > 0)
			distance_contributions[entity] = neighbors[num_kept - 1].distance;
		else
			distance_contributions[entity] = 0.0;
	}
#ifdef MULTITHREAD_SUPPORT
		, runConcurrently
#endif
	);

	//entity indices, sorted ascending by distance contribution
	std::vector<size_t> order;
	order.reserve(num_entities);
	for(auto entity : entities_to_compute)
		order.push_back(entity);
	std::stable_sort(order.begin(), order.end(),
		[&](size_t i, size_t j) { return distance_contributions[i] < distance_contributions[j]; });

	std::vector<size_t> cluster_ids_tmp(num_entity_indices, 0);
	size_t next_cluster_id = 1;
	std::vector<size_t> cluster_ids_to_merge;
	std::vector<size_t> entities_to_add_to_cluster;

	for(auto cur_entity_index : order)
	{
		double dist_contrib_threshold = 0;

		//make a copy of the neighbors and only consider actual neighbors, because TransformDistances will
		//reduce the number of neighbors in the vector, but we don't want to reduce the number of neighbors in the cache
		//this is because we want to check if the neighbors' neighbors can mutually reach each other within
		//2x distance contribution (one for each neighbor), but that requires having the cache maintain
		//a longer list of neighbors
		auto &extended_neighbors = knnCache->GetKnnCache(cur_entity_index);

		//truncate nearest neighbors -- if don't need this, then don't need to make a copy above
		//auto neighbors(extended_neighbors);
		//distanceTransform->TransformDistances(neighbors, false);
		auto &neighbors = extended_neighbors;

		size_t num_neighbors = entity_bandwidth_case_count[cur_entity_index];

		//max
		//for(size_t i = 0; i < num_neighbors; i++)
		//	dist_contrib_threshold = std::max(dist_contrib_threshold, distance_contributions[neighbors[i].reference]);

		//geomean
		//dist_contrib_threshold = 1.0;
		//for(auto &neighbor : neighbors)
		//	dist_contrib_threshold *= distance_contributions[neighbor.reference];
		//dist_contrib_threshold = std::pow(dist_contrib_threshold, 1.0 / neighbors.size());

		//average
		for(size_t i = 0; i < num_neighbors; i++)
			dist_contrib_threshold += distance_contributions[neighbors[i].reference];
		dist_contrib_threshold /= num_neighbors;

		//average to dist contrib
		//size_t counted = 0;
		//for(auto &neighbor : neighbors)
		//{
		//	if(neighbor.distance > distance_contributions[cur_entity_index])
		//		break;
		//	counted++;
		//	dist_contrib_threshold += distance_contributions[neighbor.reference];
		//}
		//dist_contrib_threshold /= counted;

		//rmse
		//for(auto &neighbor : neighbors)
		//	dist_contrib_threshold += distance_contributions[neighbor.reference] * distance_contributions[neighbor.reference];
		//dist_contrib_threshold /= neighbors.size();
		//dist_contrib_threshold = std::sqrt(dist_contrib_threshold);

		//neighbor bandwidth weight
		double neighbor_bandwidth_weight_threshold = 0.0;
		num_neighbors = entity_bandwidth_case_count[cur_entity_index];

		//average
		for(size_t i = 0; i < num_neighbors; i++)
			neighbor_bandwidth_weight_threshold += entity_bandwidth_case_weight[neighbors[i].reference];
		neighbor_bandwidth_weight_threshold /= num_neighbors;

		//max
		//for(size_t i = 0; i < num_neighbors; i++)
		//	neighbor_bandwidth_weight_threshold = std::max(neighbor_bandwidth_weight_threshold, entity_bandwidth_case_weight[neighbors[i].reference]);

		/////////////

		//ensure closest enough mutual neighbor using both average distance contribution to assess reachability
		//use 2x max neighbor dist contribution, as each point would have its own distance contribution
		dist_contrib_threshold *= 2;

		//auto dist_eval = knnCache->GetDistanceEvaluator();
		//dist_contrib_threshold *= std::sqrt(dist_eval->featureAttribs.size());


		//accumulate all clusters that are potentially overlapping or touching this one
		//that requires going through all neighbors, and looking to see if each one can reach back
		bool found_mutual_neighbor = false;
		cluster_ids_to_merge.clear();
		entities_to_add_to_cluster.clear();
		double cumulative_neighbor_weight = 0.0;
		for(auto &neighbor : extended_neighbors)
		{
			double neighbor_weight = 1.0;
			distanceTransform->getEntityWeightFunction(cur_entity_index, neighbor_weight);
			cumulative_neighbor_weight += neighbor_weight;

			double cumulative_neighbor_neighbor_weight = 0.0;
			auto &neighbors_neighbors = knnCache->GetKnnCache(neighbor.reference);
			for(auto &neighbor_neighbor : neighbors_neighbors)
			{
				double neighbor_neighbor_weight = 1.0;
				distanceTransform->getEntityWeightFunction(neighbor_neighbor.reference, neighbor_neighbor_weight);
				cumulative_neighbor_neighbor_weight += neighbor_neighbor_weight;

				//mutual neighbor
				if(neighbor_neighbor.reference == cur_entity_index)
				{
					//candidate distance is the max of either distances, and the max of either distance contribution,
					//as this makes sure sparse points do not connect; this is similar to core distance in the HDBSCAN algorithm
					double max_distance = std::max(neighbor.distance, neighbor_neighbor.distance);
					double dist_contrib_cur = distance_contributions[cur_entity_index];
					double dist_contrib_neighbor = distance_contributions[neighbor.reference];
					double max_distance_contrib = std::max(dist_contrib_neighbor, dist_contrib_cur);

					double candidate_distance = std::max(max_distance, max_distance_contrib);

					if(candidate_distance > dist_contrib_threshold)
						continue;

					found_mutual_neighbor = true;

					if(cluster_ids_tmp[neighbor.reference] != 0)
						cluster_ids_to_merge.emplace_back(cluster_ids_tmp[neighbor.reference]);
					else
						entities_to_add_to_cluster.emplace_back(neighbor.reference);

					break;
				}

				if(cumulative_neighbor_neighbor_weight >= neighbor_bandwidth_weight_threshold)
					break;
			}

			if(cumulative_neighbor_weight >= neighbor_bandwidth_weight_threshold)
				break;
		}

		//if nothing mutually reachable, leave out of clustering
		if(!found_mutual_neighbor)
			continue;

		RemoveDuplicates(cluster_ids_to_merge);

		if(cluster_ids_to_merge.size() > 0)
		{
			//use smallest cluster id and remove it from the list
			size_t cur_cluster_id = cluster_ids_to_merge.front();
			cluster_ids_to_merge.erase(begin(cluster_ids_to_merge));
			cluster_ids_tmp[cur_entity_index] = cur_cluster_id;

			for(auto updating_index : order)
			{
				auto found = std::find(begin(cluster_ids_to_merge), end(cluster_ids_to_merge), cluster_ids_tmp[updating_index]);
				if(found != end(cluster_ids_to_merge))
					cluster_ids_tmp[updating_index] = cur_cluster_id;
			}
		}
		else //new cluster
		{
			size_t cur_cluster_id = next_cluster_id++;
			cluster_ids_tmp[cur_entity_index] = cur_cluster_id;
		}

		for(auto &neighbor_entity_index : entities_to_add_to_cluster)
			cluster_ids_tmp[neighbor_entity_index] = cluster_ids_tmp[cur_entity_index];
			//cluster_ids_tmp[neighbor_entity_index] = 1;
	}

	PruneAndCompactClusterIds(entities_to_compute, cluster_ids_tmp, *distanceTransform, minimum_cluster_weight, clusters_out);
}

#endif

//TODO 24886: make algorithms more efficient
//TODO 24886: add documentation
//TODO 24886: remove HDBSCAN code

