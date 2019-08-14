/*
 * Betweenness.cpp
 *
 *  Created on: 29.07.2014
 *      Author: cls, ebergamini
 */

#include <memory>
#include <omp.h>

#include <networkit/centrality/Betweenness.hpp>
#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/SignalHandling.hpp>
#include <networkit/distance/Dijkstra.hpp>
#include <networkit/distance/BFS.hpp>

namespace NetworKit {

Betweenness::Betweenness(const Graph& G, bool normalized, bool computeEdgeCentrality) :
	Centrality(G, normalized, computeEdgeCentrality) {}

void Betweenness::run() {
	Aux::SignalHandler handler;
	count z = G.upperNodeIdBound();
	scoreData.clear();
	scoreData.resize(z);
	lengthScaled.clear();
	lengthScaled.resize(z);

	if (computeEdgeCentrality) {
		count z2 = G.upperEdgeIdBound();
		edgeScoreData.clear();
		edgeScoreData.resize(z2);
	}

	// thread-local scores for efficient parallelism
	count maxThreads = omp_get_max_threads();
	std::vector<std::vector<double> > scorePerThread(maxThreads, std::vector<double>(G.upperNodeIdBound()));

	DEBUG("score per thread: ", scorePerThread.size());
	DEBUG("G.upperEdgeIdBound(): ", G.upperEdgeIdBound());
	std::vector<std::vector<double> > edgeScorePerThread;
	if (computeEdgeCentrality) {
		edgeScorePerThread.resize(maxThreads, std::vector<double>(G.upperEdgeIdBound()));
	}
	DEBUG("edge score per thread: ", edgeScorePerThread.size());

	std::vector<std::vector<double>> dependencies(maxThreads, std::vector<double>(z));
	std::vector<std::unique_ptr<SSSP>> sssps;
	sssps.resize(maxThreads);
#pragma omp parallel
	{
		omp_index i = omp_get_thread_num();
		if (G.isWeighted())
			sssps[i] = std::unique_ptr<SSSP>(new Dijkstra(G, 0, true, true));
		else
			sssps[i] = std::unique_ptr<SSSP>(new BFS(G, 0, true, true));
	}

	auto computeDependencies = [&](node s) {

		std::vector<double> &dependency = dependencies[omp_get_thread_num()];
		std::fill(dependency.begin(), dependency.end(), 0);

		// run SSSP algorithm and keep track of everything
		auto sssp = sssps[omp_get_thread_num()].get();
		sssp->setSource(s);
		if (!handler.isRunning()) return;
		sssp->run();
		if (!handler.isRunning()) return;
		// compute dependencies for nodes in order of decreasing distance from s
		std::vector<node> stack = sssp->getNodesSortedByDistance();
		while (!stack.empty()) {
			node t = stack.back();
			stack.pop_back();
#define WATCH_NODE 3

			for (node p : sssp->getPredecessors(t)) {
				// workaround for integer overflow in large graphs
				bigfloat tmp = sssp->numberOfPaths(p) / sssp->numberOfPaths(t);
				double weight;
				tmp.ToDouble(weight);
				double c= weight * (1 + dependency[t]);
				dependency[p] += c;
				if (computeEdgeCentrality) {
					edgeScorePerThread[omp_get_thread_num()][G.edgeId(p,t)] += c;
				}

			}

			if (t != s ) {
				scorePerThread[omp_get_thread_num()][t] += dependency[t];

				// Length scalled betweenness begin
				const auto paths = sssp->getPaths(t);
				const auto pathsCount = paths.size();
				const auto pathLength = sssp->distance(t);

				if (pathsCount == 0) continue;

				const double lengthScale = (double)pathsCount / (double)(pathLength);

				for (auto it = paths.begin(); it != paths.end(); ++it) {
					const auto path = *it;

					if (path.size() <= 2) continue;

					for (auto node = path.begin() + 1; node != path.end() - 1; node++) {

						lengthScaled[*node] += lengthScale;
					}
				}

				// Length scalled betweenness end
			}
		}
	};
	handler.assureRunning();
	G.balancedParallelForNodes(computeDependencies);

	handler.assureRunning();
	DEBUG("adding thread-local scores");
	// add up all thread-local values
	for (const auto &local : scorePerThread) {
		G.parallelForNodes([&](node v){
			scoreData[v] += local[v];
		});
	}
	if (computeEdgeCentrality) {
		for (const auto &local : edgeScorePerThread) {
			for (count i = 0; i < local.size(); i++) {
				edgeScoreData[i] += local[i];
			}
		}
	}
	if (normalized) {
		// divide by the number of possible pairs
		count n = G.numberOfNodes();
		count pairs = (n-2) * (n-1);
		count edges =  n    * (n-1);
		if (!G.isDirected()) {
			pairs = pairs / 2;
			edges = edges / 2;
		}
		G.forNodes([&](node u){
			scoreData[u] = scoreData[u] / pairs;
		});
		if (computeEdgeCentrality) {
			for (count edge = 0; edge < edgeScoreData.size(); edge++) {
				edgeScoreData[edge] =  edgeScoreData[edge] / edges;
			}
		}
	}

	hasRun = true;
}

double Betweenness::maximum(){
	if (normalized) {
		return 1;
	}
	double score;
	count n = G.numberOfNodes();
	if (G.isDirected()) {
		score = (n-1)*(n-2);
	} else {
		score = (n-1)*(n-2)/2;
	}
	return score;
}

} /* namespace NetworKit */
