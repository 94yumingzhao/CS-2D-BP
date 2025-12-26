// =============================================================================
// root_node_column_generation.cpp - 根节点列生成
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// RootNodeColumnGeneration - 根节点列生成主循环
// -----------------------------------------------------------------------------
void RootNodeColumnGeneration(All_Values& Values, All_Lists& Lists, Node& root_node) {

	cout << "[列生成] 根节点列生成开始\n";

	// 初始化 CPLEX 环境
	IloEnv Env_MP;
	IloModel Model_MP(Env_MP);
	IloObjective Obj_MP = IloAdd(Model_MP, IloMinimize(Env_MP));
	IloNumVarArray Vars_MP(Env_MP);
	IloRangeArray Cons_MP(Env_MP);

	root_node.iter = 0;

	// 求解初始主问题
	bool MP_flag = SolveRootNodeFirstMasterProblem(
		Values, Lists, Env_MP, Model_MP, Obj_MP, Cons_MP, Vars_MP, root_node);

	if (MP_flag == 1) {
		// 初始主问题可行, 进入列生成循环
		while (1) {
			root_node.iter++;

			if (root_node.iter == 100) {
				cout << "[警告] 达到最大迭代次数 100\n";
				break;
			}

			// 求解子问题
			int SP_flag = SolveStageOneSubProblem(Values, Lists, root_node);

			if (SP_flag == 0) {
				// 无改进列, 列生成收敛
				cout << "[列生成] 收敛 (迭代 " << root_node.iter << " 次)\n";
				break;
			}

			if (SP_flag == 1) {
				// 找到改进列, 更新主问题
				SolveUpdateMasterProblem(
					Values, Lists, Env_MP, Model_MP, Obj_MP, Cons_MP, Vars_MP, root_node);
			}
		}

		// 求解最终主问题
		SolveFinalMasterProblem(
			Values, Lists, Env_MP, Model_MP, Obj_MP, Cons_MP, Vars_MP, root_node);
	}

	// 释放 CPLEX 资源
	Obj_MP.removeAllProperties();
	Obj_MP.end();
	Vars_MP.clear();
	Vars_MP.end();
	Cons_MP.clear();
	Cons_MP.end();
	Model_MP.removeAllProperties();
	Model_MP.end();
	Env_MP.removeAllProperties();
	Env_MP.end();

	cout << "[列生成] 根节点列生成结束\n";
}
