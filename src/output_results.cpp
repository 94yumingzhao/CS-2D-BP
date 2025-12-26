// =============================================================================
// output_results.cpp - 结果输出模块
// =============================================================================
//
// 功能: 输出启发式求解的切割方案结果
//
// 输出内容:
//   1. 控制台输出: 每块母板的切割方案详情
//   2. 文件输出: 切割方案坐标信息 (用于可视化绘图)
//
// 坐标格式:
//   - 每个矩形输出四个顶点坐标
//   - 标识符: x=母板边界, I=子件, S=条带
//
// =============================================================================

#include "2DBP.h"

using namespace std;

// -----------------------------------------------------------------------------
// OutputHeuristicResults - 输出启发式求解结果
// -----------------------------------------------------------------------------
// 功能: 将切割方案以坐标形式输出到控制台和文件
// 参数:
//   Values - 全局参数
//   Lists  - 全局列表
// 输出文件:
//   D:/CuttingTXT/Stock_N.txt (N = 母板索引)
// -----------------------------------------------------------------------------
void OutputHeuristicResults(All_Values& Values, All_Lists& Lists) {

	int stocks_num = Lists.occupied_stocks_list.size();
	int items_num = Lists.occupied_items_list.size();
	int strips_num = Lists.all_strips_list.size();

	// =========================================================================
	// 控制台输出
	// =========================================================================
	cout << "[结果] 切割方案输出 (母板数: " << stocks_num << ")\n";

	for (int pos = 0; pos < stocks_num; pos++) {
		int LL = Lists.occupied_stocks_list[0].length;
		int WW = Lists.occupied_stocks_list[0].width;

		// 统计条带和子件数量
		int strip_count = 0;
		int item_count = 0;
		for (size_t i = 0; i < strips_num; i++) {
			if (Lists.all_strips_list[i].stock_idx == pos) strip_count++;
		}
		for (size_t i = 0; i < items_num; i++) {
			if (Lists.occupied_items_list[i].stock_idx == pos) item_count++;
		}

		cout << "[结果] 母板_" << Lists.occupied_stocks_list[pos].stock_idx
		     << " (" << LL << " x " << WW << "): 条带=" << strip_count << ", 子件=" << item_count << "\n";
	}

	// =========================================================================
	// 文件输出 (用于可视化绘图)
	// =========================================================================
	ostringstream s_in, s_out;
	string in_str, out_str;
	ofstream f_out;

	for (int pos = 0; pos < stocks_num; pos++) {
		// 设置输出文件路径
		s_out.str("");
		s_out << "D:/CuttingTXT/Stock_" << pos << ".txt";
		out_str = s_out.str();
		f_out.open(out_str, ios::out);

		int LL = Lists.occupied_stocks_list[0].length;
		int WW = Lists.occupied_stocks_list[0].width;

		// ----- 输出母板边界 (标识符: x) -----
		f_out << 0 << "\t" << 0 << "\t" << "x" << endl;
		f_out << 0 << "\t" << WW << "\t" << "x" << endl;
		f_out << LL << "\t" << WW << "\t" << "x" << endl;
		f_out << LL << "\t" << 0 << "\t" << "x" << endl;

		// ----- 输出子件坐标 (标识符: I) -----
		for (size_t i = 0; i < items_num; i++) {
			if (Lists.occupied_items_list[i].stock_idx == pos) {
				int X = Lists.occupied_items_list[i].pos_x;
				int Y = Lists.occupied_items_list[i].pos_y;
				int L = Lists.occupied_items_list[i].length;
				int W = Lists.occupied_items_list[i].width;
				int item_type_idx = Lists.occupied_items_list[i].item_type_idx;

				f_out << X << "\t" << Y << "\t" << "I" << item_type_idx << endl;
				f_out << X << "\t" << Y + W << "\t" << "I" << item_type_idx << endl;
				f_out << X + L << "\t" << Y + W << "\t" << "I" << item_type_idx << endl;
				f_out << X + L << "\t" << Y << "\t" << "I" << item_type_idx << endl;
			}
		}

		// ----- 输出条带坐标 (标识符: S) -----
		for (size_t i = 0; i < strips_num; i++) {
			if (Lists.all_strips_list[i].stock_idx == pos) {
				int X = Lists.all_strips_list[i].pos_x;
				int Y = Lists.all_strips_list[i].pos_y;
				int L = Lists.all_strips_list[i].length;
				int W = Lists.all_strips_list[i].width;
				int strip_type_idx = Lists.all_strips_list[i].strip_type_idx;

				f_out << X << "\t" << Y << "\t" << "S" << strip_type_idx << endl;
				f_out << X << "\t" << Y + W << "\t" << "S" << strip_type_idx << endl;
				f_out << X + L << "\t" << Y + W << "\t" << "S" << strip_type_idx << endl;
				f_out << X + L << "\t" << Y << "\t" << "S" << strip_type_idx << endl;
			}
		}

		f_out.close();
	}
}
