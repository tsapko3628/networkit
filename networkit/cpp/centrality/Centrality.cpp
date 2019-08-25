/*
 * Centrality.cpp
 *
 *  Created on: 19.02.2014
 *      Author: cls
 */

#include <networkit/centrality/Centrality.hpp>
#include <networkit/auxiliary/Parallel.hpp>

namespace NetworKit {

Centrality::Centrality(const Graph &G, bool normalized,
                       bool computeEdgeCentrality)
    : Algorithm(), G(G), normalized(normalized),
      computeEdgeCentrality(computeEdgeCentrality) {
  if (computeEdgeCentrality && !G.hasEdgeIds()) {
    throw std::runtime_error("For edge centralities to be computed, edges must "
                             "be indexed first: call G.indexEdges()");
  }
}

double Centrality::score(node v) {
  assureFinished();
  return scoreData.at(v);
}

std::vector<std::pair<node, double>> Centrality::ranking() {
  assureFinished();
  std::vector<std::pair<node, double>> ranking;
  G.forNodes([&](node v) { ranking.push_back({v, scoreData[v]}); });
  Aux::Parallel::sort(ranking.begin(), ranking.end(),
                      [](std::pair<node, double> x, std::pair<node, double> y) {
                        if (x.second == y.second) {
                          return x.first < y.first;
                        }
                        return x.second > y.second;
                      });
  return ranking;
}

std::vector<std::pair<node, double>> Centrality::lengthRanking() {
  assureFinished();
  std::vector<std::pair<node, double>> ranking;
  G.forNodes([&](node v) { ranking.push_back({v, lengthScaled[v]}); });
  Aux::Parallel::sort(ranking.begin(), ranking.end(),
                      [](std::pair<node, double> x, std::pair<node, double> y) {
                        if (x.second == y.second) {
                          return x.first < y.first;
                        }
                        return x.second > y.second;
                      });
  return ranking;
}

std::vector<double> Centrality::scores(bool moveOut) {
  assureFinished();
  hasRun = !moveOut;
  return moveOut ? std::move(scoreData) : scoreData;
}

std::vector<double> Centrality::lengthScaleScores(bool moveOut) {
  assureFinished();
  hasRun = !moveOut;
  return moveOut ? std::move(lengthScaled) : lengthScaled;
}

std::vector<double> Centrality::edgeScores() {
  assureFinished();
  return edgeScoreData;
}

double Centrality::maximum() {
  throw std::runtime_error("Not implemented: Compute the maximum centrality "
                           "score in the respective centrality subclass.");
}

double Centrality::centralization() {
  assureFinished();
  double centerScore = 0.0;
  G.forNodes([&](node v) {
    if (scoreData[v] > centerScore) {
      centerScore = scoreData[v];
    }
  });
  INFO("center score: ", centerScore);
  double maxScore = maximum();
  double diff1 = 0.0;
  double diff2 = 0.0;
  G.forNodes([&](node v) {
    diff1 += (centerScore - scoreData[v]);
    diff2 += (maxScore - scoreData[v]);
  });
  return diff1 / diff2;
}

} /* namespace NetworKit */
