#pragma once
#include <unistd.h>
#define BUFSIZE 8192

class RecvStream {
  private:
    int _fd;
    int _cnt;
    char *_bufptr;
    char _buf[BUFSIZE];
  public:
    RecvStream(int fd);
    ssize_t bufferedRecv(char *usrbuf, size_t n);
    ssize_t	bufferedRecvLine(char *usrbuf, size_t maxlen);
  private:
    ssize_t _recv(char *usrbuf, size_t n);
};
