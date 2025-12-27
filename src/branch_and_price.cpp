// =============================================================================
// branch_and_price.cpp - 分支定价算法
// =============================================================================

#include "2DBP.h"

using namespace std;

// 检查解是否为整数解
bool IsIntegerSolution(NodeSolution& solution) {
    // 检查Y列
    for (int i = 0; i < (int)solution.y_columns_.size(); i++) {
        double val = solution.y_columns_[i].value_;
        if (val > kZeroTolerance) {
            double frac = val - floor(val);
            if (frac > kZeroTolerance && frac < 1 - kZeroTolerance) {
                return false;
            }
        }
    }

    // 检查X列
    for (int i = 0; i < (int)solution.x_columns_.size(); i++) {
        double val = solution.x_columns_[i].value_;
        if (val > kZeroTolerance) {
            double frac = val - floor(val);
            if (frac > kZeroTolerance && frac < 1 - kZeroTolerance) {
                return false;
            }
        }
    }

    return true;
}

// 选择分支变量
// 返回: 待分支变量索引, -1表示无需分支
int SelectBranchVar(BPNode* node) {
    double max_frac = 0;
    int branch_idx = -1;

    // 检查Y列
    int y_count = node->solution_.y_columns_.size();
    for (int i = 0; i < y_count; i++) {
        double val = node->solution_.y_columns_[i].value_;
        if (val > kZeroTolerance) {
            double frac = val - floor(val);
            if (frac > kZeroTolerance && frac < 1 - kZeroTolerance) {
                if (frac > max_frac) {
                    max_frac = frac;
                    branch_idx = i;
                    node->branch_var_val_ = val;
                }
            }
        }
    }

    // 检查X列
    for (int i = 0; i < (int)node->solution_.x_columns_.size(); i++) {
        double val = node->solution_.x_columns_[i].value_;
        if (val > kZeroTolerance) {
            double frac = val - floor(val);
            if (frac > kZeroTolerance && frac < 1 - kZeroTolerance) {
                if (frac > max_frac) {
                    max_frac = frac;
                    branch_idx = y_count + i;
                    node->branch_var_val_ = val;
                }
            }
        }
    }

    if (branch_idx >= 0) {
        node->branch_var_id_ = branch_idx;
        node->branch_floor_ = floor(node->branch_var_val_);
        node->branch_ceil_ = ceil(node->branch_var_val_);
    }

    return branch_idx;
}

// 创建左子节点 (向下取整分支)
void CreateLeftChild(BPNode* parent, int new_id, BPNode* child) {
    // 复制基本信息
    child->id_ = new_id;
    child->parent_id_ = parent->id_;
    child->branch_dir_ = 1;  // 左分支
    child->sp1_method_ = parent->sp1_method_;
    child->sp2_method_ = parent->sp2_method_;

    // 复制列集合
    child->y_columns_ = parent->y_columns_;
    child->x_columns_ = parent->x_columns_;

    // 继承父节点的分支约束
    child->branched_var_ids_ = parent->branched_var_ids_;
    child->branched_bounds_ = parent->branched_bounds_;

    // 添加新的分支约束 (x <= floor)
    child->branched_var_ids_.push_back(parent->branch_var_id_);
    child->branched_bounds_.push_back(parent->branch_floor_);

    LOG_FMT("[Branch] 创建左子节点 %d (var_%d <= %.0f)\n",
        new_id, parent->branch_var_id_, parent->branch_floor_);
}

// 创建右子节点 (向上取整分支)
void CreateRightChild(BPNode* parent, int new_id, BPNode* child) {
    // 复制基本信息
    child->id_ = new_id;
    child->parent_id_ = parent->id_;
    child->branch_dir_ = 2;  // 右分支
    child->sp1_method_ = parent->sp1_method_;
    child->sp2_method_ = parent->sp2_method_;

    // 复制列集合
    child->y_columns_ = parent->y_columns_;
    child->x_columns_ = parent->x_columns_;

    // 继承父节点的分支约束
    child->branched_var_ids_ = parent->branched_var_ids_;
    child->branched_bounds_ = parent->branched_bounds_;

    // 添加新的分支约束 (x >= ceil, 实现为将其他分支的变量设为0)
    // 简化处理: 右分支固定变量为ceil值
    child->branched_var_ids_.push_back(parent->branch_var_id_);
    child->branched_bounds_.push_back(parent->branch_ceil_);

    LOG_FMT("[Branch] 创建右子节点 %d (var_%d >= %.0f)\n",
        new_id, parent->branch_var_id_, parent->branch_ceil_);
}

// 选择待分支节点 (从链表中选择下界最小的未剪枝节点)
BPNode* SelectBranchNode(BPNode* head) {
    BPNode* best = nullptr;
    double best_lb = INFINITY;

    BPNode* curr = head;
    while (curr != nullptr) {
        if (curr->prune_flag_ == 0 && curr->branched_flag_ == 0) {
            if (curr->lower_bound_ < best_lb) {
                best_lb = curr->lower_bound_;
                best = curr;
            }
        }
        curr = curr->next_;
    }

    return best;
}

// 分支定价主循环
int RunBranchAndPrice(ProblemParams& params, ProblemData& data, BPNode* root) {
    LOG("[BP] 分支定价开始");

    // 初始化节点链表
    BPNode* head = root;
    BPNode* tail = root;
    int node_count = 1;

    // 检查根节点是否为整数解
    if (IsIntegerSolution(root->solution_)) {
        params.global_best_int_ = root->solution_.obj_val_;
        params.global_best_y_cols_ = root->solution_.y_columns_;
        params.global_best_x_cols_ = root->solution_.x_columns_;
        LOG("[BP] 根节点即为整数解");
        return 0;
    }

    // 选择根节点的分支变量
    SelectBranchVar(root);

    // 分支定价主循环
    while (true) {
        // 选择待分支节点
        BPNode* parent = SelectBranchNode(head);

        if (parent == nullptr) {
            LOG("[BP] 无可分支节点, 搜索完成");
            break;
        }

        LOG_FMT("[BP] 选择节点 %d 进行分支 (LB=%.4f)\n",
            parent->id_, parent->lower_bound_);

        // 创建左子节点
        BPNode* left = new BPNode();
        node_count++;
        params.node_counter_++;
        CreateLeftChild(parent, params.node_counter_, left);

        // 求解左子节点
        SolveNodeCG(params, data, left);

        // 将左子节点加入链表
        tail->next_ = left;
        tail = left;

        // 检查左子节点
        if (left->prune_flag_ == 0) {
            if (IsIntegerSolution(left->solution_)) {
                // 更新全局最优整数解
                if (left->solution_.obj_val_ < params.global_best_int_) {
                    params.global_best_int_ = left->solution_.obj_val_;
                    params.global_best_y_cols_ = left->solution_.y_columns_;
                    params.global_best_x_cols_ = left->solution_.x_columns_;
                    LOG_FMT("[BP] 找到新整数解, 目标值=%.4f\n", params.global_best_int_);
                }
                left->branched_flag_ = 1;  // 无需再分支
            } else {
                SelectBranchVar(left);
            }
        }

        // 创建右子节点
        BPNode* right = new BPNode();
        node_count++;
        params.node_counter_++;
        CreateRightChild(parent, params.node_counter_, right);

        // 求解右子节点
        SolveNodeCG(params, data, right);

        // 将右子节点加入链表
        tail->next_ = right;
        tail = right;

        // 检查右子节点
        if (right->prune_flag_ == 0) {
            if (IsIntegerSolution(right->solution_)) {
                if (right->solution_.obj_val_ < params.global_best_int_) {
                    params.global_best_int_ = right->solution_.obj_val_;
                    params.global_best_y_cols_ = right->solution_.y_columns_;
                    params.global_best_x_cols_ = right->solution_.x_columns_;
                    LOG_FMT("[BP] 找到新整数解, 目标值=%.4f\n", params.global_best_int_);
                }
                right->branched_flag_ = 1;
            } else {
                SelectBranchVar(right);
            }
        }

        // 标记父节点已分支
        parent->branched_flag_ = 1;

        // 剪枝: 下界 >= 全局最优整数解
        BPNode* curr = head;
        while (curr != nullptr) {
            if (curr->prune_flag_ == 0 && curr->branched_flag_ == 0) {
                if (curr->lower_bound_ >= params.global_best_int_ - kZeroTolerance) {
                    curr->prune_flag_ = 1;
                    LOG_FMT("[BP] 节点 %d 被剪枝 (LB=%.4f >= UB=%.4f)\n",
                        curr->id_, curr->lower_bound_, params.global_best_int_);
                }
            }
            curr = curr->next_;
        }

        // 安全检查
        if (node_count > 100) {
            LOG("[BP] 达到最大节点数, 强制终止");
            break;
        }
    }

    // 计算最优性间隙
    double best_lb = INFINITY;
    BPNode* curr = head;
    while (curr != nullptr) {
        if (curr->prune_flag_ == 0 && curr->lower_bound_ < best_lb) {
            best_lb = curr->lower_bound_;
        }
        curr = curr->next_;
    }

    if (params.global_best_int_ < INFINITY && best_lb < INFINITY) {
        params.gap_ = (params.global_best_int_ - best_lb) / params.global_best_int_;
    }

    LOG_FMT("[BP] 分支定价结束, 最优解=%.4f, 间隙=%.2f%%\n",
        params.global_best_int_, params.gap_ * 100);

    return 0;
}
