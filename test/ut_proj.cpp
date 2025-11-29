#include "log.h"
#include "../proj/front/front.h"
#include "../proj/back/back.h"
#include "../engine_base/no_copy_move.h"
#include <gtest/gtest.h>

namespace proj_test {
// ========== 测试1：继承nocopy（仅禁用拷贝，允许移动） ==========
    class TestNoCopy : public NoCopy {
    public:
        explicit TestNoCopy(int val) : value_(val) {
            std::cout << "TestNoCopy 构造: " << value_ << std::endl;
        }

        TestNoCopy(TestNoCopy&& other) noexcept = default;
        TestNoCopy& operator=(TestNoCopy&& other) noexcept = default;

        int getValue() const { return value_; }

    private:
        int value_;
    };

    // ========== 测试2：继承nocopymove（禁用拷贝+移动） ==========
    class TestNoCopyMove : public NoCopyMove {
    public:
        explicit TestNoCopyMove(const std::string& msg) : message_(msg) {
            std::cout << "TestNoCopyMove 构造: " << message_ << std::endl;
        }

        std::string getMessage() const { return message_; }

    private:
        std::string message_;
    };
}  // namespace proj_test

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

TEST(ProjTest, CopyMoveTest) {
    TEST_INFO("Start CopyMovetest");

    proj_test::TestNoCopy t1(10);
    // 尝试移动构造（允许）：正常执行
    proj_test::TestNoCopy t1_move = std::move(t1);
    std::cout << "TestNoCopy 移动后的值: " << t1_move.getValue() << std::endl;

    // 尝试移动赋值（允许）：正常执行
    proj_test::TestNoCopy t1_move_assign(30);
    t1_move_assign = std::move(t1_move);
    std::cout << "TestNoCopy 移动赋值后的值: " << t1_move_assign.getValue() << std::endl;

    // ---------------- 拷贝+移动均被禁用 ----------------
    proj_test::TestNoCopyMove t2("hello nocopymove");

    TEST_WARN("CopyMoveTest finished");
}

int main(int argc, char **argv) {
    // 设置全局日志级别为INFO（可改为WARN/DEBUG等）
    // export PROJ_LOG_LEVEL=debug
    // proj_logger::set_global_log_level(proj_logger::LogLevel::OFF);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}