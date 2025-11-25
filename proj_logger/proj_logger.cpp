#include "proj_logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <unordered_map>
#include <mutex>
#include <cstdarg>
#include <vector>

namespace proj_logger {

// 单例日志管理器：管理共享Sink、日志器映射、线程安全
class LoggerManager {
public:
    static LoggerManager& get_instance() {
        static LoggerManager instance;
        return instance;
    }

    // 获取共享Sink（多线程彩色标准输出）
    std::shared_ptr<spdlog::sinks::sink> get_shared_sink() {
        return shared_sink_;
    }

    // 获取日志器：不存在则自动注册（随插随用核心）
    std::shared_ptr<spdlog::logger> get_logger(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = loggers_.find(name);
        if (it != loggers_.end()) {
            return it->second;
        }

        // 注册新日志器：使用共享Sink
        auto logger = std::make_shared<spdlog::logger>(name, shared_sink_);
        logger->set_level(spdlog::level::info); // 默认级别
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"); // 日志格式
        loggers_[name] = logger;
        return logger;
    }

    // 设置所有日志器的级别
    void set_all_log_level(spdlog::level::level_enum level) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto& [name, logger] : loggers_) {
            logger->set_level(level);
        }
    }

private:
    LoggerManager() {
        // 初始化共享Sink
        shared_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    }

    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    std::shared_ptr<spdlog::sinks::sink> shared_sink_; // 共享Sink
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_; // 日志器映射
    std::mutex mtx_; // 线程安全锁
};

// 转换自定义级别到spdlog级别
static spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARN: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

#include <string>
#include <cstdarg>

void log(LogLevel level, const std::string& logger_name, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // 计算所需长度
    va_list args_copy;
    va_copy(args_copy, args);
    int len = std::vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (len <= 0) {
        va_end(args);
        return;
    }

    // 创建string并格式化
    std::string message(len + 1, '\0');
    std::vsnprintf(&message[0], message.size(), fmt, args);
    va_end(args);

    // 移除末尾的null字符
    message.resize(len);

    auto logger = LoggerManager::get_instance().get_logger(logger_name);
    logger->log(to_spdlog_level(level), message);
}

// 统一设置全局日志级别
void set_global_log_level(LogLevel level) {
    LoggerManager::get_instance().set_all_log_level(to_spdlog_level(level));
}

} // namespace proj_logger