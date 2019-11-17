#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QMainWindow>
#include <QObject>
#include <thread>
#include <vector>
#include "./RecvStream.h"

QT_BEGIN_NAMESPACE
namespace Ui { class HttpServer; }
QT_END_NAMESPACE

struct TimeVal {
  time_t sec;
  time_t usec;
};


class HttpServer : public QMainWindow
{
    Q_OBJECT

public:
    HttpServer(QWidget *parent = nullptr);
    ~HttpServer();
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

private slots:
    void on_runButton_clicked();

    void on_exitButton_clicked();

    void printMsg(QString msg);

    void on_closeButton_clicked();

private:
    Ui::HttpServer *ui;

    uint _addr;
    int _port;
    std::string _root;
    int _listenQueue;
    TimeVal _timeoutVal;
    int _threadNum;

    int _listenfd;
    bool _isRunning = false;
    std::vector<std::thread> _threadPool;

    // QString msg;

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

signals:
    void  msgSignal(QString msg);
};
#endif // HTTPSERVER_H
