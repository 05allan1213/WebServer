#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // 在栈上定义额外的缓冲区，大小为64KB
    char extrabuf[65536] = {0};
    // 设置iovec结构体数组，第一个元素指向缓冲区中可读数据的起始位置，第二个元素指向额外的栈上缓冲区
    struct iovec vec[2];

    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_; // 指向主 Buffer 的可写起始位置
    vec[0].iov_len = writable;                // 主 Buffer 当前可写的长度

    vec[1].iov_base = extrabuf;       // 指向栈上缓冲区
    vec[1].iov_len = sizeof extrabuf; // 栈上缓冲区的长度

    // 如果主缓冲区的可写空间小于栈缓冲区大小 (64KB)，则同时使用主缓冲区和栈缓冲区 (iovcnt =
    // 2)，期望一次 readv 最多能读入 writable + 64KB 数据。 如果主缓冲区可写空间已经很大
    // (>=64KB)，则只使用主缓冲区 (iovcnt = 1)，避免不必要的栈缓冲区参与
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) // 错误
    {
        *saveErrno = errno; // 读取失败，设置 errno
    }
    else if (n <= writable) // 数据完全读入主缓冲区
    {
        writerIndex_ += n; // 更新可写索引
    }
    else // 数据填满主缓冲区并溢出到栈缓冲区
    {
        writerIndex_ = buffer_.size();  // 将写索引移到末尾
        append(extrabuf, n - writable); // 将栈缓冲区数据追加到主缓冲区
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    // 从可写索引(开始与可读索引相同)写入数据到fd中
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno; // 写入失败，设置 errno
    }
    return n;
}