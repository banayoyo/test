#include "front.h"
#include "../common/log.h"

namespace proj {
namespace front {

void FrontClass::do_work() {
    // 调用业务日志宏
    PROJ_INFO("FrontClass do work start");
    PROJ_WARN("FrontClass low performance, current thread: {}", (unsigned long)pthread_self());
}

} // namespace front
} // namespace proj