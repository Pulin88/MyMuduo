#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>


/**
 *          channel_    epollfd
 * kNew     false       false
 * kDel..   true        false
 * kAdd..   true        true
 */
// channel未添加到poller中
const int kNew = -1;
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if(epollfd_ < 0)
        LOG_FATAL("epoll_create error: %d\n", errno);
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

// poll == epoll_wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 操作频繁，实际应用中使用LOG_DEBUG更为合适
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    // &*events_中的*表示operator*()获得vector中的数据成员，而后使用&取地址
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;  // 多线程并发，全局变量可能会发生变动
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size())
            events_.resize(events_.size() * 2);
    }
    else if(numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// 更新poller中的channel
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s=>fd=%d events=%d index=%d\n",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index == kNew || index == kDeleted)
    {
        if(index == kNew)
            channels_[channel->fd()] = channel;
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    if(channel->index() == kAdded)
        update(EPOLL_CTL_DEL, channel);

    channel->set_index(kNew);
}

// 填写活跃的连接   
// 将epoll_wait得到的epoll_event填写到activeChannels中
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i = 0; i < numEvents; i++)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); //EventLoop拿到了poller返回的发生事件channel列表
    }
}


// 更新epollfd_中的channel     区别与上面的updateChannel:更新poller中的channel
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    event.events = channel->events();
    event.data.fd = channel->fd();
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, channel->fd(), &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d \n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
        }
    }
}
