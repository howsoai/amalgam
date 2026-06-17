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

int main()
{
	TestBuildMST();
	TestBuildSingleLinkageTree();

	std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed" << std::endl;
	return g_failures == 0 ? 0 : 1;
}
