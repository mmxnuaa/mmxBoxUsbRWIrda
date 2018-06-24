//
// Created by mmx on 2018/6/20.
//
#include <uWS.h>
#include "IrdaMgr.h"

using namespace uWS;

#define DBG printf
int main() {
    Hub h;
    auto irdaMgr = new IrdaMgr(h.getLoop());

    h.onConnection([&](WebSocket<SERVER> *ws, HttpRequest req){
        auto addr = ws->getAddress();
        DBG("Got connection from <%s>%s:%d\n", addr.family, addr.address, addr.port);
        irdaMgr->onWebConnAdd(ws);
    });
    h.onDisconnection([&](WebSocket<SERVER> * ws, int code, char *message, size_t length){
        auto addr = ws->getAddress();
        DBG("lost connection from <%s>%s:%d\n", addr.family, addr.address, addr.port);
        irdaMgr->onWebConnRemove(ws);
    });
    h.onMessage([&](WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode) {
        irdaMgr->onWebConnMsg(ws, std::string(message, length));
    });

    h.onHttpRequest([&](HttpResponse *res, HttpRequest req, char *data, size_t length,
                        size_t remainingBytes) {
//        res->end(response.data(), response.length());
    });

    if (h.listen(3000)) {
        DBG("listen...\n");
        h.run();
    }

    return 0;
}

