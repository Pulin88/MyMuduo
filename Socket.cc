#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

/**
 * 假设客户端有connfd，服务端有listenfd和clientfd，
 * 一般情况下以上三者都必须都必须设置成非阻塞，
 * 此时只会影响到connect/accept/recv/send，不会影响到epoll_wait/poll/select,
 * connfd非阻塞：connect(connfd,&serveraddr,sizeof(serveraddr));返回-1并且errno==EINPROGRESS -> select监听connfd写事件 -> getsocketopt检测connfd是否出错
 * listenfd非阻塞：select监听listenfd读事件 -> clientfd=accept4(listenfd, &clientaddr, sizeof(clientaddr), SoCK_NONBLOCK|SOCK_CLOEXEC); -> while接收accept4返回-1并且errno==EAGAIN
 * connfd与clientfd非阻塞：send当对方TCP窗口太小发不出去立即返回，recv当无数据可接收立即返回，返回-1并且errno==EAGAIN
 * epoll_wait/poll/select无论绑定的fd是否阻塞都会等到超时才返回，除非监听的是非阻塞的eventfd..，一旦eventfd上有所需的事件就返回
 */

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if(0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
        LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
}

void Socket::listen()
{
    if(0 != ::listen(sockfd_, 1024))
        LOG_FATAL("listen sockfd:%d fail\n", sockfd_);
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, sizeof addr);
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0)
        peeraddr->setSockAddr(addr);
    return connfd;
}

void Socket::shutdownWrite()
{
    /**
     * shutdown不完全等价于close
     * close：关闭输入和输出方向的数据流
     *  输入：不可读，任何读都可能导致异常
     *  输出：内核缓冲区内容正常发送给对端并携带FIN，之后再写导致异常
     *  同时对端发和读的结果都是0字节
     * shutdown：可选择性地关闭输入或输出方向数据流
     *  SHUT_RD：读返回EOF，若有新数据流到达，内核丢弃回复ACK
     *  SHUT_WR：半关闭，内核缓冲区内容正常发送给对端并携带FIN，之后再写导致异常
     *  SHUT_RDWR：不完全等价于close
     *      close：关闭连接，释放连接资源，存在引用计数(不一定导致该套接字不可用)，故不一定发出FIN报文
     *      SHUT_RWDR：关闭连接，不释放资源，不存在引用计数(导致其他使用该套接字的进程也受影响)，一定发出FIN报文(故对于accept4的套接字都要设置为SOCK_CLOEXEC)
     * 综上可得：大多数情况选择shutdown，A端先SHUR_WR，等B端处理完发完以后，A端再SHUT_RD
     */
    if(::shutdown(sockfd_, SHUT_WR) < 0)
        LOG_ERROR("shutdownWrite error");
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}