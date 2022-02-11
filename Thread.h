#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <memory>
#include <unistd.h>
#include <thread>
#include <atomic>

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() const { return name_; }

    // 一个类对象可以创建一个线程
    // 统计类创建的线程个数
    static int numCreated() { return numCreated_; } // numCreated_虽然是atomic_int，但可以和int转换

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_; // numCreated_很显然要使用原子类型，因为可能被不同的类对象调用
};