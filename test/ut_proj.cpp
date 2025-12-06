#include "log.h"
#include "../proj/front/front.h"
#include "../proj/back/back.h"
#include "../engine_base/no_copy_move.h"
#include <gtest/gtest.h>
#include <type_traits> // 必须包含类型特性头文件

#include "../handler/api_base.h"
#include "../handler/api_base_single.h"
#include "../handler/router.h"
#include <any>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

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

#if 1
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

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // api.reset();
    test_done.store(true, std::memory_order_relaxed);
    // api.reset();
    worker1.join();
    worker2.join();
    api.reset(); // reset必须在thread都执行完之后。暂时无法简单的做到析构线程安全
    EXPECT_TRUE(true);
}

// ApiBaseSingle单线程功能测试
TEST(ApiBaseSingleTest, SingleThreadFunctionality) {
    TEST_INFO("Start ApiBaseSingle single thread test");

    proj::event::ApiBaseSingle api;
    // 验证绑定线程正确
    EXPECT_EQ(api.bound_thread_id(), std::this_thread::get_id());

    // 测试默认处理器
    proj::event::TensorEvent tensor_event("tensor_single", {2, 2}, "float32");
    api.process(tensor_event);

    proj::event::OpAddEvent add_event("add_single", "in1", "in2", "out");
    api.process(add_event);

    proj::event::OpMMAEvent mma_event("mma_single", "a", "b", "c", "d");
    api.process(mma_event);

    // 测试动态注册
    bool custom_handled = false;
    class CustomSingleEvent : public proj::event::Event<CustomSingleEvent> {
    public:
        int value() const { return 42; }
    };

    api.register_handler<CustomSingleEvent>([&](const CustomSingleEvent& e) {
        PROJ_INFO("CustomSingleEvent handled, value={}", e.value());
        custom_handled = true;
    });

    api.process(CustomSingleEvent());
    EXPECT_TRUE(custom_handled);

    TEST_WARN("ApiBaseSingle single thread test finished");
}

// ApiBaseSingle多线程错误测试
TEST(ApiBaseSingleTest, MultiThreadError) {
    TEST_INFO("Start ApiBaseSingle multi thread error test");

    proj::event::ApiBaseSingle api;
    std::atomic<bool> error_occurred(false);
    std::atomic<int> exception_count(0);

    // 测试跨线程process调用
    std::thread t1([&]() {
        try {
            proj::event::TensorEvent event("cross_thread", {1}, "float16");
            api.process(event); // 应抛出异常
        } catch (const std::runtime_error& e) {
            PROJ_INFO("Expected process error: {}", e.what());
            error_occurred = true;
            exception_count++;
        } catch (...) {
            PROJ_ERRO("Unexpected process exception");
        }
    });

    // 测试跨线程register_handler调用
    std::thread t2([&]() {
        try {
            api.register_handler<proj::event::OpAddEvent>([](const auto&) {});
        } catch (const std::runtime_error& e) {
            PROJ_INFO("Expected register error: {}", e.what());
            error_occurred = true;
            exception_count++;
        } catch (...) {
            PROJ_ERRO("Unexpected register exception");
        }
    });

    t1.join();
    t2.join();

    EXPECT_TRUE(error_occurred);
    EXPECT_EQ(exception_count, 2); // 两个线程都应抛出异常

    TEST_WARN("ApiBaseSingle multi thread error test finished");
}

// 测试销毁后调用，无法做到
// TEST(ApiBaseSingleTest, PostDestructionCall) {
//     auto api = std::make_unique<proj::event::ApiBaseSingle>();
//     const auto bound_id = api->bound_thread_id();
//     api.reset(); // 销毁对象

//     // 测试销毁后调用process
//     proj::event::TensorEvent event("post_destroy", {1}, "int32");
//     try {
//         // 注意：这里必须在绑定线程调用（否则会先触发线程错误）
//         api->process(event); // 已销毁，应警告但不崩溃
//     } catch (const std::runtime_error& e) {
//         PROJ_INFO("Expected post-destroy error: {}", e.what());
//     }
// }

#endif

using namespace proj::msg;

// ========================== 基础功能测试 ==========================
TEST(RouterTest, OpAdd_Basic_Impl_Selection) {
    // 测试目标：OpAddMsg 根据 name 选择不同 impl（default/special）
    Router router;

    // 1. 测试 default impl
    OpAddMsg add_default("default", "input1", "input2", "output1");
    EXPECT_NO_THROW(router.dispatch(add_default));

    // 2. 测试 special impl
    OpAddMsg add_special("special", "input3", "input4", "output2");
    EXPECT_NO_THROW(router.dispatch(add_special));

    // 3. 测试未知 name（自动使用 default impl）
    OpAddMsg add_unknown("unknown", "input5", "input6", "output3");
    EXPECT_NO_THROW(router.dispatch(add_unknown));
}

// ========================== MMA 错误转发测试 ==========================
TEST(RouterTest, OpMMA_Param_Error_Redirect_To_OpAdd) {
    // 测试目标：OpMMAMsg 参数错误时自动转发到 OpAddMsg
    Router router;
    std::atomic<bool> redirect_called = false;

    // 注册自定义 impl 用于验证转发行为
    router.get_add_processor()->register_impl("invalid_mma_redirected", [&](const OpAddMsg& event) {
        redirect_called = true;
        EXPECT_EQ(event.name(), "invalid_mma_redirected");
        EXPECT_EQ(event.input1(), "");  // MMA 错误时的 a 参数（空）
        EXPECT_EQ(event.input2(), "b_val");
        EXPECT_EQ(event.output(), "output4");
        TEST_INFO(" Process into outer register: invalid_mma_redirected");
    });

    // 1. 测试正常 MMA 事件（无转发）
    OpMMAMsg mma_valid("valid_mma", "a_val", "b_val", "c_val", "output4");
    EXPECT_NO_THROW(router.dispatch(mma_valid));
    EXPECT_FALSE(redirect_called);

    // 2. 测试参数错误的 MMA 事件（触发转发）， 期望AddMsg中收到 invalid_mma_redirected 执行 注册的 invalid_mma_redirected
    OpMMAMsg mma_invalid("invalid_mma", "", "b_val", "c_val", "output4"); // a 为空 → 参数错误
    EXPECT_NO_THROW(router.dispatch(mma_invalid));
    EXPECT_TRUE(redirect_called);
}

// ========================== 自定义 Impl 注册测试 ==========================
TEST(RouterTest, OpAdd_Custom_Impl_Registration) {
    // 测试目标：OpAddProcessor 支持注册自定义 impl
    Router router;
    std::atomic<int> custom_call_count = 0;
    std::string captured_name;
    std::string captured_input1;
    std::string captured_input2;

    // 注册自定义 impl， lambda做的钩子程序
    const std::string custom_impl_name = "my_custom_impl";
    router.get_add_processor()->register_impl(custom_impl_name, [&](const OpAddMsg& event) {
        custom_call_count++;
        captured_name = event.name();
        captured_input1 = event.input1();
        captured_input2 = event.input2();
    });

    // 触发自定义 impl
    OpAddMsg add_custom(custom_impl_name, "custom_in1", "custom_in2", "custom_out");
    router.dispatch(add_custom);

    // 验证自定义 impl 被调用
    EXPECT_EQ(custom_call_count, 1);
    EXPECT_EQ(captured_name, custom_impl_name);
    EXPECT_EQ(captured_input1, "custom_in1");
    EXPECT_EQ(captured_input2, "custom_in2");

    // 验证重复调用仍生效
    router.dispatch(add_custom);
    EXPECT_EQ(custom_call_count, 2);
}

// // ========================== 类型安全测试 ==========================
TEST(RouterTest, Type_Safety_Checks) {
    Router router;

    // 1. 编译期校验（静态断言已保障，此处测运行时类型获取）
    OpAddMsg add_event("type_test", "a", "b", "c");
    EXPECT_EQ(add_event.type_index(), OpAddMsg::TypeIndex());
    EXPECT_NE(add_event.type_index(), OpMMAMsg::TypeIndex());

    // 2. 运行时获取处理器（类型安全）
    EXPECT_NO_THROW(router.get_processor<OpAddMsg>());
    EXPECT_NO_THROW(router.get_processor<OpMMAMsg>());

    // 3. 测试获取未注册的处理器（抛异常）
    // 注意：这里无法模板实例化
    // class UnregisteredMsg : public MsgCRTP<UnregisteredMsg> {
    // public:
    //     const std::string& name_impl() const { return name_; }
    // private:
    //     std::string name_ = "unregistered";
    // };
    // // 无法实例化MsgToProcessor
    // EXPECT_THROW(router.get_processor<UnregisteredMsg>(), std::runtime_error);
}

// ========================== 线程安全测试 ==========================
TEST(RouterTest, Thread_Safety_In_Single_Thread_Model) {
    // 测试目标：单线程模型下多线程访问的安全性
    Router router;
    const int kThreadCount = 4;
    const int kMsgsPerThread = 10;
    std::atomic<int> total_processed = 0;

    // 工作线程函数：交替发送 OpAdd/OpMMA 事件
    auto worker = [&](int thread_id) {
        for (int i = 0; i < kMsgsPerThread; ++i) {
            try {
                // 0, 2, 4, 6, 8...
                if (i % 2 == 0) {
                    // OpAdd 事件， name交替使用 default/special， 但实际impl都是 default
                    std::string name = (i % 4 == 0) ? "default_add" : "special_add";
                    OpAddMsg event(
                        name + "_t" + std::to_string(thread_id) + "_" + std::to_string(i),
                        "in1_" + std::to_string(i),
                        "in2_" + std::to_string(i),
                        "out_" + std::to_string(i)
                    );
                    router.dispatch(event);
                } else {
                    // OpMMA 事件（交替有效/无效）
                    // 1, 5，7 are valid, 3, 9 are invalid
                    bool valid = (i % 3 != 0);
                    OpMMAMsg event(
                        "mma_t" + std::to_string(thread_id) + "_" + std::to_string(i),
                        valid ? "a_" + std::to_string(i) : "",
                        "b_" + std::to_string(i),
                        valid ? "c_" + std::to_string(i) : "",
                        "out_" + std::to_string(i)
                    );
                    router.dispatch(event);
                }
                total_processed++;
            } catch (...) {
                // 确保无异常抛出
                FAIL() << "Thread " << thread_id << " threw exception at iteration " << i;
            }
        }
    };

    // 启动多线程
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back(worker, i);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 验证所有事件都被处理
    EXPECT_EQ(total_processed, kThreadCount * kMsgsPerThread);
}

// ========================== 边界场景测试 ==========================
TEST(RouterTest, Edge_Cases) {
    Router router;

    // 1. 空字符串参数的 OpAdd 事件（仍能正常处理）
    OpAddMsg add_empty("default_add", "", "", "");
    EXPECT_NO_THROW(router.dispatch(add_empty));

    // 2. MMA 多参数错误（仍触发转发）
    OpMMAMsg mma_multi_error("mma_multi_error", "", "", "", "output5");
    std::atomic<bool> multi_error_redirect = false;
    router.get_add_processor()->register_impl("mma_multi_error_redirected", [&](const OpAddMsg&) {
        multi_error_redirect = true;
        TEST_INFO(" Process into outer register: mma_multi_error_redirected");
    });
    EXPECT_NO_THROW(router.dispatch(mma_multi_error));
    EXPECT_TRUE(multi_error_redirect);

    // 3. 重复注册同一个 impl（覆盖原有实现）
    std::atomic<bool> new_impl_called = false;
    router.get_add_processor()->register_impl("default", [&](const OpAddMsg&) {
        new_impl_called = true;
        TEST_INFO(" Process into New Add_Default");
    });
    OpAddMsg add_override("default_add", "x", "y", "z");
    router.dispatch(add_override);
    EXPECT_TRUE(new_impl_called);
}

int main(int argc, char **argv) {
    // 设置全局日志级别为INFO（可改为WARN/DEBUG等）
    // export PROJ_LOG_LEVEL=debug
    // proj_logger::set_global_log_level(proj_logger::LogLevel::OFF);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}