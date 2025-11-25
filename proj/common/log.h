#ifndef PROJ_COMMON_LOG_H
#define PROJ_COMMON_LOG_H

#include "../../proj_logger/proj_logger.h"

// 业务模块日志器名称：固定为"proj_logger"
#define PROJ_LOGGER_NAME "PROJ"

// 用户要求的PROJ_WARN/INFO宏
#define PROJ_WARN(fmt, ...) MALOG_WARN(PROJ_LOGGER_NAME, fmt, ##__VA_ARGS__)
#define PROJ_INFO(fmt, ...) MALOG_INFO(PROJ_LOGGER_NAME, fmt, ##__VA_ARGS__)

#endif // PROJ_COMMON_LOG_H