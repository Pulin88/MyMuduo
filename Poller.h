#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
class Channel;
class EventLoop;

#include <unordered_map>
#include <vector>

using namespace std;
/**
 * 多路事件分发器的IO复用模块
 * 多路事件分发器：Reactor模型中的Demultiplex
 * IO复用模块：epoll，poll，select
 */

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default; // 虚函数必须要定义，除非是纯虚函数

    /**
     * poll = epoll_wait
     * updateChannel 和 removeChannel = epoll_ctl add/del
     * 给所有IO复用保留统一的接口
     */
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断Channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

    // 获得IO复用的具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    // 记录 sockfd : Channel*
    using ChannelMap = unordered_map<int, Channel *>;
    ChannelMap channels_;

private:
    // 记录Poller所属的EventLoop
    EventLoop *ownerLoop_;
};