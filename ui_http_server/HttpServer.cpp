#include "HttpServer.h"
#include "./ui_HttpServer.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <thread>
#include <regex>
// #include <QMutex>

#define MAXLINE 4096
#define BUFSIZE 8192

// static QMutex mtx_w;
// static QMutex mtx_r;


HttpServer::HttpServer(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::HttpServer)
{
    ui->setupUi(this);

    _i_addr = INADDR_ANY;
    _addr = "INADDR_ANY";

    _port = 23333;
    _root = "/home/cyx/platoneko.github.io/public";
    _threadNum = 32;
    _listenQueue = 32;

    _i_timeoutVal = 5000;
    _timeoutVal.sec = 5;
    _timeoutVal.usec = 0;

    _isRunning = false;

    connect(this, SIGNAL(msgSignal(QString)), this, SLOT(printMsg(QString)), Qt::QueuedConnection);
}

HttpServer::~HttpServer()
{
    delete ui;
}

int HttpServer::run() {
    auto handleThreadRoutine = [&]() -> void {
        struct sockaddr_storage clientaddr;
        int connfd;
        socklen_t clientlen;
        char client_hostname[MAXLINE], client_port[MAXLINE];

        QString msg;
        while (_isRunning) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = accept(_listenfd, (struct sockaddr *)&clientaddr, &clientlen);
            if (connfd < 0) {
                continue;
            }
            setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &_timeoutVal, sizeof(_timeoutVal));
            getnameinfo((struct sockaddr *)&clientaddr, clientlen, client_hostname, MAXLINE,
                        client_port, MAXLINE, 0);
            printf("CONNECTION: %s:%s at connfd: %d\n", client_hostname, client_port, connfd);

            // mtx_w.tryLock();
            msg.sprintf("CONNECTION: %s:%s at connfd: %d\n", client_hostname, client_port, connfd);
            emit msgSignal(msg);
            // mtx_r.unlock();

            _handleRequest(connfd);
            ::close(connfd);
            printf("CLOSE CONNECTION: connfd: %d\n", connfd);

            // mtx_w.tryLock();
            msg.sprintf("CLOSE CONNECTION: connfd: %d\n", connfd);
            emit msgSignal(msg);
            // mtx_r.unlock();
        }
    };

    _listenfd = _openListenfd();
    if (_listenfd < 0) {
        return -1;
    }
    _isRunning = true;
    for (int i=0; i<_threadNum; ++i) {
        _threadPool.emplace_back(handleThreadRoutine);
    }
    return 0;
}

void HttpServer::close_() {
    _isRunning = false;
    int i = 0;
    QString msg;
    for (auto it = _threadPool.begin(), end = _threadPool.end();
         it != end; ++it, ++i) {
        (*it).join();
        printf("exit ThreadRoutine %d\n", i);
        // msg.sprintf("exit ThreadRoutine %d\n", i);
        // emit msgSignal();
    }
    // mtx_w.tryLock();
    msg.sprintf("Exit All ThreadRoutine!\n");
    emit msgSignal(msg);
    // mtx_r.unlock();

    ::close(_listenfd);
    _threadPool.clear();
}

int HttpServer::_openListenfd () {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    TimeVal timeoutVal;
    timeoutVal.sec = 5;
    timeoutVal.usec = 0;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
           (const void *)&optval , sizeof(int)) < 0)
        return -1;

    setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, &timeoutVal, sizeof(timeoutVal));
    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(_i_addr);
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

    QString msg;

    RecvStream recvStream = RecvStream(connfd);
    if (recvStream.bufferedRecvLine(buf, BUFSIZE) <= 0) {
        return;
    }

    // mtx_w.tryLock();
    msg.sprintf("%s", buf);
    emit msgSignal(msg);
    // mtx_r.unlock();

    _readRequestLine(buf, method, uri, version);  // 解析HTTP请求行
    _readRequestHeaders(recvStream, persistent);
    _readRequestBody(recvStream);

    if (strcmp(method, "GET") != 0) {
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

        // mtx_w.tryLock();
        msg.sprintf("%s", buf);
        emit msgSignal(msg);
        // mtx_r.unlock();

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
    char buf[BUFSIZE], body[BUFSIZE];

    sprintf(body, "<html><title>Oops!</title>");
    sprintf(body, "%s<meta charset=\"utf-8\">\r\n", body);
    sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    /*
    strcpy(body, "<html><title>Oops!</title>");
    strcat(body, "<meta charset=\"utf-8\">\r\n");
    strcat(body, "<body bgcolor=\"ffffff\">\r\n");
    snprintf(tmp, sizeof(tmp), "%s: %s\r\n", errnum, shortmsg);
    strcat(body, tmp);
    snprintf(tmp, sizeof(tmp), "<p>%s: %s\r\n", longmsg, cause);
    strcat(body, tmp);
    */

    // HTTP响应报文
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    sprintf(buf, "%sContent-type: text/html\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));

    /*
    snprintf(buf, sizeof(buf), "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    strcat(buf, "Content-type: text/html\r\n");
    snprintf(tmp, sizeof(tmp), "Content-length: %d\r\n\r\n", (int)strlen(body));
    strcat(buf, tmp);
    */

    send(fd, buf, strlen(buf), 0);
    send(fd, body, strlen(body), 0);
}

void HttpServer::_staticResponse(int fd, const char *filepath, int filesize, bool persistent) {
    int srcfd;
    char MIME[MAXLINE], buf[BUFSIZE];
    void *srcp;

    _getMIME(filepath, MIME);

    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: CYX PC\r\n", buf);
    if (persistent) {
        sprintf(buf, "%sConnection: keep-alive\r\n", buf);
    } else {
        sprintf(buf, "%sConnection: close\r\n", buf);
    }
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, MIME);

    /*
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
    */
    send(fd, buf, strlen(buf), 0);

    srcfd = open(filepath, O_RDONLY, 0);
    if (srcfd < 0) {
        printf("OPEN ERROR!!!\n");
        fflush(stdout);
        exit(-1);
    }
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    ::close(srcfd);
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
        strcpy(MIME,"image/webp");
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
    if (strcmp(buf,  "Connection: keep-alive\r\n") == 0) {
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
        strcpy(filepath,(_root + "index.html").c_str());
    } else if (uri[strlen(uri)-1] == '/') {
        strcpy(filepath, (_root + uri + "index.html").c_str());
    } else if ((strcmp(uri, "/about") == 0) ||
               (strcmp(uri, "/archives") == 0)) {
        strcpy(filepath, (_root + uri + "/index.html").c_str());
    } else {
        strcpy(filepath, (_root + uri).c_str());
    }
}

void HttpServer::on_runButton_clicked()
{
    QString msg;
    if (_isRunning) {
        // printf("FAILURE: 服务器正在运行！\n");
        // mtx_w.tryLock();
        msg.sprintf("FAILURE: 服务器正在运行！\n");
        emit msgSignal(msg);
        // mtx_r.unlock();
    } else {
        if (run() == 0) {
            // printf("SUCCESS: 服务器启动！\n");
            // mtx_w.tryLock();
            msg.sprintf("SUCCESS: 服务器启动！\n"
                        "IP: %s\n"
                        "Port: %d\n"
                        "Root: %s\n"
                        "Listen Queue: %d\n"
                        "Thread Num: %d\n"
                        "Timeout: %d(ms)\n\n",
                        _addr.c_str(), _port, _root.c_str(), _listenQueue, _threadNum, _i_timeoutVal);
            emit msgSignal(msg);
            //  mtx_r.unlock();
        } else {
            // printf("FAILURE: 服务器启动失败！\n");
            // mtx_w.tryLock();
            msg.sprintf("FAILURE: 服务器启动失败！\n\n");
            emit msgSignal(msg);
            // mtx_r.unlock();
        }
    }
}

void HttpServer::on_exitButton_clicked()
{
    printf("SUCCESS: 退出程序！\n");
    QApplication::exit(0);
}

void HttpServer::printMsg(QString msg) {
    // mtx_r.tryLock();
    ui->logBrowser->insertPlainText(msg);
    // mtx_w.unlock();
}

void HttpServer::on_closeButton_clicked()
{
    QString msg;
    if (_isRunning) {
        close_();
        // printf("SUCCESS: 服务器停止运行！\n");
        // mtx_w.tryLock();
        msg.sprintf("SUCCESS: 服务器停止运行！\n\n");
        emit msgSignal(msg);
        // mtx_r.unlock();
        } else {
            // printf("FAILURE: 服务器已处于停机状态！\n");
            // mtx_w.tryLock();
            msg.sprintf("FAILURE: 服务器已处于停机状态！\n\n");
            emit msgSignal(msg);
            // mtx_r.unlock();
        }
}

void HttpServer::on_resetButton_clicked()
{
    QString msg;

    if (_isRunning) {
        msg.sprintf("FAILURE: 服务器正在运行！\n");
        emit msgSignal(msg);
        return;
    }

    std::string addr;
    uint i_addr;
    int port;
    std::string root;
    int listenQueue;
    TimeVal timeoutVal;
    int i_timeoutVal;
    int threadNum;

    addr = ui->ipEdit->text().toStdString();
    if (addr == "0.0.0.0") {
        i_addr = INADDR_ANY;
        addr = "INADDR_ANY";
    } else {
        std::smatch match;
        std::regex IP_PATTERN("(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\."
                              "(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\."
                              "(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\."
                              "(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)");
        if (!regex_match(addr, match, IP_PATTERN)) {
            msg.sprintf("FAILURE: 非法的ip地址！\n");
            emit msgSignal(msg);
            return;
        } else {
            // std::cout << match.str(1) << "." << match.str(2) << "." << match.str(3) << "." << match.str(4) << std::endl;
            i_addr = (std::stoi(match.str(1)) << 24) +
                     (std::stoi(match.str(2)) << 16) +
                     (std::stoi(match.str(3)) << 8) +
                      std::stoi(match.str(4));
            // printf("ip: %x\n", i_addr);
            // fflush(stdout);
        }
    }

    port = ui->portEdit->text().toInt();
    if (port < 0 || port > 0xffff) {
        msg.sprintf("FAILURE: 非法的端口号！\n");
        emit msgSignal(msg);
        return;
    }

    root = ui->rootEdit->text().toStdString();

    listenQueue = ui->listenQueueEdit->text().toInt();
    if (listenQueue < 1) {
        msg.sprintf("FAILURE: 非法的线程数目！\n");
        emit msgSignal(msg);
        return;
    }

    threadNum = ui->threadEdit->text().toInt();
    if (threadNum < 1) {
        msg.sprintf("FAILURE: 非法的队列数目！\n");
        emit msgSignal(msg);
        return;
    }

    i_timeoutVal = ui->timeoutEdit->text().toInt();
    if (i_timeoutVal < 100) {
        msg.sprintf("FAILURE: 非法的超时时间！\n");
        emit msgSignal(msg);
        return;
    } else {
        timeoutVal.sec = i_timeoutVal / 1000;
        timeoutVal.usec = (i_timeoutVal % 1000) * 1000;
    }

    _addr = addr;
    _i_addr = i_addr;
    _port = port;
    _root = root;
    _listenQueue = listenQueue;
    _timeoutVal = timeoutVal;
    _i_timeoutVal = i_timeoutVal;
    _threadNum = threadNum;

    msg.sprintf("SUCCESS: 参数设置成功！\n");
    emit msgSignal(msg);
}
