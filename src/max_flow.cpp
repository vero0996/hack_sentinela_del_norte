
#include "max_flow.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace sentinela {
namespace {

struct ResidualEdge {
    int to;
    int reverseIndex;
    long long capacity;
};

class Dinic {
public:
    explicit Dinic(int n)
        : graph_(n), level_(n), nextEdge_(n) {}

    void addEdge(int from, int to, long long capacity) {
        ResidualEdge forward{
            to, static_cast<int>(graph_[to].size()), capacity
        };
        ResidualEdge reverse{
            from, static_cast<int>(graph_[from].size()), 0
        };

        graph_[from].push_back(forward);
        graph_[to].push_back(reverse);
    }

    long long solve(int source, int sink) {
        long long totalFlow = 0;

        while (buildLevels(source, sink)) {
            std::fill(nextEdge_.begin(), nextEdge_.end(), 0);

            while (true) {
                const long long pushed = sendFlow(
                    source, sink, std::numeric_limits<long long>::max()
                );

                if (pushed == 0) {
                    break;
                }

                if (totalFlow >
                    std::numeric_limits<long long>::max() - pushed) {
                    throw std::overflow_error("El flujo maximo excede long long");
                }

                totalFlow += pushed;
            }
        }

        return totalFlow;
    }

private:
    std::vector<std::vector<ResidualEdge>> graph_;
    std::vector<int> level_;
    std::vector<std::size_t> nextEdge_;

    bool buildLevels(int source, int sink) {
        std::fill(level_.begin(), level_.end(), -1);
        std::queue<int> pending;

        level_[source] = 0;
        pending.push(source);

        while (!pending.empty()) {
            const int u = pending.front();
            pending.pop();

            for (const auto& edge : graph_[u]) {
                if (edge.capacity > 0 && level_[edge.to] == -1) {
                    level_[edge.to] = level_[u] + 1;
                    pending.push(edge.to);
                }
            }
        }

        return level_[sink] != -1;
    }

    long long sendFlow(int u, int sink, long long available) {
        if (u == sink) {
            return available;
        }

        for (std::size_t& i = nextEdge_[u];
             i < graph_[u].size(); ++i) {
            ResidualEdge& edge = graph_[u][i];

            if (edge.capacity <= 0 ||
                level_[edge.to] != level_[u] + 1) {
                continue;
            }

            const long long pushed = sendFlow(
                edge.to, sink, std::min(available, edge.capacity)
            );

            if (pushed == 0) {
                continue;
            }

            edge.capacity -= pushed;
            graph_[edge.to][edge.reverseIndex].capacity += pushed;

            return pushed;
        }

        return 0;
    }
};

}  // namespace

MaxFlowResult maxFlow(
    int n,
    const std::vector<FlowEdge>& edges,
    int source,
    int sink
) {
    if (n <= 0) {
        throw std::invalid_argument("El numero de nodos debe ser positivo");
    }

    if (source < 0 || source >= n || sink < 0 || sink >= n) {
        throw std::invalid_argument("Source o sink fuera de rango");
    }

    for (const auto& edge : edges) {
        if (edge.from < 0 || edge.from >= n ||
            edge.to < 0 || edge.to >= n) {
            throw std::invalid_argument("Extremo de arista fuera de rango");
        }

        if (edge.capacity < 0) {
            throw std::invalid_argument("Las capacidades no pueden ser negativas");
        }
    }

    MaxFlowResult result;
    result.edges = edges;

    if (source == sink) {
        result.flow = 0;
        return result;
    }

    Dinic dinic(n);

    for (const auto& edge : edges) {
        dinic.addEdge(edge.from, edge.to, edge.capacity);
    }

    result.flow = dinic.solve(source, sink);
    return result;
}

}  // namespace sentinela
