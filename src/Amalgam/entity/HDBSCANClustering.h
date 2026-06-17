#pragma once

//system headers:
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

//Pure, std-only HDBSCAN* clustering. No Amalgam dependencies.
namespace HDBSCAN
{
	//A candidate undirected edge with its mutual-reachability weight.
	struct Edge
	{
		size_t u;
		size_t v;
		double weight;
	};

	//One internal node of the single-linkage dendrogram (one per accepted MST edge).
	struct SingleLinkageNode
	{
		size_t left;	//child node id (point id < m, or internal node id >= m)
		size_t right;	//child node id
		double weight;	//mutual-reachability distance at which the children merged
		double mass;	//summed point weight of this subtree
	};

	//A child leaving cluster `parent` at density level `lambda`.
	struct CondensedEdge
	{
		size_t parent;		//cluster label (>= m)
		size_t child;		//point id (< m) or sub-cluster label (>= m)
		double lambda;		//1 / weight at which the child separates from parent
		double child_mass;	//summed weight of the child
	};

	//Builds the minimum spanning tree (forest, if disconnected) via Kruskal's
	//algorithm.  Returns accepted edges in ascending weight order.
	inline std::vector<Edge> BuildMST(size_t m, std::vector<Edge> edges)
	{
		std::sort(edges.begin(), edges.end(),
			[](const Edge &a, const Edge &b) { return a.weight < b.weight; });

		std::vector<size_t> parent(m);
		std::vector<size_t> rank(m, 0);
		for(size_t i = 0; i < m; ++i)
			parent[i] = i;

		std::function<size_t(size_t)> find = [&](size_t x) -> size_t
		{
			while(parent[x] != x)
			{
				parent[x] = parent[parent[x]];
				x = parent[x];
			}
			return x;
		};

		std::vector<Edge> mst;
		for(const Edge &e : edges)
		{
			size_t ra = find(e.u);
			size_t rb = find(e.v);
			if(ra == rb)
				continue;
			mst.push_back(e);
			if(rank[ra] < rank[rb])
				std::swap(ra, rb);
			parent[rb] = ra;
			if(rank[ra] == rank[rb])
				++rank[ra];
		}
		return mst;
	}
}
