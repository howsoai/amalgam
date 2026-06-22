#pragma once

//project headers:
#include "HashMaps.h"

//system headers:
#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

//Self-contained HDBSCAN* clustering.  Apart from the in-tree FastHashMap /
//FastHashSet associative containers, it has no Amalgam dependencies.
namespace HDBSCAN
{
	//Node-id convention used throughout: m is the number of input points being
	//clustered.  Ids in [0, m) are individual points; ids >= m are internal nodes,
	//where m + k is the k-th single-linkage dendrogram node (which later doubles as a
	//condensed-tree cluster id).  Testing (id < m) distinguishes a point from an
	//internal node, so one size_t can refer uniformly to either.

	//A candidate undirected edge with its mutual-reachability weight.
	struct Edge
	{
		size_t u;		//endpoint point id (< m)
		size_t v;		//endpoint point id (< m)
		double weight;	//mutual-reachability distance between u and v
	};

	//One internal node of the single-linkage dendrogram (one per accepted MST edge).
	struct SingleLinkageNode
	{
		size_t leftChildId;		//child node id (point id < m, or internal node id >= m)
		size_t rightChildId;	//child node id (point id < m, or internal node id >= m)
		double weight;			//mutual-reachability distance at which the children merged
		double mass;			//summed point weight of this subtree
	};

	//A child node leaving its parent cluster at density level lambda (= 1 / weight).
	struct CondensedEdge
	{
		size_t parentId;	//cluster id of the parent (>= m)
		size_t childId;		//point id (< m) or sub-cluster id (>= m) that departs the parent
		double lambda;		//1 / weight at which the child separates from the parent
		double childMass;	//summed point weight of the child
	};

	//Builds the minimum spanning tree (forest, if disconnected) via Kruskal's
	//algorithm.  Returns accepted edges in ascending weight order.
	inline std::vector<Edge> BuildMST(size_t m, std::vector<Edge> edges)
	{
		std::sort(edges.begin(), edges.end(),
			[](const Edge &a, const Edge &b) { return a.weight < b.weight; });

		std::vector<size_t> parent(m);
		std::vector<size_t> rank(m, 0);
		for(size_t i = 0; i < m; i++)
			parent[i] = i;

		auto find = [&](size_t x) -> size_t
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
				rank[ra]++;
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
	//mst is taken by value: a moved-in argument is consumed and freed when this
	//returns, so the caller need not hold the MST alive past this stage.
	inline std::vector<SingleLinkageNode> BuildSingleLinkageTree(size_t m,
		std::vector<Edge> mst, const std::vector<double> &point_weights)
	{
		std::vector<SingleLinkageNode> nodes;
		nodes.reserve(mst.size());

		std::vector<size_t> parent(m);
		for(size_t i = 0; i < m; i++)
			parent[i] = i;
		auto find = [&](size_t x) -> size_t
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
		for(size_t i = 0; i < m; i++)
			comp_node[i] = i;

		for(const Edge &e : mst)
		{
			size_t ra = find(e.u);
			size_t rb = find(e.v);
			size_t na = comp_node[ra];
			size_t nb = comp_node[rb];

			SingleLinkageNode node;
			node.leftChildId = na;
			node.rightChildId = nb;
			node.weight = e.weight;
			node.mass = NodeMass(na, m, point_weights, nodes) + NodeMass(nb, m, point_weights, nodes);
			size_t new_id = m + nodes.size();
			nodes.push_back(node);

			parent[rb] = ra;
			comp_node[ra] = new_id;
		}
		return nodes;
	}

	//Appends every leaf point id under node to out.  stack is a caller-owned scratch
	//buffer, reused across calls and cleared on entry, to avoid a per-call allocation.
	inline void CollectLeaves(size_t node, size_t m,
		const std::vector<SingleLinkageNode> &nodes,
		std::vector<size_t> &out, std::vector<size_t> &stack)
	{
		stack.clear();
		stack.push_back(node);
		while(!stack.empty())
		{
			size_t n = stack.back();
			stack.pop_back();
			if(n < m)
			{
				out.push_back(n);
				continue;
			}
			const SingleLinkageNode &sn = nodes[n - m];
			stack.push_back(sn.leftChildId);
			stack.push_back(sn.rightChildId);
		}
	}

	//Condenses the dendrogram: small branches fall out as noise, balanced splits
	//create child clusters, single big branches continue the parent cluster.
	//nodes is taken by value: a moved-in single-linkage tree is consumed and freed
	//when this returns, so the caller need not hold it alive past this stage.
	inline std::vector<CondensedEdge> CondenseTree(size_t m,
		std::vector<SingleLinkageNode> nodes,
		const std::vector<double> &point_weights, double min_cluster_weight)
	{
		std::vector<CondensedEdge> result;
		if(nodes.empty())
			return result;

		//largest finite lambda, used in place of "leaves at infinity"
		double lambda_max = 0.0;
		for(const SingleLinkageNode &sn : nodes)
		{
			if(sn.weight > 0.0)
				lambda_max = std::max(lambda_max, 1.0 / sn.weight);
		}

		size_t total_nodes = m + nodes.size();
		std::vector<bool> is_child(total_nodes, false);
		for(const SingleLinkageNode &sn : nodes)
		{
			is_child[sn.leftChildId] = true;
			is_child[sn.rightChildId] = true;
		}

		size_t next_cluster_id = m;	//condensed cluster ids live above point ids
		FastHashMap<size_t, size_t> node_cluster_id;	//dendrogram node id -> cluster id

		//seed the work list with the forest roots heavy enough to be clusters
		std::vector<size_t> pending_nodes;	//internal node ids still to process
		for(size_t k = 0; k < nodes.size(); k++)
		{
			size_t id = m + k;
			if(!is_child[id])	//a root of the forest (one connected component)
			{
				//a component only becomes a cluster if its total mass meets the
				//threshold; smaller components (e.g. scattered outliers) stay noise
				if(NodeMass(id, m, point_weights, nodes) >= min_cluster_weight)
				{
					node_cluster_id[id] = next_cluster_id++;
					pending_nodes.push_back(id);
				}
			}
		}

		//scratch buffers reused across the walk below to avoid per-iteration allocation
		std::vector<size_t> leaf_scratch;	//leaves of a small branch falling out as noise
		std::vector<size_t> collect_stack;	//DFS stack for CollectLeaves

		//Walk each pending cluster node down the dendrogram.  The merge at this node
		//happened at density level lambda = 1 / weight; classify its two children
		//against min_cluster_weight to decide their fate:
		//  - a child too light to be a cluster dissolves here: its leaves fall out of
		//    the current cluster as noise edges at this lambda;
		//  - when BOTH children are heavy enough it is a genuine split, so each heavy
		//    child is born as a new cluster id (and, if internal, queued for processing);
		//  - when only ONE child is heavy the current cluster simply continues down
		//    into it under the same cluster id, with no split recorded.
		while(!pending_nodes.empty())
		{
			size_t id = pending_nodes.back();
			pending_nodes.pop_back();
			size_t cluster_id = node_cluster_id[id];
			const SingleLinkageNode &sn = nodes[id - m];
			double lambda = (sn.weight > 0.0) ? 1.0 / sn.weight : lambda_max;

			size_t children[2] = { sn.leftChildId, sn.rightChildId };
			double masses[2];
			bool big[2];
			for(int c = 0; c < 2; c++)
			{
				masses[c] = NodeMass(children[c], m, point_weights, nodes);
				big[c] = masses[c] >= min_cluster_weight;
			}
			int num_big = (big[0] ? 1 : 0) + (big[1] ? 1 : 0);

			for(int c = 0; c < 2; c++)
			{
				size_t child = children[c];
				if(!big[c])
				{
					//small branch: all its points fall out of the cluster as noise
					leaf_scratch.clear();
					CollectLeaves(child, m, nodes, leaf_scratch, collect_stack);
					for(size_t p : leaf_scratch)
						result.push_back(CondensedEdge{cluster_id, p, lambda, point_weights[p]});
				}
				else if(num_big == 2)
				{
					//genuine split: the big child becomes a new cluster
					size_t child_cluster_id = next_cluster_id++;
					result.push_back(CondensedEdge{cluster_id, child_cluster_id, lambda, masses[c]});
					if(child >= m)
					{
						node_cluster_id[child] = child_cluster_id;
						pending_nodes.push_back(child);
					}
					else	//a single heavy point that is itself a cluster
						result.push_back(CondensedEdge{child_cluster_id, child, lambda_max, point_weights[child]});
				}
				else
				{
					//exactly one big child: it continues the same cluster
					if(child >= m)
					{
						node_cluster_id[child] = cluster_id;
						pending_nodes.push_back(child);
					}
					else	//a heavy continuing leaf stays in the cluster to the bottom
						result.push_back(CondensedEdge{cluster_id, child, lambda_max, point_weights[child]});
				}
			}
		}
		return result;
	}

	//Birth lambda (the density level 1/weight at which a cluster first appears as its
	//own cluster) for every cluster id in the condensed tree.  A non-root cluster is
	//born at the lambda of the condensed edge that split it off its parent; a root
	//cluster (one that never splits off of anything) is born at lambda 0.
	inline FastHashMap<size_t, double> ClusterBirthLambdas(size_t m,
		const std::vector<CondensedEdge> &condensed)
	{
		FastHashMap<size_t, double> birth;
		FastHashSet<size_t> clusters;
		for(const CondensedEdge &e : condensed)
		{
			clusters.insert(e.parentId);
			if(e.childId >= m)
			{
				clusters.insert(e.childId);
				birth[e.childId] = e.lambda;
			}
		}
		for(size_t c : clusters)
		{
			if(birth.find(c) == birth.end())
				birth[c] = 0.0;
		}
		return birth;
	}

	//Stability of each cluster: sum over its departing children of
	//childMass * (lambda_leave - lambda_birth).  birth is the map returned by
	//ClusterBirthLambdas; it is passed in so the pipeline computes it only once and
	//shares it with SelectClusters.
	inline FastHashMap<size_t, double> ComputeStabilities(
		const std::vector<CondensedEdge> &condensed,
		const FastHashMap<size_t, double> &birth)
	{
		FastHashMap<size_t, double> stability;
		for(const auto &kv : birth)
			stability[kv.first] = 0.0;
		for(const CondensedEdge &e : condensed)
			stability[e.parentId] += e.childMass * (e.lambda - birth.at(e.parentId));
		return stability;
	}

	//Excess-of-mass cluster selection.  Returns the set of kept cluster labels.
	//Root (whole-component) clusters are never selected.
	//stability and birth are taken by value: both are moved-in maps consumed and
	//freed when this returns, since selection is their last reader.  birth is the
	//ClusterBirthLambdas map, shared with ComputeStabilities so it is computed once.
	inline FastHashSet<size_t> SelectClusters(size_t m,
		const std::vector<CondensedEdge> &condensed,
		FastHashMap<size_t, double> stability,
		FastHashMap<size_t, double> birth)
	{
		//cluster -> parent cluster, and cluster -> child clusters
		FastHashMap<size_t, size_t> cluster_parent;
		FastHashMap<size_t, std::vector<size_t>> children;
		FastHashSet<size_t> all_clusters;
		for(const CondensedEdge &e : condensed)
		{
			all_clusters.insert(e.parentId);
			if(e.childId >= m)
			{
				all_clusters.insert(e.childId);
				cluster_parent[e.childId] = e.parentId;
				children[e.parentId].push_back(e.childId);
			}
		}

		//process children before parents by descending birth lambda.  A child is
		//born where it splits off its parent (a deeper, denser level), so its birth
		//lambda is strictly greater than its parent's; sorting descending therefore
		//guarantees every child is processed (and its propagated stability set)
		//before its parent reads it.
		std::vector<size_t> order(all_clusters.begin(), all_clusters.end());
		std::sort(order.begin(), order.end(),
			[&](size_t a, size_t b) { return birth[a] > birth[b]; });

		FastHashMap<size_t, double> propagated;
		FastHashSet<size_t> selected;

		auto deselect_descendants = [&](size_t c)
		{
			std::vector<size_t> stack(children[c].begin(), children[c].end());
			while(!stack.empty())
			{
				size_t d = stack.back();
				stack.pop_back();
				selected.erase(d);
				for(size_t g : children[d])
					stack.push_back(g);
			}
		};

		//A root cluster has birth lambda 0 (it splits off from nothing).  The KNN
		//candidate graph omits long inter-cluster links, so well-separated clusters
		//arrive as a disconnected spanning forest: one root per cluster.  Rather than
		//fabricating bridge edges to force them under one artificial root, we allow the
		//multiple forest roots to be selected directly ("allow multiple tree roots").
		//
		//The single-root case keeps allow_single_cluster = false (matching scikit-learn's
		//default): when there is exactly one root it is the whole-dataset cluster, which
		//is never selected, so selection descends to genuine sub-clusters.  When there are
		//two or more roots each is a genuinely separated population and is eligible for
		//excess-of-mass selection like any other cluster.
		size_t num_roots = 0;
		for(const auto &kv : birth)
		{
			if(!(kv.second > 0.0))
				num_roots++;
		}

		for(size_t c : order)
		{
			double sum_child = 0.0;
			auto it = children.find(c);
			if(it != children.end())
			{
				for(size_t ch : it->second)
					sum_child += propagated[ch];
			}

			double own = stability.count(c) ? stability.at(c) : 0.0;

			//a lone whole-dataset root is never selectable; forest roots are
			bool is_root = !(birth.at(c) > 0.0);
			bool selectable = !is_root || num_roots >= 2;

			if(selectable && own >= sum_child)
			{
				selected.insert(c);
				deselect_descendants(c);
				propagated[c] = own;
			}
			else
			{
				//unselectable lone root, or children win: keep descendants and
				//propagate their summed stability up
				propagated[c] = sum_child;
			}
		}
		return selected;
	}

	//Assigns a 1-based cluster id to each point: the nearest selected ancestor cluster
	//of the cluster it falls out of, or 0 (noise) if no ancestor is selected.
	inline std::vector<size_t> AssignClusterIds(size_t m,
		const std::vector<CondensedEdge> &condensed,
		const FastHashSet<size_t> &selected)
	{
		std::vector<size_t> cluster_ids(m, 0);
		if(selected.empty())
			return cluster_ids;

		//compact, deterministic 1-based ids in ascending selected-cluster order
		std::vector<size_t> sel(selected.begin(), selected.end());
		std::sort(sel.begin(), sel.end());
		FastHashMap<size_t, size_t> cluster_to_id;
		for(size_t i = 0; i < sel.size(); i++)
			cluster_to_id[sel[i]] = i + 1;

		FastHashMap<size_t, size_t> cluster_parent;
		for(const CondensedEdge &e : condensed)
		{
			if(e.childId >= m)
				cluster_parent[e.childId] = e.parentId;
		}

		for(const CondensedEdge &e : condensed)
		{
			if(e.childId >= m)	//sub-cluster edge, not a point
				continue;
			size_t p = e.childId;
			size_t c = e.parentId;
			while(true)
			{
				if(selected.find(c) != selected.end())
				{
					cluster_ids[p] = cluster_to_id[c];
					break;
				}
				auto it = cluster_parent.find(c);
				if(it == cluster_parent.end())
					break;	//reached a root with no selected ancestor -> noise
				c = it->second;
			}
		}
		return cluster_ids;
	}

	//Full pipeline: candidate edges + point weights -> per-point cluster ids (0 = noise).
	//The KNN candidate graph is generally a spanning forest (well-separated clusters
	//share no candidate edge); BuildMST returns one tree per component and the forest
	//roots are selected directly by SelectClusters (the "multiple tree roots" approach),
	//so no artificial bridge edges are introduced.
	//
	//Each single-use intermediate (the MST, the single-linkage tree, the stability
	//map) is std::move()d into the stage that consumes it; those stages take their
	//argument by value, so each structure is freed when that stage returns rather
	//than living until the whole pipeline finishes.  Only condensed has more than
	//one downstream reader, so it alone is kept by reference until the end.
	inline std::vector<size_t> Cluster(size_t m, std::vector<Edge> edges,
		const std::vector<double> &point_weights, double min_cluster_weight)
	{
		std::vector<Edge> mst = BuildMST(m, std::move(edges));
		std::vector<SingleLinkageNode> slt = BuildSingleLinkageTree(m, std::move(mst), point_weights);
		std::vector<CondensedEdge> condensed = CondenseTree(m, std::move(slt), point_weights, min_cluster_weight);
		FastHashMap<size_t, double> birth = ClusterBirthLambdas(m, condensed);
		FastHashMap<size_t, double> stability = ComputeStabilities(condensed, birth);
		FastHashSet<size_t> selected = SelectClusters(m, condensed, std::move(stability), std::move(birth));
		return AssignClusterIds(m, condensed, selected);
	}
}
