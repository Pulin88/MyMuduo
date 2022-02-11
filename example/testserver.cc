#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoSerevr
{
public:
    EchoSerevr(EventLoop *loop,
        const InetAddress &addr,
        const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        server_.setConnectionCallback(std::bind(&EchoSerevr::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoSerevr::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
        server_.setThreadNum(3);
    }

    void start()
    {server_.start();}

private:
    // 连接建立 or 连接断开
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected())
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        else 
            LOG_INFO("Connection Down : %s", conn->peerAddress().toIpPort().c_str());
    }
    // 读事件回调
    void onMessage(const TcpConnectionPtr &conn,
        Buffer *buf,
        Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown(); // 关闭写端fd
    }
    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoSerevr server(&loop, addr, "EchoServer-01"); 

    server.start();
    loop.loop();

    return 0;
}