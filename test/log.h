#ifndef TEST_LOG_H
#define TEST_LOG_H

#include "../proj_logger/proj_logger.h"

// 测试模块日志器名称：固定为"test_logger"
#define TEST_LOGGER_NAME "TEST"

// 用户要求的TEST_WARN/INFO宏
#define TEST_WARN(fmt, ...) MALOG_WARN(TEST_LOGGER_NAME, fmt, ##__VA_ARGS__)
#define TEST_INFO(fmt, ...) MALOG_INFO(TEST_LOGGER_NAME, fmt, ##__VA_ARGS__)

#endif // TEST_LOG_H