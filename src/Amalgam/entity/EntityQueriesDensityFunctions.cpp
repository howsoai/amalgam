//project headers:
#include "EntityQueriesDensityFunctions.h"
#include "HDBSCANClustering.h"

void EntityQueriesDensityProcessor::ComputeCaseClusters(EntityReferenceSet &entities_to_compute,
		std::vector<double> &clusters_out, double minimum_cluster_weight)
{
	//prime the cache
#ifdef MULTITHREAD_SUPPORT
	knnCache->PreCacheKnn(&entities_to_compute, numNearestNeighbors, true, runConcurrently);
#else
	knnCache->PreCacheKnn(&entities_to_compute, numNearestNeighbors, true);
#endif

	//find distances and core distances
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();
	auto &core_distances = buffers.baseDistanceContributions;
	core_distances.clear();
	core_distances.resize(num_entity_indices, std::numeric_limits<double>::infinity());

	//capture by reference (capture-default) so the lambda writes through
	//core_distances, which aliases this (main) thread's buffers.baseDistanceContributions
	//sized above.  buffers is thread_local under MULTITHREAD_SUPPORT, so worker threads
	//must write through this reference rather than to their own (unsized) thread-local
	//buffers.  A capture-default is used rather than an explicit [this, &core_distances]
	//because in the single-threaded build buffers is a plain static, which makes clang
	//flag the explicit core_distances capture as unused (-Werror,-Wunused-lambda-capture).
	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[&](auto /*unused*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

		double entity_weight = 1.0;
		distanceTransform->getEntityWeightFunction(entity, entity_weight);

		//experimental algorithm, leave out for now
		//core_distances[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);
		size_t num_neighbors_by_bandwidth = distanceTransform->TransformDistances(neighbors, false, false);
		if(num_neighbors_by_bandwidth > 0)
			core_distances[entity] = neighbors[num_neighbors_by_bandwidth - 1].distance;
		else //treat as infinite distance if no neighbors (not included in query)
			core_distances[entity] = std::numeric_limits<double>::infinity();
	}
#ifdef MULTITHREAD_SUPPORT
		, runConcurrently
#endif
	);

	//dense 0..m-1 indexing over the entities we are clustering
	std::vector<size_t> entity_to_dense(num_entity_indices, std::numeric_limits<size_t>::max());
	std::vector<size_t> dense_to_entity;
	dense_to_entity.reserve(num_entities);
	for(auto entity : entities_to_compute)
	{
		entity_to_dense[entity] = dense_to_entity.size();
		dense_to_entity.push_back(entity);
	}
	size_t m = dense_to_entity.size();

	//per-point weights (summed mass uses these)
	std::vector<double> point_weights(m, 1.0);
	for(size_t d = 0; d < m; ++d)
	{
		double w = 1.0;
		distanceTransform->getEntityWeightFunction(dense_to_entity[d], w);
		point_weights[d] = w;
	}

	//candidate mutual-reachability edges from the KNN cache
	std::vector<HDBSCAN::Edge> edges;
	edges.reserve(m * numNearestNeighbors);
	for(auto entity : entities_to_compute)
	{
		size_t di = entity_to_dense[entity];
		for(auto &n : knnCache->GetKnnCache(entity))
		{
			if(n.reference == entity)
				continue;
			if(n.reference >= entity_to_dense.size())
				continue;
			size_t dj = entity_to_dense[n.reference];
			if(dj == std::numeric_limits<size_t>::max())
				continue;	//neighbor not part of this clustering request
			double w = std::max(std::max(core_distances[entity], core_distances[n.reference]), n.distance);
			edges.push_back(HDBSCAN::Edge{di, dj, w});
		}
	}

	std::vector<size_t> labels = HDBSCAN::Cluster(m, std::move(edges), point_weights, minimum_cluster_weight);

	clusters_out.clear();
	clusters_out.reserve(num_entities);
	for(auto entity : entities_to_compute)
		clusters_out.emplace_back(static_cast<double>(labels[entity_to_dense[entity]]));
}
