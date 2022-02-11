#include "Buffer.h"

#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

// 从fd上读取数据
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上内存空间 64K
    
    struct iovec vec[2];

    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    // 保证writable的空间稳定大于64K
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const size_t n = ::readv(fd, vec, iovcnt);
    if(n < 0)
        *saveErrno = errno;
    else if(n <= writable)
        writerIndex_ += n;
    else
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // 填够当前buffer_，同时makeSpace
    }
    return n;
}

// 通过fd发送数据
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0)
        *saveErrno = errno;
    return n;
}