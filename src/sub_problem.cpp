// =============================================================================
// sub_problem.cpp - 子问题求解模块
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// SolveStageOneSubProblem - 求解第一阶段子问题 (宽度背包)
// -----------------------------------------------------------------------------
int SolveStageOneSubProblem(All_Values& Values, All_Lists& Lists, Node& this_node) {

	int K_num = this_node.Y_cols_list.size();
	int P_num = this_node.X_cols_list.size();
	int J_num = Values.strip_types_num;
	int N_num = Values.item_types_num;
	int all_cols_num = K_num + P_num;
	int all_rows_num = J_num + N_num;

	int loop_continue_flag = -1;

	// 构建 SP1 模型
	IloEnv Env_SP1;
	IloModel Model_SP1(Env_SP1);
	IloNumVarArray Ga_Vars(Env_SP1);

	for (int j = 0; j < J_num; j++) {
		IloNum var_min = 0;
		IloNum var_max = IloInfinity;
		string var_name = "G_" + to_string(j + 1);
		IloNumVar Var_Ga(Env_SP1, var_min, var_max, ILOINT, var_name.c_str());
		Ga_Vars.add(Var_Ga);
	}

	// 目标函数
	IloExpr obj_sum(Env_SP1);
	for (int j = 0; j < J_num; j++) {
		double val = this_node.dual_prices_list[j];
		obj_sum += val * Ga_Vars[j];
	}
	IloObjective Obj_SP1 = IloMaximize(Env_SP1, obj_sum);
	Model_SP1.add(Obj_SP1);
	obj_sum.end();

	// 宽度约束
	IloExpr con_sum(Env_SP1);
	for (int j = 0; j < J_num; j++) {
		double val = Lists.all_strip_types_list[j].width;
		con_sum += val * Ga_Vars[j];
	}
	Model_SP1.add(con_sum <= Values.stock_width);
	con_sum.end();

	// 求解 SP1
	IloCplex Cplex_SP1(Env_SP1);
	Cplex_SP1.extract(Model_SP1);
	Cplex_SP1.setOut(Env_SP1.getNullStream());
	bool SP1_flag = Cplex_SP1.solve();

	if (SP1_flag == 0) {
		cout << "[子问题] SP1 不可行\n";
	} else {
		double SP1_obj_val = Cplex_SP1.getValue(Obj_SP1);

		// 构建新 Y 列
		this_node.new_Y_col.clear();
		for (int j = 0; j < J_num; j++) {
			double soln_val = Cplex_SP1.getValue(Ga_Vars[j]);
			if (soln_val == -0) soln_val = 0;
			this_node.new_Y_col.push_back(soln_val);
		}

		for (int j = 0; j < N_num; j++) {
			this_node.new_Y_col.push_back(0);
		}

		// 定价决策
		if (SP1_obj_val > 1 + RC_EPS) {
			cout << "[子问题] SP1 找到改进列 (rc=" << fixed << setprecision(4) << SP1_obj_val << ")\n";
			cout.unsetf(ios::fixed);
			this_node.Y_col_flag = 1;
			loop_continue_flag = 1;
		} else {
			// 需要继续求解 SP2
			this_node.new_Y_col.clear();
			this_node.new_X_cols_list.clear();
			this_node.Y_col_flag = 0;
			this_node.SP2_obj_val = -1;

			int feasible_flag = 0;
			int SP2_flag = -1;

			for (int k = 0; k < J_num; k++) {
				SP2_flag = SolveStageTwoSubProblem(Values, Lists, this_node, k + 1);
				if (SP2_flag == 1) {
					double a_val = this_node.dual_prices_list[k];

					if (this_node.SP2_obj_val > a_val + RC_EPS) {
						feasible_flag = 1;

						vector<double> temp_col;
						for (int j = 0; j < J_num; j++) {
							if (k == j) {
								temp_col.push_back(-1);
							} else {
								temp_col.push_back(0);
							}
						}

						for (int i = 0; i < N_num; i++) {
							double soln_val = this_node.SP2_solns_list[i];
							if (soln_val == -0) soln_val = 0;
							temp_col.push_back(soln_val);
						}

						this_node.new_X_cols_list.push_back(temp_col);
					}
				}
			}

			if (feasible_flag == 0) {
				cout << "[子问题] 未找到改进列, 列生成收敛\n";
			} else {
				cout << "[子问题] SP2 找到 " << this_node.new_X_cols_list.size() << " 个改进列\n";
			}

			loop_continue_flag = feasible_flag;
		}
	}

	// 释放资源
	Obj_SP1.removeAllProperties();
	Obj_SP1.end();
	Ga_Vars.clear();
	Ga_Vars.end();
	Model_SP1.removeAllProperties();
	Model_SP1.end();
	Env_SP1.removeAllProperties();
	Env_SP1.end();

	return loop_continue_flag;
}

// -----------------------------------------------------------------------------
// SolveStageTwoSubProblem - 求解第二阶段子问题 (长度背包)
// -----------------------------------------------------------------------------
int SolveStageTwoSubProblem(All_Values& Values, All_Lists& Lists, Node& this_node, int strip_type_idx) {

	int all_cols_num = this_node.model_matrix.size();
	int strip_types_num = Values.strip_types_num;
	int item_types_num = Values.item_types_num;
	int J_num = strip_types_num;
	int N_num = item_types_num;

	int final_return = -1;

	// 构建 SP2 模型
	IloEnv Env_SP2;
	IloModel Model_SP2(Env_SP2);
	IloNumVarArray De_Vars(Env_SP2);

	for (int i = 0; i < N_num; i++) {
		IloNum var_min = 0;
		IloNum var_max = IloInfinity;
		string var_name = "D_" + to_string(i + 1);
		IloNumVar De_Var(Env_SP2, var_min, var_max, ILOINT, var_name.c_str());
		De_Vars.add(De_Var);
	}

	IloObjective Obj_SP2(Env_SP2);
	IloExpr obj_sum(Env_SP2);
	IloExpr sum_1(Env_SP2);
	double sum_val = 0;
	vector<int> fsb_idx;

	// 筛选可行子件
	for (int i = 0; i < N_num; i++) {
		if (Lists.all_item_types_list[i].width <= Lists.all_strip_types_list[strip_type_idx - 1].width) {
			int row_pos = i + N_num;
			double b_val = this_node.dual_prices_list[row_pos];
			if (b_val > 0) {
				obj_sum += b_val * De_Vars[i];
				sum_val += b_val;
				double l_val = Lists.all_item_types_list[i].length;
				sum_1 += l_val * De_Vars[i];
				fsb_idx.push_back(i);
			}
		}
	}

	if (sum_val > 0) {
		Obj_SP2 = IloMaximize(Env_SP2, obj_sum);
		Model_SP2.add(Obj_SP2);
		Model_SP2.add(sum_1 <= Values.stock_length);
		obj_sum.end();
		sum_1.end();
		final_return = 1;
	}

	if (sum_val == 0) {
		sum_1.end();
		Obj_SP2.removeAllProperties();
		Obj_SP2.end();
		De_Vars.clear();
		De_Vars.end();
		Model_SP2.removeAllProperties();
		Model_SP2.end();
		Env_SP2.removeAllProperties();
		Env_SP2.end();
		return 0;
	}

	// 求解 SP2
	IloCplex Cplex_SP2(Env_SP2);
	Cplex_SP2.extract(Model_SP2);
	Cplex_SP2.setOut(Env_SP2.getNullStream());
	bool SP2_flag = Cplex_SP2.solve();

	if (SP2_flag == 0) {
		// 不可行
	} else {
		this_node.SP2_obj_val = Cplex_SP2.getValue(Obj_SP2);
		this_node.SP2_solns_list.clear();
		int fsb_num = fsb_idx.size();

		for (int i = 0; i < N_num; i++) {
			int fsb_flag = -1;
			for (int k = 0; k < fsb_num; k++) {
				if (i == fsb_idx[k]) {
					fsb_flag = 1;
					double soln_val = Cplex_SP2.getValue(De_Vars[i]);
					if (soln_val == -0) soln_val = 0;
					this_node.SP2_solns_list.push_back(soln_val);
				}
			}
			if (fsb_flag != 1) {
				this_node.SP2_solns_list.push_back(0);
			}
		}
	}

	// 释放资源
	Obj_SP2.removeAllProperties();
	Obj_SP2.end();
	De_Vars.clear();
	De_Vars.end();
	Model_SP2.removeAllProperties();
	Model_SP2.end();
	Env_SP2.removeAllProperties();
	Env_SP2.end();

	return final_return;
}
