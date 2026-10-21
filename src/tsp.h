#ifndef TSP_H
#define TSP_H

#include <vector>

namespace sentinela {

struct TSPResult {
    bool feasible = false;
    long long cost = 0;
    std::vector<int> tour;
};

TSPResult solveTSP(const std::vector<std::vector<long long>>& graph);

}  // namespace sentinela

#endif