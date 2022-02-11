#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

//类的静态成员变量一定要在类外声明，因为它位于全局作用域，必须全局定义或定义，让全局获知
std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
        thread_->detach(); // thread线程分离
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程 注意这里的线程不是在构造函数的时候就生成了
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]()
                                                           {
        // 获取线程的tid
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该函数
        func_(); }));

    // 这里必须等待获取了上面新创建线程的tid值以后，才能退出
    sem_wait(&sem);
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}