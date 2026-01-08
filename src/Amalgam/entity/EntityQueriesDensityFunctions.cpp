//project headers:
#include "EntityQueriesDensityFunctions.h"

#ifdef HDBSCAN
void EntityQueriesDensityProcessor::BuildMutualReachabilityMST(std::vector<double> &core_distances, std::vector<size_t> &order,
	std::vector<double> &edge_distances, std::vector<size_t> &parent_entities)
{
	size_t num_entity_ids = core_distances.size();
	size_t num_entities = order.size();
	edge_distances.clear();
	edge_distances.resize(num_entity_ids, std::numeric_limits<double>::infinity());
	parent_entities.clear();
	parent_entities.resize(num_entity_ids, std::numeric_limits<size_t>::max());

	//used to mark vertices (entities) as they are added to the tree
	std::vector<bool> processed_flags;
	processed_flags.resize(num_entity_ids, false);

	//initialize the first point, largest core distance, as the root
	size_t root = order[0];
	processed_flags[root] = true;
	//root points to itself
	parent_entities[root] = root;
	//no edge weight
	edge_distances[root] = 0.0;

	for(size_t order_index = 1; order_index < num_entities; ++order_index)
	{
		size_t cur_entity_index = order[order_index];

		size_t best_parent = std::numeric_limits<size_t>::max();
		double best_dist = std::numeric_limits<double>::max();

		auto &neighbors = knnCache->GetKnnCache(cur_entity_index);
		for(auto &nb : neighbors)
		{
			size_t neighbor_entity_index = nb.reference;
			//ignore neighbors that have not yet been processed
			if(!processed_flags[neighbor_entity_index])
				continue;

			double mutual_reachability_distance = std::max({ core_distances[cur_entity_index],
									core_distances[neighbor_entity_index], nb.distance });

			if(mutual_reachability_distance < best_dist)
			{
				best_dist = mutual_reachability_distance;
				best_parent = neighbor_entity_index;
			}
		}

		//it is possible but rare that none of the neighbours have not been processed yet,
		// e.g., the graph is disconnected.  if so, fall back to a
		// direct connection to the root using only core distances
		if(best_parent == std::numeric_limits<size_t>::max())
		{
			//TODO 24886: need a better way to connect disconnected cliques, maybe get nearest neighbor among spanning tree?
			best_dist = std::max(core_distances[cur_entity_index], core_distances[root]);
			best_parent = root;
		}

		//record the processed entity
		parent_entities[cur_entity_index] = best_parent;
		edge_distances[cur_entity_index] = best_dist;
		processed_flags[cur_entity_index] = true;
	}
}

void EntityQueriesDensityProcessor::ExtractClustersFromMST(EntityReferenceSet &entities_to_compute,
	std::vector<double> &core_distances, std::vector<double> &edge_distances,
	std::vector<size_t> &parent_entities, std::vector<size_t> &order, double minimum_cluster_weight,
	std::vector<size_t> &cluster_ids, std::vector<double> &stabilities)
{
	size_t num_entity_ids = edge_distances.size();

	//density is 1 / mutual reachability distance
	std::vector<double> densities(num_entity_ids, 0.0);
	for(auto entity_index : entities_to_compute)
	{
		if(edge_distances[entity_index] > 0.0)
			densities[entity_index] = 1.0 / edge_distances[entity_index];
	}
	//root has a 0 edge distance, so compute its density separately
	size_t root_index = order.front();
	densities[root_index] = 1.0 / core_distances[root_index];

	//bottom-up pass to construct the total entity weights of the potential clusters
	std::vector<double> subtree_cumulative_weights(num_entity_ids, 0.0);

	//accumulate the total distances up the MST
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;
		size_t parent_index = parent_entities[entity_index];

		double w = 1.0;
		distanceTransform->getEntityWeightFunction(entity_index, w);

		subtree_cumulative_weights[entity_index] += w;

		//if root, doesn't have a different parent, so don't accumulate
		if(parent_index != entity_index)
			subtree_cumulative_weights[parent_index] += subtree_cumulative_weights[entity_index];
	}

	stabilities.clear();
	stabilities.resize(num_entity_ids, 0.0);

	//accumulate stabilities using differences in densities
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;

		//TODO 24886: use a different algorithm to make faster
		//check all entities to find children of this node
		for(size_t i = 0; i < num_entity_ids; i++)
		{
			if(parent_entities[i] == entity_index)
			{
				double delta_density = densities[i] - densities[entity_index];
				if(delta_density < 0.0)
					delta_density = 0.0;

				stabilities[entity_index] += delta_density * subtree_cumulative_weights[i];
			}
		}
	}

	cluster_ids.clear();
	cluster_ids.resize(num_entity_ids, 0);

	//cluster id 0 is considered noise / not a cluster
	size_t next_cluster_id = 1;

	//minimum stability to avoid treating floating point noise as a cluster
	constexpr double stability_eps = 1e-12;

	//stack to search all descendents
	std::vector<size_t> descendent_search_stack;

	//walk the tree from leaves to root (reverse order)
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;

		//skip if has already been assigned
		if(cluster_ids[entity_index] != 0)
			continue;

		//decide whether entity_index is eligible to become a cluster
		if(stabilities[entity_index] < stability_eps)
			continue;

		//skip if not enough weight
		if(subtree_cumulative_weights[entity_index] < minimum_cluster_weight)
			continue;

		//ensure no ancestor is already a cluster
		bool ancestor_clustered = false;
		size_t ancestor_index = parent_entities[entity_index];
		//walk up until hit the root
		while(ancestor_index != entity_index)
		{
			if(cluster_ids[ancestor_index] != 0)
			{
				ancestor_clustered = true;
				break;
			}

			//stop if hit root
			if(ancestor_index == parent_entities[ancestor_index])
				break;
			ancestor_index = parent_entities[ancestor_index];
		}
		if(ancestor_clustered)
			continue;

		//mark this entity as a new cluster with an id
		cluster_ids[entity_index] = next_cluster_id;

		//depth‑first walk to label all descendants that are still unassigned
		descendent_search_stack.clear();
		descendent_search_stack.emplace_back(entity_index);
		while(!descendent_search_stack.empty())
		{
			size_t cur_id = descendent_search_stack.back();
			descendent_search_stack.pop_back();

			cluster_ids[cur_id] = next_cluster_id;

			//push child entities that are not yet labelled
			for(size_t i = 0; i < num_entity_ids; i++)
			{
				if(parent_entities[i] == cur_id && cluster_ids[i] == 0 && i != cur_id)
					descendent_search_stack.push_back(i);
			}
		}

		//given that the cluster was accepted, need to remove its weight from its parents
		double consumed = subtree_cumulative_weights[entity_index];
		size_t up = parent_entities[entity_index];
		while(up != entity_index)
		{
			subtree_cumulative_weights[up] -= consumed;
			if(up == parent_entities[up])
				break;
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
		core_distances[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);

		distanceTransform->TransformDistances(neighbors, false);
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
	//prime the cache
#ifdef MULTITHREAD_SUPPORT
	knnCache->PreCacheKnn(&entities_to_compute, numNearestNeighbors, true, runConcurrently);
#else
	knnCache->PreCacheKnn(&entities_to_compute, 3 * numNearestNeighbors, true);
#endif

	//find distance contributions to use as core weights
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();
	auto &distance_contributions = buffers.baseDistanceContributions;
	distance_contributions.clear();
	distance_contributions.resize(num_entity_indices, std::numeric_limits<double>::infinity());

	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[this, &distance_contributions](auto /*unused index*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

		double entity_weight = 1.0;
		distanceTransform->getEntityWeightFunction(entity, entity_weight);
		distance_contributions[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);
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
	for(auto cur_entity_index : order)
	{
		double max_neighbor_dist_contrib = 0;

		//make a copy of the neighbors and only consider actual neighbors, because TransformDistances will
		//reduce the number of neighbors in the vector, but we don't want to reduce the number of neighbors in the cache
		//this is because we want to check if the neighbors' neighbors can mutually reach each other within
		//2x distance contribution (one for each neighbor), but that requires having the cache maintain
		//a longer list of neighbors
		auto neighbors(knnCache->GetKnnCache(cur_entity_index));
		distanceTransform->TransformDistances(neighbors, false);
		for(auto &neighbor : neighbors)
			max_neighbor_dist_contrib = std::max(max_neighbor_dist_contrib, distance_contributions[neighbor.reference]);

		//accumulate all clusters that are potentially overlapping or touching this one
		//that requires going through all neighbors, and looking to see if each one can reach back
		bool found_mutual_neighbor = false;
		cluster_ids_to_merge.clear();
		for(auto &neighbor : neighbors)
		{
			auto &neighbors_neighbors = knnCache->GetKnnCache(neighbor.reference);
			for(auto &neighbor_neighbor : neighbors_neighbors)
			{
				//mutual neighbor
				if(neighbor_neighbor.reference == cur_entity_index)
				{
					//ensure closest enough mutual neighbor using both average distance contribution to assess reachability
					//use 2x max neighbor dist contribution, as each point would have its own distance contribution
					double dist_contrib_threshold = 2 * max_neighbor_dist_contrib;
					if(neighbor.distance > dist_contrib_threshold || neighbor_neighbor.distance > dist_contrib_threshold)
						continue;

					found_mutual_neighbor = true;

					if(cluster_ids_tmp[neighbor.reference] != 0)
						cluster_ids_to_merge.emplace_back(cluster_ids_tmp[neighbor.reference]);

					break;
				}
			}
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
	}

	PruneAndCompactClusterIds(entities_to_compute, cluster_ids_tmp, *distanceTransform, minimum_cluster_weight, clusters_out);
}

#endif

//TODO 24886: make algorithms more efficient
//TODO 24886: add documentation
//TODO 24886: add tests to full_test.amlg
//TODO 24886: remove HDBSCAN code

