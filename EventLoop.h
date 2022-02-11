#pragma once

#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

// 头文件的class声明 == 源文件中包含class所需的头文件
class Channel;
class Poller;

// 时间循环类 主要包含channel和poller(epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueLoop(Functor cb);

    // 唤醒loop所在的线程
    void wakeup();

    // EventLoop的方法 => Poller的方法
    // 使得channel可以借助EventLoop调用Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();        //wake up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_; // 是否在loop中
    std::atomic_bool quit_;    // 是否退出loop

    const pid_t threadId_; // 记录当前loop所在的线程id

    Timestamp pollReturnTime_; // poller返回发生事件的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_; // 当mainLoop获取一个新channel时，通过轮询选择一个subloop，并唤醒它处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_; // 是否在doPendingFunctor中
    std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的回调操作
    std::mutex mutex_;                        // 互斥锁，保护上面vector线程安全
};