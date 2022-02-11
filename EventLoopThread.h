#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <string>
#include <mutex>
#include <condition_variable>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc();

    // 在一个类中，如果一个变量在多个函数调用(增删改)，则需要加锁(数组...) 或 原子类型(变量...)
    // 在全局范围内，如果有一个变量被多个线程调用，则需要加锁(数组...) 或 先保留当前状态等到后面再调用(errno)
    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};