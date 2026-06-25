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

	//Cluster directly in entity-index space: entity ids index the per-point arrays and
	//are the point ids handed to HDBSCAN, so num_entity_indices (one past the largest
	//entity index) is the point count.  Entity indices not in entities_to_compute are
	//gaps: they get no edges below, so the pipeline leaves them isolated (noise) and we
	//never read them back.  This avoids a separate entity->dense remap, which the entity
	//index space being almost entirely full makes near-pointless.
	size_t num_entity_indices = knnCache->GetEndEntityIndex();
	size_t num_entities = entities_to_compute.size();

	//core distance (distance to the numNearestNeighbors-th neighbor) per entity, indexed
	//by entity id.  buffers is thread_local under MULTITHREAD_SUPPORT, so core_distances
	//aliases this (main) thread's buffer; gap entries keep infinity and are never read.
	auto &core_distances = buffers.baseDistanceContributions;
	core_distances.clear();
	core_distances.resize(num_entity_indices, std::numeric_limits<double>::infinity());

	//capture-default [&] so worker threads write through core_distances (the main
	//thread's buffer) rather than their own unsized thread-local buffers.  An explicit
	//capture list is avoided because in the single-threaded build buffers is a plain
	//static, which would make clang flag the core_distances capture as unused
	//(-Werror,-Wunused-lambda-capture).
	IterateOverConcurrentlyIfPossible(entities_to_compute,
		[&](auto /*unused*/, auto entity)
	{
		auto &neighbors = knnCache->GetKnnCache(entity);

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

	//candidate mutual-reachability edges from the KNN cache, keyed by entity id.  A
	//neighbor outside entities_to_compute is skipped so only requested entities are
	//linked (and so clustered); the membership test also keeps n.reference in range.
	std::vector<HDBSCAN::Edge> edges;
	edges.reserve(num_entities * numNearestNeighbors);
	for(auto entity : entities_to_compute)
	{
		for(auto &n : knnCache->GetKnnCache(entity))
		{
			if(n.reference == entity)
				continue;
			if(n.reference >= num_entity_indices || !entities_to_compute.contains(n.reference))
				continue;	//neighbor not part of this clustering request
			double w = std::max(std::max(core_distances[entity], core_distances[n.reference]), n.distance);
			edges.push_back(HDBSCAN::Edge{entity, n.reference, w});
		}
	}

	//look each point's weight up on demand (entity id == point id), so no weights vector
	//is materialized; the lambda is a template arg to Cluster, so it inlines.
	std::vector<size_t> labels = HDBSCAN::Cluster(num_entity_indices, std::move(edges),
		[&](size_t entity)
		{
			double entity_weight = 1.0;
			distanceTransform->getEntityWeightFunction(entity, entity_weight);
			return entity_weight;
		},
		minimum_cluster_weight);

	clusters_out.clear();
	clusters_out.reserve(num_entities);
	for(auto entity : entities_to_compute)
		clusters_out.emplace_back(static_cast<double>(labels[entity]));
}
