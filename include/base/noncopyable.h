#pragma once

/**
 * noncopyable被继承以后,派生类对象可以正常的构造和析构,但是派生类对象无法进行拷贝构造和赋值操作
 */

class noncopyable
{
public:
    // 禁止拷贝构造和赋值
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;

protected:
    // 默认构造和析构函数
    noncopyable() = default;
    ~noncopyable() = default;
};