#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Socket.h"
#include "Logger.h"

#include <functional>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
        LOG_FATAL("%s:%s:%d TcpConnection Looop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
              const std::string &nameArg,
              int sockfd,
              const InetAddress &localAddr,
              const InetAddress &peerAddr)
    : loop_(loop)
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) //64M
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd = %d state = %d\n", name_.c_str(), channel_->fd(),(int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
            sendInLoop(buf.c_str(), buf.size());
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}

// 发送数据，应用发送数据快，但是内核发送数据慢
// 如果发送数据成功，关闭EPOLLOUT事件
// 如果发送数据失败，缓存起来，开启EPOLLOUT事件
//// EPOLLOUT事件不能一直开启，因为只要对端能够接收数据，EPOLLOUT就会不断响应
//// 所以正确的做法是不判断EPOLLOUT，直接发送数据，发送成功就ok，发送不成功才开启EPOLLOUT监听
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前connection的connectDistroyed调用过，则不能再进行发送
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    // channel_第一次开始写数据，而且缓冲区没有数据
    // channel_如果之前发送数据失败，那么会监听EPOLLOUT事件并且缓冲有数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
                // 既然数据全部发送完成，不用给channel_设置epollout事件了
                loop_->queueLoop(std::bind(writeCompleteCallback_, shared_from_this()));            
        }
        else
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                // SIGPIPE RESET
                /**
                 * case1：A端关闭，B端send =》A内核发送RST给B
                 * case2：A端设置SO_LINGER，A端关闭 =》A内核发送RST给socket
                 *        B端收到RST，调用resv，errno为ECONNRESET，再调用resv，errno为EPIPE
                 *        B端收到RST，调用send，errno为EPIPE
                 */
                if(errno == EPIPE || errno == ECONNRESET)
                    faultError = true;
            }    
        }
    }

    // 对端或本端关闭，缓存数据，
    // 监听EPOLLOUT直到可写，调用channel的writeCallback_。调用TcpConnection的handleWrite
    if(!faultError && remaining>0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        if(remaining +  oldLen >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_)
                loop_->queueLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
            channel_->enableWriting();
    }
}

// 关闭连接 kDistconnecting的意义：还有数据没有发送到对端，尚且滞留在服务器中，需要标志此状态
// 关闭连接：shutdown写端，相当于半关闭
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  // outputBuffer中的数据没有全部发送完成
    {
        socket_->shutdownWrite(); // 关闭写端
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的epollin事件
    connectionCallback_(shared_from_this()); // 新连接建立，执行回调
}

// 连接销毁
// 连接销毁!=关闭连接
// 连接销毁：取消epoller对该事件的监听
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0)
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    else if(n == 0)
        handleClose();
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

/** 
 * handleWrite和sendInLoop不一样
 * handleWrite是由于之前发送失败，监听EPOLLOUT时的回调函数，把缓存在outputBuffer_中的数据发送到对端
 * sendInLoop发送的是用户指定的数组的数据
 */
void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                // 关闭channel_写操作，因为只有发现对端write失败时，才需要开启EPOLLOUT等待事件
                channel_->disableWriting();
                if(writeCompleteCallback_)
                    loop_->queueLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                if(state_ == kDisconnecting)
                    shutdownInLoop();
            }
        }
        else
        {LOG_ERROR("TcpConnection::handleWrite");}
    }
    else
    {LOG_ERROR("TcpConnection fd = %d is down, no more writing \n", channel_->fd());}
}

// poller => channel::closeCallback => TcpConnection::handleClose => TcpSerevr::removeConnection => TcpConnection::connectDestroyed
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd = %d state = %d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); // 连接关闭，执行回调
    closeCallback_(connPtr);    //关闭连接的回调
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        err = errno;
    else
        err = optval;
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}


