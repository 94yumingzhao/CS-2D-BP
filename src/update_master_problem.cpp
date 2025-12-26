// =============================================================================
// update_master_problem.cpp - 主问题更新模块
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// SolveUpdateMasterProblem - 添加新列并重新求解主问题
// -----------------------------------------------------------------------------
void SolveUpdateMasterProblem(
	All_Values& Values,
	All_Lists& Lists,
	IloEnv& Env_MP,
	IloModel& Model_MP,
	IloObjective& Obj_MP,
	IloRangeArray& Cons_MP,
	IloNumVarArray& Vars_MP,
	Node& this_node) {

	int K_num = this_node.Y_cols_list.size();
	int P_num = this_node.X_cols_list.size();
	int J_num = Values.strip_types_num;
	int N_num = Values.item_types_num;
	int all_cols_num = K_num + P_num;
	int all_rows_num = J_num + N_num;

	if (this_node.Y_col_flag == 1) {
		// 添加新 Y 列
		IloNum obj_para = 1;
		IloNumColumn CplexCol = (Obj_MP)(obj_para);

		for (int row = 0; row < all_rows_num; row++) {
			IloNum row_para = this_node.new_Y_col[row];
			CplexCol += (Cons_MP)[row](row_para);
		}

		int cols_num = this_node.Y_cols_list.size();
		string var_name = "Y_" + to_string(cols_num + 1);
		IloNum var_min = 0;
		IloNum var_max = IloInfinity;
		IloNumVar Var_Y(CplexCol, var_min, var_max, ILOFLOAT, var_name.c_str());
		(Vars_MP).add(Var_Y);
		CplexCol.end();

		vector<double> new_col;
		for (int row = 0; row < all_rows_num; row++) {
			new_col.push_back(this_node.new_Y_col[row]);
		}

		this_node.Y_cols_list.push_back(new_col);
		this_node.model_matrix.insert(this_node.model_matrix.begin() + this_node.Y_cols_list.size(), new_col);
		this_node.new_Y_col.clear();
	}

	if (this_node.Y_col_flag == 0) {
		// 添加新 X 列
		int new_cols_num = this_node.new_X_cols_list.size();
		for (int col = 0; col < new_cols_num; col++) {

			IloNum obj_para = 0;
			IloNumColumn CplexCol = (Obj_MP)(obj_para);

			for (int row = 0; row < all_rows_num; row++) {
				IloNum row_para = this_node.new_X_cols_list[col][row];
				CplexCol += (Cons_MP)[row](row_para);
			}

			int old_item_cols_num = this_node.X_cols_list.size();
			string var_name = "X_" + to_string(old_item_cols_num + 1);
			IloNum var_min = 0;
			IloNum var_max = IloInfinity;
			IloNumVar Var_X(CplexCol, var_min, var_max, ILOFLOAT, var_name.c_str());
			(Vars_MP).add(Var_X);
			CplexCol.end();

			vector<double> temp_col;
			for (int row = 0; row < all_rows_num; row++) {
				temp_col.push_back(this_node.new_X_cols_list[col][row]);
			}

			this_node.X_cols_list.push_back(temp_col);
			this_node.model_matrix.insert(this_node.model_matrix.end(), temp_col);
		}
		this_node.new_X_cols_list.clear();
	}

	// 求解更新后的主问题
	IloCplex MP_cplex(Model_MP);
	MP_cplex.extract(Model_MP);
	MP_cplex.setOut(Env_MP.getNullStream());
	MP_cplex.solve();

	cout << "[主问题] 迭代 " << this_node.iter << ", 目标值 = "
	     << fixed << setprecision(4) << MP_cplex.getValue(Obj_MP) << "\n";
	cout.unsetf(ios::fixed);

	// 提取对偶价格
	this_node.dual_prices_list.clear();

	for (int row = 0; row < J_num; row++) {
		double dual_val = MP_cplex.getDual(Cons_MP[row]);
		if (dual_val == -0) dual_val = 0;
		this_node.dual_prices_list.push_back(dual_val);
	}

	for (int row = J_num; row < J_num + N_num; row++) {
		double dual_val = MP_cplex.getDual(Cons_MP[row]);
		if (dual_val == -0) dual_val = 0;
		this_node.dual_prices_list.push_back(dual_val);
	}
}

// -----------------------------------------------------------------------------
// SolveFinalMasterProblem - 求解列生成收敛后的最终主问题
// -----------------------------------------------------------------------------
void SolveFinalMasterProblem(
	All_Values& Values,
	All_Lists& Lists,
	IloEnv& Env_MP,
	IloModel& Model_MP,
	IloObjective& Obj_MP,
	IloRangeArray& Cons_MP,
	IloNumVarArray& Vars_MP,
	Node& this_node) {

	int K_num = this_node.Y_cols_list.size();
	int P_num = this_node.X_cols_list.size();
	int N_num = Values.item_types_num;
	int J_num = Values.strip_types_num;
	int all_rows_num = N_num + J_num;
	int all_cols_num = K_num + P_num;

	// 求解最终主问题
	IloCplex MP_cplex(Model_MP);
	MP_cplex.extract(Model_MP);
	MP_cplex.setOut(Env_MP.getNullStream());
	MP_cplex.exportModel("Final Master Problem.lp");
	MP_cplex.solve();

	this_node.LB = MP_cplex.getValue(Obj_MP);

	cout << "[主问题] 最终目标值 = " << fixed << setprecision(4) << this_node.LB << "\n";
	cout.unsetf(ios::fixed);

	// 保存所有变量的解
	for (int col = 0; col < all_cols_num; col++) {
		IloNum soln_val = MP_cplex.getValue(Vars_MP[col]);
		if (soln_val == -0) soln_val = 0;
		this_node.all_solns_val_list.push_back(soln_val);
	}

	// 统计非零解
	int Y_fsb_num = 0;
	int X_fsb_num = 0;

	for (int col = 0; col < K_num; col++) {
		double soln_val = MP_cplex.getValue(Vars_MP[col]);
		if (soln_val > 0) Y_fsb_num++;
	}

	for (int col = K_num; col < K_num + P_num; col++) {
		double soln_val = MP_cplex.getValue(Vars_MP[col]);
		if (soln_val > 0) X_fsb_num++;
	}

	cout << "[主问题] 非零解: Y=" << Y_fsb_num << ", X=" << X_fsb_num
	     << " (总变量: " << all_cols_num << ")\n";
}
