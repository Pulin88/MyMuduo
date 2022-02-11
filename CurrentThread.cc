#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if(t_cachedTid == 0)
        {
            // 通过linux系统调用，获得当前线程的tid
            // 不同于pthread_self()，它是posix描述的tid，新建线程可以重用销毁线程的tid
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}