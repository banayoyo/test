#include "log.h"
#include "../proj/front/front.h"
#include "../proj/back/back.h"
#include <gtest/gtest.h>

TEST(ProjTest, FrontClassTest) {
    TEST_INFO("Start test FrontClass::do_work()");
    proj::front::FrontClass front;
    front.do_work();
    TEST_WARN("FrontClass test finished, check log");
}

TEST(ProjTest, BackClassTest) {
    TEST_INFO("Start test BackClass::process_data()");
    proj::back::BackClass back;
    back.process_data(100);  // 正常数据
    back.process_data(-10);  // 异常数据
    TEST_WARN("BackClass test finished, check log");
}

int main(int argc, char **argv) {
    // 设置全局日志级别为INFO（可改为WARN/DEBUG等）
    // export PROJ_LOG_LEVEL=debug
    // proj_logger::set_global_log_level(proj_logger::LogLevel::OFF);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}