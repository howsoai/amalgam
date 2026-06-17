//Unit tests for the pure HDBSCAN* algorithm (no Amalgam dependencies).
#include "HDBSCANClustering.h"

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

//Sum of edge weights, for comparing MST totals.
static double TotalWeight(const std::vector<HDBSCAN::Edge> &edges)
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
	std::vector<HDBSCAN::Edge> edges = {
		{0, 1, 1.0}, {1, 2, 2.0}, {2, 3, 1.0}, {3, 0, 2.0}, {0, 2, 0.5}
	};
	auto mst = HDBSCAN::BuildMST(4, edges);
	CHECK(mst.size() == 3);
	CHECK(TotalWeight(mst) == 2.5);
	// ascending weight order
	CHECK(mst[0].weight <= mst[1].weight);
	CHECK(mst[1].weight <= mst[2].weight);

	// Disconnected: {0,1} and {2,3}, no edge between -> forest of 2 edges.
	std::vector<HDBSCAN::Edge> disc = { {0, 1, 1.0}, {2, 3, 1.0} };
	auto forest = HDBSCAN::BuildMST(4, disc);
	CHECK(forest.size() == 2);
}

static void TestBuildSingleLinkageTree()
{
	// 3 points on a line: 0--(1.0)--1--(2.0)--2, weights all 1.0.
	std::vector<HDBSCAN::Edge> mst = { {0, 1, 1.0}, {1, 2, 2.0} };
	std::vector<double> w = {1.0, 1.0, 1.0};
	auto slt = HDBSCAN::BuildSingleLinkageTree(3, mst, w);

	CHECK(slt.size() == 2);
	// First merge joins points 0 and 1 at weight 1.0, mass 2.0 -> node id 3.
	CHECK(slt[0].weight == 1.0);
	CHECK(slt[0].mass == 2.0);
	// Second merge joins node 3 with point 2 at weight 2.0, mass 3.0 -> node id 4.
	CHECK(slt[1].weight == 2.0);
	CHECK(slt[1].mass == 3.0);
	CHECK((slt[1].left == 3 || slt[1].right == 3));
	CHECK((slt[1].left == 2 || slt[1].right == 2));
}

//Count, in a condensed tree, how many distinct points fall out of each parent.
static size_t CountPointChildren(const std::vector<HDBSCAN::CondensedEdge> &c, size_t m)
{
	std::set<size_t> pts;
	for(const auto &e : c)
		if(e.child < m)
			pts.insert(e.child);
	return pts.size();
}

static void TestCondenseTree()
{
	// Two tight pairs joined by a long edge:
	//   0-1 : 1.0  (tight)        2-3 : 1.0  (tight)
	//   then {0,1}-{2,3} : 10.0   (the split)
	// weights all 1.0; min_cluster_weight = 2.0
	std::vector<HDBSCAN::Edge> mst = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	std::vector<double> w = {1.0, 1.0, 1.0, 1.0};
	auto slt = HDBSCAN::BuildSingleLinkageTree(4, mst, w);
	auto condensed = HDBSCAN::CondenseTree(4, slt, w, 2.0);

	// Every one of the 4 points eventually falls out / is placed.
	CHECK(CountPointChildren(condensed, 4) == 4);
	// The top split (weight 10 -> lambda 0.1) is a genuine split: two child clusters
	// are born, so there are >= 2 sub-cluster edges (child >= m).
	size_t subcluster_edges = 0;
	for(const auto &e : condensed)
		if(e.child >= 4)
			++subcluster_edges;
	CHECK(subcluster_edges == 2);

	// With min_cluster_weight = 3.0, neither side of the top split (mass 2 each)
	// qualifies, so there is no genuine split: 0 sub-cluster edges.
	auto condensed2 = HDBSCAN::CondenseTree(4, slt, w, 3.0);
	size_t subcluster_edges2 = 0;
	for(const auto &e : condensed2)
		if(e.child >= 4)
			++subcluster_edges2;
	CHECK(subcluster_edges2 == 0);
}

static void TestComputeStabilities()
{
	// Same two-tight-pairs tree as TestCondenseTree, min_cluster_weight = 2.0.
	std::vector<HDBSCAN::Edge> mst = { {0, 1, 1.0}, {2, 3, 1.0}, {1, 2, 10.0} };
	std::vector<double> w = {1.0, 1.0, 1.0, 1.0};
	auto slt = HDBSCAN::BuildSingleLinkageTree(4, mst, w);
	auto condensed = HDBSCAN::CondenseTree(4, slt, w, 2.0);
	auto stab = HDBSCAN::ComputeStabilities(4, condensed);

	// There is a root cluster plus two children. Each child cluster is born at
	// lambda 0.1 (1/10) and loses its 2 points at lambda 1.0 (1/1):
	//   S(child) = 2 * (1.0 - 0.1) = 1.8
	// Find the two non-root clusters (they have positive birth lambda).
	auto birth = HDBSCAN::ClusterBirthLambdas(4, condensed);
	double child_stability_sum = 0.0;
	size_t child_count = 0;
	for(const auto &kv : stab)
	{
		if(birth[kv.first] > 0.0)	//a child (non-root) cluster
		{
			child_stability_sum += kv.second;
			++child_count;
		}
	}
	CHECK(child_count == 2);
	CHECK(child_stability_sum > 3.5 && child_stability_sum < 3.7);	//~3.6
}

int main()
{
	TestBuildMST();
	TestBuildSingleLinkageTree();
	TestCondenseTree();
	TestComputeStabilities();

	std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed" << std::endl;
	return g_failures == 0 ? 0 : 1;
}
