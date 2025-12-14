#include <iostream>
#include <syslog.h>
#include <exception>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#define EPOLL_ARRAY_SIZE 1024
#define EPOLL_SIZE 5
class MyEpoll
{
public:
    int err = 0;
    MyEpoll()
        : _epfd(-1)
    {
    }
    ~MyEpoll()
    {
        if (_epfd >= 0)
        {
            close(_epfd);
        }
    }
    void Epoll_create()
    {
        // 创建epoll模型
        _epfd = epoll_create(EPOLL_SIZE);
        if (_epfd < 0)
        {
            // 说明出现错误了
            syslog(LOG_ERR, "创建epoll模型失败！");
            std::cout << "创建epoll模型失败！" << std::endl;
            throw std::exception();
        }
        else
        {
            syslog(LOG_INFO, "创建epoll模型成功！");
            std::cout << "创建epoll模型成功！" << std::endl;
        }
    }
    void Epoll_Addevent(int sock, uint32_t event) // 把这个文件描述符设置进epoll模型
    {
        // 把这个文件描述符添加到epoll模型中
        struct epoll_event _event;
        _event.events = event;
        _event.data.fd = sock;
        err = epoll_ctl(_epfd, EPOLL_CTL_ADD, sock, &_event);
        if (err < 0)
        {
            syslog(LOG_ERR, "添加到epoll模型失败！");
            std::cout << "添加到epoll模型失败！" << std::endl;
            throw std::exception();
        }
        else
        {
            // 走到这里说明成功了
            std::cout << "成功添加文件描述符为:" << sock << "到epoll模型中！" << std::endl;
            syslog(LOG_INFO, "添加文件描述符到epoll模型成功！");
        }
    }
    int Epoll_Wait(struct epoll_event events[], int num, int timeout)
    {

        int ready = epoll_wait(_epfd, events, num, timeout);
        return ready; // 返回已经就绪的文件描述符
    }

    bool Epoll_control(int sock, uint32_t event, int op)
    {
        if (op == EPOLL_CTL_MOD)
        {
            // 如果是修改的话
            struct epoll_event ev;
            ev.events = event;
            ev.data.fd = sock;
            err = epoll_ctl(_epfd, op, sock, &ev);
            if (err == -1)
            {
                syslog(LOG_ERR, "修改发生错误");
                std::cout << "修改发生错误" << std::endl;
                throw std::exception();
            }
            else
            {
                //std::cout << "修改成功" << std::endl;
            }
            return true;
        }
        else if (op == EPOLL_CTL_DEL)
        {
            // 说明想要进行删除了
            err = epoll_ctl(_epfd, op, sock, nullptr);
            if (err == -1)
            {
                syslog(LOG_ERR, "删除发生错误");
                std::cout << "删除发生错误" << std::endl;
                throw std::exception();
            }
            else
            {
                std::cout << "删除成功" << std::endl;
            }
            return true;
        }
        else
        {
            std::cout << "非法操作" << std::endl;
            throw std::exception();
        }
    }
    int Setnoblockfd(int fd)
    {
        int old_option = fcntl(fd, F_GETFL);      // 获取旧的文件描述符
        int new_option = old_option | O_NONBLOCK; // 设置文件描述符为非阻塞的
        fcntl(fd, F_SETFL, new_option);           // 对这个文件描述符进行设置
        return old_option;                        // 返回旧的文件描述符的状态标志，以便日后恢复
    }
    bool Getepollfd()
    {
        return _epfd != -1; // epoll_create调用成功的返回值是0，所以之里为0的时候才返回
    }

private:
    int _epfd;
};