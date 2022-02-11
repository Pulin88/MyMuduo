#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
        LOG_FATAL("%s:%S:%d listen socket create err: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);  //bind
    // TcpServer::start() Acceptor.listen 新用户连接，执行回调 connfd=>channel=>subloop
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll(); 
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading(); // acceptChannel_ => Poller
}

// listenfd有事件发生，即有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
            newConnectionCallback_(connfd, peerAddr);  // 轮询找到subLoop，唤醒，分发新客户
        else
            {::close(connfd);}
    }
    else
    {
        LOG_ERROR("%s%s:%d accept err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno == EMFILE)
            LOG_ERROR("%s%s:%d sockfd reached limit!\n",__FILE__,__FUNCTION__,__LINE__);
    }
}
