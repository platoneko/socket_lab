#pragma once
#include <string>
#include <netinet/in.h>
#include "../include/RecvStream.hpp"
#include <thread>
#include <vector>

struct TimeVal {
  time_t sec;
  time_t usec;
};


class HttpServer  {
  private:
    uint _addr;
    int _port;
    std::string _root;
    int _listenQueue;
    TimeVal _timeoutVal;
    int _threadNum;

    int _listenfd;
    bool _isRunning = false;
    std::vector<std::thread> _threadPool;

  public:
    HttpServer(uint addr, int port, const std::string &root, 
               int listenQueue, TimeVal timeoutVal, int threadNum):
               _addr(addr), _port(port), _root(root), 
               _listenQueue(listenQueue), _timeoutVal(timeoutVal), _threadNum(threadNum) {}
    int run();
    void close_();
    bool isRunning() {
      return _isRunning;
    }
    void setAddr(uint addr) { _addr = addr; }
    void setPort(int port) { _port = port; }
    void setRoot(const std::string &root) { _root = root; }
    void setListenQueue(int listenQueue) { _listenQueue = listenQueue; }
    void setTimeoutVal(TimeVal timeoutVal) { _timeoutVal = timeoutVal; }
    void setThreadNum(int threadNum) { _threadNum = threadNum; }

  private:
    int _openListenfd();
    void _handleRequest(int clientfd);
    void _errorPage(int fd, const char *cause, const char *errnum, 
                    const char *shortmsg, const char *longmsg);
    void _staticResponse(int fd, const char *filename, int filesize, bool persistent);
    void _getMIME(const char *filepath, char *MIME);
    void _readRequestHeaders(RecvStream &recvStream, bool &persistent);
    void _readRequestLine(char *buf, char *method, char *uri, char *version);
    void _readRequestBody(RecvStream &recvStream);
    void _getFilepath(char *filepath, const char *uri);
};
