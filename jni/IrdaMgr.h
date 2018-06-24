//
// Created by mmx on 2018/6/24.
//

#ifndef USBRW_CIRDAMGR_H
#define USBRW_CIRDAMGR_H

#include <uWS.h>
#include <list>

using namespace uWS;
using namespace uS;
class CmdUsbWR;
struct usb_host_context;

template <int InOrOut>
class FdPoll : protected Poll {
private:
    std::function<void(FdPoll *)> urbReadyHandler;
    std::function<void(FdPoll *)> finishHandler;
    Loop *loop;
    void *data;

public:
    FdPoll(Loop *loop, int usbFd) : Poll(loop, usbFd) {
        this->loop = loop;
        this->data = nullptr;
    }

    void start(const std::function<void(FdPoll *)>& handler) {
        this->urbReadyHandler = handler;
        Poll::setCb([](Poll *p, int, int) {
            ((FdPoll *) p)->urbReadyHandler((FdPoll *) p);
        });
//        Poll::start(loop, this, UV_READABLE);
        Poll::start(loop, this, InOrOut);
    }

    void close(const std::function<void(FdPoll *)>& finishHandler) {
        Poll::stop(loop);
//        ::close(state.fd);
        this->finishHandler = finishHandler;
        Poll::close(loop, [](Poll *p) {
            ((FdPoll *) p)->finishHandler((FdPoll *) p);
            delete (FdPoll *) p;
        });
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }
};


class IrdaMgr {
public:
    explicit IrdaMgr(Loop *loop);

    void onWebConnAdd(WebSocket<SERVER> *ws);
    void onWebConnRemove(WebSocket<SERVER> *ws);
    void onWebConnMsg(WebSocket<SERVER> *ws, const std::string& msg);

    void close();

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }

private:
    void onUsbDevAdd(const char *dev_name);
    void onUsbDevRemove(const char *dev_name);
    void newUsbIO();
    void broadcastDevState();
    void broadcastNewReaded(const std::string &rsp);
    void sendDevState(WebSocket<SERVER> *ws);
    void doSendCmd();
    void clean();
private:
    void *data;
    Loop *loop;
    std::vector<WebSocket<SERVER> *> mWebConns;
    CmdUsbWR *usbIO;
    struct usb_host_context *usbHost;
    std::vector<std::string> mMatchedDeviceNames;
    std::list<std::string> mPendingCmds;
    bool mbSendBusy;
    FdPoll<UV_READABLE> *mDevMoniter;

};


#endif //USBRW_CIRDAMGR_H
