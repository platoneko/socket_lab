#pragma once
#include <string>
#include <netinet/in.h>
#include "../include/RecvStream.h"


class HttpServer  {
  private:
    uint _addr;
    int _port;
    std::string _root;
    int _listenQueue;
  public:
    HttpServer(uint addr, int port, const std::string &root, int listenQueue):
               _addr(addr), _port(port), _root(root), _listenQueue(listenQueue) {}
    int run();
  private:
    int _openListenfd();
    void _handleRequest(int clientfd);
    void _errorPage(int fd, const char *cause, const char *errnum, 
                    const char *shortmsg, const char *longmsg);
    void _staticResponse(int fd, const char *filename, int filesize);
    void _getMIME(const char *filepath, char *MIME);
    void _readRequestHeaders(RecvStream &recvStream, bool &persistent);
    void _readRequestLine(char *buf, char *method, char *uri, char *version);
    void _readRequestBody(RecvStream &recvStream);
    void _getFilepath(char *filepath, const char *uri);
};
