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

	//Summed point weight of a node (point if < m, else internal subtree).
	inline double NodeMass(size_t node, size_t m,
		const std::vector<double> &point_weights,
		const std::vector<SingleLinkageNode> &nodes)
	{
		return node < m ? point_weights[node] : nodes[node - m].mass;
	}

	//Builds the single-linkage dendrogram from the (ascending) MST edges.
	//Each accepted merge becomes node id m + k with summed-weight mass.
	inline std::vector<SingleLinkageNode> BuildSingleLinkageTree(size_t m,
		const std::vector<Edge> &mst, const std::vector<double> &point_weights)
	{
		std::vector<SingleLinkageNode> nodes;
		nodes.reserve(mst.size());

		std::vector<size_t> parent(m);
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

		//top node id currently representing each component (indexed by point id)
		std::vector<size_t> comp_node(m);
		for(size_t i = 0; i < m; ++i)
			comp_node[i] = i;

		for(const Edge &e : mst)
		{
			size_t ra = find(e.u);
			size_t rb = find(e.v);
			size_t na = comp_node[ra];
			size_t nb = comp_node[rb];

			SingleLinkageNode node;
			node.left = na;
			node.right = nb;
			node.weight = e.weight;
			node.mass = NodeMass(na, m, point_weights, nodes) + NodeMass(nb, m, point_weights, nodes);
			size_t new_id = m + nodes.size();
			nodes.push_back(node);

			parent[rb] = ra;
			comp_node[ra] = new_id;
		}
		return nodes;
	}
}
