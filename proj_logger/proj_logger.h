// proj_logger.h
#ifndef PROJ_LOGGER_H
#define PROJ_LOGGER_H

#include <string>
#include <spdlog/spdlog.h>
#include <memory>

// 日志级别枚举
namespace proj_logger {
enum class LogLevel {
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL,
    OFF
};

// 完整定义日志管理器类（解决不完全类型问题）
class LoggerManager {
public:
    static LoggerManager& get_instance() {
        static LoggerManager instance;
        return instance;
    }

    // 获取日志器（在头文件中声明，确保编译器可见）
    std::shared_ptr<spdlog::logger> get_logger(const std::string& name);

    // 设置所有日志器级别
    void set_all_log_level(spdlog::level::level_enum level);
    void init_level_from_env();
    LogLevel str_to_loglevel(const std::string& level_str);

    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

private:
    LoggerManager();  // 构造函数在cpp中实现
    ~LoggerManager() = default;

    std::shared_ptr<spdlog::sinks::sink> shared_sink_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    std::mutex mtx_;
    spdlog::level::level_enum default_level_ = spdlog::level::info; // 默认日志级别
};

// 转换日志级别
inline spdlog::level::level_enum to_spdlog_level(LogLevel level) {
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

// 模板日志函数（头文件实现）
template<typename... Args>
void log(LogLevel level, const std::string& logger_name, const char* fmt, const Args&... args) {
    auto logger = LoggerManager::get_instance().get_logger(logger_name);
    logger->log(to_spdlog_level(level), fmt, args...);
}

void set_global_log_level(LogLevel level);

// 宏定义
#define LOGGER(LEVEL, FMT, LOGGER_NAME, ...) \
    proj_logger::log(proj_logger::LogLevel::LEVEL, LOGGER_NAME, FMT, ##__VA_ARGS__)

#define MALOG_WARN(module, fmt, ...) LOGGER(WARN, fmt, module, ##__VA_ARGS__)
#define MALOG_INFO(module, fmt, ...) LOGGER(INFO, fmt, module, ##__VA_ARGS__)

} // namespace proj_logger

#endif // PROJ_LOGGER_H