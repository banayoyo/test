#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <atomic>
#include "../common/log.h"
#include "../../engine_base/no_copy_move.h"

namespace proj {
namespace event {

// ===================== 1. CRTP事件基类 =====================
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

// ===================== 2. 事件类型定义 =====================
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

// ===================== 3. 二次处理器 =====================
class TensorHandler {
public:
    void handle(const TensorEvent& event) {
        PROJ_INFO("TensorHandler: CreateTensor name={}, dtype={}, shape=[",
                  event.name(), event.dtype());
        for (size_t i = 0; i < event.shape().size(); ++i) {
            if (i > 0) PROJ_INFO(", ");
            PROJ_INFO("{}", event.shape()[i]);
        }
        PROJ_INFO("]");
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

// ===================== 4. 主类ApiBase（仅定义通用模板，无类内特化） =====================
class ApiBase : public NoCopyMove {
public:
    // 构造函数：初始化原子标志位
    ApiBase() : destroyed_(false) {}

    // 析构函数：线程安全销毁
    ~ApiBase() {
        destroyed_.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mtx_);
        PROJ_INFO("ApiBase destroyed, registered handler count: {}", handlers_.size());
    }

    // 注册事件处理函数（线程安全）
    template <typename EventType>
    void register_handler(std::function<void(const EventType&)> handler) {
        if (destroyed_.load(std::memory_order_relaxed)) {
            PROJ_WARN("ApiBase has been destroyed, ignore register handler for {}", typeid(EventType).name());
            return;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        handlers_[EventType::type()] = [handler](const void* event_ptr) {
            handler(*static_cast<const EventType*>(event_ptr));
        };
        PROJ_INFO("Registered custom handler for {}", typeid(EventType).name());
    }

    // 处理事件（线程安全+延迟注册）
    template <typename EventType>
    void process(const EventType& event) {
        if (destroyed_.load(std::memory_order_relaxed)) {
            PROJ_WARN("ApiBase has been destroyed, ignore process event for {}", typeid(EventType).name());
            return;
        }

        std::lock_guard<std::mutex> lock(mtx_);
        auto type = EventType::type();
        auto it = handlers_.find(type);

        // 首次处理时自动注册默认处理器
        if (it == handlers_.end()) {
            register_default_handler<EventType>(); // 调用特化/通用模板
            it = handlers_.find(type);
        }

        if (it != handlers_.end()) {
            it->second(&event);
        } else {
            PROJ_WARN("No handler registered for event type: {}", typeid(EventType).name());
        }
    }

private:
    // 通用版本：成员模板（仅声明，类外无通用实现，直接在类内定义警告逻辑）
    template <typename EventType>
    void register_default_handler() {
        PROJ_WARN("No default handler defined for event type: {}", typeid(EventType).name());
    }

private:
    std::atomic<bool> destroyed_;               // 析构标志位
    std::mutex mtx_;                            // 线程安全锁
    TensorHandler tensor_handler_;              // Tensor二次处理器
    OpHandler op_handler_;                      // Op二次处理器
    std::unordered_map<std::type_index, std::function<void(const void*)>> handlers_; // 处理器映射
};

// ===================== 5. 命名空间中显式特化成员模板（核心！） =====================
// 特化TensorEvent的默认处理器
template <>
void ApiBase::register_default_handler<TensorEvent>() {
    this->register_handler<TensorEvent>([this](const TensorEvent& e) {
        this->tensor_handler_.handle(e); // 直接访问私有成员
    });
    PROJ_INFO("Lazy registered default handler for TensorEvent");
}

// 特化OpAddEvent的默认处理器
template <>
void ApiBase::register_default_handler<OpAddEvent>() {
    this->register_handler<OpAddEvent>([this](const OpAddEvent& e) {
        this->op_handler_.handle(e); // 直接访问私有成员
    });
    PROJ_INFO("Lazy registered default handler for OpAddEvent");
}

// 特化OpMMAEvent的默认处理器
template <>
void ApiBase::register_default_handler<OpMMAEvent>() {
    this->register_handler<OpMMAEvent>([this](const OpMMAEvent& e) {
        this->op_handler_.handle(e); // 直接访问私有成员
    });
    PROJ_INFO("Lazy registered default handler for OpMMAEvent");
}

} // namespace event
} // namespace proj