#ifndef __M_TCP_H__
#define __M_TCP_H__

#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

const size_t MAX_HEAD = 8192;

class TcpSocket{
    int m_sockfd;
public:
    TcpSocket():m_sockfd(-1) {
    }

    int GefFd(){
        return m_sockfd;
    }

    void SetFd(int fd) {
        m_sockfd = fd;
    }

    void SetNonBlock() {
        int flag = fcntl(m_sockfd, F_GETFL, 0);
        fcntl(m_sockfd, F_SETFL, flag | O_NONBLOCK);
    }

    bool SocketInit(int port) {
        m_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(m_sockfd < 0) {
            std::cerr << "socket error\n";
            return false;
        }

        int opt = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));
        struct sockaddr_in addr;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;
        socklen_t len = sizeof(addr);
        int ret = bind(m_sockfd, (struct sockaddr*)&addr, len);

        if(ret < 0) {
            std::cerr << "bind error\n";
            close(m_sockfd);
            return false;
        }
        
        ret = listen(m_sockfd, 10);
        if (ret < 0) {
            std::cerr << "listen error\n";
            close(m_sockfd);
            return false;
        }
        return true;
    }

    bool Accept(TcpSocket &sock){
        int fd;
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        fd = Accept(m_sockfd, (struct sockaddr*)&adddr, &len);
        if (fd < 0) {
            std::cerr << "accept error\n";
            return false;
        }
        sock.SetFd(fd);
        sock.SetNonBlock();
        return true;
    }

    bool RecvPeek(std::string &buf) {
        buf.clear();
        char tmp[MAX_HEAD] = {0};
        int ret = recv(m_sockfd, tmp, MAX_HEAD, MSG_PEEK);

        if(ret <= 0) {
            if (errno == EAGAIN) {
                return true;
            }
            std::cerr << "recv error\n";
            return false;
        }
        buf.assign(tmp, ret);
        return true;
    }

    bool Recv(std::string &buf, int len){
        buf.resize(len);
        int recvlen = 0, ret;
        while(recvlen < len) {
            ret = recv(m_sockfd, &buf[0] + recvlen, len - recvlen, 0);
            if (ret <= 0){
                
                if (errno == EAGAIN) {
                    usleep(1000);
                    continue;
                }
                return false;
            }
            recvlen += ret;
        }
        return true;
    }

    bool Send(const std::string &buf) {
        int ret = send(m_sockfd, &buf[0], buf.size(), 0);
        if (ret < 0) {
            std::cerr << "send error\n";
            return false;
        }
        return true;
    }

    bool Close(){
        close(m_sockfd);
    }
};

#endif