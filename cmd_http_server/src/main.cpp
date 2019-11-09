#include "../include/HttpServer.hpp"
#include <getopt.h>
#include <cstring>
#include <regex>
#include <iostream>
#include "../include/ThreadGuard.hpp"

#define MAXLINE 4096

enum SignalType {NONE_SIG, RUN_SIG, CLOSE_SIG, EXIT_SIG};
int signal = NONE_SIG;

void listenThreadRoutine() {
    char command[MAXLINE];
    while (1) {
        scanf("%s", command);
        if (strcmp(command, "run") == 0) {
            signal = RUN_SIG;
        } else if (strcmp(command, "close") == 0) {
            signal = CLOSE_SIG;
        } else if (strcmp(command, "exit") == 0) {
            signal = EXIT_SIG;
        } else {
            printf("未定义的命令: %s\n", command);
            signal = NONE_SIG;
        }
        usleep(100*1000);
    }
}

int main(int argc,char *argv[]) {
    char opt;
    int port = 80, listenQueue = 32;
    std::string addr = "INADDR_ANY", root = "/home/cyx/platoneko.github.io/public";
    uint i_addr = INADDR_ANY;
    std::regex IP_PATTERN("((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\.){3}(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)");
    
    uint i_timeoutVal = 5000;
    TimeVal timeoutVal;
    timeoutVal.sec = 5;
    timeoutVal.usec = 0;

    while ((opt=getopt(argc,argv,"hp:r:a:q:t:"))!=-1) {
        switch (opt) {
          case 'h':
            std::cout << "-h for help\n";
            std::cout << "-p for port\n";
            std::cout << "-d for root directory\n";
            std::cout << "-a for ip address\n";
            std::cout << "-t for timeout value (ms)\n" << std::endl;
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
          case 't':
            i_timeoutVal = atoi(optarg);
            timeoutVal.sec = i_timeoutVal / 1000;
            timeoutVal.usec = (i_timeoutVal % 1000) * 1000;
            break;                   
        }
    }
    std::cout << "服务器配置：\n  ip地址: " << addr << "\n";
    std::cout << "  端口: " << port << "\n";
    std::cout << "  根目录: " << root << "\n";
    std::cout << "  持续超时时间: " << i_timeoutVal << "ms\n" << std::endl;

    HttpServer httpServer = HttpServer(i_addr, port, root, listenQueue, timeoutVal);

    std::thread listenThread(listenThreadRoutine);
    ThreadGuard g(listenThread);

    while (1) {
      switch (signal)
      {
        case NONE_SIG:
          break;
        case RUN_SIG:
          if (httpServer.isRunning()) {
              printf("FAILURE: 服务器正在运行！\n");
          } else {
              httpServer.run();
              printf("SUCCESS: 服务器启动！\n");              
          }
          fflush(stdout);
          signal = NONE_SIG;
          break;
        case CLOSE_SIG:
          if (httpServer.isRunning()) {
              httpServer.close_();
              printf("SUCCESS: 服务器停止运行！\n");
          } else {
              printf("FAILURE: 服务器已处于停机状态！\n");              
          }
          fflush(stdout);
          signal = NONE_SIG;
          break;
        case EXIT_SIG:
          printf("SUCCESS: 退出程序！\n");
          fflush(stdout);
          exit(0);
      }
      usleep(100*1000);
    }

    exit(0);
}
