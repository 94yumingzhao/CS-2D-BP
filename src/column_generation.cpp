// =============================================================================
// column_generation.cpp - 列生成方法选择
// =============================================================================

#include "2DBP.h"

using namespace std;

// 根节点SP1方法选择
// 根据设置选择使用CPLEX/Arc Flow/DP求解SP1
bool SolveRootSP1(ProblemParams& params, ProblemData& data, BPNode& node) {
    int method = node.sp1_method_;

    switch (method) {
        case kArcFlow:
            return SolveRootSP1ArcFlow(params, data, node);
        case kDP:
            return SolveRootSP1DP(params, data, node);
        case kCplexIP:
        default:
            return SolveRootSP1Knapsack(params, data, node);
    }
}

// 根节点SP2方法选择
// 根据设置选择使用CPLEX/Arc Flow/DP求解SP2
bool SolveRootSP2(ProblemParams& params, ProblemData& data,
    BPNode& node, int strip_type_id) {

    int method = node.sp2_method_;

    switch (method) {
        case kArcFlow:
            return SolveRootSP2ArcFlow(params, data, node, strip_type_id);
        case kDP:
            return SolveRootSP2DP(params, data, node, strip_type_id);
        case kCplexIP:
        default:
            return SolveRootSP2Knapsack(params, data, node, strip_type_id);
    }
}

// 非根节点SP1方法选择
bool SolveNodeSP1(ProblemParams& params, ProblemData& data, BPNode* node) {
    int method = node->sp1_method_;

    switch (method) {
        case kArcFlow:
            return SolveNodeSP1ArcFlow(params, data, node);
        case kDP:
            return SolveNodeSP1DP(params, data, node);
        case kCplexIP:
        default:
            return SolveNodeSP1Knapsack(params, data, node);
    }
}

// 非根节点SP2方法选择
bool SolveNodeSP2(ProblemParams& params, ProblemData& data,
    BPNode* node, int strip_type_id) {

    int method = node->sp2_method_;

    switch (method) {
        case kArcFlow:
            return SolveNodeSP2ArcFlow(params, data, node, strip_type_id);
        case kDP:
            return SolveNodeSP2DP(params, data, node, strip_type_id);
        case kCplexIP:
        default:
            return SolveNodeSP2Knapsack(params, data, node, strip_type_id);
    }
}
