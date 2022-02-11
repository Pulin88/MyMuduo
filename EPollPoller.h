#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

class EventLoop;

/**
 * epoll的使用
 * epoll_create
 * epoll_ctl add/mod/del
 * epoll_wait
 */
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller();
    
    /**
     * poll = epoll_wait
     * updateChannel 和 removeChannel = epoll_ctl add/del
     * 给所有IO复用保留统一的接口
     */
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16;
    
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

    // EventList保存epoll_wait返回的事件
    // epoll_wait的共用体data保存channel
    using EventList = std::vector<epoll_event>;


    int epollfd_;
    EventList events_;
};