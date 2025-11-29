#pragma once
class NoCopy {
protected:
    // 保护构造/析构：仅允许派生类继承，禁止直接实例化
    constexpr NoCopy() = default;
    ~NoCopy() = default;

public:
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;

    // otherwise 派生类会被隐式删除
    NoCopy(NoCopy&&) = default;
    NoCopy& operator=(NoCopy&&) = default;
};

class NoMove {
protected:
    constexpr NoMove() = default;
    ~NoMove() = default;

public:
    NoMove(const NoMove&) = default;
    NoMove& operator=(const NoMove&) = default;

    NoMove(NoMove&&) = delete;
    NoMove& operator=(NoMove&&) = delete;
};

class NoCopyMove {
protected:
        constexpr NoCopyMove() = default;
        ~NoCopyMove() = default;

public:
        NoCopyMove(const NoCopyMove&) = delete;
        NoCopyMove& operator=(const NoCopyMove&) = delete;

        NoCopyMove(NoCopyMove&&) = delete;
        NoCopyMove& operator=(NoCopyMove&&) = delete;
};