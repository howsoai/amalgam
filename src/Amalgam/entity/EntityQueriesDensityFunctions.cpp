//project headers:
#include "EntityQueriesDensityFunctions.h"

#define HDBSCAN

#ifdef HDBSCAN

struct DisjointSetUnion
{
	DisjointSetUnion(size_t n)
		: p(n), r(n, 0)
	{
		for(size_t i = 0; i < n; ++i)
			p[i] = i;
	}

	size_t Find(size_t x)
	{
		size_t root = x;
		while(p[root] != root)
			root = p[root];

		//path compression
		while(p[x] != x)
		{
			size_t parent = p[x];
			p[x] = root;
			x = parent;
		}
		return root;
	}

	bool Unite(size_t a, size_t b)
	{
		a = Find(a);
		b = Find(b);

		if(a == b)
			return false;

		if(r[a] < r[b])
			std::swap(a, b);

		p[b] = a;

		if(r[a] == r[b])
			r[a]++;

		return true;
	}

	std::vector<size_t> p, r;
};

struct Edge
{
	size_t from;
	size_t to;
	double weight;
};

//Build the minimum spanning tree on the mutual‑reachability graph using Kruskal’s algorithm.
//Edge weight = max(core_dist[i], core_dist[j])  (the mutual‑reachability distance)
//nearest_neighbors_cache supplies the candidate edges; we keep only the smallest
// ones that connect previously‑disconnected components.
//core_distances contains the core distance corresponding to each corresponding element in nearest_neighbors_cache
//order stores the indices for both nearest_neighbors_cache and core_distances sorted by decreasing core-distance
//For each vertex we also store its parent in the final tree so that the
// condensed‑tree step can compare a node to its true MST neighbor.
//
//The function fills parent_entities such that for every i (except the
// global root), parent_entities[i] = the vertex that i was attached to
// when its edge was added to the MST.  The root points to itself.
void BuildMutualReachabilityMST(size_t num_nearest_neighbors,
	std::vector<std::vector<DistanceReferencePair<size_t>>> &nearest_neighbors_cache,
	std::vector<double> &core_distances, std::vector<size_t> &parent_entities)
{
	const size_t n = nearest_neighbors_cache.size();
	DisjointSetUnion dsu(n);
	parent_entities.resize(n);
	for(size_t i = 0; i < n; ++i)
		parent_entities[i] = i;

	//Build all candidate edges with their mutual reachability distances
	std::vector<Edge> edges;
	edges.reserve(n * num_nearest_neighbors);

	for(size_t i = 0; i < n; ++i)
	{
		for(const auto &candidate : nearest_neighbors_cache[i])
		{
			size_t j = candidate.reference;
			if(j >= n || i >= j) //avoid duplicates
				continue;

			double mutual_reachability = std::max({
				candidate.distance, //actual distance between i and j
				core_distances[i],	//core distance of i
				core_distances[j]	//core distance of j
			});

			edges.push_back({i, j, mutual_reachability});
		}
	}

	//Sort edges by mutual reachability distance (ascending for Kruskal's)
	std::sort(edges.begin(), edges.end(), [](const Edge &a, const Edge &b) { return a.weight < b.weight; });

	//Kruskal's algorithm
	size_t components = n;
	for(const auto &edge : edges)
	{
		if(components <= 1)
			break;

		size_t ri = dsu.Find(edge.from);
		size_t rj = dsu.Find(edge.to);

		if(ri != rj)
		{
			//Record parent relationship before union
			size_t parent = dsu.Unite(ri, rj) ? dsu.Find(ri) : ri;

			//The non-root component's representative gets attached
			size_t child = (parent == ri) ? rj : ri;
			parent_entities[child] = parent;

			components--;
		}
	}
}

template<typename T> bool VectorsEqual(const std::vector<T> &a, const std::vector<T> &b, const std::string &name)
{
	if(a.size() != b.size())
	{
		std::cout << name << " size mismatch: " << a.size() << " vs " << b.size() << '\n';
		return false;
	}
	for(size_t i = 0; i < a.size(); ++i)
	{
		if(a[i] != b[i])
		{
			std::cout << name << " differs at index " << i << " (got " << a[i] << ", expected " << b[i] << ")\n";
			return false;
		}
	}
	return true;
}

//Robust verification that parent array represents a valid Kruskal forest
bool VerifyMSTRobust(const std::vector<size_t> &parent,
	const std::vector<std::vector<DistanceReferencePair<size_t>>> &nbrs, const std::vector<double> &core)
{
	const size_t n = parent.size();

	//1) Each vertex has a valid parent in [0,n) and roots point to themselves
	for(size_t v = 0; v < n; ++v)
	{
		if(parent[v] >= n)
		{
			std::cout << "Invalid parent for vertex " << v << ": " << parent[v] << " (out of range)\n";
			return false;
		}
	}

	//2) No cycles: for every vertex, follow parent pointers until a root is reached.
	for(size_t s = 0; s < n; ++s)
	{
		std::vector<bool> seen(n, false);
		size_t cur = s;
		while(parent[cur] != cur)
		{
			if(seen[cur])
			{
				std::cout << "Cycle detected starting at vertex " << s << '\n';
				return false;
			}
			seen[cur] = true;
			cur = parent[cur];
		}
	}

	//3) For every non-root edge (i -> parent[i]), verify candidate edge exists with correct weight
	for(size_t i = 0; i < n; ++i)
	{
		if(parent[i] == i)
			continue;
		size_t p = parent[i];
		double expected = std::max(core[i], core[p]);
		bool found = false;
		for(const auto &cand : nbrs[i])
		{
			if(cand.reference == p && std::abs(cand.distance - expected) < 1e-9)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			std::cout << "Edge (" << i << " -> " << p << ") not found in candidate list with weight " << expected
						<< '\n';
			return false;
		}
	}

	//4) Optional: check the number of components matches the forest
	size_t components = 0;
	for(size_t i = 0; i < n; ++i)
		if(parent[i] == i)
			++components;
	//Forest has exactly n - edges edges; here edges = n - components
	size_t edges = 0;
	for(size_t i = 0; i < n; ++i)
		if(parent[i] != i)
			++edges;
	if(n - components != edges)
	{
		std::cout << "Edge count mismatch (forest property violated): n=" << n << " components=" << components
					<< " edges=" << edges << '\n';
		return false;
	}

	return true;
}

bool TestBuildMutualReachabilityMST_Robust_Test1()
{
	//Tiny connected graph (3 vertices, k = 2)
	const size_t V = 3;
	const size_t K = 2;
	std::vector<double> core = {0.5, 1.0, 2.0};

	std::vector<std::vector<DistanceReferencePair<size_t>>> nbrs(V);
	nbrs[0] = {{1.0, 1}, {2.0, 2}};
	nbrs[1] = {{1.0, 0}, {2.0, 2}};
	nbrs[2] = {{2.0, 0}, {2.0, 1}};

	std::vector<size_t> parent(V, V);
	BuildMutualReachabilityMST(K, nbrs, core, parent);

	//Check invariants
	if(!VerifyMSTRobust(parent, nbrs, core))
	{
		std::cout << "Test1 robustness failed\n";
		return false;
	}

	//Additionally allow any valid forest with one root
	//Ensure exactly one root
	size_t roots = 0;
	for(size_t i = 0; i < V; ++i)
		if(parent[i] == i)
			++roots;
	if(roots != 1)
	{
		std::cout << "Test1: expected exactly 1 root, got " << roots << '\n';
		return false;
	}

	return true;
}

bool TestBuildMutualReachabilityMST_Robust_Test2()
{
	//Test 2 - disconnected components (2 components of size 2)
	const size_t V = 4;
	const size_t K = 1;
	std::vector<double> core = {0.2, 0.4, 1.0, 1.5};

	std::vector<std::vector<DistanceReferencePair<size_t>>> nbrs(V);
	nbrs[0] = {{0.4, 1}};
	nbrs[1] = {{0.4, 0}};
	nbrs[2] = {{1.5, 3}};
	nbrs[3] = {{1.5, 2}};

	std::vector<size_t> parent(V, V);
	BuildMutualReachabilityMST(K, nbrs, core, parent);

	if(!VerifyMSTRobust(parent, nbrs, core))
	{
		std::cout << "Test2 robustness failed\n";
		return false;
	}

	//Ensure forest has two components (two roots)
	size_t roots = 0;
	for(size_t i = 0; i < V; ++i)
		if(parent[i] == i)
			++roots;
	if(roots != 2)
	{
		std::cout << "Test2: expected 2 roots, got " << roots << '\n';
		return false;
	}

	return true;
}

//Builds the condensed tree from the MST that was created with Kruskal’s algorithm
//The condensed tree contains only the edges that change the core‑distance level,
// edges whose two endpoints have the same core distance are omitted because they
// do not create a new density level.
//For every point *i*, `parent_entities[i]` must be the index of its
// parent in the mutual‑reachability MST (the root points to itself).
//core_distances correspond to each point
//condensed_nodes will contain the the indices of the points that represent a
// distinct density level (i.e. nodes whose core distance differs from their
// parent's core distance by more than a tiny epsilon)
static void GenerateCondensedTree(const std::vector<size_t> &parent_entities,
	const std::vector<double> &core_distances, std::vector<size_t> &condensed_nodes)
{
	constexpr double epsilon = 1e-10;
	condensed_nodes.clear();

	if(parent_entities.empty())
		return;

	//find all roots
	for(size_t i = 0; i < parent_entities.size(); ++i)
	{
		if(parent_entities[i] == i)
			condensed_nodes.push_back(i);
	}

	//include any node whose core distance differs from its parent's
	for(size_t i = 0; i < parent_entities.size(); ++i)
	{
		if(parent_entities[i] == i)
			continue;

		size_t parent = parent_entities[i];
		if(std::abs(core_distances[i] - core_distances[parent]) > epsilon)
			condensed_nodes.push_back(i);
	}
}

//Tests for GenerateCondensedTree

struct TestCase
{
	std::string name;
	std::vector<size_t> parents;
	std::vector<double> core_distances;
	std::vector<size_t> expected_condensed;
};

static bool run_test(const TestCase &tc)
{
	std::vector<size_t> result;
	GenerateCondensedTree(tc.parents, tc.core_distances, result);

	//Sort both for comparison (order doesn't matter for condensed nodes)
	std::vector<size_t> sorted_result = result;
	std::vector<size_t> sorted_expected = tc.expected_condensed;
	std::sort(sorted_result.begin(), sorted_result.end());
	std::sort(sorted_expected.begin(), sorted_expected.end());

	bool pass = (sorted_result == sorted_expected);

	std::cout << (pass ? "PASS" : "FAIL") << " | " << tc.name << std::endl;
	if(!pass)
	{
		std::cout << "  Expected: {";
		for(size_t i = 0; i < tc.expected_condensed.size(); ++i)
		{
			if(i > 0)
				std::cout << ", ";
			std::cout << tc.expected_condensed[i];
		}
		std::cout << "}" << std::endl;

		std::cout << "  Got:      {";
		for(size_t i = 0; i < result.size(); ++i)
		{
			if(i > 0)
				std::cout << ", ";
			std::cout << result[i];
		}
		std::cout << "}" << std::endl;
	}

	return pass;
}

bool run_tests()
{
	std::vector<TestCase> tests;
	int passed = 0;
	int failed = 0;

	//Test 1: Single point (trivial case)
	tests.push_back({
		"Single point - root only", {0}, //parent_entities: point 0 is its own parent (root)
		{1.0},							 //core_distances
		{0}								 //expected: only root is a condensed node
	});

	//Test 2: Two points, different core distances
	tests.push_back({
		"Two points - different core distances", {0, 0}, //point 0 is root, point 1's parent is 0
		{1.0, 2.0},										 //different core distances
		{0, 1}											 //both should be condensed nodes
	});

	//Test 3: Two points, same core distance
	tests.push_back({
		"Two points - same core distance", {0, 0}, //point 0 is root, point 1's parent is 0
		{1.0, 1.0},								   //identical core distances
		{0}										   //only root should be condensed (child is redundant)
	});

	//Test 4: Chain of 4 points, all different core distances
	tests.push_back({
		"Chain - all different core distances", {0, 0, 1, 2}, //0 is root, 1->0, 2->1, 3->2
		{1.0, 2.0, 3.0, 4.0}, {0, 1, 2, 3}					  //all should be condensed
	});

	//Test 5: Chain of 4 points, some redundant
	tests.push_back({
		"Chain - some redundant core distances", {0, 0, 1, 2}, //0 is root, 1->0, 2->1, 3->2
		{1.0, 1.0, 2.0, 2.0},								   //0 and 1 same, 2 and 3 same
		{0, 2}												   //only root and first node of each level
	});

	//Test 6: Tree with branching
	tests.push_back({
		"Tree with branching - all different", {0, 0, 0, 1, 2}, //0 is root, 1->0, 2->0, 3->1, 4->2
		{1.0, 2.0, 3.0, 4.0, 5.0}, {0, 1, 2, 3, 4}				//all should be condensed
	});

	//Test 7: Tree with branching, some redundant
	tests.push_back({
		"Tree with branching - some redundant", {0, 0, 0, 1, 2}, //0 is root, 1->0, 2->0, 3->1, 4->2
		{1.0, 1.0, 1.0, 2.0, 2.0},								 //root and children same, grandchildren same
		{0, 3, 4}												 //only root and grandchildren (first of each level)
	});

	//Test 8: Empty input
	tests.push_back({
		"Empty input", {}, {}, {} //no condensed nodes
	});

	//Test 9: Root is not first element
	tests.push_back({
		"Root is not first element", {1, 1, 1}, //point 1 is root (points to itself), 0->1, 2->1
		{2.0, 1.0, 2.0},						//root at index 1
		{1, 0, 2}								//root and both children have different core distances
	});

	//Test 10: Very close but not equal (within epsilon)
	tests.push_back({
		"Very close core distances - within epsilon", {0, 0, 1},
		{1.0, 1.0 + 1e-12, 2.0}, //1e-12 < 1e-10 epsilon? No, 1e-12 < 1e-10, so they're "equal"
		{0, 2}					 //child 1 is redundant, child 2 is not
	});

	//Test 11: Very close but outside epsilon
	tests.push_back({
		"Very close core distances - outside epsilon", {0, 0, 1},
		{1.0, 1.0 + 1e-8, 2.0}, //1e-8 > 1e-10 epsilon, so they're different
		{0, 1, 2}				//all should be condensed
	});

	//Test 12: Deep chain with alternating redundancy
	tests.push_back({
		"Deep chain with alternating redundancy", {0, 0, 1, 2, 3, 4}, //0 root, 1->0, 2->1, 3->2, 4->3, 5->4
		{1.0, 1.0, 2.0, 2.0, 3.0, 3.0}, {0, 2, 4}					  //only first node of each density level
	});

	//Test 13: All same core distance (fully redundant tree)
	tests.push_back({
		"All same core distance - fully redundant", {0, 0, 0, 0, 0}, //star topology, root is 0
		{5.0, 5.0, 5.0, 5.0, 5.0}, {0}								 //only root should be condensed
	});

	//Test 14: Single child chain, all different
	tests.push_back(
		{"Single child chain - all different", {0, 0, 1, 2, 3}, {0.5, 1.0, 1.5, 2.0, 2.5}, {0, 1, 2, 3, 4}});

	//Test 15: Two separate trees (should not happen in MST, but test robustness)
	tests.push_back({
		"Two separate trees - robustness check", {0, 0, 2, 2}, //tree 1: 0 root, 1->0; tree 2: 2 root, 3->2
		{1.0, 2.0, 3.0, 4.0}, {0, 1, 2, 3}					   //all should be condensed (no redundant pairs)
	});

	std::cout << "=== HDBSCAN GenerateCondensedTree Unit Tests ===" << std::endl;
	std::cout << "================================================" << std::endl;
	std::cout << std::endl;

	for(const auto &test : tests)
	{
		if(run_test(test))
			passed++;
		else
			failed++;
	}

	std::cout << std::endl;
	std::cout << "================================================" << std::endl;
	std::cout << "Results: " << passed << " passed, " << failed << " failed, " << (passed + failed) << " total"
			  << std::endl;

	return (failed == 0);
}

//Extract the most stable clusters from the condensed‐tree produced by GenerateCondensedTree
//condensed_nodes:
//    Indices of the points that constitute the condensed tree (output
//    of GenerateCondensedTree).  The corresponding parent relationship
//    is stored implicitly in `parent_entities` – the caller must have
//    built that tree correctly.
//
//core_distances:
//    Core distance for every point (size == number of entities).
//
//parent_entities  : vector<size_t>
//    MST parent for each point (same vector that was passed to
//    GenerateCondensedTree).  Required to walk the hierarchy.
//
//point_weights:
//    Optional per‑point weight (size == number of entities).  If empty
//    the function assumes a weight of 1.0 for each point.
//
//minimum_cluster_weight:
//    Minimum total weight a segment must have to be considered a
//    cluster.
//
//cluster_ids:
//    Output, cluster label for every point (0 = noise).
//
//stabilities:
//    Output, stability value for each cluster (useful for diagnostics;
//    not used by the caller in the current code path but kept for
//    compatibility).
static void ExtractStableClusters(const std::vector<size_t> &condensed_nodes, const std::vector<double> &core_distances,
	const std::vector<size_t> &parent_entities, const std::vector<double> &point_weights, double minimum_cluster_weight,
	std::vector<size_t> &cluster_ids, std::vector<double> &stabilities)
{
	const double EPS_CORE = 1e-9;	//tolerance for core‑distance equality
	const double SEGMENT_EPS = 4; //groups “nearby” core distances

	const size_t N = core_distances.size();

	cluster_ids.assign(N, 0);
	stabilities.clear();

	if(condensed_nodes.empty())
		return; //nothing to cluster – everything stays noise

	//1.  Sort condensed nodes by increasing core distance – this is the
	//    order in which HDBSCAN sweeps density levels.
	std::vector<size_t> sorted = condensed_nodes;
	std::stable_sort(
		sorted.begin(), sorted.end(), [&](size_t a, size_t b) { return core_distances[a] < core_distances[b]; });

	//2.  Build segments (connected components) where successive nodes
	//    have core distances within SEGMENT_EPS.
	struct SegmentInfo
	{
		std::vector<size_t> members; //point indices belonging to this segment
		double weight = 0.0;
		double lambda_start = 0.0;	  //core distance where segment appears
		double lambda_end = 0.0;	  //core distance where it merges upward
		size_t parent_seg = SIZE_MAX; //index of parent segment (if any)
		size_t cluster_id = 0;		  //final id (0 = noise)
		double stability = 0.0;
	};

	std::vector<SegmentInfo> segments;
	std::unordered_map<size_t, size_t> node_to_seg; //point → segment index

	size_t cur_seg = SIZE_MAX;
	for(size_t idx = 0; idx < sorted.size(); ++idx)
	{
		size_t node = sorted[idx];
		double lambda = core_distances[node];

		//start a new segment if this is the first node or the distance jumps
		if(cur_seg == SIZE_MAX || std::abs(lambda - segments[cur_seg].lambda_start) > SEGMENT_EPS)
		{
			SegmentInfo seg;
			seg.lambda_start = lambda;
			seg.members.push_back(node);
			seg.weight = point_weights.empty() ? 1.0 : point_weights[node];
			cur_seg = segments.size();
			segments.push_back(std::move(seg));
		}
		else
		{
			//extend current segment
			segments[cur_seg].members.push_back(node);
			segments[cur_seg].weight += point_weights.empty() ? 1.0 : point_weights[node];
		}
		node_to_seg[node] = cur_seg;
	}

	//3.  Determine the parent segment for each segment by climbing the
	//    MST until we hit a node that belongs to a *different* segment.
	for(size_t s = 0; s < segments.size(); ++s)
	{
		//all members share the same lambda_start; pick the first member.
		size_t exemplar = segments[s].members.front();
		size_t parent = parent_entities[exemplar];

		while(parent != exemplar) //walk upward in the MST
		{
			auto it = node_to_seg.find(parent);
			if(it != node_to_seg.end() && it->second != s) //found a different segment
			{
				segments[s].parent_seg = it->second;
				break;
			}
			//otherwise keep climbing
			if(parent == parent_entities[parent]) //reached ultimate root
				break;
			exemplar = parent;
			parent = parent_entities[parent];
		}

		//lambda_end is the core distance of the parent segment (or INF for root)
		if(segments[s].parent_seg != SIZE_MAX)
			segments[s].lambda_end = core_distances[segments[segments[s].parent_seg].members.front()];
		else
			segments[s].lambda_end = std::numeric_limits<double>::infinity();
	}

	//4.  Compute stability for every segment.
	//   stability = Σ (λ_parent – λ_child) * weight_child
	for(auto &seg : segments)
	{
		double delta = seg.lambda_end - seg.lambda_start;
		if(delta < 0)
			delta = 0; //safety (should not happen)
		seg.stability = delta * seg.weight;
	}

	//5.  Choose which segments become genuine clusters.
	//   A segment is kept if its weight ≥ minimum_cluster_weight.
	//   Cluster ids are assigned in a bottom‑up pass so that a child
	//   inherits the id of the deepest qualified ancestor.
	size_t next_cluster_id = 1; //0 is reserved for noise
	for(size_t s = 0; s < segments.size(); ++s)
	{
		//If this segment itself meets the weight requirement, it becomes a cluster.
		if(segments[s].weight >= minimum_cluster_weight)
		{
			segments[s].cluster_id = next_cluster_id++;
		}
		else
		{
			//Otherwise inherit the id of the nearest ancestor that is a cluster.
			size_t anc = segments[s].parent_seg;
			while(anc != SIZE_MAX && segments[anc].cluster_id == 0)
				anc = segments[anc].parent_seg;
			segments[s].cluster_id = (anc == SIZE_MAX) ? 0 : segments[anc].cluster_id;
		}
	}

	//6.  Populate the final per‑point label vector.
	for(size_t s = 0; s < segments.size(); ++s)
	{
		for(size_t pt : segments[s].members)
			cluster_ids[pt] = segments[s].cluster_id;
	}

	//Optional: expose stability per cluster (useful for debugging)
	//Build a map from cluster_id → total stability.
	std::unordered_map<size_t, double> cluster_stab;
	for(const auto &seg : segments)
	{
		if(seg.cluster_id != 0)
			cluster_stab[seg.cluster_id] += seg.stability;
	}

	//Convert map to the output vector ordered by cluster id (1‑based).
	if(!cluster_stab.empty())
	{
		stabilities.resize(next_cluster_id - 1, 0.0); //index 0 unused (noise)
		for(const auto &kv : cluster_stab)
			stabilities[kv.first - 1] = kv.second;
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

	//find distances and core distances
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

		//experimental algorithm, leave out for now
		//core_distances[entity] = distanceTransform->ComputeDistanceContribution(neighbors, entity_weight);
		size_t num_neighbors_by_bandwidth = distanceTransform->TransformDistances(neighbors, false);

		//TODO 24886: remove this once the clustering algorithm is ready and known to be able to
		// handle slight discrepancies in sorting due to numeric precision during distance recomputations
		std::stable_sort(neighbors.begin(), neighbors.end());

		core_distances[entity] = neighbors[num_neighbors_by_bandwidth - 1].distance;
	}
#ifdef MULTITHREAD_SUPPORT
		, runConcurrently
#endif
	);

	//update core distances to be the max of the entity's own core distance
	//and the core distance of its nearest neighbor (for mutual reachability)
	IterateOverConcurrentlyIfPossible(
		entities_to_compute,
		[this, &core_distances](auto /*unused index*/, auto entity)
		{
			auto &neighbors = knnCache->GetKnnCache(entity);
			double own_core = core_distances[entity];
			double max_core = own_core;

			//find the maximum core distance among neighbors
			for(auto &neighbor : neighbors)
				max_core = std::max(max_core, core_distances[neighbor.reference]);

			core_distances[entity] = max_core;
		}
	#ifdef MULTITHREAD_SUPPORT
		,
		runConcurrently
	#endif
	);

	//reuse baseDistanceProbabilities, but because clustering is not typically done repeatedly,
	//don't reuse any of the other buffers
	std::vector<size_t> parent_entities;
	BuildMutualReachabilityMST(
		numNearestNeighbors, knnCache->GetFullKnnCache(), core_distances, parent_entities);

	//a condensed tree removes redundant edges where both nodes have the same core distance (or very similar ones),
	// simplifying the hierarchy into distinct levels of density
	std::vector<size_t> condensed_nodes;
	GenerateCondensedTree(parent_entities, core_distances, condensed_nodes);

	//evaluate each cluster in the condensed tree across all possible threshold values to find the most stable clusters
	std::vector<size_t> cluster_ids_tmp;
	std::vector<double> node_stabilities;
	std::vector<double> point_weights;
	ExtractStableClusters(condensed_nodes, core_distances, parent_entities, point_weights, minimum_cluster_weight, cluster_ids_tmp, node_stabilities);

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
	}

	PruneAndCompactClusterIds(entities_to_compute, cluster_ids_tmp, *distanceTransform, minimum_cluster_weight, clusters_out);
}

#endif

//TODO 24886: make algorithms more efficient
//TODO 24886: add documentation
//TODO 24886: remove HDBSCAN code

