#pragma once

#include "../proj_logger/proj_logger.h"

// 测试模块日志器名称：固定为"test_logger"
#define HANDLER_LOGGER_NAME "hand"

// 用户要求的TEST_WARN/INFO宏
#define HANDLER_WARN(fmt, ...) MALOG_WARN(HANDLER_LOGGER_NAME, fmt, ##__VA_ARGS__)
#define HANDLER_INFO(fmt, ...) MALOG_INFO(HANDLER_LOGGER_NAME, fmt, ##__VA_ARGS__)
