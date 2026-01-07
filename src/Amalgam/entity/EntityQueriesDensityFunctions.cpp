#pragma once

//project headers:
#include "EntityQueriesDensityFunctions.h"

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
			best_dist = std::max(core_distances[cur_entity_index], core_distances[root]);
			best_parent = root;
		}

		//record the procssed entity
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

		//don't reaccumulate to the root
		if(parent_index == entity_index)
			continue;

		double w = 1.0;
		distanceTransform->getEntityWeightFunction(entity_index, w);

		subtree_cumulative_weights[entity_index] += w;
		subtree_cumulative_weights[parent_index] += subtree_cumulative_weights[entity_index];
	}

	stabilities.clear();
	stabilities.resize(num_entity_ids, 0.0);

	//accumulate stabilities using differences in densities
	for(auto it = order.rbegin(); it != order.rend(); ++it)
	{
		size_t entity_index = *it;
		size_t parent_index = parent_entities[entity_index];

		//don't reaccumulate to the root
		if(parent_index == entity_index)
			continue;

		double delta_density = densities[entity_index] - densities[parent_index];
		if(delta_density < 0.0)
			delta_density = 0.0;

		stabilities[parent_index] += delta_density * subtree_cumulative_weights[entity_index];
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
		size_t ancestor_id = parent_entities[entity_index];
		//walk up until hit the root
		while(ancestor_id != entity_index)
		{
			if(cluster_ids[ancestor_id] != 0)
			{
				ancestor_clustered = true;
				break;
			}

			//stop if hit root
			if(ancestor_id == parent_entities[ancestor_id])
				break;
			ancestor_id = parent_entities[ancestor_id];
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

		next_cluster_id++;
	}
}

//TODO 24886: add documentation
//TODO 24886: add tests to full_test.amlg
