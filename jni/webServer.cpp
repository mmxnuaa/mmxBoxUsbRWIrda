//
// Created by mmx on 2018/6/20.
//
#include <uWS.h>
using namespace uWS;

#define DBG printf
int main() {
    Hub h;
    std::string response = "Hello!";

    h.onConnection([&](WebSocket<SERVER> *ws, HttpRequest req){
        auto addr = ws->getAddress();
        DBG("Got connection from <%s>%s:%d\n", addr.family, addr.address, addr.port);
//        ws->send("mmx socket server hi!");
        uS::Timer *timer = new uS::Timer(h.getLoop());
        timer->setData(ws);
        timer->start([](uS::Timer *timer) {
            // send a message to the browser
            DBG("send %ld\n", clock());
            std::string message = "data: Clock sent from the server: " + patch::to_string(clock()) + "\n\n";
            ((WebSocket<SERVER> *) (timer->getData()))->send((char *) message.c_str());
        }, 5000, 5000);
        ws->setUserData(timer);
    });
    h.onDisconnection([](WebSocket<SERVER> * ws, int code, char *message, size_t length){
        auto addr = ws->getAddress();
        DBG("lost connection from <%s>%s:%d\n", addr.family, addr.address, addr.port);
        uS::Timer *timer = (uS::Timer *) ws->getUserData();
        if (timer) {
            timer->stop();
            timer->close();
        }
    });
    h.onMessage([](WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode) {
        ws->send(message, length, opCode);
    });

    h.onHttpRequest([&](HttpResponse *res, HttpRequest req, char *data, size_t length,
                        size_t remainingBytes) {
        res->end(response.data(), response.length());
    });

    if (h.listen(3000)) {
        DBG("listen...\n");
        h.run();
    }

    return 0;
}

