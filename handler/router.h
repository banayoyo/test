#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>
#include <typeindex>
#include <type_traits>
#include <any>
#include <stdexcept>
#include "api_base.h"
#include "../proj/common/log.h"

namespace proj {
namespace msg {

// ========================== 事件CRTP基类（零虚函数，纯静态多态） ==========================
template <typename Derived>
class MsgCRTP : public NoCopyMove {
public:
    const std::string& name() const {
        // 编译期强制检查：派生类必须实现 name_impl()
        static_assert(
            std::is_invocable_r_v<const std::string&, decltype(&Derived::name_impl), const Derived*>,
            "Derived msg must implement 'const std::string& name_impl() const'"
        );
        return static_cast<const Derived*>(this)->name_impl();
    }

    // 静态获取事件类型索引（编译期常量，无运行时开销）
    static std::type_index TypeIndex() {
        return std::type_index(typeid(Derived));
    }

    // 成员方法获取类型索引
    std::type_index type_index() const {
        return Derived::TypeIndex();
    }

    // 非虚析构：静态多态下，不会用基类指针持有派生类对象，无需虚析构
    ~MsgCRTP() = default;

protected:
    // 保护构造：仅允许派生类实例化（防止直接创建基类对象）
    MsgCRTP() = default;
};

// ========================== 具体事件定义（零虚函数） ==========================
class OpAddMsg : public MsgCRTP<OpAddMsg> {
public:
    OpAddMsg(std::string name, std::string input1, std::string input2, std::string output)
        : name_(std::move(name)), input1_(std::move(input1)), input2_(std::move(input2)), output_(std::move(output)) {}

    // 静态多态要求的具体实现（替代原虚函数）
    const std::string& name_impl() const { return name_; }

    // 纯静态参数访问器（无虚函数）
    const std::string& input1() const { return input1_; }
    const std::string& input2() const { return input2_; }
    const std::string& output() const { return output_; }

    // 非虚析构（默认生成，无额外开销）
    ~OpAddMsg() = default;

private:
    std::string name_;
    std::string input1_;
    std::string input2_;
    std::string output_;
};

class OpMMAMsg : public MsgCRTP<OpMMAMsg> {
public:
    OpMMAMsg(std::string name, std::string a, std::string b, std::string c, std::string output)
        : name_(std::move(name)), a_(std::move(a)), b_(std::move(b)), c_(std::move(c)), output_(std::move(output)) {}

    // 静态多态要求的具体实现（替代原虚函数）
    const std::string& name_impl() const { return name_; }

    // 纯静态参数访问器（无虚函数）
    const std::string& a() const { return a_; }
    const std::string& b() const { return b_; }
    const std::string& c() const { return c_; }
    const std::string& output() const { return output_; }

    // 非虚析构（默认生成）
    ~OpMMAMsg() = default;

private:
    std::string name_;
    std::string a_;
    std::string b_;
    std::string c_;
    std::string output_;
};

// ========================== 处理器CRTP基类（零虚函数，纯静态多态） ==========================
template <typename Derived, typename MsgType>
class MsgProcessorCRTP : public NoCopyMove {
public:
    // 静态多态：编译期转发到派生类的process_impl（无虚函数）
    void process(const MsgType& msg) {
        // 编译期强制检查：派生类必须实现 process_impl()
        static_assert(
            std::is_invocable_r_v<void, decltype(&Derived::process_impl), Derived*, const MsgType&>,
            "Derived processor must implement 'void process_impl(const MsgType&)'"
        );
        static_cast<Derived*>(this)->process_impl(msg);
    }

    // 非虚析构：静态多态下无需虚析构
    ~MsgProcessorCRTP() = default;

protected:
    // 保护构造：仅允许派生类实例化
    MsgProcessorCRTP() = default;
};

// ========================== 具体处理器实现（零虚函数） ==========================
class OpAddProcessor : public MsgProcessorCRTP<OpAddProcessor, OpAddMsg> {
public:
    using ImplFunc = std::function<void(const OpAddMsg&)>;

    OpAddProcessor() {
        register_impl("default", [this](const OpAddMsg& msg) { impl_default(msg); });
        register_impl("special", [this](const OpAddMsg& msg) { impl_special(msg); });
    }

    void process_impl(const OpAddMsg& msg) {
        auto it = impls_.find(msg.name());
        (it != impls_.end() ? it->second : impls_["default"])(msg);
    }

    // 注册自定义实现（线程安全）
    void register_impl(const std::string& name, ImplFunc func) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 容错处理
        impls_[name] = func;
    }

    ~OpAddProcessor() = default; // 非虚析构

private:
    void impl_default(const OpAddMsg& msg) {
        PROJ_INFO("OpAdd[default] - {}: {} + {} -> {}",
                  msg.name(), msg.input1(), msg.input2(), msg.output());
    }

    void impl_special(const OpAddMsg& msg) {
        PROJ_INFO("OpAdd[special] - {}: {} + {} -> {}",
                  msg.name(), msg.input1(), msg.input2(), msg.output());
    }

    std::unordered_map<std::string, ImplFunc> impls_;
    std::mutex mutex_;
};

class OpMMAProcessor : public MsgProcessorCRTP<OpMMAProcessor, OpMMAMsg> {
public:
    void process_impl(const OpMMAMsg& msg) {
        PROJ_INFO("OpMMA - {}: {} * {} + {} -> {}",
                  msg.name(), msg.a(), msg.b(), msg.c(), msg.output());
    }

    ~OpMMAProcessor() = default; // 非虚析构
};

// ========================== 编译期类型关联（事件→处理器） ==========================
template <typename MsgType> struct MsgToProcessor;
template <> struct MsgToProcessor<OpAddMsg> { using Type = OpAddProcessor; };
template <> struct MsgToProcessor<OpMMAMsg> { using Type = OpMMAProcessor; };

// ========================== 核心：5个语义化宏定义（放在Router前，便于类内使用） ==========================
// 宏1：模板版处理器注册（通用，对应用户要求的第二个宏）
#define REGISTER_PROCESSOR(msg_prefix) \
    do { \
        using MsgType = msg_prefix##Msg; \
        using ProcessorType = msg_prefix##Processor; \
        register_processor<MsgType, ProcessorType>(std::make_shared<ProcessorType>()); \
    } while (0)

// 宏2：重载版处理器注册（语义化命名，逻辑同通用版，对应用户要求的第三个宏）
#define REGISTER_PROCESSOR_OVERLOAD(msg_prefix) REGISTER_PROCESSOR(msg_prefix)

// 宏3：模板版处理函数注册（绑定process_msg<MsgType>，对应用户要求的第四个宏）
#define REGISTER_HANDLER_TEMPLATE(msg_prefix) \
    do { \
        using MsgType = msg_prefix##Msg; \
        register_handler<MsgType>(&Router::process_msg<MsgType>); \
    } while (0)

// 宏4：重载版处理函数注册（自动解析重载版process_msg，对应用户要求的第五个宏）
#define REGISTER_HANDLER_OVERLOAD(msg_prefix) \
    do { \
        using MsgType = msg_prefix##Msg; \
        using HandlerFunc = void (Router::*)(const MsgType&); \
        HandlerFunc overload_handler = &Router::process_msg; \
        register_handler<MsgType>(overload_handler); \
    } while (0)

// 宏5：模板版整合宏（复用模板版processor+handler，对应用户要求的第一个宏）
#define REGISTER_MSG_HANDLER_TEMPLATE(msg_prefix) \
    do { \
        REGISTER_PROCESSOR(msg_prefix); \
        REGISTER_HANDLER_TEMPLATE(msg_prefix); \
    } while (0)


// ========================== 路由核心类（零虚函数 + 通用化） ==========================
class Router : public NoCopyMove {
public:

    template <typename MsgType>
    void process_msg(const MsgType& msg);

    // ========================== 2. 仅重载 OpMMAMsg 版本（OpAddMsg 不重载） ==========================
    // 否则无法解决模板实例化和特例化的顺序问题
    void process_msg(const OpMMAMsg& msg) {
        PROJ_INFO("process_msg<OpMMAMsg>.name = {}", msg.name());
        if (has_mma_param_error(msg)) {
            PROJ_WARN("OpMMA {} has parameter error, redirect to OpAdd", msg.name());
            // 转换为 OpAddMsg，复用模板版 process_msg
            // FIXME:: 这里强制转发为 msg.name() + "_redirected",
            OpAddMsg redirect_msg(
                msg.name() + "_redirected",
                msg.a(), msg.b(), msg.output()
            );
            this->process_msg(redirect_msg); // 自动匹配模板版（OpAddMsg 无重载）
        } else {
            auto processor = get_processor<OpMMAMsg>();
            processor->process(msg); // 调用 OpMMAProcessor 逻辑
        }
    }

    Router() {
        // OpAdd：模板版整合宏（一行搞定，兼容原有逻辑）
        REGISTER_MSG_HANDLER_TEMPLATE(OpAdd);

        // OpMMA：重载版注册（processor+handler分开，语义化）
        REGISTER_PROCESSOR_OVERLOAD(OpMMA);
        REGISTER_HANDLER_OVERLOAD(OpMMA);
    }

    template <typename MsgType>
    void dispatch(const MsgType& msg) {
        // 编译期检查：事件必须继承自 MsgCRTP<MsgType>
        PROJ_INFO("dispatch<Msg>.name = {}", msg.name());
        static_assert(
            std::is_base_of_v<MsgCRTP<MsgType>, MsgType>,
            "MsgType must inherit from MsgCRTP<MsgType> (CRTP static polymorphism)"
        );

        std::lock_guard<std::mutex> lock(mutex_); // 单线程安全保障
        auto it = handler_map_.find(MsgType::TypeIndex());
        if (it != handler_map_.end()) {
            it->second(reinterpret_cast<const void*>(&msg));
        } else {
            PROJ_ERRO("Unsupported msg type: {}", typeid(MsgType).name());
        }
    }

    // 通用化处理器获取（编译期类型安全）
    template <typename MsgType>
    std::shared_ptr<typename MsgToProcessor<MsgType>::Type> get_processor() {
        using ProcessorType = typename MsgToProcessor<MsgType>::Type;
        auto it = processor_map_.find(MsgType::TypeIndex());
        if (it == processor_map_.end()) {
            throw std::runtime_error(
                "Processor not registered for msg: " + std::string(typeid(MsgType).name())
            );
        }

        // 编译期类型转换（无运行时开销）
        return std::any_cast<std::shared_ptr<ProcessorType>>(it->second);
    }

    // 语法糖：简化 OpAdd 处理器获取
    std::shared_ptr<OpAddProcessor> get_add_processor() {
        return get_processor<OpAddMsg>();
    }

    ~Router() = default; // 非虚析构

private:
    // ========================== 类型别名（简化模板） ==========================
    using MsgHandler = std::function<void(const void*)>;
    using ProcessorMap = std::unordered_map<std::type_index, std::any>;
    using HandlerMap = std::unordered_map<std::type_index, MsgHandler>;

    // ========================== 通用注册逻辑（编译期绑定） ==========================
    // 注册处理器（编译期类型校验）
    template <typename MsgType, typename ProcessorType>
    void register_processor(std::shared_ptr<ProcessorType> processor) {
        static_assert(
            std::is_base_of_v<MsgCRTP<MsgType>, MsgType>,
            "MsgType must inherit from MsgCRTP<MsgType>"
        );

        std::lock_guard<std::mutex> lock(mutex_); // 单线程安全保障
        // std::any和std::unique_ptr有冲突
        processor_map_[MsgType::TypeIndex()] = processor;
    }

    // 注册事件处理函数（编译期绑定）
    template <typename MsgType>
    void register_handler(void (Router::*handler)(const MsgType&)) {
        std::lock_guard<std::mutex> lock(mutex_); // 单线程安全保障
        handler_map_[MsgType::TypeIndex()] = [this, handler](const void* msg_ptr) {
            (this->*handler)(*static_cast<const MsgType*>(msg_ptr));
        };
    }

    // MMA 参数校验（纯静态，无虚函数）
    bool has_mma_param_error(const OpMMAMsg& msg) {
        return msg.a().empty() || msg.b().empty() || msg.c().empty();
    }

    // ========================== 成员变量（极简，零冗余） ==========================
    ProcessorMap processor_map_; // 静态多态处理器注册表
    HandlerMap handler_map_;     // 事件处理函数注册表
    std::mutex mutex_;           // 线程安全锁（单线程处理保障）
};

    // ========================== 统一事件处理逻辑（纯静态多态） ==========================
    // 通用事件处理（编译期绑定）
template <typename MsgType>
void Router::process_msg(const MsgType& msg) {
        auto processor = get_processor<MsgType>();
        processor->process(msg);
    }

} // namespace msg
} // namespace proj