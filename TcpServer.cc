#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
        LOG_FATAL("%s:%s:%d mainLoop is null!\n",__FILE__,__FUNCTION__,__LINE__);
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
          const InetAddress &listenAddr,
          const std::string &nameArg,
          Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop,listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
        std::placeholders::_1, std::placeholders::_2));
}

/**
 * 不仅需要释放TcpConnection指针，同时还要让EventLoop的poller放弃监听channel的事件
 * 这里的做法很巧妙：使用局部变量来绑定shared_ptr，释放类作用域的shared_ptr
 * 但我感觉先EPOLLEDL掉poller监听的事件，再释放类作用域的资源不是更好么.........
 */
TcpServer::~TcpServer()
{
    for(auto &item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        // bind绑定，绑定智能指针shared_ptr也是可以的，不一定非要是纯指针
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听 loop.loop()
void TcpServer::start()
{
   if(started_++ == 0) // 防止一个TcpServer对象被多次start
   {
       threadPool_->start(threadInitCallback_); // 启动底层loop线程池
       loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); // baseLoop启动监听
   } 
}

// 有一个新的客户端连接，acceptor执行这个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 轮询算法，选择一个subLoop来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] -new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取本地主机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
        LOG_ERROR("sockets::getLocalAddr");
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
        ioLoop,
        connName,
        sockfd, 
        localAddr,
        peerAddr));
    connections_[connName] = conn;
    // 用户设置回调函数TcpServer=>TcpConnection=>channel
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置了如何销毁连接，而不是关闭连接
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
        name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name()); // map_erase以后不会立即释放内存，而是有自己的回收机制，在合适时间进行回收
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
