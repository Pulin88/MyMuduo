#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

// 头文件声明的函数，可以在不同的源文件中实现
Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;
    }
    else
    {
        return new EPollPoller(loop); //生成epoll实例
    }
}