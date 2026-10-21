#ifndef SENTINEL_MST_H
#define SENTINEL_MST_H

#include <algorithm>
#include <numeric>
#include <vector>

namespace sentinel {

struct MSTEdge {
    int u;
    int v;
    long long weight;
};

struct MSTResult {
    long long total_cost = 0;
    std::vector<MSTEdge> edges;
    bool connected = false;
};

class DSU {
private:
    std::vector<int> parent;
    std::vector<int> rank_;

public:
    explicit DSU(int n) : parent(n), rank_(n, 0) {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int find(int x) {
        if (parent[x] != x) {
            parent[x] = find(parent[x]);
        }
        return parent[x];
    }

    bool unite(int a, int b) {
        a = find(a);
        b = find(b);

        if (a == b) {
            return false;
        }

        if (rank_[a] < rank_[b]) {
            std::swap(a, b);
        }

        parent[b] = a;

        if (rank_[a] == rank_[b]) {
            ++rank_[a];
        }

        return true;
    }
};

inline MSTResult kruskalMST(int n, std::vector<MSTEdge> edges) {
    MSTResult result;

    if (n < 0) {
        return result;
    }

    if (n == 0) {
        result.connected = true;
        return result;
    }

    std::sort(edges.begin(), edges.end(),
        [](const MSTEdge& a, const MSTEdge& b) {
            if (a.weight != b.weight) {
                return a.weight < b.weight;
            }
            if (a.u != b.u) {
                return a.u < b.u;
            }
            return a.v < b.v;
        });

    DSU dsu(n);

    for (const MSTEdge& edge : edges) {
        if (edge.u < 0 || edge.u >= n ||
            edge.v < 0 || edge.v >= n ||
            edge.u == edge.v) {
            continue;
        }

        if (dsu.unite(edge.u, edge.v)) {
            result.edges.push_back(edge);
            result.total_cost += edge.weight;

            if (static_cast<int>(result.edges.size()) == n - 1) {
                break;
            }
        }
    }

    result.connected =
        (n == 1 || static_cast<int>(result.edges.size()) == n - 1);

    return result;
}

} // namespace sentinel

#endif
