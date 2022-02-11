#include "Channel.h"
#include "EventLoop.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
{
}

Channel::~Channel()
{
}

// tie在TcpConnection中调用，用来绑定
/**
 * 通过阅读TcpConnection的源码发现，所有...Callback_函数bind的都是TcpConnection的指针
 * 所以进行回调的时候一定需要判断TcpConnectino的指针是否为NULL
 * 如果不为NULL，才可以进行调用，否则导致未知错误
 */
void Channel::tie(const shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
     *  改变channel的fd的events后
     *  以EventLoop为媒介，把更改信息传递到Poller中
     *  update()，remove()同理
     */
void Channel::update() // 相当于epoll_ctl()
{
    loop_->updateChannel(this);
}
void Channel::remove()
{
    loop_->removeChannel(this);
}

// fd得到poller通知后,处理事件的回调函数
// weak_ptr的lock()操作可保证线程安全
void Channel::handleEvent(Timestamp recieveTime)
{
    if (tied_)
    {
        shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(recieveTime);
        }
    }
    else
    {
        handleEventWithGuard(recieveTime);
    }
}

// 注意这里的函数对象必须要判断是否为空，因为如果不调用set...则为NULL
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
            readCallback_(receiveTime);
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
            writeCallback_();
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
            errorCallback_();
    }

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
            closeCallback_();
    }
}
