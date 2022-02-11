#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    //  __thread表示线程中的局部变量
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {
        // 内置期望：cpu一般会预存下一条指令，因此跳转指令很可能使流水线效率降低
        // 使用该命令提前告诉cpu可能执行的指令
        // 本句的意思：t_cachedTid == 0 很可能结果为 0，即t_cachedTid基本不为0
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }

}