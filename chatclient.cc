#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <atomic>

#include "threadpool.hpp"

#define BUFFER_SIZE 1024

// 极简消息任务
class SimpleTask {
private:
    std::string _msg;
public:
    SimpleTask(const std::string& m) : _msg(m) {}
    void process() { std::cout << _msg; }
};

class MinimalClient {
private:
    int _sockfd;
    std::string _server_ip;
    int _port;
    std::atomic<bool> _running;
    ThreadPool<SimpleTask>* _pool;
    pthread_t _recv_thread;
    
    static void* recv_func(void* arg) {
        MinimalClient* self = (MinimalClient*)arg;
        char buf[BUFFER_SIZE];
        while (self->_running) {
            ssize_t n = recv(self->_sockfd, buf, sizeof(buf)-1, 0);
            if (n > 0) {
                buf[n] = '\0';
                SimpleTask* task = new SimpleTask(buf);
                if (!self->_pool->append(task)) {
                    std::cout << buf;
                    delete task;
                }
            } else if (n == 0) {
                self->_running = false;
                break;
            }
        }
        return nullptr;
    }
    
public:
    MinimalClient(const std::string& ip, int p) 
        : _server_ip(ip), _port(p), _running(false), _pool(nullptr), _sockfd(-1) {}
    
    bool connect() {
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0) return false;
        
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);
        inet_pton(AF_INET, _server_ip.c_str(), &addr.sin_addr);
        
        if (::connect(_sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(_sockfd);
            return false;
        }
        
        try {
            _pool = new ThreadPool<SimpleTask>(2, 30);
        } catch (...) {
            close(_sockfd);
            return false;
        }
        
        _running = true;
        return true;
    }
    
    void run() {
        pthread_create(&_recv_thread, nullptr, recv_func, this);
        
        std::string input;
        while (_running) {
            std::cout << "> ";
            std::getline(std::cin, input);
            
            if (input == "quit") {
                _running = false;
                break;
            }
            
            if (!input.empty() && input.back() != '\n')
                input += '\n';
                
            send(_sockfd, input.c_str(), input.length(), 0);
        }
        
        pthread_join(_recv_thread, nullptr);
    }
    
    ~MinimalClient() {
        if (_sockfd >= 0) close(_sockfd);
        if (_pool) delete _pool;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "用法: " << argv[0] << " IP 端口" << std::endl;
        return 1;
    }
    
    MinimalClient client(argv[1], std::stoi(argv[2]));
    
    if (!client.connect()) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }
    
    client.run();
    return 0;
}