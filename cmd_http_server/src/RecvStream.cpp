#include "../include/RecvStream.h"
#include <sys/socket.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>


RecvStream::RecvStream(int fd) {
    _fd = fd;
    _cnt = 0;  
    _bufptr = _buf;
}

ssize_t RecvStream::_recv(char *usrbuf, size_t n) {
    int cnt;

    if (_cnt <= 0) {  // 如果缓冲区为空，则从连接套接字(connfd)读取字节来填满缓冲区
	    _cnt = recv(_fd, _buf, sizeof(_buf), 0);
	    if (_cnt < 0) {
            printf("RECV ERROR!!!\n");
            exit(-1);
		    return -1;
	    }
	    else if (_cnt == 0) {  // EOF
            return 0;
        }  
	    else {
	        _bufptr = _buf;  // 之前缓冲区已经清空，重置缓冲区指针    
        }
    }

    cnt = _cnt < n ? _cnt : n;  // 缓冲区不足n个byte则拷贝缓冲区所有byte
    memcpy(usrbuf, _bufptr, cnt);  // 线程安全?
    _bufptr += cnt;
    _cnt -= cnt;
    return cnt;
}

ssize_t	RecvStream::bufferedRecv(char *usrbuf, size_t n) {
    size_t nleft = n;  // 还未读取的字节数
    ssize_t nread;  // 已读取的字节数
    char *bufp = usrbuf;
    
    while (nleft > 0) {  // 请求读取的字节数n可能大于缓冲区当前字节数
	    if ((nread = _recv(bufp, nleft)) < 0) {
		    return -1;
	    } 
	    else if (nread == 0) {  // EOF 
	        break;                         
        }
	    nleft -= nread;
	    bufp += nread;
    }
    return (n - nleft);
}

ssize_t	RecvStream::bufferedRecvLine(char *usrbuf, size_t maxlen) {
    int cnt, nread;
    char c, *bufp = usrbuf;

    for (cnt = 1; cnt < maxlen; ++cnt) { 
	    if ((nread = _recv(&c, 1)) == 1) {
	        *bufp++ = c;
	        if (c == '\n') {
                break;
            }
	    } else if (nread == 0) {
	        --cnt;
            break;
	    } else {
	        return -1;
        }
    }
    *bufp = '\0';
    return cnt;
}