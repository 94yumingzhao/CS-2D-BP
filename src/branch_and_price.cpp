// =============================================================================
// branch_and_price.cpp - 分支定价主循环
// =============================================================================
//
// 功能: 实现分支定价算法的主控制循环
//
// 算法流程:
//   1. 选择待分支节点 (基于下界选择)
//   2. 生成左右子节点 (向下取整/向上取整)
//   3. 对子节点进行列生成求解
//   4. 检查整数性, 更新上下界
//   5. 剪枝或继续搜索
//
// 搜索策略:
//   - 优先搜索下界较小的节点
//   - 当分支变量值 <= 1 时, 优先探索右子节点
//   - 当分支变量值 > 1 时, 选择下界更优的子节点
//
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// BranchAndPriceTree - 分支定价搜索树主循环
// -----------------------------------------------------------------------------
// 功能: 管理分支定界树的搜索过程, 直至找到最优整数解或穷尽搜索
// 参数:
//   Values - 全局参数
//   Lists  - 全局列表
// 返回值: 0 = 正常结束
// -----------------------------------------------------------------------------
int BranchAndPriceTree(All_Values& Values, All_Lists& Lists) {

	Values.node_num = 1;  // 根节点已生成

	while (1) {
		if (Values.search_flag == 0) {
			// search_flag = 0: 继续分支当前父节点

			Node parent_node;
			int parent_branch_flag = ChooseNodeToBranch(Values, Lists, parent_node);

			if (parent_branch_flag == 0) {
				// 无可分支节点, 算法终止
				cout << "[分支定价] 求解完成\n";
				cout << "[分支定价] 最优下界 = " << fixed << setprecision(4) << Values.optimal_LB << "\n";
				cout.unsetf(ios::fixed);
				break;
			}

			if (parent_branch_flag == 1) {
				// 找到待分支节点, 生成左右子节点

				Node new_left_node;
				Node new_right_node;

				// =====================================================
				// 步骤1: 先处理左子节点 (向下取整)
				// =====================================================
				Values.branch_status = 1;
				Values.node_num++;
				GenerateNewNode(Values, Lists, new_left_node, parent_node);
				NewNodeColumnGeneration(Values, Lists, new_left_node, parent_node);
				int left_search_flag = FinishNode(Values, Lists, new_left_node);
				Lists.all_nodes_list.push_back(new_left_node);

				// =====================================================
				// 步骤2: 再处理右子节点 (向上取整)
				// =====================================================
				Values.branch_status = 2;
				Values.node_num++;
				GenerateNewNode(Values, Lists, new_right_node, parent_node);
				NewNodeColumnGeneration(Values, Lists, new_right_node, parent_node);
				int right_search_flag = FinishNode(Values, Lists, new_right_node);
				Lists.all_nodes_list.push_back(new_right_node);

				Values.root_flag = 0;  // 已离开根节点

				// =====================================================
				// 步骤3: 选择下一个探索方向
				// =====================================================
				double parent_branch_val = parent_node.var_to_branch_soln;

				if (parent_branch_val <= 1) {
					// 分支变量值 <= 1, 优先探索右子节点
					Values.search_flag = right_search_flag;

					if (Values.search_flag != 1) {
						Values.fathom_flag = 2;
						cout << "[分支定价] 父节点分支值 = " << fixed << setprecision(4) << parent_branch_val
						     << " <= 1, 剪枝右子节点_" << new_right_node.index << "\n";
						cout.unsetf(ios::fixed);
					}
				}
				if (parent_branch_val > 1) {
					// 分支变量值 > 1, 选择下界更优的子节点
					if (new_left_node.LB < new_right_node.LB) {
						Values.search_flag = left_search_flag;

						if (Values.search_flag != 1) {
							Values.fathom_flag = 1;
							cout << "[分支定价] 左子节点_" << new_left_node.index
							     << " LB=" << fixed << setprecision(4) << new_left_node.LB
							     << " < 右子节点_" << new_right_node.index
							     << " LB=" << new_right_node.LB << "\n";
							cout << "[分支定价] 继续剪枝右子节点_" << new_right_node.index << "\n";
							cout.unsetf(ios::fixed);
						}
					}
					if (new_left_node.LB >= new_right_node.LB) {
						Values.search_flag = right_search_flag;

						if (Values.search_flag != 1) {
							Values.fathom_flag = 2;
							cout << "[分支定价] 左子节点_" << new_left_node.index
							     << " LB=" << fixed << setprecision(4) << new_left_node.LB
							     << " >= 右子节点_" << new_right_node.index
							     << " LB=" << new_right_node.LB << "\n";
							cout << "[分支定价] 继续剪枝右子节点_" << new_right_node.index << "\n";
							cout.unsetf(ios::fixed);
						}
					}
				}
				Values.branch_status = 1;  // 下一轮从左子节点开始
			}
		}

		if (Values.search_flag == 1) {
			// search_flag = 1: 当前节点为整数解, 搜索其他未探索节点
			Values.branch_status = 3;
			Values.fathom_flag = -1;
			Values.search_flag = 0;
			cout << "[分支定价] 当前节点解均为整数\n";
			cout << "[分支定价] 当前最优下界 = " << fixed << setprecision(4) << Values.optimal_LB << "\n";
			cout.unsetf(ios::fixed);
		}

		// 防止无限循环
		if (Values.node_num > 30) {
			cout << "[警告] 达到最大节点数 30, 强制终止\n";
			break;
		}
	}

	return 0;
}
