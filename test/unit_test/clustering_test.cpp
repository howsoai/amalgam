//Unit tests for the clustering algorithm without other dependencies
#include "clustering_test.h"
#include "EntityQueriesClustering.h"

#include <iostream>
#include <set>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond) do { \
	++g_checks; \
	if(!(cond)) { ++g_failures; \
		std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond << std::endl; } \
	} while(0)

//Wraps a weight vector as the size_t -> double accessor the templated HDBSCAN API takes.
static auto WeightsOf(const std::vector<double> &w)
{
	return [&w](size_t i) { return w[i]; };
}

//Sum of edge weights, for comparing MST totals.
static double TotalWeight(const std::vector<EntityClustering::Edge> &edges)
{
	double t = 0.0;
	for(const auto &e : edges)
		t += e.weight;
	return t;
}

static void TestBuildMST()
{
	// 4 vertices; a square with a cheap diagonal.
	//   0-1 : 1.0, 1-2 : 2.0, 2-3 : 1.0, 3-0 : 2.0, 0-2 : 0.5
	// MST should take 0-2 (0.5), 0-1 (1.0), 2-3 (1.0): total 2.5, 3 edges.
	std::vector<EntityClustering::Edge> edges = {
		{0, 1, 1.0}, {1, 2, 2.0}, {2, 3, 1.0}, {3, 0, 2.0}, {0, 2, 0.5}
	};
	auto mst = EntityClustering::BuildMST(4, edges);
	CHECK(mst.size() == 3);
	CHECK(TotalWeight(mst) == 2.5);
	// ascending weight order
	CHECK(mst[0].weight <= mst[1].weight);
	CHECK(mst[1].weight <= mst[2].weight);

	// Disconnected: {0,1} and {2,3}, no edge between -> forest of 2 edges.
	std::vector<EntityClustering::Edge> disc = { {0, 1, 1.0}, {2, 3, 1.0} };
	auto forest = EntityClustering::BuildMST(4, disc);
	CHECK(forest.size() == 2);
}

static void TestBuildSingleLinkageTree()
{
	// 3 points on a line: 0--(1.0)--1--(2.0)--2, weights all 1.0.
	std::vector<EntityClustering::Edge> mst = { {0, 1, 1.0}, {1, 2, 2.0} };
	std::vector<double> w = {1.0, 1.0, 1.0};
	auto slt = EntityClustering::BuildSingleLinkageTree(3, mst, WeightsOf(w));

	CHECK(slt.size() == 2);
	// First merge joins points 0 and 1 at weight 1.0, mass 2.0 -> node id 3.
	CHECK(slt[0].weight == 1.0);
	CHECK(slt[0].mass == 2.0);
	// Second merge joins node 3 with point 2 at weight 2.0, mass 3.0 -> node id 4.
	CHECK(slt[1].weight == 2.0);
	CHECK(slt[1].mass == 3.0);
	CHECK((slt[1].leftChildId == 3 || slt[1].rightChildId == 3));
	CHECK((slt[1].leftChildId == 2 || slt[1].rightChildId == 2));
}

//Count, in a condensed tree, how many distinct points fall out of each parent.
static size_t CountPointChildren(const std::vector<EntityClustering::CondensedEdge> &c, size_t m)
{
	std::set<size_t> pts;
	for(const auto &e : c)
		if(e.childId < m)
			pts.insert(e.childId);
	return pts.size();
}

static void TestCondenseTree()
{
	// Two tight pairs joined by a long edge:
	//   0-1 : 1.0  (tight)        2-3 : 1.0  (tight)
	//   then {0,1}-{2,3} : 10.0   (the split)
	// weights all 1.0; min_cluster_weight = 2.0
	std::vector<EntityClustering::Edge> mst = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	std::vector<double> w = {1.0, 1.0, 1.0, 1.0};
	auto slt = EntityClustering::BuildSingleLinkageTree(4, mst, WeightsOf(w));
	auto condensed = EntityClustering::CondenseTree(4, slt, WeightsOf(w), 2.0);

	// Every one of the 4 points eventually falls out / is placed.
	CHECK(CountPointChildren(condensed, 4) == 4);
	// The top split (weight 10 -> lambda 0.1) is a genuine split: two child clusters
	// are born, so there are >= 2 sub-cluster edges (child >= m).
	size_t subcluster_edges = 0;
	for(const auto &e : condensed)
		if(e.childId >= 4)
			++subcluster_edges;
	CHECK(subcluster_edges == 2);

	// With min_cluster_weight = 3.0, neither side of the top split (mass 2 each)
	// qualifies, so there is no genuine split: 0 sub-cluster edges.
	auto condensed2 = EntityClustering::CondenseTree(4, slt, WeightsOf(w), 3.0);
	size_t subcluster_edges2 = 0;
	for(const auto &e : condensed2)
		if(e.childId >= 4)
			++subcluster_edges2;
	CHECK(subcluster_edges2 == 0);
}

static void TestComputeStabilities()
{
	// Same two-tight-pairs tree as TestCondenseTree, min_cluster_weight = 2.0.
	std::vector<EntityClustering::Edge> mst = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	std::vector<double> w = {1.0, 1.0, 1.0, 1.0};
	auto slt = EntityClustering::BuildSingleLinkageTree(4, mst, WeightsOf(w));
	auto condensed = EntityClustering::CondenseTree(4, slt, WeightsOf(w), 2.0);
	auto birth = EntityClustering::ClusterBirthLambdas(4, condensed);
	auto stab = EntityClustering::ComputeStabilities(4, condensed, birth);

	// There is a root cluster plus two children. Each child cluster is born at
	// lambda 0.1 (1/10) and loses its 2 points at lambda 1.0 (1/1):
	//   S(child) = 2 * (1.0 - 0.1) = 1.8
	// Find the two non-root clusters (they have positive birth lambda).  birth and
	// stab are indexed by cluster id - m.
	double child_stability_sum = 0.0;
	size_t child_count = 0;
	for(size_t ci = 0; ci < stab.size(); ci++)
	{
		if(birth[ci] > 0.0)	//a child (non-root) cluster
		{
			child_stability_sum += stab[ci];
			child_count++;
		}
	}
	CHECK(child_count == 2);
	CHECK(child_stability_sum > 3.5 && child_stability_sum < 3.7);	//~3.6
}

static void TestSelectClusters()
{
	// Two well-separated tight pairs -> the two child clusters should be selected,
	// the root should not.
	std::vector<EntityClustering::Edge> mst = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	std::vector<double> w = {1.0, 1.0, 1.0, 1.0};
	auto slt = EntityClustering::BuildSingleLinkageTree(4, mst, WeightsOf(w));
	auto condensed = EntityClustering::CondenseTree(4, slt, WeightsOf(w), 2.0);
	auto birth = EntityClustering::ClusterBirthLambdas(4, condensed);
	auto stab = EntityClustering::ComputeStabilities(4, condensed, birth);
	auto selected = EntityClustering::SelectClusters(4, condensed, stab, birth);

	// Exactly the two child clusters are selected; the root (birth lambda 0) is not.
	// selected is a keep flag indexed by cluster id - m.
	size_t num_selected = 0;
	for(size_t ci = 0; ci < selected.size(); ci++)
	{
		if(selected[ci])
		{
			num_selected++;
			CHECK(birth[ci] > 0.0);
		}
	}
	CHECK(num_selected == 2);
}

static size_t DistinctNonzero(const std::vector<size_t> &labels)
{
	std::set<size_t> s;
	for(size_t v : labels)
		if(v != 0)
			s.insert(v);
	return s.size();
}

static void TestClusterEndToEnd()
{
	std::vector<double> w4(4, 1.0);

	// Two well-separated pairs -> exactly 2 clusters, no noise, pair-mates share a label.
	std::vector<EntityClustering::Edge> two = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	auto labels = EntityClustering::Cluster(4, two, WeightsOf(w4), 2.0);
	CHECK(labels.size() == 4);
	CHECK(DistinctNonzero(labels) == 2);
	CHECK(labels[0] == labels[1]);
	CHECK(labels[2] == labels[3]);
	CHECK(labels[0] != labels[2]);

	// One tight group of 4 with no genuine sub-split: the only cluster is the
	// whole-dataset root, which is never selected (allow_single_cluster = false,
	// matching scikit-learn), so every point is noise.
	std::vector<EntityClustering::Edge> one = { {0, 1, 1.0}, {1, 2, 1.0}, {2, 3, 1.0} };
	auto labels_one = EntityClustering::Cluster(4, one, WeightsOf(w4), 2.0);
	CHECK(DistinctNonzero(labels_one) == 0);

	// Two well-separated pairs plus one far outlier (index 4).  The two pairs are
	// genuine sibling clusters; the outlier falls out of the unselected root, so
	// it is noise while the pairs are clustered.
	std::vector<double> w5(5, 1.0);
	std::vector<EntityClustering::Edge> outlier = {
		{0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0}, {3, 4, 50.0}
	};
	auto labels_out = EntityClustering::Cluster(5, outlier, WeightsOf(w5), 2.0);
	CHECK(DistinctNonzero(labels_out) == 2);
	CHECK(labels_out[4] == 0);
	CHECK(labels_out[0] != 0);
	CHECK(labels_out[0] == labels_out[1]);
	CHECK(labels_out[2] == labels_out[3]);
	CHECK(labels_out[0] != labels_out[2]);
}

static void TestWeightThreshold()
{
	// Three sibling pairs.  Two heavy pairs (point weight 0.8 -> pair mass 1.6)
	// clear minimum_cluster_weight = 1.0; the light pair (point weight 0.4 ->
	// pair mass 0.8) falls below it and becomes noise.  A single point (0.8 or
	// 0.4) never clears the threshold alone, so the heavy pairs stay intact
	// rather than splitting into singletons.
	std::vector<double> w = {0.8, 0.8, 0.8, 0.8, 0.4, 0.4};
	std::vector<EntityClustering::Edge> edges = {
		{0, 1, 1.0}, {2, 3, 1.0}, {4, 5, 1.0}, {1, 2, 10.0}, {3, 4, 20.0}
	};
	auto labels = EntityClustering::Cluster(6, edges, WeightsOf(w), 1.0);
	CHECK(DistinctNonzero(labels) == 2);
	CHECK(labels[0] != 0);
	CHECK(labels[0] == labels[1]);
	CHECK(labels[2] == labels[3]);
	CHECK(labels[0] != labels[2]);
	CHECK(labels[4] == 0);
	CHECK(labels[5] == 0);
}

static void TestDisconnectedComponents()
{
	// Two tight groups with NO edge between them -> the candidate graph is a
	// spanning forest of two components (exactly what the KNN glue produces for
	// well-separated clusters).  With multiple forest roots each root is selectable,
	// so each component becomes its own cluster rather than being dropped as an
	// unselectable forest root.  A = {0,1,2}, B = {3,4,5}.
	std::vector<double> w(7, 1.0);
	std::vector<EntityClustering::Edge> edges = {
		{0, 1, 1.0}, {1, 2, 1.0}, {3, 4, 1.0}, {4, 5, 1.0}
	};
	auto labels = EntityClustering::Cluster(6, edges, WeightsOf(w), 2.0);
	CHECK(DistinctNonzero(labels) == 2);
	CHECK(labels[0] != 0);
	CHECK(labels[0] == labels[1]);
	CHECK(labels[1] == labels[2]);
	CHECK(labels[3] != 0);
	CHECK(labels[3] == labels[4]);
	CHECK(labels[4] == labels[5]);
	CHECK(labels[0] != labels[3]);

	// Two valid groups plus a lone point (index 6).  The lone point is a tiny
	// component (mass 1 < minimum_cluster_weight): after bridging it falls out of
	// the root as noise, while the two real groups still cluster.
	std::vector<EntityClustering::Edge> edges2 = { {0, 1, 1.0}, {1, 2, 1.0}, {3, 4, 1.0}, {4, 5, 1.0} };
	auto labels2 = EntityClustering::Cluster(7, edges2, WeightsOf(w), 2.0);  // point 6 is isolated
	CHECK(DistinctNonzero(labels2) == 2);
	CHECK(labels2[0] != 0);
	CHECK(labels2[0] == labels2[2]);
	CHECK(labels2[3] != 0);
	CHECK(labels2[3] == labels2[5]);
	CHECK(labels2[0] != labels2[3]);
	CHECK(labels2[6] == 0);  // lone sub-threshold point -> noise
}

static void TestSeparatedClustersSplitNotMerged()
{
	// Two tight pairs joined at a slightly larger weight: the split (weight 1.0)
	// happens just before the pairs dissolve (weight 0.9), so each child cluster
	// has only small stability while the root would have large excess of mass.
	// These edges form ONE connected component (single root), so allow_single_cluster
	// = false applies: the lone root is forbidden and selection returns the TWO real
	// clusters instead of letting the root absorb everything into ONE cluster.  This
	// guards against regressing to the root-absorbs-everything behavior.
	std::vector<double> w(4, 1.0);
	std::vector<EntityClustering::Edge> edges = { {0, 1, 0.9}, {2, 3, 0.9}, {1, 2, 1.0} };
	auto labels = EntityClustering::Cluster(4, edges, WeightsOf(w), 2.0);
	CHECK(DistinctNonzero(labels) == 2);
	CHECK(labels[0] == labels[1]);
	CHECK(labels[2] == labels[3]);
	CHECK(labels[0] != labels[2]);
}

static void TestExcessOfMassParentWins()
{
	// Two parent clusters P={0,1,2,3} and Q={4,5,6,7}, each of which splits into
	// two tight sub-pairs.  Each parent persists over a WIDE lambda range before
	// splitting (born at the root weight 10 -> lambda 0.1, splits at weight 1.0 ->
	// lambda 1.0), while the sub-pairs dissolve almost immediately after forming
	// (weight 0.95 -> lambda ~1.053).  Excess of mass therefore prefers each
	// PARENT over its two children: the result is 2 clusters, not 4.
	std::vector<double> w(8, 1.0);
	std::vector<EntityClustering::Edge> edges = {
		{0, 1, 0.95}, {2, 3, 0.95}, {1, 2, 1.0},   // P and its split
		{4, 5, 0.95}, {6, 7, 0.95}, {5, 6, 1.0},   // Q and its split
		{3, 4, 10.0}                                // P-Q join (root)
	};
	auto labels = EntityClustering::Cluster(8, edges, WeightsOf(w), 2.0);
	CHECK(DistinctNonzero(labels) == 2);            // parents selected, not the 4 children
	for(int i = 1; i < 4; ++i)
		CHECK(labels[i] == labels[0]);              // all of P shares one label
	for(int i = 5; i < 8; ++i)
		CHECK(labels[i] == labels[4]);              // all of Q shares one label
	CHECK(labels[0] != 0 && labels[4] != 0);
	CHECK(labels[0] != labels[4]);
}

//Runs the clustering algorithm without other dependencies.  Declared in
//clustering_test.h and invoked from the lib_smoke_test driver so the pure-algorithm checks
//ride along in the existing test executable rather than a standalone one.  Prints any
//failures and a summary; returns the number of failed checks (0 on success).
int RunClusteringUnitTests()
{
	TestBuildMST();
	TestBuildSingleLinkageTree();
	TestCondenseTree();
	TestComputeStabilities();
	TestSelectClusters();
	TestClusterEndToEnd();
	TestWeightThreshold();
	TestDisconnectedComponents();
	TestSeparatedClustersSplitNotMerged();
	TestExcessOfMassParentWins();

	std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed" << std::endl;
	return g_failures == 0 ? 0 : 1;
}
