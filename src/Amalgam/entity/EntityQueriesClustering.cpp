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

	//Dense 0..m-1 indexing over the entities being clustered, built first so the
	//core-distance pass below can fill each point's weight at the same dense index in
	//a single sweep.  The pure HDBSCAN pipeline sizes its vectors and union-find by
	//point count, so it needs a contiguous space even when the entity indices are
	//sparse (gaps from deletions, or a subset query); hashing on entity id throughout
	//the algorithm instead would be slower and allocate more.
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();
	std::vector<size_t> entity_to_dense(num_entity_indices, std::numeric_limits<size_t>::max());
	size_t m = 0;
	for(auto entity : entities_to_compute)
		entity_to_dense[entity] = m++;

	//core distance (distance to the numNearestNeighbors-th neighbor) per entity, plus
	//the per-point weight the dendrogram sums into cluster masses, filled together in
	//one pass.  buffers is thread_local under MULTITHREAD_SUPPORT, so core_distances
	//aliases this (main) thread's buffer; point_weights is an ordinary local vector.
	auto &core_distances = buffers.baseDistanceContributions;
	core_distances.clear();
	core_distances.resize(num_entity_indices, std::numeric_limits<double>::infinity());
	std::vector<double> point_weights(m, 1.0);

	//capture-default [&] so worker threads write through core_distances (the main
	//thread's buffer) rather than their own unsized thread-local buffers, and write
	//point_weights at each entity's dense index.  An explicit capture list is avoided
	//because in the single-threaded build buffers is a plain static, which would make
	//clang flag the core_distances capture as unused (-Werror,-Wunused-lambda-capture).
	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[&](auto /*unused*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

		double entity_weight = 1.0;
		distanceTransform->getEntityWeightFunction(entity, entity_weight);
		point_weights[entity_to_dense[entity]] = entity_weight;

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
