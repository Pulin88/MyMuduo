#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
                                 : loop_(nullptr)
                                 , exiting_(false)
                                 , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
                                 , mutex_()
                                 , cond_()
                                 , callback_(cb)
{}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

// 如果状态是静态判断的，就可以做出选择，则可以像exiting_一样
// 如果状态需要动态转换才能继续进行，则可以通过cond_来保证先后响应顺序
EventLoop* EventLoopThread::startLoop()
{
    thread_.start();    // 启动底层线程

    EventLoop *loop = nullptr;
    {
        /**
         * 上锁;
         * while(解锁条件)
         *  条件变量等待锁;
         */
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr)
            cond_.wait(lock);
        loop = loop_;
    }
    return loop;
}

// thread_.start()以后，调用该回调函数
// 通过callback_(&loop)实现EventLoopThread的回调操作
// loop.loop()实现one loop per thread模型，因为loop()里面是一个循环操作
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个EventLoop，由于EventLoop在创建线程时会获取线程id，所以在该处使用变量，而在前面使用指针
    
    if(callback_)
        callback_(&loop);
    
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); //EventLoop loop => Poller.poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}