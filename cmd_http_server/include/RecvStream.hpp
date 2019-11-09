#pragma once
#include <unistd.h>
#define BUFSIZE 8192

class RecvStream {
  private:
    int _fd;
    int _cnt;
    char *_bufptr;
    char _buf[BUFSIZE];
    bool _eof;
  public:
    RecvStream(int fd);
    ssize_t bufferedRecv(char *usrbuf, size_t n);
    ssize_t	bufferedRecvLine(char *usrbuf, size_t maxlen);
    bool isEmpty() {
      return _cnt == 0;
    }
    bool recvEOF() {
      return _eof;
    }
  private:
    ssize_t _recv(char *usrbuf, size_t n);
};
