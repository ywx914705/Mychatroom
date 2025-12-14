#include <iostream>
#include <string.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <unistd.h>
#include <cstring>
#include <syslog.h>
#include <errno.h>
#include <cstdlib>
#include "sock.hpp"
#include "epoll.hpp"

#define RECVBUFFER_SIZE 1024
#define EPOLLARRAY_SIZE 1024
#define FD_LIMIT 65535
#define USER_LIMIT 5

static int defaultport = 8080;
static int defaultsock = -1;

using std::string;
using std::cout;
using std::endl;

class User
{
public:
    typedef std::function<void(User *)> func_t;
    User(int sock)
        : _sock(sock)
    {
    }
    void Register(func_t read, func_t send, func_t exception)
    {
        _reader = read;
        _sender = send;
        _excepter = exception;
    }

public:
    int _sock;
    string writebuffer;
    string readbuffer;
    func_t _reader;
    func_t _sender;
    func_t _excepter;
};

class Chatserver
{
public:
    Chatserver(int port = defaultport)
        : _port(port), _revents(nullptr), _rnum(EPOLLARRAY_SIZE)
    {
    }
    
    ~Chatserver()
    {
        if (_revents)
            delete[] _revents;
    }
    
    void Reader(User *use)
    {
        char buffer[RECVBUFFER_SIZE];

        while (true)
        {
            ssize_t n = recv(use->_sock, buffer, sizeof(buffer) - 1, 0);

            if (n > 0)
            {
                // 成功接收到数据
                // 1. 累积到readbuffer
                buffer[n] = '\0'; // 确保字符串以null结尾，便于调试
                use->readbuffer.append(buffer, n);

                // 2. 按行处理消息
                size_t pos;
                while ((pos = use->readbuffer.find('\n')) != string::npos)
                {
                    // 提取一行（包含\n）
                    string line = use->readbuffer.substr(0, pos + 1);

                    // 移除已处理的行
                    use->readbuffer.erase(0, pos + 1);

                    // 广播这一行
                    cout << "客户端[" << use->_sock << "]: " << line;
                    BroadcastMessage(use->_sock, line);
                }
            }
            else if (n == 0)
            {
                // recv返回0表示客户端主动关闭了连接
                // TCP的FIN包已经到达，这是一个正常的连接关闭
                cout << "客户端[" << use->_sock << "] 主动关闭连接" << endl;

                // 调用异常处理函数来清理资源
                Excepter(use);
                break; // 退出循环，这个连接已经结束了
            }
            else // n < 0
            {
                // recv出错
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 这是非阻塞IO的正常情况：当前没有更多数据可读
                    // 在ET模式下，这意味着我们已经读取了所有可用的数据
                    // 等待下一次EPOLLIN事件通知
                    break; // 退出循环，等待下次事件
                }
                else if (errno == EINTR)
                {
                    // 被信号中断，继续尝试
                    continue;
                }
                else
                {
                    // 真正的错误：连接可能已损坏
                    cout << "客户端[" << use->_sock << "] recv错误: "
                         << strerror(errno) << endl;

                    // 调用异常处理函数来清理资源
                    Excepter(use);
                    break; // 退出循环
                }
            }
        }
    }
    
    void Sender(User *use)
    {
        if (use->writebuffer.empty())
        {
            _epoller.Epoll_control(use->_sock, EPOLLIN | EPOLLET, EPOLL_CTL_MOD);
            return;
        }

        size_t total_sent = 0;
        ssize_t n = 0;

        // 尝试发送数据
        n = send(use->_sock, use->writebuffer.c_str(),
                 use->writebuffer.size(), 0);

        if (n > 0)
        {
            total_sent = n;
            // 移除已发送的数据
            use->writebuffer.erase(0, total_sent);
        }
        else if (n == 0)
        {
            // 对端关闭连接
            Excepter(use);
            return;
        }
        else
        {
            // 发送出错
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 发送缓冲区满，等待下次EPOLLOUT
                // 保持当前的EPOLLOUT监听
                return;
            }
            else
            {
                // 其他错误
                syslog(LOG_ERR, "客户:%d发送数据出错: %s",
                       use->_sock, strerror(errno));
                Excepter(use);
                return;
            }
        }

        // 根据是否还有数据调整事件监听
        if (use->writebuffer.empty())
        {
            _epoller.Epoll_control(use->_sock, EPOLLIN | EPOLLET, EPOLL_CTL_MOD);
        }
        else
        {
            _epoller.Epoll_control(use->_sock, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_MOD);
        }
    }
    
    void Excepter(User *use)
    {
        // 正确写法
        auto it = _Users.find(use->_sock);
        if (it != _Users.end())
        {
            _epoller.Epoll_control(use->_sock, 0, EPOLL_CTL_DEL);
            close(use->_sock); // 关闭socket！
            delete it->second; // 删除User对象
            _Users.erase(it);  // 从map中移除
        }
    }
    
    void Sendtoclient(int sock, const string &message)
    {
        // 去找这个用户是否存在
        auto it = _Users.find(sock);
        if (it != _Users.end())
        {
            User *user = it->second;
            user->writebuffer.append(message);
            _epoller.Epoll_control(sock, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_MOD);
            // 把数据写入到该用户的writebuffer中并把epoll中的监听事件改成既监听读又监听写
        }
    }
    
    void BroadcastMessage(int sender_fd, const string &msg)
    {
        for (auto &pair : _Users)
        {
            int client_fd = pair.first;

            // 跳过监听socket和发送者自己
            if (client_fd == _sock.Getlistenfd() || client_fd == sender_fd)
                continue;

            User *user = pair.second;
            user->writebuffer.append(msg);

            // 添加写事件监听
            _epoller.Epoll_control(client_fd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_MOD);
        }
    }
    
    void initchatserver()
    {
        // 1、创建套接字
        _sock.Socket(); // 创建sockfd
        // 2、绑定端口号
        _sock.Bind(_port);
        // 3、监听
        _sock.Listen();
        // 4、创建epoll模型（假设MyEpoll已实现）
        _epoller.Epoll_create();
        // 5、把这个监听文件描述符添加到epoll模型中
        // 在添加之前，先把这个文件描述符设置成非阻塞模式
        _epoller.Setnoblockfd(_sock.Getlistenfd());
        // _epoller.Epoll_Add(_sock.Getlistenfd(), EPOLLIN | EPOLLET);
        _revents = new struct epoll_event[EPOLLARRAY_SIZE];
        Adduser(_sock.Getlistenfd(), EPOLLIN | EPOLLET,
                std::bind(&Chatserver::Accepter, this, std::placeholders::_1),
                nullptr,
                nullptr);
        cout << "聊天室服务器启动！监听端口: " << _port << endl;
    }
    
    void Accepter(User *use)
    {
        for (;;)
        {
            string clientip;
            uint16_t clientport;
            int sock = _sock.Accept(&clientip, &clientport);
            if (sock > 0)
            {
                // 设置非阻塞
                _epoller.Setnoblockfd(sock);
                
                Adduser(sock, EPOLLIN | EPOLLET, 
                        std::bind(&Chatserver::Reader, this, std::placeholders::_1),
                        std::bind(&Chatserver::Sender, this, std::placeholders::_1),
                        std::bind(&Chatserver::Excepter, this, std::placeholders::_1));
                
                cout << "有一个新用户进行连接 ip:" << clientip << "[" << clientport << "]" << endl;
                
                // 发送欢迎消息
                string info = "欢迎加入聊天室, 你的sockfd是: " + std::to_string(sock) + "\n";
                Sendtoclient(sock, info);
                
                // 广播给其他用户
                string enter_msg = "系统: 用户[" + std::to_string(sock) + "] 进入了聊天室\n";
                BroadcastMessage(sock, enter_msg);
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                {
                    // 真正的accept错误
                    syslog(LOG_ERR, "accept错误: %s", strerror(errno));
                    break;
                }
            }
        }
    }
    
    void Adduser(int sock, uint32_t event, User::func_t reader, User::func_t sender, User::func_t excepter)
    {
        // 1、创建一个用户对象
        User *use = new User(sock);
        // 2、注册这个用户的三个回调函数
        use->Register(reader, sender, excepter);
        // 3、来个一个用户也就是一个连接，我们就必须用用户管理组给他们进行管理起来
        _Users.insert(std::pair<int, User *>(sock, use));
        syslog(LOG_INFO, "添加一个用户成功！");
        // 4、一个用户也就是一个连接，也就是一个sockfd,把这个对应的sock加入到epoll中，让epoll帮助我们进行管理
        _epoller.Epoll_Addevent(sock, event);
        syslog(LOG_INFO, "添加这个用户的数据到epoll中成功！");
    }

    void loop(int timeout)
    {
        int ready = _epoller.Epoll_Wait(_revents, _rnum, timeout);
        for (int i = 0; i < ready; i++)
        {
            int sock = _revents[i].data.fd;
            uint32_t events = _revents[i].events;
            
            // 先查找socket是否存在
            auto it = _Users.find(sock);
            if (it == _Users.end()) {
                continue;
            }
            
            User* user = it->second;
            
            // 处理监听socket（新连接）
            if (sock == _sock.Getlistenfd())
            {
                if (events & EPOLLIN && user->_reader)
                {
                    user->_reader(user);
                }
            }
            else // 处理普通客户端socket
            {
                // 处理可读事件
                if ((events & EPOLLIN) && user->_reader)
                {
                    user->_reader(user);
                }
                
                // 处理可写事件
                if ((events & EPOLLOUT) && user->_sender)
                {
                    user->_sender(user);
                }
                
                // 处理错误/挂断事件
                if ((events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) && user->_excepter)
                {
                    user->_excepter(user);
                }
            }
        }
    }
    
    void Dispatch()
    {
        while (1)
        {
            loop(1000);
        }
    }

private:
    Sock _sock;
    uint16_t _port;
    MyEpoll _epoller; // 当前epoll对象
    std::unordered_map<int, User *> _Users;
    struct epoll_event *_revents;
    int _rnum;
};