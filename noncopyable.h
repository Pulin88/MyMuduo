// 编译器级别#pragma
// 语言级别#ifndef #define #endif
#pragma once

// noncopyable被继承以后，派生类对象可以被构造和析构，但不可以拷贝和赋值
class noncopyable
{
protected:
    // protected保证了noncopyable不会被单独构成对象来调用
    noncopyable() = default;
    ~noncopyable() = default;

public:
    // delete的方法放在public protected private都行
    noncopyable(const noncopyable &) = delete;
    // 禁止=的返回值无论是啥都行
    void operator=(const noncopyable &) = delete;
};
