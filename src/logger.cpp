// =============================================================================
// logger.cpp - 日志系统实现
// =============================================================================

#include "logger.h"

using namespace std;

// =============================================================================
// DualStreambuf 实现
// =============================================================================

// -----------------------------------------------------------------------------
// 构造函数
// -----------------------------------------------------------------------------
DualStreambuf::DualStreambuf(streambuf* console_buf, streambuf* file_buf)
    : console_buf_(console_buf)
    , file_buf_(file_buf)
    , need_timestamp_(true) {
}

// -----------------------------------------------------------------------------
// getCurrentTimestamp - 获取当前时间戳
// -----------------------------------------------------------------------------
// 返回格式: [YYYY-MM-DD HH:MM:SS.mmm]
// -----------------------------------------------------------------------------
string DualStreambuf::getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    stringstream ss;
    ss << "[" << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S")
       << "." << setfill('0') << setw(3) << ms.count() << "] ";
    return ss.str();
}

// -----------------------------------------------------------------------------
// writeTimestamp - 写入时间戳到两个缓冲区
// -----------------------------------------------------------------------------
void DualStreambuf::writeTimestamp() {
    string timestamp = getCurrentTimestamp();

    if (console_buf_) {
        console_buf_->sputn(timestamp.c_str(), timestamp.length());
    }
    if (file_buf_) {
        file_buf_->sputn(timestamp.c_str(), timestamp.length());
    }
}

// -----------------------------------------------------------------------------
// overflow - 单字符输出处理
// -----------------------------------------------------------------------------
int DualStreambuf::overflow(int c) {
    if (c != EOF) {
        // 在行首添加时间戳
        if (need_timestamp_) {
            writeTimestamp();
            need_timestamp_ = false;
        }

        // 同时写入控制台和文件
        if (console_buf_) {
            console_buf_->sputc(c);
        }
        if (file_buf_) {
            file_buf_->sputc(c);
        }

        // 遇到换行符, 下一行需要时间戳
        if (c == '\n') {
            need_timestamp_ = true;
        }
    }
    return c;
}

// -----------------------------------------------------------------------------
// xsputn - 批量字符输出处理
// -----------------------------------------------------------------------------
streamsize DualStreambuf::xsputn(const char* s, streamsize count) {
    for (streamsize i = 0; i < count; ++i) {
        overflow(s[i]);
    }
    return count;
}

// =============================================================================
// Logger 实现
// =============================================================================

// -----------------------------------------------------------------------------
// 构造函数 - 初始化日志系统
// -----------------------------------------------------------------------------
Logger::Logger(const string& log_prefix)
    : old_cout_buf_(nullptr)
    , dual_buf_(nullptr) {

    // 生成日志文件路径
    log_file_path_ = log_prefix + ".log";

    // 打开日志文件 (覆盖模式)
    log_file_.open(log_file_path_, ios::out | ios::trunc);

    if (log_file_.is_open()) {
        // 保存原始 cout 缓冲区
        old_cout_buf_ = cout.rdbuf();

        // 创建双输出缓冲区
        dual_buf_ = make_unique<DualStreambuf>(old_cout_buf_, log_file_.rdbuf());

        // 重定向 cout 到双缓冲区
        cout.rdbuf(dual_buf_.get());
    }
}

// -----------------------------------------------------------------------------
// 析构函数 - 恢复标准输出并关闭日志文件
// -----------------------------------------------------------------------------
Logger::~Logger() {
    try {
        // 恢复原始 cout 缓冲区
        if (old_cout_buf_) {
            cout.rdbuf(old_cout_buf_);
        }

        // 关闭日志文件
        if (log_file_.is_open()) {
            log_file_.close();
        }
    } catch (...) {
        // 忽略析构过程中的异常
    }
}
