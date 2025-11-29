#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "../common/log.h"
#include "../../engine_base/no_copy_move.h"

namespace proj {
namespace event {

// CRTP事件基类
template <typename Derived>
class Event {
public:
    static std::type_index type() {
        return typeid(Derived);
    }

    const Derived& as_derived() const {
        return static_cast<const Derived&>(*this);
    }
};

// 事件类型定义（保持不变）
class TensorEvent : public Event<TensorEvent> {
public:
    TensorEvent(std::string name, std::vector<int64_t> shape, std::string dtype)
        : name_(std::move(name)), shape_(std::move(shape)), dtype_(std::move(dtype)) {}

    const std::string& name() const { return name_; }
    const std::vector<int64_t>& shape() const { return shape_; }
    const std::string& dtype() const { return dtype_; }

private:
    std::string name_;
    std::vector<int64_t> shape_;
    std::string dtype_;
};

class OpAddEvent : public Event<OpAddEvent> {
public:
    OpAddEvent(std::string name, std::string input1, std::string input2, std::string output)
        : name_(std::move(name)), input1_(std::move(input1)),
          input2_(std::move(input2)), output_(std::move(output)) {}

    const std::string& name() const { return name_; }
    const std::string& input1() const { return input1_; }
    const std::string& input2() const { return input2_; }
    const std::string& output() const { return output_; }

private:
    std::string name_;
    std::string input1_;
    std::string input2_;
    std::string output_;
};

class OpMMAEvent : public Event<OpMMAEvent> {
public:
    OpMMAEvent(std::string name, std::string a, std::string b, std::string c, std::string output)
        : name_(std::move(name)), a_(std::move(a)), b_(std::move(b)),
          c_(std::move(c)), output_(std::move(output)) {}

    const std::string& name() const { return name_; }
    const std::string& a() const { return a_; }
    const std::string& b() const { return b_; }
    const std::string& c() const { return c_; }
    const std::string& output() const { return output_; }

private:
    std::string name_;
    std::string a_;
    std::string b_;
    std::string c_;
    std::string output_;
};

// 二次处理器（保持不变）
class TensorHandler {
public:
    void handle(const TensorEvent& event) {
        PROJ_INFO("TensorHandler: CreateTensor name={}, dtype={}, shape=[",
                  event.name(), event.dtype());
        for (size_t i = 0; i < event.shape().size(); ++i) {
            if (i > 0) PROJ_DEBG(", ");
            PROJ_DEBG("{}", event.shape()[i]);
        }
        PROJ_DEBG("]");
    }
};

class OpHandler {
public:
    void handle(const OpAddEvent& event) {
        PROJ_INFO("OpHandler: CreateOpAdd name={}, {} + {} -> {}",
                  event.name(), event.input1(), event.input2(), event.output());
    }

    void handle(const OpMMAEvent& event) {
        PROJ_INFO("OpHandler: CreateOpMMA name={}, {} * {} + {} -> {}",
                  event.name(), event.a(), event.b(), event.c(), event.output());
    }
};

// 主类ApiBase（支持多线程处理和安全析构）
class ApiBase : public NoCopyMove {
public:
    ApiBase() : destroyed_(false), active_handlers_(0) {}

    ~ApiBase() {
        // 1. 标记为已销毁，阻止新的处理和注册
        destroyed_.store(true, std::memory_order_seq_cst);

        // 2. 等待所有正在处理的事件完成
        std::unique_lock<std::mutex> lock(exit_mutex_);
        exit_cv_.wait(lock, [this]() {
            return active_handlers_.load(std::memory_order_seq_cst) == 0;
        });

        // 3. 安全清理资源
        std::lock_guard<std::mutex> handler_lock(handlers_mutex_);
        handlers_.clear();
        PROJ_INFO("ApiBase destroyed, all resources released");
    }

    // 注册处理器（线程安全）
    template <typename EventType>
    void register_handler(std::function<void(const EventType&)> handler) {
        if (destroyed_.load(std::memory_order_seq_cst)) {
            PROJ_WARN("ApiBase has been destroyed, ignore register handler");
            return;
        }

        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[EventType::type()] = [handler](const void* event_ptr) {
            handler(*static_cast<const EventType*>(event_ptr));
        };
    }

    // 处理事件（多线程并行支持）
    template <typename EventType>
    void process(const EventType& event) {
        if (destroyed_.load(std::memory_order_seq_cst)) {
            PROJ_WARN("ApiBase has been destroyed, ignore process event");
            return;
        }

        // 1. 先检查是否有已注册的处理器（轻量锁）
        std::function<void(const void*)> handler;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            auto it = handlers_.find(EventType::type());
            if (it != handlers_.end()) {
                handler = it->second; // 复制处理器到栈上，释放锁后执行
            }
        }

        // 2. 如果没有处理器，注册默认处理器（加锁操作）
        if (!handler) {
            register_default_handler<EventType>();
            // 再次获取处理器
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            auto it = handlers_.find(EventType::type());
            if (it != handlers_.end()) {
                handler = it->second;
            } else {
                PROJ_WARN("No handler for event type: {}", typeid(EventType).name());
                return;
            }
        }

        // 3. 并行执行事件处理（无锁）
        active_handlers_.fetch_add(1, std::memory_order_acq_rel);
        try {
            handler(&event); // 实际处理逻辑（多线程并行执行）
        } catch (...) {
            PROJ_WARN("Exception occurred while processing event");
        }
        active_handlers_.fetch_sub(1, std::memory_order_acq_rel);
        exit_cv_.notify_one(); // 通知析构线程可能可以退出
    }

private:
    // 通用默认处理器注册（线程安全）
    template <typename EventType>
    void register_default_handler() {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        // 双重检查，避免重复注册
        if (handlers_.find(EventType::type()) != handlers_.end()) {
            return;
        }
        PROJ_WARN("No default handler defined for event type: {}", typeid(EventType).name());
    }

private:
    // 处理器映射表及保护锁
    std::unordered_map<std::type_index, std::function<void(const void*)>> handlers_;
    std::mutex handlers_mutex_;

    // 线程安全析构相关
    std::atomic<bool> destroyed_;               // 析构标志（seq_cst保证可见性）
    std::atomic<int> active_handlers_;          // 活跃处理器计数（acq_rel保证同步）
    std::mutex exit_mutex_;                     // 条件变量锁
    std::condition_variable exit_cv_;           // 析构等待条件变量

    // 二次处理器实例
    TensorHandler tensor_handler_;
    OpHandler op_handler_;
};

// 显式特化默认处理器（保持不变）
template <>
void ApiBase::register_default_handler<TensorEvent>() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (handlers_.find(TensorEvent::type()) != handlers_.end()) {
        return;
    }
    handlers_[TensorEvent::type()] = [this](const void* event_ptr) {
        this->tensor_handler_.handle(*static_cast<const TensorEvent*>(event_ptr));
    };
    PROJ_INFO("Lazy registered default handler for TensorEvent");
}

template <>
void ApiBase::register_default_handler<OpAddEvent>() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (handlers_.find(OpAddEvent::type()) != handlers_.end()) {
        return;
    }
    handlers_[OpAddEvent::type()] = [this](const void* event_ptr) {
        this->op_handler_.handle(*static_cast<const OpAddEvent*>(event_ptr));
    };
    PROJ_INFO("Lazy registered default handler for OpAddEvent");
}

template <>
void ApiBase::register_default_handler<OpMMAEvent>() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (handlers_.find(OpMMAEvent::type()) != handlers_.end()) {
        return;
    }
    handlers_[OpMMAEvent::type()] = [this](const void* event_ptr) {
        this->op_handler_.handle(*static_cast<const OpMMAEvent*>(event_ptr));
    };
    PROJ_INFO("Lazy registered default handler for OpMMAEvent");
}

} // namespace event
} // namespace proj