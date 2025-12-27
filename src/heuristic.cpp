// =============================================================================
// heuristic.cpp - 启发式生成初始解
// =============================================================================

#include "2DBP.h"

using namespace std;

// 启发式生成初始可行解
// 使用对角矩阵策略: 每个Y列只使用一种条带, 每个X列只切割一种子件
void RunHeuristic(ProblemParams& params, ProblemData& data, BPNode& root_node) {
    int num_strip_types = params.num_strip_types_;
    int num_item_types = params.num_item_types_;

    LOG("[启发式] 生成初始解");

    // 生成初始Y列 (母板切割方案)
    // 每个Y列对应一种条带类型, 只切割一个该类型条带
    params.init_y_matrix_.clear();
    root_node.y_columns_.clear();

    for (int j = 0; j < num_strip_types; j++) {
        vector<int> pattern(num_strip_types, 0);
        pattern[j] = 1;  // 只切一个条带类型j

        params.init_y_matrix_.push_back(pattern);

        YColumn y_col;
        y_col.pattern_ = pattern;
        root_node.y_columns_.push_back(y_col);
    }

    LOG_FMT("  生成Y列数: %d\n", num_strip_types);

    // 生成初始X列 (条带切割方案)
    // 对每种条带类型, 生成一个X列切割一种可以放入该条带的子件
    params.init_x_matrix_.clear();
    root_node.x_columns_.clear();

    for (int j = 0; j < num_strip_types; j++) {
        int strip_width = data.strip_types_[j].width_;

        // 找到该条带可以容纳的子件类型
        for (int i = 0; i < num_item_types; i++) {
            int item_width = data.item_types_[i].width_;

            // 子件宽度必须小于等于条带宽度
            if (item_width <= strip_width) {
                vector<int> pattern(num_item_types, 0);
                pattern[i] = 1;  // 只切一个子件类型i

                params.init_x_matrix_.push_back(pattern);

                XColumn x_col;
                x_col.strip_type_id_ = j;
                x_col.pattern_ = pattern;
                root_node.x_columns_.push_back(x_col);

                break;  // 每种条带类型只生成一个初始X列
            }
        }
    }

    LOG_FMT("  生成X列数: %d\n", (int)root_node.x_columns_.size());

    // 构建完整模型矩阵
    // 矩阵结构:
    //   行0~J-1: 条带平衡约束 (sum Y*C_j - sum X*D_j >= 0)
    //   行J~J+N-1: 子件需求约束 (sum X*B_i >= demand_i)
    // Y列结构: [C_1...C_J, 0...0]
    // X列结构: [-1(位置j), B_1...B_N]
    root_node.matrix_.clear();

    int num_rows = num_strip_types + num_item_types;

    // 添加Y列到矩阵
    for (int col = 0; col < (int)root_node.y_columns_.size(); col++) {
        vector<double> col_data;

        // 条带产出部分 (C矩阵)
        for (int j = 0; j < num_strip_types; j++) {
            col_data.push_back(root_node.y_columns_[col].pattern_[j]);
        }

        // 子件产出部分 (0矩阵)
        for (int i = 0; i < num_item_types; i++) {
            col_data.push_back(0);
        }

        root_node.matrix_.push_back(col_data);
    }

    // 添加X列到矩阵
    for (int col = 0; col < (int)root_node.x_columns_.size(); col++) {
        vector<double> col_data;
        int strip_type = root_node.x_columns_[col].strip_type_id_;

        // 条带消耗部分 (D矩阵: 位置strip_type为-1)
        for (int j = 0; j < num_strip_types; j++) {
            col_data.push_back((j == strip_type) ? -1 : 0);
        }

        // 子件产出部分 (B矩阵)
        for (int i = 0; i < num_item_types; i++) {
            col_data.push_back(root_node.x_columns_[col].pattern_[i]);
        }

        root_node.matrix_.push_back(col_data);
    }

    LOG("[启发式] 初始解生成完成");
}
