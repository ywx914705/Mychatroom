#include <iostream>
#include <semaphore.h>
#include <pthread.h>
#include <exception>
class sem
{
public:
    sem()
    {
        // 构造函数
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            // 构造函数没有返回值，所以这里直接抛异常
            throw std::exception();
        }
    }
    ~sem()
    { // 析构函数，销毁信号量
        sem_destroy(&m_sem);
    }
    bool wait() // 等待信号量
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post() // 增加信号量
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem; // 定义一个信号量
};

class locker // 互斥量/互斥锁类
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, nullptr) != 0)
        {
            // 初始化失败，抛异常
            throw std::exception();
        }
    }
    ~locker()
    {
        // 析构函数
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        // 上锁
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        // 解锁
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

private:
    pthread_mutex_t m_mutex;
};
class cond // 条件变量类
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }
    bool wait()
    {
        // 在进行等待之前先要进行加锁
        int ret = 0;
        pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};
