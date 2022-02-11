#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"
#include "Logger.h"

#include <sys/eventfd.h>


// 防止一个线程创建多个EventLoop  线程中的单例,不过如果不单例就退出进程
__thread EventLoop *t_loopInThisThread = nullptr;

// 默认的Poller IO复用接口超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor处理新的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if(evtfd < 0)
        LOG_FATAL("eventfd error:%d\n", errno);
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else 
    {
        t_loopInThisThread = this;
    }
    // wakeupfd监听事件类型以及回调函数
    wakeupChannel_->enableReading();
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
}

// 析构函数可以根据构造函数来考虑，这样可以减少遗漏
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    // 好比looping_这种标识状态的标志，应该先写好false和true，再往中间填逻辑
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd client的fd和wakeupfd
        // Poller将监听到的channel事件上报给EventLoop
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for(Channel *channel : activeChannels_)
        {
            // 执行channel的回调操作
            channel->handleEvent(pollReturnTime_);
        }
        // 执行EventLoop的loop循环
        // mainloop事先为subloop注册cb回调
        // 当subloop被唤醒以后，执行下面的回调方法
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping.\n",this);
    looping_ = false;
}

// 退出事件循环
void EventLoop::quit()
{
    quit_ = true;

    // 如果在其他线程中，调用quit
    // 则wakeup唤醒目标线程来停止loop()循环
    if(!isInLoopThread())
    {
        wakeup();
    }
}

// 在目标线程中，直接执行cb
// 在其他线程中，把线程放入队列，唤醒目标线程，通过loop()中的doPendingFunctors()调用cb()
void EventLoop::runInLoop(Functor cb)
{
    // 在目标线程中，执行cb
    if(isInLoopThread())
    {
        cb();
    }
    // 在其他线程中，把cb塞入目标线程，并唤醒目标线程
    else
    {
        queueLoop(cb);
    }
}

// 把cb放入队列中，唤醒目标线程，调用cb
void EventLoop::queueLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 在其他线程中 或 又有新cb入队时 唤醒目标线程进行cb
    if(!isInLoopThread() || callingPendingFunctors_)
        wakeup();
}

void EventLoop::handleRead() //wake up
{
    uint64_t one;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);    
}

// 唤醒loop所在的线程
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n );
}

// EventLoop的方法 => Poller的方法
// 使得channel可以借助EventLoop调用Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 这里的swap用得很巧妙
    // 1.pendingFunctors_的内容不是逐个取出，而是一起取出一起执行，避免不断切换的开销
    // 这里不像生产者消费者的一样，每次从队列中取出一个来执行
    // 而是每个EventLoop负责一些fd，所以可以全部取出
    // 2.同时能把pendingFunctors_的内容置空
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    } 

    // 执行loop的回调函数
    for(const Functor &functor: functors)
    {
        functor();   
    }

    callingPendingFunctors_ = false;
}