#include "../include/HttpServer.h"
#include <getopt.h>
#include <cstring>
#include <regex>
#include <iostream>

#define MAXLINE 1024


int main(int argc,char *argv[]) {
    char opt;
    int port = 80, listenQueue = 32;
    std::string addr = "INADDR_ANY", root = "/home/cyx/platoneko.github.io/public";
    uint i_addr = INADDR_ANY;
    std::regex IP_PATTERN("((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\.){3}(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)");

    while ((opt=getopt(argc,argv,"hp:r:a:q:"))!=-1) {
        switch (opt) {
          case 'h':
            std::cout << "-h for help\n";
            std::cout << "-p for port\n";
            std::cout << "-d for root directory\n";
            std::cout << "-a for ip address\n" << std::endl;
            exit(0);
          case 'p':
            port = atoi(optarg);
            if (port < 0 || port > 0xffff) {
                std::cerr << "非法的端口号！" << std::endl;
                exit(-1);
            }
            break;
          case 'r':
            root = std::string(optarg);
            break;
          case 'a':
            addr = std::string(optarg); 
            {   
                std::smatch match;
                if (!regex_match(addr, match, IP_PATTERN)) {
                    std::cerr << "非法的ip地址！" << std::endl;
                    exit(-1);
                }
                i_addr = (stoi(match.str(1)) << 24) + 
                         (stoi(match.str(2)) << 16) + 
                         (stoi(match.str(3)) << 8) + 
                          stoi(match.str(4));
            }
            break;
          case 'q':
            listenQueue = atoi(optarg);
            if (listenQueue <= 1) {
                std::cerr << "非法的队列数目！" << std::endl;
                exit(-1);
            }
            break;                                    
        }
    }
    std::cout << "服务器配置：\n  ip地址: " << addr << "\n";
    std::cout << "  端口: " << port << "\n";
    std::cout << "  根目录: " << root << "\n" << std::endl;

    HttpServer httpServer = HttpServer(i_addr, port, root, listenQueue);
    httpServer.run();
    exit(0);
}
