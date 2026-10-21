
#ifndef MAX_FLOW_H
#define MAX_FLOW_H

#include <vector>

namespace sentinela {

struct FlowEdge {
    int from;
    int to;
    long long capacity;
};

struct MaxFlowResult {
    long long flow = 0;
    std::vector<FlowEdge> edges;
};

// Calcula el flujo maximo desde source hasta sink.
// Los nodos se identifican con indices de 0 a n - 1.
// Las capacidades deben ser no negativas.
// Si source == sink, el flujo maximo se define como 0.
MaxFlowResult maxFlow(
    int n,
    const std::vector<FlowEdge>& edges,
    int source,
    int sink
);

}  // namespace sentinela

#endif  // MAX_FLOW_H
