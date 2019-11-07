#include "../include/HttpServer.h"
#include "../include/RecvStream.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <thread>
#include <time.h>

#define MAXLINE 1024
#define BUFSIZE 8192


int HttpServer::run() {
    int listenfd = _openListenfd();

    auto threadFun = [&](int connfd, char *client_hostname, char *client_port) -> void {
        _handleRequest(connfd);
        close(connfd);
        printf("CLOSE CONNECTION: %s:%s at connfd: %d\n", client_hostname, client_port, connfd);
    };
    
    while (1) {
        struct sockaddr_storage clientaddr;
        int connfd;
        socklen_t clientlen;
        char client_hostname[MAXLINE], client_port[MAXLINE];

        clientlen = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &_timeoutVal, sizeof(_timeoutVal));
        getnameinfo((struct sockaddr *)&clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("CONNECTION: %s:%s at connfd: %d\n", client_hostname, client_port, connfd);
        std::thread t(threadFun, connfd, client_hostname, client_port);
        t.detach();
    }
}

int HttpServer::_openListenfd () {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1;
 
    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		   (const void *)&optval , sizeof(int)) < 0)
	return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(_addr); 
    serveraddr.sin_port = htons((unsigned short)_port); 
    if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, _listenQueue) < 0)
	return -1;
    return listenfd;
}

void HttpServer::_handleRequest(int connfd) {
    struct stat sbuf;
    char buf[BUFSIZE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filepath[MAXLINE];

    bool persistent;
    uint n;

    RecvStream recvStream = RecvStream(connfd);
    if ((n = recvStream.bufferedRecvLine(buf, BUFSIZE)) <= 0) {
        return;
    };
    _readRequestLine(buf, method, uri, version);  // 解析HTTP请求行
    _readRequestHeaders(recvStream, persistent);
    _readRequestBody(recvStream);

    if (strcmp(method, "GET") != 0) {  // 线程安全?
        _errorPage(connfd, method, "501", "Not implemented", 
                   "本辣鸡不提供这种服务！");
        return;
    }
    
    _getFilepath(filepath, uri);
    if (stat(filepath, &sbuf) < 0) {
        _errorPage(connfd, uri, "404", "Not found", 
                   "垃圾堆里没有这个文件！");
        return;
    }

    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        _errorPage(connfd, uri, "403", "Forbidden", 
                   "无权访问！");
        return;
    } else {
        _staticResponse(connfd, filepath, sbuf.st_size, persistent);
    }
    
    while (persistent) {
        if (recvStream.bufferedRecvLine(buf, BUFSIZE) <= 0) {
            return;
        };
        printf("%s", buf);
        _readRequestLine(buf, method, uri, version);  // 解析HTTP请求行
        _readRequestHeaders(recvStream, persistent);
        _readRequestBody(recvStream);

        if (strcmp(method, "GET") != 0) {  // 线程安全?
            _errorPage(connfd, method, "501", "Not implemented", 
                        "本辣鸡不提供这种服务！");
            return;
        }
    
        _getFilepath(filepath, uri);
        if (stat(filepath, &sbuf) < 0) {
            _errorPage(connfd, uri, "404", "Not found", 
                    "垃圾堆里没有这个文件！");
            return;
        }

        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            _errorPage(connfd, uri, "403", "Forbidden", 
                    "无权访问！");
            return;
        } else {
            _staticResponse(connfd, filepath, sbuf.st_size, persistent);
        }
    }
}

void HttpServer::_errorPage(int fd, const char *cause, const char *errnum, const char *shortmsg, const char *longmsg) {
    char buf[BUFSIZE], body[BUFSIZE], tmp[MAXLINE];

    strcpy(body, "<html><title>Oops!</title>");
    strcat(body, "<meta charset=\"utf-8\">\r\n");
    strcat(body, "<body bgcolor=\"ffffff\">\r\n");
    snprintf(tmp, sizeof(tmp), "%s: %s\r\n", errnum, shortmsg);
    strcat(body, tmp);
    snprintf(tmp, sizeof(tmp), "<p>%s: %s\r\n", longmsg, cause);
    strcat(body, tmp);

    // HTTP响应报文
    snprintf(buf, sizeof(buf), "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    strcat(buf, "Content-type: text/html\r\n");
    snprintf(tmp, sizeof(tmp), "Content-length: %d\r\n\r\n", (int)strlen(body));
    strcat(buf, tmp);
    send(fd, buf, strlen(buf), 0);
    send(fd, body, strlen(body), 0);

}

void HttpServer::_staticResponse(int fd, const char *filepath, int filesize, bool persistent) {
    int srcfd;
    char MIME[MAXLINE], buf[BUFSIZE], tmp[MAXLINE];
    void *srcp;

    _getMIME(filepath, MIME);
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    strcat(buf, "Server: CYX PC\r\n");
    if (persistent) {
        strcat(buf, "Connection: keep-alive\r\n");
    } else {
        strcat(buf, "Connection: close\r\n");
    }
    snprintf(tmp, sizeof(tmp), "Content-length: %d\r\n", filesize);
    strcat(buf, tmp);
    snprintf(tmp, sizeof(tmp), "Content-type: %s\r\n\r\n", MIME);
    strcat(buf, tmp);
    send(fd, buf, strlen(buf), 0);
    
    srcfd = open(filepath, O_RDONLY, 0);
    if (srcfd < 0) {
        exit(-1);
    }
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    send(fd, srcp, filesize, 0);
    munmap(srcp, filesize);
}

void HttpServer::_getMIME(const char *filepath, char *MIME) {
    if (strstr(filepath, ".html")) 
        strcpy(MIME, "text/html");
    else if (strstr(filepath, ".css")) 
        strcpy(MIME, "text/css");
    else if (strstr(filepath, ".js")) 
        strcpy(MIME, "text/javascript");
    else if (strstr(filepath, ".gif")) 
        strcpy(MIME, "image/gif");
    else if (strstr(filepath, ".png")) 
        strcpy(MIME, "image/png");
    else if (strstr(filepath, ".jpg")) 
        strcpy(MIME, "image/jpeg");
    else if (strstr(filepath, ".bmp")) 
        strcpy(MIME, "image/bmp");
    else if (strstr(filepath, ".webp")) 
        strcpy(MIME, "image/webp");
    else if (strstr(filepath, ".ico")) 
        strcpy(MIME, "image/x-icon");
    else
        strcpy(MIME, "text/plain");
}

// 线程安全的请求头解析方法
void HttpServer::_readRequestLine(char *buf, char *method, char *uri, char *version) {
    char *ptr = buf, c;
    while ((c = *ptr++) != ' ') {
        *method++ = c;
    }
    *method = '\0';
    while ((c = *ptr++) != ' ') {
        *uri++ = c;
    }
    *uri = '\0';
    while ((c = *ptr++) != '\r') {
        *version++ = c;
    }
    *version = '\0';
}

void HttpServer::_readRequestHeaders(RecvStream &recvStream, bool &persistent) {
    char buf[BUFSIZE];
    persistent = false;
    recvStream.bufferedRecvLine(buf, BUFSIZE);
    if (strcmp(buf, "Connection: keep-alive\r\n") == 0) {
        persistent = true;
    }
    while (strcmp(buf, "\r\n")) {
        recvStream.bufferedRecvLine(buf, BUFSIZE);
        if (persistent || strcmp(buf, "Connection: keep-alive\r\n") == 0) {
            persistent = true;
        }
    }
}

void HttpServer::_readRequestBody(RecvStream &recvStream) {
    char buf[BUFSIZE];
    if (recvStream.recvEOF()) {
        if (recvStream.isEmpty()) {  // body为空
            return;
        }  else {
            recvStream.bufferedRecv(buf, BUFSIZE);
            return;
        }
    }
    if (!recvStream.recvEOF()) {
        recvStream.bufferedRecv(buf, BUFSIZE);
        while (!recvStream.recvEOF()) {
            recvStream.bufferedRecv(buf, BUFSIZE);
        } 
        if (recvStream.isEmpty()) {
            return;
        } else {
            recvStream.bufferedRecv(buf, BUFSIZE);
            return;
        }
    }
}

void HttpServer::_getFilepath(char *filepath, const char *uri) {
    if (strcmp(uri, "") == 0) {
        strcpy(filepath, (_root + "index.html").c_str());
    } else if (uri[strlen(uri)-1] == '/') {
        strcpy(filepath, (_root + uri + "index.html").c_str());
    } else if (strcmp(uri, "/about") == 0 ||
               strcmp(uri, "/archives") == 0) {
        strcpy(filepath, (_root + uri + "/index.html").c_str());
    } else {
        strcpy(filepath, (_root + uri).c_str());
    }
}
