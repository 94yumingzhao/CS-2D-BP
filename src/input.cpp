// =============================================================================
// input.cpp - 数据读取和工具函数
// =============================================================================

#include "2DBP.h"

using namespace std;

// 字符串分割
void SplitString(const string& str, vector<string>& result, const string& delimiter) {
    string::size_type start_pos = 0;
    string::size_type delim_pos = str.find(delimiter);
    result.clear();

    while (string::npos != delim_pos) {
        result.push_back(str.substr(start_pos, delim_pos - start_pos));
        start_pos = delim_pos + delimiter.size();
        delim_pos = str.find(delimiter, start_pos);
    }

    if (start_pos != str.length()) {
        result.push_back(str.substr(start_pos));
    }
}

// 读取问题数据
// 返回: (状态码, 子件类型数, 条带类型数)
tuple<int, int, int> LoadInput(ProblemParams& params, ProblemData& data) {
    string line;
    vector<string> tokens;

    LOG_FMT("[数据] 读取文件: %s\n", kFilePath.c_str());

    ifstream fin(kFilePath.c_str());
    if (!fin) {
        LOG_FMT("[错误] 无法打开文件: %s\n", kFilePath.c_str());
        return make_tuple(-1, 0, 0);
    }

    // 第1行: 母板长度 (暂不使用)
    getline(fin, line);

    // 第2行: 子件类型数量
    getline(fin, line);
    SplitString(line, tokens, "\t");
    params.num_item_types_ = stoi(tokens[0]);

    // 第3行: 母板尺寸 (长度 宽度)
    getline(fin, line);
    SplitString(line, tokens, "\t");
    params.stock_length_ = stoi(tokens[0]);
    params.stock_width_ = stoi(tokens[1]);

    LOG_FMT("[数据] 母板尺寸: %d x %d\n", params.stock_length_, params.stock_width_);
    LOG_FMT("[数据] 子件类型数: %d\n", params.num_item_types_);

    // 读取子件数据
    int total_demand = 0;
    set<int> unique_widths;

    for (int i = 0; i < params.num_item_types_; i++) {
        getline(fin, line);
        SplitString(line, tokens, "\t");

        ItemType item_type;
        item_type.type_id_ = i;
        item_type.length_ = stoi(tokens[0]);
        item_type.width_ = stoi(tokens[1]);
        item_type.demand_ = stoi(tokens[2]);

        data.item_types_.push_back(item_type);
        total_demand += item_type.demand_;
        unique_widths.insert(item_type.width_);
    }
    fin.close();

    // 条带类型数量等于不同宽度的数量
    params.num_strip_types_ = static_cast<int>(unique_widths.size());
    params.num_items_ = total_demand;

    LOG_FMT("[数据] 子件总需求: %d\n", total_demand);
    LOG_FMT("[数据] 条带类型数: %d\n", params.num_strip_types_);

    // 创建条带类型 (按宽度降序)
    vector<int> widths(unique_widths.begin(), unique_widths.end());
    sort(widths.begin(), widths.end(), greater<int>());

    for (int i = 0; i < params.num_strip_types_; i++) {
        StripType strip_type;
        strip_type.type_id_ = i;
        strip_type.width_ = widths[i];
        strip_type.length_ = params.stock_length_;
        data.strip_types_.push_back(strip_type);
        data.strip_widths_.push_back(widths[i]);
    }

    // 构建索引映射
    BuildLengthIndex(data);
    BuildWidthIndex(data);

    LOG("[数据] 数据读取完成");
    return make_tuple(0, params.num_item_types_, params.num_strip_types_);
}

// 构建长度到子件类型的索引
void BuildLengthIndex(ProblemData& data) {
    data.item_lengths_.clear();
    data.length_to_item_index_.clear();

    for (int i = 0; i < (int)data.item_types_.size(); i++) {
        int len = data.item_types_[i].length_;
        data.length_to_item_index_[len] = i;
        data.item_lengths_.push_back(len);
    }

    // 按降序排序长度
    sort(data.item_lengths_.begin(), data.item_lengths_.end(), greater<int>());
}

// 构建宽度到条带类型和子件类型的索引
void BuildWidthIndex(ProblemData& data) {
    data.width_to_strip_index_.clear();
    data.width_to_item_indices_.clear();

    // 宽度到条带类型索引
    for (int i = 0; i < (int)data.strip_types_.size(); i++) {
        int wid = data.strip_types_[i].width_;
        data.width_to_strip_index_[wid] = i;
    }

    // 宽度到子件类型索引列表
    for (int i = 0; i < (int)data.item_types_.size(); i++) {
        int wid = data.item_types_[i].width_;
        data.width_to_item_indices_[wid].push_back(i);
    }
}

// 打印问题参数
void PrintParams(ProblemParams& params) {
    LOG("=== 问题参数 ===");
    LOG_FMT("  母板尺寸: %d x %d\n", params.stock_length_, params.stock_width_);
    LOG_FMT("  子件类型数: %d\n", params.num_item_types_);
    LOG_FMT("  条带类型数: %d\n", params.num_strip_types_);
    LOG_FMT("  SP1方法: %d\n", params.sp1_method_);
    LOG_FMT("  SP2方法: %d\n", params.sp2_method_);
}

// 打印需求信息
void PrintDemand(ProblemData& data) {
    LOG("=== 子件需求 ===");
    for (int i = 0; i < (int)data.item_types_.size(); i++) {
        LOG_FMT("  类型%d: %dx%d 需求=%d\n",
            i + 1,
            data.item_types_[i].length_,
            data.item_types_[i].width_,
            data.item_types_[i].demand_);
    }
}

// 打印初始矩阵
void PrintInitMatrix(ProblemParams& params) {
    LOG("=== 初始Y列矩阵 ===");
    for (int i = 0; i < (int)params.init_y_matrix_.size(); i++) {
        ostringstream oss;
        oss << "  Y" << (i + 1) << ": [";
        for (int j = 0; j < (int)params.init_y_matrix_[i].size(); j++) {
            if (j > 0) oss << ", ";
            oss << params.init_y_matrix_[i][j];
        }
        oss << "]";
        LOG(oss.str().c_str());
    }

    LOG("=== 初始X列矩阵 ===");
    for (int i = 0; i < (int)params.init_x_matrix_.size(); i++) {
        ostringstream oss;
        oss << "  X" << (i + 1) << ": [";
        for (int j = 0; j < (int)params.init_x_matrix_[i].size(); j++) {
            if (j > 0) oss << ", ";
            oss << params.init_x_matrix_[i][j];
        }
        oss << "]";
        LOG(oss.str().c_str());
    }
}

// 打印列生成解
void PrintCGSolution(BPNode* node, ProblemData& data) {
    LOG("=== 列生成解 ===");
    LOG_FMT("  目标值: %.4f\n", node->solution_.obj_val_);

    LOG("  Y列 (母板切割方案):");
    for (int i = 0; i < (int)node->y_columns_.size(); i++) {
        if (node->y_columns_[i].value_ > kZeroTolerance) {
            ostringstream oss;
            oss << "    Y" << (i + 1) << " = " << fixed << setprecision(4)
                << node->y_columns_[i].value_ << " [";
            for (int j = 0; j < (int)node->y_columns_[i].pattern_.size(); j++) {
                if (j > 0) oss << ", ";
                oss << node->y_columns_[i].pattern_[j];
            }
            oss << "]";
            LOG(oss.str().c_str());
        }
    }

    LOG("  X列 (条带切割方案):");
    for (int i = 0; i < (int)node->x_columns_.size(); i++) {
        if (node->x_columns_[i].value_ > kZeroTolerance) {
            ostringstream oss;
            oss << "    X" << (i + 1) << " (条带" << node->x_columns_[i].strip_type_id_ + 1
                << ") = " << fixed << setprecision(4) << node->x_columns_[i].value_ << " [";
            for (int j = 0; j < (int)node->x_columns_[i].pattern_.size(); j++) {
                if (j > 0) oss << ", ";
                oss << node->x_columns_[i].pattern_[j];
            }
            oss << "]";
            LOG(oss.str().c_str());
        }
    }
}

// 打印节点信息
void PrintNodeInfo(BPNode* node) {
    LOG_FMT("=== 节点 %d 信息 ===\n", node->id_);
    LOG_FMT("  父节点: %d\n", node->parent_id_);
    LOG_FMT("  下界: %.4f\n", node->lower_bound_);
    LOG_FMT("  剪枝: %d\n", node->prune_flag_);
    LOG_FMT("  分支完成: %d\n", node->branched_flag_);
    LOG_FMT("  Y列数: %d\n", (int)node->y_columns_.size());
    LOG_FMT("  X列数: %d\n", (int)node->x_columns_.size());
}
