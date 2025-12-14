#pragma once
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <exception>
#include "err.hpp"
#define BLOCKSIZE 32
using namespace std;

class Sock
{

public:
    int Socket()
    {
        // 1. 创建socket文件套接字对象
        _listensock = socket(AF_INET, SOCK_STREAM, 0);
        if (_listensock < 0)
        {
            syslog(LOG_ERR, "创建套接字失败！");
            throw std::exception();
            exit(SOCKET_ERR);
        }
        syslog(LOG_INFO, "创建套接字成功！");
        int opt = 1;
        setsockopt(_listensock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
        return _listensock;
    }

    void Bind(int port)
    {
        // 2. bind绑定自己的网络信息
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;
        if (bind(_listensock, (struct sockaddr *)&local, sizeof(local)) < 0)
        {
            syslog(LOG_ERR, "绑定端口失败");
            throw std::exception();
            exit(BIND_ERR);
        }
        syslog(LOG_INFO, "绑定端口成功！");
        cout << "端口绑定成功" << endl;
    }

    void Listen()
    {
        // 3. 设置socket 为监听状态
        if (listen(_listensock, BLOCKSIZE) < 0) // 第二个参数backlog后面在填这个坑
        {
            syslog(LOG_ERR, "监听失败！");
            throw std::exception();
            exit(LISTEN_ERR);
        }
        syslog(LOG_INFO, "监听成功！");
        cout << "监听成功！" << endl;
    }

    int Accept(std::string *clientip, uint16_t *clientport)
    {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);
        int sock = accept(_listensock, (struct sockaddr *)&peer, &len);
        syslog(LOG_INFO, "建立一个新的连接成功，文件描述符为:%d", sock);
        *clientip = inet_ntoa(peer.sin_addr);
        *clientport = ntohs(peer.sin_port);

        return sock;
    }
    int Getlistenfd()
    {
        return _listensock;
    }

private:
    int _listensock;
    uint16_t _port;
};
