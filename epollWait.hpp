#ifndef __M_EPOLL_H__
#define __M_EPOLL_H__

#include <iostream>
#include <string>
#include <vector>
#include <sys/epoll.h>
#include "tcpSocket.hpp"

#define MAX_EPOLL 1024

class Epoll{
    int m_epfd;
    public:
    Epoll():m_epfd(-1){
    }

    ~Epoll(){
    }

    bool Init() {
        m_epfd = epoll_create(MAX_EPOLL);
        if (m_epfd < 0) {
            std::cerr << "create epoll error\n";
            return false;
        }
        return true;
    }

    bool Add(TcpSocket &sock) {
        struct epoll_event ev;
        int fd = sock.GetFd();
        ev.events = EPOLLIN;//  | EPOLLET
        ev.data.fd = fd;
        int ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
        if (ret < 0) {
            std::cerr << "append monitor error\n";
            return false;
        }
        return true;
    }

    bool Del(TcpSocket &sock) {
        int fd = sock.GetFd();
        int ret = epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, NULL);
        if (ret < 0) {
            std::cerr << "remove monitor error\n";
            return false;
        }
        return true;
    }

    bool Wait(std::vector<TcpSocket> &v, int timeout = 3000) {
        struct epoll_event evs[MAX_EPOLL];
        int nfds = epoll_wait(m_epfd, evs, MAX_EPOLL, timeout);
        if (nfds < 0) {
            std::cerr << "epoll monitor error\n";
            return false;
        }else if (nfds == 0) {
            std::cerr << "epoll wait timeout\n";
            return false;
        }
        for (int i = 0; i < nfds; i++) {
            int fd = evs[i].data.fd;
            TcpSocket sock;
            sock.SetFd(fd);
            v.push_back(sock);
        }
        return true;
    }
};
#endif
