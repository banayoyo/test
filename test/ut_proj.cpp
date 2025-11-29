#include "log.h"
#include "../proj/front/front.h"
#include "../proj/back/back.h"
#include "../engine_base/no_copy_move.h"
#include <gtest/gtest.h>
#include <type_traits> // 必须包含类型特性头文件

#include "../handler/api_base.h"
#include <any>
#include <string>
#include <chrono>

namespace proj_test {
// ========== 测试1：继承nocopy（仅禁用拷贝，允许移动） ==========
    class TestNoCopy : public NoCopy {
    public:
        explicit TestNoCopy(int val) : value_(val) {
            TEST_INFO("TestNoCopy 构造: {}", value_);
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
            TEST_INFO("TestNoCopyMove 构造: {}", message_);
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
    // 移动构造
    proj_test::TestNoCopy t1_move = std::move(t1);
    TEST_INFO("TestNoCopy 移动后的值: {}" ,t1_move.getValue());
    // 移动赋值
    proj_test::TestNoCopy t1_move_assign(30);
    t1_move_assign = std::move(t1_move);
    TEST_INFO("TestNoCopy 移动赋值后的值: {}" , t1_move_assign.getValue());
    // 拷贝+移动均被禁用
    proj_test::TestNoCopyMove t2("hello nocopymove");
    static_assert(!std::is_copy_constructible_v<proj_test::TestNoCopyMove>,
        "TestNoCopyMove should be copy constructible");
    static_assert(!std::is_move_assignable_v<proj_test::TestNoCopyMove>,
        "TestNoCopyMove should be move assignable");

    TEST_WARN("CopyMoveTest finished");
}

// 基础功能测试（原EventHandlerTest改为ApiBaseTest）
TEST(ApiBaseTest, BasicFunctionality) {
    proj::event::ApiBase api;
    proj::event::TensorEvent tensor_event("tensor_0", {2, 3, 4}, "float32");
    api.process(tensor_event);

    proj::event::OpAddEvent add_event("add_0", "tensor_0", "tensor_1", "tensor_2");
    api.process(add_event);

    proj::event::OpMMAEvent mma_event("mma_0", "tensor_3", "tensor_4", "tensor_5", "tensor_6");
    api.process(mma_event);
}

// 动态注册测试
TEST(ApiBaseTest, DynamicRegistration) {
    proj::event::ApiBase api;
    bool custom_handled = false;

    class CustomEvent : public proj::event::Event<CustomEvent> {
    public:
        int value() const { return 42; }
    };

    api.register_handler<CustomEvent>([&](const CustomEvent& e) {
        PROJ_INFO("CustomEvent handled, value={}", e.value());
        custom_handled = true;
    });

    api.process(CustomEvent());
    EXPECT_TRUE(custom_handled);
}
// 线程安全测试
TEST(ApiBaseTest, ThreadSafety) {
    proj::event::ApiBase api;
    const int kThreadCount = 8;
    const int kEventsPerThread = 100;
    std::atomic<int> total_processed(0);

    auto worker = [&](int thread_id) {
        for (int i = 0; i < kEventsPerThread; ++i) {
            switch (i % 3) {
                case 0: {
                    proj::event::TensorEvent event("t" + std::to_string(thread_id), {1, 2}, "float16");
                    api.process(event);
                    break;
                }
                case 1: {
                    proj::event::OpAddEvent event("add" + std::to_string(thread_id), "a", "b", "c");
                    api.process(event);
                    break;
                }
                case 2: {
                    proj::event::OpMMAEvent event("mma" + std::to_string(thread_id), "a", "b", "c", "d");
                    api.process(event);
                    break;
                }
            }
            total_processed++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_processed, kThreadCount * kEventsPerThread);
}

// 析构线程安全测试：验证对象销毁后调用process/register_handler不会崩溃
TEST(ApiBaseTest, DestructionThreadSafety) {
    std::atomic<bool> test_done(false);
    std::shared_ptr<proj::event::ApiBase> api = std::make_shared<proj::event::ApiBase>();

    // lazy atomic
    std::thread worker1([&]() {
        while (!test_done.load(std::memory_order_relaxed)) {
            proj::event::TensorEvent event("test", {1}, "float32");
            api->process(event);
            std::this_thread::yield();
        }
    });

    std::thread worker2([&]() {
        while (!test_done.load(std::memory_order_relaxed)) {
            api->register_handler<proj::event::OpAddEvent>([](const auto& e) {
                PROJ_INFO("Custom OpAdd handler in worker2");
            });
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // api.reset();
    test_done.store(true, std::memory_order_relaxed);
    api.reset();
    worker1.join();
    worker2.join();
    EXPECT_TRUE(true);
}

int main(int argc, char **argv) {
    // 设置全局日志级别为INFO（可改为WARN/DEBUG等）
    // export PROJ_LOG_LEVEL=debug
    // proj_logger::set_global_log_level(proj_logger::LogLevel::OFF);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}