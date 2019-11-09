#pragma once
#include <string>
#include <netinet/in.h>
#include "../include/RecvStream.hpp"
#include <thread>
#include <list>

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

    int _listenfd;
    bool _isRunning = false;
    
  public:
    HttpServer(uint addr, int port, const std::string &root, int listenQueue, TimeVal timeoutVal):
               _addr(addr), _port(port), _root(root), _listenQueue(listenQueue), _timeoutVal(timeoutVal) {}
    void run();
    void close_();
    bool isRunning() {
      return _isRunning;
    }
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
