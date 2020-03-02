#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

using std::cout;
using std::endl;

#define WWW_ROOT "./www/"

class Boundary{
    public:
        int64_t m_startAddr;
        int64_t m_dataLen;
        std::string m_name;
        std::string m_filename;
};

bool GetHeader(const std::string &key, std::string &val){
    std::string body;
    char *ptr = getenv(key.c_str());
    if (ptr == NULL){
        return false;
    }
    val = ptr;
    return true;
}

bool HeaderParse(std::string &header, Boundary &file) {
    std::vector<std::string> list;
    boost::split(list, header, boost::is_any_of("\r\n"), boost::token_compress_on);

    for(int i = 0; i < list.size(); ++i) {
        std::string sep = ": ";
        size_t pos = list[i].find(sep);
        if(pos == std::string::npos) {
            std::cerr << "find : error\n";
            return false;
        }
        std::string key = list[i].substr(0, pos);
        std::string val = list[i].substr(pos + sep.size());
        if (key != "Content-Disposition") {
            std::cerr << "can not find disposition\n";
            continue;
        }
        std::string nameFiled = "fileupload";
        std::string filenameSep = "filename=\"";
        pos = val.find(nameFiled);
        if (pos == std::string::npos) {
            std::cerr << "have no fileupload area\n";
            continue;
        }
        pos = val.find(filenameSep);
        if(pos == std::string::npos) {
            std::cerr << "have no filename\n";
            return false;
        }
        pos += filenameSep.size();
        size_t nextpos = val.find("\"", pos);
        if (nextpos == std::string::npos) {
            std::cerr << "have no \"\n";
            return false;
        }
        file.m_filename = val.substr(pos, nextpos - pos);
        file.m_name = "fileupload";
        // std::cerr << "----------filename:[" << file.m_filename << "]" << std::endl;
    }
    return true;
}

bool BoundaryParse(std::string &body, std::vector<Boundary> &list){
    std::string contboundary = "boundary=";
    std::string tmp;
    if (GetHeader("Content-Type", tmp) == false) {
        return false;
    }
    size_t pos, nextpos;
    pos = tmp.find(contboundary);
    if (pos == std::string::npos) {
        return false;
    }

    std::string dash = "--";
    std::string craf = "\r\n";
    std::string tail = "\r\n\r\n";
    std::string boundary = tmp.substr(pos + contboundary.size());
    std::string firBoundary = dash + boundary + craf;
    std::string midBoundary = craf + dash + boundary;

    pos = body.find(firBoundary);
    if (pos != 0) { 
        std::cerr << "first boundary error\n";
        return false;
    }
    pos += firBoundary.size();
    while (pos < body.size()) {
        nextpos = body.find(tail, pos); 
        if (nextpos == std::string::npos) {
            return false;
        }
        std::string header = body.substr(pos, nextpos - pos);

        pos = nextpos + tail.size(); 
        nextpos = body.find(midBoundary, pos);
        if (nextpos == std::string::npos) {
            return false;
        }

        int64_t offset = pos; 
        int64_t length = nextpos - pos;
        nextpos += midBoundary.size(); 
        pos = body.find(craf, nextpos);
        if (pos == std::string::npos) {
            return false;
        }

        pos +=  craf.size();
        Boundary file;
        file.m_dataLen = length;
        file.m_startAddr = offset;

        if (HeaderParse(header, file) == false) {
            std::cerr << "header parse error\n";
            return false;
        }
        list.push_back(file);

    }
    std::cerr << "parse boundary over\n";
    return true;
}

bool StorageFile(std::string &body, std::vector<Boundary> &list) {
    for(int i = 0; i <list.size(); ++i) {
        if (list[i].m_name != "fileupload") {
            continue;
        }
        std::string realpath = WWW_ROOT + list[i].m_filename;
        std::ofstream file(realpath);
        if (!file.is_open()) {
            std::cerr << "open file" << realpath << "failed\n";
            return false;
        }

        file.write(&body[list[i].m_startAddr], list[i].m_dataLen);
        if (!file.good()) {
            std::cerr << "write file error\n";
            return false;
        }
        file.close();
    }
    return true;
}

int main(int argc, char* argv[], char *env[]){
    std::string body;
    char *contLen = getenv("Content-Length");
    std::string err = "<html>Failed!!!</html>";
    std::string suc = "<html>Success!!!</html>";
    if (contLen != NULL){
        std:: stringstream tmp;
        tmp << contLen;
        int64_t fsize;
        tmp >> fsize;

        body.resize(fsize);
        int rlen = 0;
        while (rlen < fsize) {
            int ret = read(0, &body[0] + rlen, fsize - rlen);
            if (ret <= 0) {
                exit(-1);
            }
            rlen += ret;
        }
        std::vector<Boundary> list;
        bool ret;
        ret = BoundaryParse(body, list);
        if (ret == false) {
            std::cerr << "boundary parse error\n";
            std::cout << err;
            return -1;
        }
        ret = StorageFile(body, list);
        if (ret == false) {
            std::cerr << "storage error\n";
            std::cout << err;
            return -1;
        }
        std::cout << suc;
        return 0;
    }
    std::cout << err;
    return 0;
}
