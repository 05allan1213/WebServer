#include "base/Buffer.h"
#include "base/MemoryPool.h"
#include "log/Log.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>

std::atomic<size_t> Buffer::activeBuffers{0};
std::atomic<size_t> Buffer::poolMemory{0};
std::atomic<size_t> Buffer::heapMemory{0};
std::atomic<size_t> Buffer::resizeCount{0};

Buffer::Buffer(size_t initialSize)
    : readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend), fromPool_(true)
{
    capacity_ = initialSize + kCheapPrepend;
    data_ = static_cast<char *>(MemoryPool::getInstance().allocate(capacity_));
    if (!data_)
    {
        throw std::bad_alloc();
    }

    activeBuffers++;
    poolMemory += capacity_;
}

Buffer::~Buffer()
{
    if (data_)
    {
        if (fromPool_)
        {
            MemoryPool::getInstance().deallocate(data_, capacity_);
            poolMemory -= capacity_;
        }
        else
        {
            delete[] data_;
            heapMemory -= capacity_;
        }
    }
    activeBuffers--;
}

Buffer::Buffer(Buffer &&rhs) noexcept
    : data_(rhs.data_),
      capacity_(rhs.capacity_),
      readerIndex_(rhs.readerIndex_),
      writerIndex_(rhs.writerIndex_),
      fromPool_(rhs.fromPool_)
{
    rhs.data_ = nullptr;
    rhs.capacity_ = 0;
}

Buffer &Buffer::operator=(Buffer &&rhs) noexcept
{
    swap(rhs);
    return *this;
}

void Buffer::swap(Buffer &rhs) noexcept
{
    std::swap(data_, rhs.data_);
    std::swap(capacity_, rhs.capacity_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
    std::swap(fromPool_, rhs.fromPool_);
}

void Buffer::retrieve(size_t len)
{
    if (len > readableBytes())
    {
        throw std::out_of_range("Buffer::retrieve error: len > readableBytes()");
    }
    if (len < readableBytes())
    {
        readerIndex_ += len;
    }
    else
    {
        retrieveAll();
    }
}

void Buffer::retrieveAll()
{
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
}

std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}

std::string Buffer::retrieveAsString(size_t len)
{
    if (len > readableBytes())
    {
        len = readableBytes();
    }
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

void Buffer::append(const char *data, size_t len)
{
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
}

void Buffer::ensureWritableBytes(size_t len)
{
    if (writableBytes() >= len)
    {
        return;
    }

    if (prependableBytes() + writableBytes() >= len + kCheapPrepend)
    {
        // 整理内部空间
        size_t readable = readableBytes();
        std::move(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
    else
    {
        // 扩容
        resizeCount++;
        size_t newCapacity = writerIndex_ + len;
        char *newData = new char[newCapacity];
        heapMemory += newCapacity;

        std::copy(peek(), peek() + readableBytes(), newData + kCheapPrepend);

        // 释放旧内存
        if (fromPool_)
        {
            MemoryPool::getInstance().deallocate(data_, capacity_);
            poolMemory -= capacity_;
        }
        else
        {
            delete[] data_;
            heapMemory -= capacity_;
        }

        data_ = newData;
        capacity_ = newCapacity;
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readableBytes();
        fromPool_ = false; // 内存现在来自堆
    }
}

ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0};
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(n) <= writable)
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ = capacity_;
        append(extrabuf, n - writable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}