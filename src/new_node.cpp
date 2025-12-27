// =============================================================================
// new_node.cpp - 非根节点列生成和主问题
// =============================================================================

#include "2DBP.h"

using namespace std;

// 非根节点列生成主循环
int SolveNodeCG(ProblemParams& params, ProblemData& data, BPNode* node) {
    LOG_FMT("[CG] 节点%d 列生成开始\n", node->id_);

    // 继承全局SP方法设置
    node->sp1_method_ = params.sp1_method_;
    node->sp2_method_ = params.sp2_method_;

    // 初始化CPLEX环境
    IloEnv env;
    IloModel model(env);
    IloObjective obj = IloAdd(model, IloMinimize(env));
    IloNumVarArray vars(env);
    IloRangeArray cons(env);

    node->iter_ = 0;

    // 求解初始主问题
    bool feasible = SolveNodeInitMP(params, data, env, model, obj, cons, vars, node);

    if (!feasible) {
        // 节点不可行, 标记剪枝
        node->prune_flag_ = 1;
        env.end();
        LOG_FMT("[CG] 节点%d 不可行, 剪枝\n", node->id_);
        return -1;
    }

    // 列生成主循环
    while (true) {
        node->iter_++;

        if (node->iter_ >= kMaxCgIter) {
            LOG_FMT("[CG] 达到最大迭代次数 %d, 终止\n", kMaxCgIter);
            break;
        }

        // 求解子问题SP1
        bool sp1_converged = SolveNodeSP1(params, data, node);

        if (sp1_converged) {
            // SP1收敛, 尝试SP2
            bool all_sp2_converged = true;

            for (int j = 0; j < params.num_strip_types_; j++) {
                bool sp2_converged = SolveNodeSP2(params, data, node, j);

                if (!sp2_converged) {
                    all_sp2_converged = false;
                    SolveNodeUpdateMP(params, data, env, model, obj, cons, vars, node);
                }
            }

            if (all_sp2_converged) {
                LOG_FMT("[CG] 列生成收敛, 迭代%d次\n", node->iter_);
                break;
            }
        } else {
            // SP1找到改进列
            SolveNodeUpdateMP(params, data, env, model, obj, cons, vars, node);
        }
    }

    // 求解最终主问题
    SolveNodeFinalMP(params, data, env, model, obj, cons, vars, node);

    // 释放资源
    obj.end();
    vars.end();
    cons.end();
    model.end();
    env.end();

    LOG_FMT("[CG] 节点%d 列生成结束, 下界=%.4f\n", node->id_, node->lower_bound_);
    return 0;
}

// 非根节点初始主问题
bool SolveNodeInitMP(ProblemParams& params, ProblemData& data,
    IloEnv& env, IloModel& model, IloObjective& obj,
    IloRangeArray& cons, IloNumVarArray& vars, BPNode* node) {

    int num_y_cols = static_cast<int>(node->y_columns_.size());
    int num_x_cols = static_cast<int>(node->x_columns_.size());
    int num_strip_types = params.num_strip_types_;
    int num_item_types = params.num_item_types_;
    int num_rows = num_strip_types + num_item_types;

    LOG_FMT("[MP-0] 节点%d 构建初始主问题 (Y=%d, X=%d)\n",
        node->id_, num_y_cols, num_x_cols);

    // 构建约束
    IloNumArray con_min(env);
    IloNumArray con_max(env);

    for (int j = 0; j < num_strip_types; j++) {
        con_min.add(0);
        con_max.add(IloInfinity);
    }

    for (int i = 0; i < num_item_types; i++) {
        con_min.add(data.item_types_[i].demand_);
        con_max.add(IloInfinity);
    }

    cons = IloRangeArray(env, con_min, con_max);
    model.add(cons);
    con_min.end();
    con_max.end();

    // 添加Y变量
    for (int col = 0; col < num_y_cols; col++) {
        IloNumColumn cplex_col = obj(1.0);

        for (int j = 0; j < num_strip_types; j++) {
            cplex_col += cons[j](node->y_columns_[col].pattern_[j]);
        }
        for (int i = 0; i < num_item_types; i++) {
            cplex_col += cons[num_strip_types + i](0);
        }

        // 检查分支约束
        double var_ub = IloInfinity;
        for (int k = 0; k < (int)node->branched_var_ids_.size(); k++) {
            if (node->branched_var_ids_[k] == col) {
                var_ub = node->branched_bounds_[k];
                break;
            }
        }

        string var_name = "Y_" + to_string(col + 1);
        IloNumVar var(cplex_col, 0, var_ub, ILOFLOAT, var_name.c_str());
        vars.add(var);
        cplex_col.end();
    }

    // 添加X变量
    for (int col = 0; col < num_x_cols; col++) {
        IloNumColumn cplex_col = obj(0.0);
        int strip_type = node->x_columns_[col].strip_type_id_;

        for (int j = 0; j < num_strip_types; j++) {
            cplex_col += cons[j]((j == strip_type) ? -1 : 0);
        }
        for (int i = 0; i < num_item_types; i++) {
            cplex_col += cons[num_strip_types + i](node->x_columns_[col].pattern_[i]);
        }

        // 检查分支约束
        int var_idx = num_y_cols + col;
        double var_ub = IloInfinity;
        for (int k = 0; k < (int)node->branched_var_ids_.size(); k++) {
            if (node->branched_var_ids_[k] == var_idx) {
                var_ub = node->branched_bounds_[k];
                break;
            }
        }

        string var_name = "X_" + to_string(col + 1);
        IloNumVar var(cplex_col, 0, var_ub, ILOFLOAT, var_name.c_str());
        vars.add(var);
        cplex_col.end();
    }

    // 求解
    IloCplex cplex(env);
    cplex.extract(model);
    cplex.setOut(env.getNullStream());
    bool feasible = cplex.solve();

    if (!feasible) {
        LOG("[MP] 初始主问题不可行");
        cplex.end();
        return false;
    }

    double obj_val = cplex.getValue(obj);
    LOG_FMT("[MP] 目标值: %.4f\n", obj_val);

    // 提取对偶价格
    node->duals_.clear();
    for (int row = 0; row < num_rows; row++) {
        double dual = cplex.getDual(cons[row]);
        if (dual == -0.0) dual = 0.0;
        node->duals_.push_back(dual);
    }

    cplex.end();
    return true;
}

// 更新非根节点主问题
bool SolveNodeUpdateMP(ProblemParams& params, ProblemData& data,
    IloEnv& env, IloModel& model, IloObjective& obj,
    IloRangeArray& cons, IloNumVarArray& vars, BPNode* node) {

    int num_strip_types = params.num_strip_types_;
    int num_item_types = params.num_item_types_;

    // 添加新Y列
    if (!node->new_y_col_.pattern_.empty()) {
        IloNumColumn cplex_col = obj(1.0);

        for (int j = 0; j < num_strip_types; j++) {
            cplex_col += cons[j](node->new_y_col_.pattern_[j]);
        }
        for (int i = 0; i < num_item_types; i++) {
            cplex_col += cons[num_strip_types + i](0);
        }

        int col_id = static_cast<int>(node->y_columns_.size()) + 1;
        string var_name = "Y_" + to_string(col_id);
        IloNumVar var(cplex_col, 0, IloInfinity, ILOFLOAT, var_name.c_str());
        vars.add(var);
        cplex_col.end();

        YColumn y_col;
        y_col.pattern_ = node->new_y_col_.pattern_;
        node->y_columns_.push_back(y_col);
        node->new_y_col_.pattern_.clear();
    }

    // 添加新X列
    if (!node->new_x_col_.pattern_.empty()) {
        int strip_type = node->new_strip_type_;
        IloNumColumn cplex_col = obj(0.0);

        for (int j = 0; j < num_strip_types; j++) {
            cplex_col += cons[j]((j == strip_type) ? -1 : 0);
        }
        for (int i = 0; i < num_item_types; i++) {
            cplex_col += cons[num_strip_types + i](node->new_x_col_.pattern_[i]);
        }

        int col_id = static_cast<int>(node->x_columns_.size()) + 1;
        string var_name = "X_" + to_string(col_id);
        IloNumVar var(cplex_col, 0, IloInfinity, ILOFLOAT, var_name.c_str());
        vars.add(var);
        cplex_col.end();

        XColumn x_col;
        x_col.strip_type_id_ = strip_type;
        x_col.pattern_ = node->new_x_col_.pattern_;
        node->x_columns_.push_back(x_col);
        node->new_x_col_.pattern_.clear();
    }

    // 求解
    LOG_FMT("[MP-%d] 更新并求解主问题\n", node->iter_);
    IloCplex cplex(env);
    cplex.extract(model);
    cplex.setOut(env.getNullStream());
    bool feasible = cplex.solve();

    if (!feasible) {
        LOG("[MP] 更新后主问题不可行");
        cplex.end();
        return false;
    }

    double obj_val = cplex.getValue(obj);
    LOG_FMT("[MP] 目标值: %.4f\n", obj_val);

    // 提取对偶价格
    node->duals_.clear();
    int num_rows = num_strip_types + num_item_types;
    for (int row = 0; row < num_rows; row++) {
        double dual = cplex.getDual(cons[row]);
        if (dual == -0.0) dual = 0.0;
        node->duals_.push_back(dual);
    }

    cplex.end();
    return true;
}

// 求解非根节点最终主问题
bool SolveNodeFinalMP(ProblemParams& params, ProblemData& data,
    IloEnv& env, IloModel& model, IloObjective& obj,
    IloRangeArray& cons, IloNumVarArray& vars, BPNode* node) {

    LOG_FMT("[MP-Final] 节点%d 求解最终主问题\n", node->id_);

    IloCplex cplex(env);
    cplex.extract(model);
    cplex.setOut(env.getNullStream());
    bool feasible = cplex.solve();

    if (!feasible) {
        LOG("[MP] 最终主问题不可行");
        node->prune_flag_ = 1;
        cplex.end();
        return false;
    }

    double obj_val = cplex.getValue(obj);
    node->lower_bound_ = obj_val;
    node->solution_.obj_val_ = obj_val;

    LOG_FMT("[MP] 最终目标值: %.4f\n", obj_val);

    // 提取解
    node->solution_.y_columns_.clear();
    for (int col = 0; col < (int)node->y_columns_.size(); col++) {
        double val = cplex.getValue(vars[col]);
        if (fabs(val) < kZeroTolerance) val = 0;

        YColumn y_col = node->y_columns_[col];
        y_col.value_ = val;
        node->solution_.y_columns_.push_back(y_col);
    }

    node->solution_.x_columns_.clear();
    int y_count = static_cast<int>(node->y_columns_.size());
    for (int col = 0; col < static_cast<int>(node->x_columns_.size()); col++) {
        double val = cplex.getValue(vars[y_count + col]);
        if (fabs(val) < kZeroTolerance) val = 0;

        XColumn x_col = node->x_columns_[col];
        x_col.value_ = val;
        node->solution_.x_columns_.push_back(x_col);
    }

    cplex.end();
    return true;
}
