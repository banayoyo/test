#ifndef PROJ_LOGGER_H
#define PROJ_LOGGER_H

#include <string>

// 日志级别枚举（封装spdlog级别，避免暴露）
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

// 全局日志接口：自动注册日志器，随插随用
void log(LogLevel level, const std::string& logger_name, const char* fmt, ...);

// 统一设置所有日志器的级别
void set_global_log_level(LogLevel level);

// 内部宏封装（用户要求的MALOG_WARN/INFO）
#define LOGGER(LEVEL, FMT, LOGGER_NAME, ...) \
    proj_logger::log(proj_logger::LogLevel::LEVEL, LOGGER_NAME, FMT, ##__VA_ARGS__)

#define MALOG_WARN(module, fmt, ...) LOGGER(WARN, fmt, module, ##__VA_ARGS__)
#define MALOG_INFO(module, fmt, ...) LOGGER(INFO, fmt, module, ##__VA_ARGS__)

} // namespace proj_logger

#endif // PROJ_LOGGER_H