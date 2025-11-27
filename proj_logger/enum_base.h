#ifndef ENUM_BASE_H
#define ENUM_BASE_H

#include <string>
#include <array>
#include <type_traits>
#include <cstdint>

// 所有逻辑严格在proj_logger命名空间内
namespace proj_logger {

// 通用的枚举边界检查工具函数（内部使用）
template <typename EnumType>
inline bool is_enum_valid(EnumType value, size_t enum_count) {
    using Underlying = std::underlying_type_t<EnumType>;
    Underlying idx = static_cast<Underlying>(value);
    return idx >= 0 && static_cast<size_t>(idx) < enum_count;
}

} // namespace proj_logger

// ===================== X宏核心定义（全局宏，仅做展开） =====================
// 辅助宏1：展开枚举值（支持带赋值，如 TRACE=0）
#define ENUM_ITEM_VALUE(name) name,
// 辅助宏2：展开枚举对应的字符串字面量
#define ENUM_ITEM_STRING(name) #name,
// 辅助宏3：用于自动计算枚举项数量（生成计数标记）
#define ENUM_ITEM_COUNT(name) 1,

// 核心宏：生成枚举类 + 字符串数组 + 专属转换函数
// 参数说明：
// 1. EnumName：枚举类名（如LogLevel）
// 2. EnumItemList：X宏列表名（如LOG_LEVEL_ITEMS）
// （已移除EnumCount参数，改为自动计算）
#define DEFINE_PROJ_ENUM(EnumName, EnumItemList) \
    /* 显式指定底层类型为int32_t，避免跨平台类型差异 */ \
    enum class EnumName : int32_t { \
        EnumItemList(ENUM_ITEM_VALUE) \
    }; \
    \
    /* 自动计算枚举项数量（通过计数数组大小） */ \
    constexpr int EnumName##_count[] = { EnumItemList(ENUM_ITEM_COUNT) }; \
    constexpr size_t EnumName##_size = sizeof(EnumName##_count) / sizeof(EnumName##_count[0]); \
    \
    /* 生成枚举对应的字符串数组（编译期常量，大小自动匹配） */ \
    constexpr std::array<const char*, EnumName##_size> EnumName##_str_array = { \
        EnumItemList(ENUM_ITEM_STRING) \
    }; \
    \
    /* 生成该枚举专属的字符串转换函数（无模板，无歧义） */ \
    inline std::string cvt##EnumName(EnumName value) { \
        if (proj_logger::is_enum_valid(value, EnumName##_size)) { \
            using Underlying = std::underlying_type_t<EnumName>; \
            return EnumName##_str_array[static_cast<Underlying>(value)]; \
        } \
        return "UNKNOWN_" + std::to_string(static_cast<int32_t>(value)); \
    }

#endif // ENUM_BASE_H