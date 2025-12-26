// =============================================================================
// root_node_first_master_problem.cpp - 根节点初始主问题
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// SolveRootNodeFirstMasterProblem - 求解根节点初始主问题
// -----------------------------------------------------------------------------
bool SolveRootNodeFirstMasterProblem(
	All_Values& Values,
	All_Lists& Lists,
	IloEnv& Env_MP,
	IloModel& Model_MP,
	IloObjective& Obj_MP,
	IloRangeArray& Cons_MP,
	IloNumVarArray& Vars_MP,
	Node& root_node) {

	int K_num = root_node.Y_cols_list.size();
	int P_num = root_node.X_cols_list.size();
	int J_num = Values.strip_types_num;
	int N_num = Values.item_types_num;
	int all_cols_num = K_num + P_num;
	int all_rows_num = J_num + N_num;

	cout << "[主问题] 构建初始主问题 (Y=" << K_num << ", X=" << P_num << ")\n";

	// 构建约束
	IloNumArray con_min(Env_MP);
	IloNumArray con_max(Env_MP);

	for (int row = 0; row < J_num + N_num; row++) {
		if (row < J_num) {
			con_min.add(IloNum(0));
			con_max.add(IloNum(IloInfinity));
		}
		if (row >= J_num) {
			int row_pos = row - J_num;
			double demand_val = Lists.all_item_types_list[row_pos].demand;
			con_min.add(IloNum(demand_val));
			con_max.add(IloNum(IloInfinity));
		}
	}

	Cons_MP = IloRangeArray(Env_MP, con_min, con_max);
	Model_MP.add(Cons_MP);
	con_min.end();
	con_max.end();

	// 构建 Y 变量
	for (int col = 0; col < K_num; col++) {
		IloNum obj_para = 1;
		IloNumColumn CplexCol = Obj_MP(obj_para);

		for (int row = 0; row < J_num + N_num; row++) {
			IloNum row_para = root_node.model_matrix[col][row];
			CplexCol += Cons_MP[row](row_para);
		}

		string Y_name = "Y_" + to_string(col + 1);
		IloNum var_min = 0;
		IloNum var_max = IloInfinity;
		IloNumVar Var_Y(CplexCol, var_min, var_max, ILOFLOAT, Y_name.c_str());
		Vars_MP.add(Var_Y);
		CplexCol.end();
	}

	// 构建 X 变量
	for (int col = K_num; col < K_num + P_num; col++) {
		IloNum obj_para = 0;
		IloNumColumn CplexCol = Obj_MP(obj_para);

		for (int row = 0; row < J_num + N_num; row++) {
			IloNum row_para = root_node.model_matrix[col][row];
			CplexCol += Cons_MP[row](row_para);
		}

		string X_name = "X_" + to_string(col + 1 - K_num);
		IloNum var_min = 0;
		IloNum var_max = IloInfinity;
		IloNumVar Var_X(CplexCol, var_min, var_max, ILOFLOAT, X_name.c_str());
		Vars_MP.add(Var_X);
		CplexCol.end();
	}

	// 求解主问题
	cout << "[主问题] 求解初始主问题...\n";
	IloCplex MP_cplex(Model_MP);
	MP_cplex.extract(Model_MP);
	MP_cplex.setOut(Env_MP.getNullStream());  // 关闭 CPLEX 输出
	MP_cplex.exportModel("The First Master Problem.lp");
	bool MP_flag = MP_cplex.solve();

	if (MP_flag == 0) {
		cout << "[主问题] 初始主问题不可行\n";
	} else {
		cout << "[主问题] 初始主问题可行, 目标值 = "
		     << fixed << setprecision(4) << MP_cplex.getValue(Obj_MP) << "\n";
		cout.unsetf(ios::fixed);

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

		cout << "[主问题] 非零解: Y=" << Y_fsb_num << ", X=" << X_fsb_num << "\n";

		// 提取对偶价格
		root_node.dual_prices_list.clear();

		for (int row = 0; row < J_num; row++) {
			double dual_val = MP_cplex.getDual(Cons_MP[row]);
			if (dual_val == -0) dual_val = 0;
			root_node.dual_prices_list.push_back(dual_val);
		}

		for (int row = J_num; row < J_num + N_num; row++) {
			double dual_val = MP_cplex.getDual(Cons_MP[row]);
			if (dual_val == -0) dual_val = 0;
			root_node.dual_prices_list.push_back(dual_val);
		}
	}

	return MP_flag;
}
