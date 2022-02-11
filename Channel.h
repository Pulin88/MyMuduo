#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <memory>
#include <functional>
using namespace std;

/**头文件中尽可能包含少的信息，这样生成so库时才不会暴露出太多内容
 * 对于其他文件的类，类的指针可以用class A;，继承或创建对象则需#include头文件
 */
class EventLoop;

// channel封装了socketfd，socketfd感兴趣的event，poller返回的具体事件
/**
 * channel增加：Channel()
 * channel移除：remove()
 * channel修改：disable...() enable...()
 * channel修改后的更新：update()
 */
class Channel : noncopyable
{
public:
    using EventCallback = function<void()>;
    using ReadEventCallback = function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知后,处理事件的回调函数
    void handleEvent(Timestamp recieveTime);

    // 设置回调函数对象
    // move移动语义，避免拷贝
    void setReadCallback(ReadEventCallback cb) { readCallback_ = move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = move(cb); }

    // 防止当channel被手动remove掉，channel还执行回调操作
    /*
        shared_ptr和weak_ptr在++和--操作时都采用CAS，故是线程安全的
        但是若线程A析构这个对象，线程B又要使用该对象
        当线程A析构后，线程B访问会有未知后果
        void func(weak_ptr<pointer> pw)
        {shared_ptr<pointer> ps = pw.lock(); if(ps != nullptr) ps->show();}
        pw = ps; 不需要强转
    */
    void tie(const shared_ptr<void> &);

    int fd() const { return fd_; }                 // 查看fd
    int events() const { return events_; }         // 查看fd感兴趣的事件
    int set_revents(int revt) { revents_ = revt; } // poller监听到事件后写入channel

    // 设置fd所感兴趣的事件 update()=epoll_ctl() 位使能操作
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的所感兴趣的事件
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }
    // 移除loop_中的channel
    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 事件循环
    const int fd_;    //poller监听fd_
    int events_;      // 注册fd_感兴趣的事件
    int revents_;     // poller返回具体发生的事件
    int index_;       // 记录当前channel在poller中的状态

    std::weak_ptr<void> tie_;   // 使用weak_ptr，避免了相互引用所造成的死锁，导致资源不能正确释放
    bool tied_;

    // 通过channel中的revents_来确定回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};