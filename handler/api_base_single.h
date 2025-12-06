#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <thread>
#include <stdexcept>
#include <cassert>
#include "../common/log.h"
#include "../../engine_base/no_copy_move.h"
#include "api_base.h"

namespace proj {
namespace event {

// ApiBaseSingle类实现
class ApiBaseSingle : public NoCopyMove {
public:
    // C++17: constexpr构造函数
    ApiBaseSingle() noexcept
        : bound_thread_id_(std::this_thread::get_id()),
          destroyed_(false) {}

    // C++17: noexcept析构函数
    ~ApiBaseSingle() noexcept {
        destroyed_ = true;
        PROJ_INFO("ApiBaseSingle destroyed, registered handler count: {}", handlers_.size());
    }

    // 注册处理器 - 仅允许绑定线程调用
    template <typename EventType>
    void register_handler(std::function<void(const EventType&)> handler) {
        check_thread();
        if (destroyed_) {
            PROJ_WARN("ApiBaseSingle has been destroyed, ignore register handler");
            return;
        }

        handlers_[EventType::type()] = [handler = std::move(handler)](const void* event_ptr) {
            handler(*static_cast<const EventType*>(event_ptr));
        };
    }

    // 处理事件 - 仅允许绑定线程调用
    template <typename EventType>
    void process(const EventType& event) {
        check_thread();
        if (destroyed_) {
            PROJ_WARN("ApiBaseSingle has been destroyed, ignore process event");
            return;
        }

        const auto type = EventType::type();
        auto it = handlers_.find(type);
        if (it != handlers_.end()) {
            it->second(&event);
            return;
        }

        // 延迟注册默认处理器
        register_default_handler<EventType>();
        it = handlers_.find(type);
        if (it != handlers_.end()) {
            it->second(&event);
        } else {
            PROJ_WARN("No handler registered for event type: {}", typeid(EventType).name());
        }
    }

    // C++17: 获取绑定的线程ID
    std::thread::id bound_thread_id() const noexcept {
        return bound_thread_id_;
    }

private:
    // 线程检查：非绑定线程调用则报错
    void check_thread() const {
        const auto current_thread = std::this_thread::get_id();
        if (current_thread != bound_thread_id_) {
            PROJ_ERRO("ApiBaseSingle accessed from wrong thread! "
                      "Bound: {}, Current: {}",
                      std::hash<std::thread::id>{}(bound_thread_id_),
                      std::hash<std::thread::id>{}(current_thread));
            throw std::runtime_error("Cross-thread access to ApiBaseSingle");
        }
    }

    // 默认处理器注册（通用版本）
    template <typename EventType>
    void register_default_handler() {
        PROJ_WARN("No default handler for event type: {}", typeid(EventType).name());
    }

private:
    const std::thread::id bound_thread_id_;  // 绑定的线程ID
    bool destroyed_;                         // 销毁标志
    std::unordered_map<std::type_index, std::function<void(const void*)>> handlers_;
    TensorHandler tensor_handler_;           // 内置Tensor处理器
    OpHandler op_handler_;                   // 内置Op处理器
};

// 特化默认处理器（C++17成员模板特化）
template <>
inline void ApiBaseSingle::register_default_handler<TensorEvent>() {
    register_handler<TensorEvent>([this](const TensorEvent& e) {
        tensor_handler_.handle(e);
    });
    PROJ_INFO("Lazy registered default TensorEvent handler");
}

template <>
inline void ApiBaseSingle::register_default_handler<OpAddEvent>() {
    register_handler<OpAddEvent>([this](const OpAddEvent& e) {
        op_handler_.handle(e);
    });
    PROJ_INFO("Lazy registered default OpAddEvent handler");
}

template <>
inline void ApiBaseSingle::register_default_handler<OpMMAEvent>() {
    register_handler<OpMMAEvent>([this](const OpMMAEvent& e) {
        op_handler_.handle(e);
    });
    PROJ_INFO("Lazy registered default OpMMAEvent handler");
}

} // namespace event
} // namespace proj