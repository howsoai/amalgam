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

	//Appends every leaf point id under `node` to `out`.
	inline void CollectLeaves(size_t node, size_t m,
		const std::vector<SingleLinkageNode> &nodes, std::vector<size_t> &out)
	{
		std::vector<size_t> stack;
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
			stack.push_back(sn.left);
			stack.push_back(sn.right);
		}
	}

	//Condenses the dendrogram: small branches fall out as noise, balanced splits
	//create child clusters, single big branches continue the parent cluster.
	inline std::vector<CondensedEdge> CondenseTree(size_t m,
		const std::vector<SingleLinkageNode> &nodes,
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
			is_child[sn.left] = true;
			is_child[sn.right] = true;
		}

		size_t next_label = m;	//condensed cluster labels live above point ids
		std::unordered_map<size_t, size_t> node_label;	//dendrogram node id -> cluster label

		std::vector<size_t> work;	//internal node ids still to process
		for(size_t k = 0; k < nodes.size(); ++k)
		{
			size_t id = m + k;
			if(!is_child[id])	//a root of the forest (one connected component)
			{
				//a component only becomes a cluster if its total mass meets the
				//threshold; smaller components (e.g. scattered outliers) stay noise
				if(NodeMass(id, m, point_weights, nodes) >= min_cluster_weight)
				{
					node_label[id] = next_label++;
					work.push_back(id);
				}
			}
		}

		while(!work.empty())
		{
			size_t id = work.back();
			work.pop_back();
			size_t label = node_label[id];
			const SingleLinkageNode &sn = nodes[id - m];
			double lambda = (sn.weight > 0.0) ? 1.0 / sn.weight : lambda_max;

			size_t children[2] = { sn.left, sn.right };
			double masses[2];
			bool big[2];
			for(int c = 0; c < 2; ++c)
			{
				masses[c] = NodeMass(children[c], m, point_weights, nodes);
				big[c] = masses[c] >= min_cluster_weight;
			}
			int num_big = (big[0] ? 1 : 0) + (big[1] ? 1 : 0);

			for(int c = 0; c < 2; ++c)
			{
				size_t child = children[c];
				if(!big[c])
				{
					//small branch: all its points fall out of `label` as noise
					std::vector<size_t> leaves;
					CollectLeaves(child, m, nodes, leaves);
					for(size_t p : leaves)
						result.push_back(CondensedEdge{label, p, lambda, point_weights[p]});
				}
				else if(num_big == 2)
				{
					//genuine split: the big child becomes a new cluster
					size_t child_label = next_label++;
					result.push_back(CondensedEdge{label, child_label, lambda, masses[c]});
					if(child >= m)
					{
						node_label[child] = child_label;
						work.push_back(child);
					}
					else	//a single heavy point that is itself a cluster
						result.push_back(CondensedEdge{child_label, child, lambda_max, point_weights[child]});
				}
				else
				{
					//exactly one big child: it continues the same cluster `label`
					if(child >= m)
					{
						node_label[child] = label;
						work.push_back(child);
					}
					else	//a heavy continuing leaf stays in `label` to the bottom
						result.push_back(CondensedEdge{label, child, lambda_max, point_weights[child]});
				}
			}
		}
		return result;
	}

	//Birth lambda for every cluster label.  A cluster is born at the lambda of
	//the edge that split it off its parent; roots are born at lambda 0.
	inline std::unordered_map<size_t, double> ClusterBirthLambdas(size_t m,
		const std::vector<CondensedEdge> &condensed)
	{
		std::unordered_map<size_t, double> birth;
		std::unordered_set<size_t> clusters;
		for(const CondensedEdge &e : condensed)
		{
			clusters.insert(e.parent);
			if(e.child >= m)
			{
				clusters.insert(e.child);
				birth[e.child] = e.lambda;
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
	//child_mass * (lambda_leave - lambda_birth).
	inline std::unordered_map<size_t, double> ComputeStabilities(size_t m,
		const std::vector<CondensedEdge> &condensed)
	{
		std::unordered_map<size_t, double> birth = ClusterBirthLambdas(m, condensed);
		std::unordered_map<size_t, double> stability;
		for(const auto &kv : birth)
			stability[kv.first] = 0.0;
		for(const CondensedEdge &e : condensed)
			stability[e.parent] += e.child_mass * (e.lambda - birth[e.parent]);
		return stability;
	}

	//Excess-of-mass cluster selection.  Returns the set of kept cluster labels.
	//Root (whole-component) clusters are never selected.
	inline std::unordered_set<size_t> SelectClusters(size_t m,
		const std::vector<CondensedEdge> &condensed,
		const std::unordered_map<size_t, double> &stability)
	{
		//cluster -> parent cluster, and cluster -> child clusters
		std::unordered_map<size_t, size_t> cluster_parent;
		std::unordered_map<size_t, std::vector<size_t>> children;
		std::unordered_set<size_t> all_clusters;
		for(const CondensedEdge &e : condensed)
		{
			all_clusters.insert(e.parent);
			if(e.child >= m)
			{
				all_clusters.insert(e.child);
				cluster_parent[e.child] = e.parent;
				children[e.parent].push_back(e.child);
			}
		}

		std::unordered_map<size_t, double> birth = ClusterBirthLambdas(m, condensed);

		//process children before parents: descending birth lambda
		std::vector<size_t> order(all_clusters.begin(), all_clusters.end());
		std::sort(order.begin(), order.end(),
			[&](size_t a, size_t b) { return birth[a] > birth[b]; });

		std::unordered_map<size_t, double> propagated;
		std::unordered_set<size_t> selected;

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

		//allow_single_cluster = true: a root (whole connected component) competes in
		//excess-of-mass like any other cluster.  This is required because the KNN
		//candidate graph disconnects well-separated clusters into separate components
		//(a spanning forest); each component is a root, and must be selectable for its
		//points to be clustered rather than dropped as noise.
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

			if(own >= sum_child)
			{
				selected.insert(c);
				deselect_descendants(c);
				propagated[c] = own;
			}
			else
			{
				//children win: keep descendants, propagate their mass up
				propagated[c] = sum_child;
			}
		}
		return selected;
	}

	//Assigns a 1-based label to each point: the nearest selected ancestor cluster
	//of the cluster it falls out of, or 0 (noise) if none is selected.
	inline std::vector<size_t> LabelPoints(size_t m,
		const std::vector<CondensedEdge> &condensed,
		const std::unordered_set<size_t> &selected)
	{
		std::vector<size_t> labels(m, 0);
		if(selected.empty())
			return labels;

		//compact, deterministic 1-based ids in ascending cluster-label order
		std::vector<size_t> sel(selected.begin(), selected.end());
		std::sort(sel.begin(), sel.end());
		std::unordered_map<size_t, size_t> cluster_to_id;
		for(size_t i = 0; i < sel.size(); ++i)
			cluster_to_id[sel[i]] = i + 1;

		std::unordered_map<size_t, size_t> cluster_parent;
		for(const CondensedEdge &e : condensed)
		{
			if(e.child >= m)
				cluster_parent[e.child] = e.parent;
		}

		for(const CondensedEdge &e : condensed)
		{
			if(e.child >= m)	//sub-cluster edge, not a point
				continue;
			size_t p = e.child;
			size_t c = e.parent;
			while(true)
			{
				if(selected.find(c) != selected.end())
				{
					labels[p] = cluster_to_id[c];
					break;
				}
				auto it = cluster_parent.find(c);
				if(it == cluster_parent.end())
					break;	//reached a root with no selected ancestor -> noise
				c = it->second;
			}
		}
		return labels;
	}

	//Full pipeline: candidate edges + point weights -> per-point labels (0 = noise).
	inline std::vector<size_t> Cluster(size_t m, std::vector<Edge> edges,
		const std::vector<double> &point_weights, double min_cluster_weight)
	{
		std::vector<Edge> mst = BuildMST(m, std::move(edges));
		std::vector<SingleLinkageNode> slt = BuildSingleLinkageTree(m, mst, point_weights);
		std::vector<CondensedEdge> condensed = CondenseTree(m, slt, point_weights, min_cluster_weight);
		std::unordered_map<size_t, double> stability = ComputeStabilities(m, condensed);
		std::unordered_set<size_t> selected = SelectClusters(m, condensed, stability);
		return LabelPoints(m, condensed, selected);
	}
}
