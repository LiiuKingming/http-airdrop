#ifndef __M_HTTP_H__
#define __M_HTTP_H__

#include <unordered_map>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "tcpSocket.hpp"

using std::cout;
using std::endl;

class HttpRequest {
    public:
        std::string m_method;
        std::string m_path;
        std::unordered_map<std::string, std::string> m_param;
        std::unordered_map<std::string, std::string> m_headers;
        std::string m_body;
    private:
        bool RecvHeader(TcpSocket &sock, std::string &header){
            while(1) {
                std::string tmp;
                if (sock.RecvPeek(tmp) == false) {
                    return false;
                }

                size_t pos;
                pos = tmp.find("\r\n\r\n", 0);

                if (pos == std::string::npos && tmp.size() == MAX_HEAD){
                    return false;
                } else if(pos != std::string::npos) {
                    size_t headerLength = pos + 4;
                    sock.Recv(header, headerLength);
                    return true;
                }
            }
        }

        bool FirstLineParse(std::string &line) {
            std::vector<std::string> lineList;
            boost::split(lineList, line, boost::is_any_of(" "), boost::token_compress_on);

            if(lineList.size() != 3){
                std::cerr << "parse first line error\n";
                return false;
            }
            m_method = lineList[0];

            size_t pos = lineList[1].find("?", 0);
            if (pos == std::string::npos) {
                m_path = lineList[1];
            } else {
                m_path = lineList[1].substr(0, pos);
                std::string queryString; 
                queryString = lineList[1].substr(pos + 1);

                std::vector<std::string> paramList;
                boost::split(paramList, queryString, 
                        boost::is_any_of("&"), 
                        boost::token_compress_on);
                for(auto &i : paramList) {
                    size_t paramPos = -1;
                    paramPos = i.find("=");
                    if (paramPos == std::string::npos) {
                        std::cerr << "parse param error\n";
                        return false;
                    }
                    std::string key = i.substr(0, paramPos);
                    std::string val = i.substr(paramPos + 1);
                    m_param[key] = val; 
                }
            }
            return true;
        }

        bool PathIsLegal() {
            return true;
        }

    public:
        bool RequestParse(TcpSocket &sock) {
            std::string header;
            if (RecvHeader(sock, header) == false) {
                return 400;
            }

            std::vector<std::string> headerList;
            boost::split(headerList, header, boost::is_any_of("\r\n"),
                    boost::token_compress_on);

            for(int i = 0; i < headerList.size(); ++i){
                std::cout << "list[i] = [" << headerList[i] << "]\n";
            }

            if(FirstLineParse(headerList[0]) == false){
                return 400;
            }

            size_t pos = 0;
            for (int i = 1; i < headerList.size(); ++i) {
                pos = headerList[i].find(": ");
                if (pos == std::string::npos) {
                    std::cerr << "header parse error\n";
                    return false;
                }
                std::string key = headerList[i].substr(0, pos);
                std::string val = headerList[i].substr(pos + 2);
                m_headers[key] = val;
            }

            cout << "mothod:[" << m_method <<"]\n";
            cout << "path:[" << m_path <<"]\n";
            for (auto &i : m_param) {
                cout << i.first << " = " << i.second << endl;
            }
            for (auto &i : m_headers) {
                cout << i.first << " = " << i.second << endl;
            }

            auto it = m_headers.find("Content-Length");
            if (it != m_headers.end()) {
                std::stringstream tmp;
                tmp << it->second;
                int64_t filelength;
                tmp >>filelength;
                sock.Recv(m_body, filelength);
            }
            return 200;
        }
};

class HttpResponse{
    public:
        int m_status;
        std::unordered_map<std::string, std::string> m_headers;
        std::string m_body;
    private:
        std::string GetDesc() {
            switch (m_status) {
                case 200: return "OK";
                case 400: return "Bad Request";
                case 404: return "Not Found";
            }
            return "Unknow";
        }
    public:
        bool SetHeader(const std::string &key, const std::string &val) {
            m_headers[key] = val;
            return true;
        }

        bool ErrorProcess(TcpSocket &sock){
            return true;
        }

        bool NormalProcess(TcpSocket &sock){
            std::stringstream tmp;
            tmp << "HTTP/1.1" << " " << m_status << " " << GetDesc();
            tmp << "\r\n";
            if (m_headers.find("Content-Length") == m_headers.end()) {
                std::string len = std::to_string(m_body.size());
                m_headers["Content-Length"] = len;
            }

            for (auto &i : m_headers) {
                tmp << i.first << ": " << i.second << "\r\n";
            }
            tmp << "\r\n";
            sock.Send(tmp.str());
            sock.Send(m_body);
            return true;
        }
};
#endif
