#include "proj_logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <unordered_map>
#include <mutex>
#include <cstdarg>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace proj_logger {

// 实现日志管理器构造函数
LoggerManager::LoggerManager() {
    shared_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // 自动从环境变量初始化日志级别（仅执行一次）
    init_level_from_env();
}

// 字符串转日志级别（支持大小写不敏感）
proj_logger::LogLevel LoggerManager::str_to_loglevel(const std::string& level_str) {
    std::string lower_str = level_str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);

    if (lower_str == "trace") return proj_logger::LogLevel::TRACE;
    if (lower_str == "debug") return proj_logger::LogLevel::DEBUG;
    if (lower_str == "info") return proj_logger::LogLevel::INFO;
    if (lower_str == "warn") return proj_logger::LogLevel::WARN;
    if (lower_str == "error") return proj_logger::LogLevel::ERROR;
    if (lower_str == "critical") return proj_logger::LogLevel::CRITICAL;
    if (lower_str == "off") return proj_logger::LogLevel::OFF;
    return proj_logger::LogLevel::INFO; // 默认级别
}

// 从环境变量初始化日志级别（仅执行一次）
void LoggerManager::init_level_from_env() {
    static bool has_checked = false; // 保证只读取一次环境变量
    if (has_checked) return;

    const char* env_val = std::getenv("PROJ_LOG_LEVEL");
    if (env_val != nullptr && *env_val != '\0') {
        proj_logger::LogLevel level = str_to_loglevel(env_val);
        default_level_ = to_spdlog_level(level);
        std::cout<<"!!! Env set log_level to "<<cvtLogLevel(level).c_str()<<std::endl;
    }

    set_all_log_level(default_level_);
    has_checked = true;
}


// 实现获取日志器
std::shared_ptr<spdlog::logger> LoggerManager::get_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    auto logger = std::make_shared<spdlog::logger>(name, shared_sink_);
    logger->set_level(default_level_);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    loggers_[name] = logger;
    return logger;
}

// 实现设置全局级别
void LoggerManager::set_all_log_level(spdlog::level::level_enum level) {
    std::lock_guard<std::mutex> lock(mtx_);
    default_level_ = level;
    for (auto& [name, logger] : loggers_) {
        logger->set_level(level);
    }
}

// 实现全局日志级别设置
void set_global_log_level(proj_logger::LogLevel level) {
    LoggerManager::get_instance().set_all_log_level(to_spdlog_level(level));
}

} // namespace proj_logger