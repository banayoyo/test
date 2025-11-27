#include "back.h"
#include "../common/log.h"
#include <string.h>
namespace proj {
namespace back {

void BackClass::process_data(int data) {
    // 调用业务日志宏
    std::string var= "just testing";
    PROJ_INFO("BackClass process data: 0x{:<8x}, var {}", data, var); // 0x-a
    if (data < 0) {
        PROJ_WARN("BackClass invalid data: 0x{:<08x}", uint(data)); // 0xfffffff6
    }
}

} // namespace back
} // namespace proj