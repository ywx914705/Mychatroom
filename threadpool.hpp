#include <iostream>
#include "lockGuard.hpp"
#include <cstring>
#include <vector>
#include <pthread.h>
#include <exception>
#include "mylist.hpp"
#include <unistd.h>
using namespace Mylist;
template <typename T>
class ThreadPool
{
public:
    ThreadPool(int threadnum = 10, int maxthread_num = 65535); // 构造函数
    ~ThreadPool();                                             // 析构函数
    bool append(T *request);                                   // 将任务/数据/请求加入到工作队列中
private:
    void run();                     // 启动线程池
    static void *worker(void *arg); // 线程所执行的动作

private:
    std::vector<pthread_t *> _threads; // 存储/管理线程的数组
    int _thread_num;                   // 线程的数量
    int _maxthread_num;                // 线程的最大数量
    Mylist::List<T *> _workqueue;      // 存储任务/数据/请求的队列
    int _maxqueue_num;                 // 队列中任务/数据/请求的最大数量
    locker _queue_lock;                // 保护队列的锁
    sem _queue_sem;                    // 的信号量
    bool _stop;                        // 表示线程池是否退出
};
template <typename T>
ThreadPool<T>::ThreadPool(int threadnum, int maxthread_num)
    : _thread_num(threadnum), _maxthread_num(maxthread_num), _stop(false)
{

    _maxqueue_num = 10;
    if (_thread_num < 0 || _maxthread_num < 0)
    {
        // 一个线程都没有，或者说线程池是空的，直接抛异常
        throw std::exception();
    }
    // 为每个线程开辟空间
    _threads.resize(_thread_num);
    for (int i = 0; i < _thread_num; i++)
    {
        _threads[i] = new pthread_t;
    }
    // 1、作为一个线程池是不是得有线程？
    for (int i = 0; i < _thread_num; i++)
    {
        std::cout << "线程" << i + 1 << "创建成功" << std::endl;
        int n = pthread_create(_threads[i], nullptr, worker, this); // 这个函数成功会返回0
        if (n != 0)
        {
            throw std::exception();
        }
    }
    // 2、将线程设置为分离线程
    for (int i = 0; i < _thread_num; i++)
    {
        if (pthread_detach(*_threads[i]))
        {
            throw std::exception();
            // 如果调用失败则抛异常
        }
    }
}
template <typename T>
ThreadPool<T>::~ThreadPool()
{
    _stop = true;
    for (auto thread : _threads)
    {
        if (thread)
            delete thread;
    }
}
template <typename T>
bool ThreadPool<T>::append(T *request)
{
    // 先进行加锁
    _queue_lock.lock();
    if (_workqueue.size() > _maxqueue_num) // 队列中的请求数大于队列所能承载的最大请求数
    {
        _queue_lock.unlock();
        return false;
    }
    // 走到这里说明队列中有空闲的位置了
    _workqueue.push_back(request);
    _queue_lock.unlock();
    _queue_sem.post();
    return true;
}
template <typename T>
void ThreadPool<T>::run()
{
    while (!_stop) // 只要线程池不退出，这就是一个死循环
    {
        _queue_sem.wait();
        _queue_lock.lock();
        if (_workqueue.empty())
        {
            _queue_lock.unlock();
            continue; // 如果队列为空就继续
        }
        // 走到这里说明队列不为空了
        T *req = _workqueue.front();
        _workqueue.pop_front();
        // req.func(); // 进行处理
        _queue_lock.unlock();
    }
}
template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool<T> *pool = (ThreadPool<T> *)arg;
    std::cout << "线程开始工作，线程ID: " << pthread_self() << std::endl;
    pool->run();
    return pool;
}
