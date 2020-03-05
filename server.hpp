#ifndef __M_SRV_H__
#define __M_SRV_H__

#include "tcpSocket.hpp"
#include "epollWait.hpp"
#include "http.hpp"
#include "threadPool.hpp"
#include <boost/filesystem.hpp>
#include <fstream>

using std::cout;
using std::endl;

#define WWW_ROOT "./www/"

class Server{
    TcpSocket m_listenSock;
    ThreadPool m_pool;
    Epoll m_epoll;
    public:
    bool Start(int port) {
        bool ret;
        ret = m_listenSock.SocketInit(port);
        if(ret == false){
            return false;
        }

        ret = m_pool.PoolInit();
        if(ret == false){
            return false;
        }

        ret = m_epoll.Init();
        if(ret == false){
            return false;
        }

        m_epoll.Add(m_listenSock);
        while(1){
            std::vector<TcpSocket> list;
            ret = m_epoll.Wait(list);
            if(ret == false){
                continue;
            }

            for(int i = 0; i < list.size(); ++i){
                if (list[i].GetFd() == m_listenSock.GetFd()) {
                    TcpSocket clientSock;
                    ret = m_listenSock.Accept(clientSock);
                    if (ret == false) {
                        continue;
                    }
                    clientSock.SetNonBlock();
                    m_epoll.Add(clientSock);
                } else {
                    ThreadTask tt(list[i].GetFd(), ThreadHandler);
                    m_pool.TaskPush(tt);
                    m_epoll.Del(list[i]);
                }
            }
        }
        m_listenSock.Close();
        return true;
    }
    public:
    static void ThreadHandler(int sockfd) {
        TcpSocket sock;
        sock.SetFd(sockfd);

        HttpRequest req;
        HttpResponse rsp;
        int status = req.RequestParse(sock);
        if (status != 200) {

            rsp.m_status = status;
            rsp.ErrorProcess(sock);
            sock.Close();
            return;   
        }
        std::cout << "---------------" << endl;

        HttpProcess(req, rsp);
        rsp.NormalProcess(sock);
        sock.Close();
        return; 
    }

    static int64_t str_to_digit(const std::string val){
        std::stringstream tmp;
        tmp << val;
        int64_t dig = 0;
        tmp >> dig;
        return dig;

    }

    static bool HttpProcess(HttpRequest &req, HttpResponse &rsp) {
        std::string realpath = WWW_ROOT + req.m_path;
        if (!boost::filesystem::exists(realpath)) {
            rsp.m_status = 404;
            std::cerr << "realpath:[" << realpath << "]\n";
            return false;
        } 

        if ((req.m_method == "GET" && req.m_param.size() != 0) || req.m_method == "POST") {
            CGIProcess(req, rsp);
        } else {
            if (boost::filesystem::is_directory(realpath)) {
                ListShow(req, rsp);
            } else {
                RangeDownload(req, rsp);
                return true;
            }
        }
        rsp.m_status = 200;
        return true;
    }

    static bool RangeDownload(HttpRequest &req, HttpResponse &rsp) {
        std::string realpath = WWW_ROOT + req.m_path;
        int64_t dataLen = boost::filesystem::file_size(realpath);
        int64_t lastwTime = boost::filesystem::last_write_time(realpath);
        std::string etag = std::to_string(dataLen) + std::to_string(lastwTime);

        auto it = req.m_headers.find("Range");
        if (it == req.m_headers.end()) {
            Download(realpath, 0, dataLen, rsp.m_body);
            rsp.m_status = 200;
        }else {
            std::string range = it->second;

            std::string unit = "bytes=";
            size_t pos = range.find(unit);
            if (pos == std::string::npos) {
                return false;
            } 
            pos += unit.size();
            size_t pos2 = range.find("-", pos);
            if (pos2 == std::string::npos) {
                return false;
            }
            std::string start = range.substr(pos, pos2 - pos);
            std::string end = range.substr(pos2 + 1);
            int64_t digStart, digEnd;

            digStart = str_to_digit(start);
            if (end.size() == 0) {
                digEnd = dataLen - 1;
            } else {
                digEnd = str_to_digit(end);
            }

            int64_t rangeLen = digEnd - digStart + 1;
            Download(realpath, digStart, rangeLen, rsp.m_body);
            std::stringstream tmp;
            tmp << "bytes " << digStart << "-" << digEnd << "/" << dataLen;
            rsp.SetHeader("Content-Range", tmp.str());
            rsp.m_status = 206;
        }
        rsp.SetHeader("Content-Type", "application/octet-stream");
        rsp.SetHeader("Accept-Ranges", "bytes");
        rsp.SetHeader("ETag", etag);

        return true;
    }

    static bool Download(std::string &path, int64_t start, int64_t len, std::string &body){
        body.resize(len);
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "open file error\n";
            return false;
        }

        file.seekg(start, std::ios::beg);
        file.read(&body[0], len);
        if (!file.good()) {
            std::cerr << "read file data error\n";
            return false;
        } 
        file.close();
        return true;
    }

    static bool CGIProcess(HttpRequest &req,HttpResponse &rsp){
        int pipeIn[2], pipeOut[2];
        if (pipe(pipeIn) < 0 || pipe(pipeOut) < 0) {
            std::cerr << "create pipe error\n";
            return false;
        }

        int pid = fork();
        if (pid < 0) {
            return false;
        } else if (pid == 0) {
            close(pipeIn[0]);
            close(pipeOut[1]);
            dup2(pipeIn[1], 1);
            dup2(pipeOut[0], 0);
            setenv("METHOD", req.m_method.c_str(), 1);
            setenv("PATH", req.m_path.c_str(), 1);

            for (auto &i : req.m_headers) {
                setenv(i.first.c_str(), i.second.c_str(), 1);
            }
            std::string realpath = WWW_ROOT + req.m_path;
            execl(realpath.c_str(), realpath.c_str(), NULL);
            exit(0);
        } 
        close(pipeIn[1]);
        close(pipeOut[0]);
        write(pipeOut[1], &req.m_body[0], req.m_body.size());
        while(1) {
            char buf[1024] = {0};
            int ret = read(pipeIn[0], buf, 1024);
            if (ret == 0) {
                break;
            }
            buf[ret] = '\0';
            rsp.m_body += buf;
        }
        close(pipeIn[0]);
        close(pipeOut[1]);
        rsp.m_status = 200;
        return true;
    }

    static bool ListShow(HttpRequest &req, HttpResponse &rsp){
        std::string realpath = WWW_ROOT + req.m_path;
        std::string req_path = req.m_path;
        std::stringstream tmp;
        tmp << "<html><head><style>";
        tmp << "*{margin : 0;}";
        tmp << ".main-window {height : 100%;width : 80%;margin : 0 auto;}";
        tmp << ".upload {position : relative;height : 20%;width : 100%;background-color : #33c0b9; text-align:center;}";
        tmp << ".listshow {position : relative;height : 80%;width : 100%;background : #6fcad6;}";
        tmp << "</style></head>";
        tmp << "<body><div class='main-window'>";
        tmp << "<div class='upload'>";
        tmp << "<form action='/upload' method='POST'";
        tmp << "enctype='multipart/form-data'>";
        tmp << "<div class='upload-btn'>";
        tmp << "<input type='file' name='fileupload'>";
        tmp<<"<input type='submit' name='submit' >";
        tmp << "</div></form>";
        tmp << "</div><hr />";
        tmp << "<div class='listshow'><ol>";

        boost::filesystem::directory_iterator begin(realpath);
        boost::filesystem::directory_iterator end;
        for ( ; begin != end; ++begin ) {
            int64_t ssize, mtime;
            std::string pathname = begin->path().string();
            std::string name=begin->path().filename().string();
            std::string uri = req_path + name;
            if (boost::filesystem::is_directory(pathname)) {
                tmp<<"<li><strong><a href='";
                tmp<<uri<<"'>";
                tmp<<name<< "/";
                tmp<<"</a><br /></strong>";
                tmp<<"<small>filetype: directory";
                tmp<<"</small></li>";
            } else {
                mtime = boost::filesystem::last_write_time(begin->path());
                ssize = boost::filesystem::file_size(begin->path());
                tmp<<"<li><strong><a href='";
                tmp<<uri<<"'>";
                tmp<<name;
                tmp<<"</a><br /></strong>";
                tmp<<"<small>modified: ";
                tmp<<mtime;
                tmp<<"<br /> filetype: application-octstream ";
                tmp<<ssize / 1024 << "kbytes";
                tmp<<"</small></li>";
            }
        }
        tmp << "</ol></div><hr /></div></body></html>";
        rsp.m_body = tmp.str();
        rsp.m_status = 200;
        rsp.SetHeader("Content-Type", "text/html");
        return true;
    }
};
#endif
