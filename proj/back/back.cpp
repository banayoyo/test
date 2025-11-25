#include "back.h"
#include "../common/log.h"

namespace proj {
namespace back {

void BackClass::process_data(int data) {
    // 调用业务日志宏
    PROJ_INFO("BackClass process data: {}", data);
    if (data < 0) {
        PROJ_WARN("BackClass invalid data: {}", data);
    }
}

} // namespace back
} // namespace proj