//
// Created by mmx on 2018/6/24.
//

#include "IrdaMgr.h"
#include "usbhost.h"

#define DBG printf
#define ERR printf
#define IRDA_USB_DEV_VID 0x0483
#define IRDA_USB_DEV_PID 0x5740
#define IRDA_USB_WRITE_EP (0x01)
#define IRDA_USB_READ_EP (0x81)

class CmdUsbWR {
public:
    explicit CmdUsbWR(Loop *loop){
        this->loop = loop;
        this->urbPoll = nullptr;
        this->device = nullptr;
        this->urbRead = nullptr;
        this->urbWrite = nullptr;
    }
    bool start(const std::string &dev_name){
        if (urbWrite != nullptr || urbRead != nullptr || urbPoll != nullptr || device != nullptr){
            ERR("Duplicate start !!!\n");
            return false;
        }
        this->dev_name = dev_name;
        device = usb_device_open(dev_name.c_str());
        if (device == nullptr){
            ERR("open device %s fail\n", dev_name.c_str());
            return false;
        }
        usb_descriptor_iter iter = {};
        usb_descriptor_iter_init(device, &iter);
        while (struct usb_descriptor_header *h = usb_descriptor_iter_next(&iter)){
            if (h->bDescriptorType == USB_DT_ENDPOINT){
                auto *epdesc = reinterpret_cast<usb_endpoint_descriptor *>(h);
//                printf("---endpoint 0x%02X, maxpacketsize=%d, attr=%d\n", epdesc->bEndpointAddress, epdesc->wMaxPacketSize, epdesc->bmAttributes);
                if (epdesc->bEndpointAddress == IRDA_USB_WRITE_EP && urbWrite == nullptr){
                    urbWrite = usb_request_new(device, epdesc);
                }else if (epdesc->bEndpointAddress == IRDA_USB_READ_EP && urbRead == nullptr){
                    urbRead = usb_request_new(device, epdesc);
                    if (urbRead){
                        urbRead->buffer_length = epdesc->wMaxPacketSize;
                        urbRead->buffer = new char[epdesc->wMaxPacketSize];
                    }
                }
            }
        }
        if (urbWrite == nullptr || urbRead == nullptr || urbRead->buffer == nullptr){
            ERR("Can not find read ep %p, OR write ep %p\n", urbRead, urbWrite);
            goto fail_clean;
        }

        urbPoll = new FdPoll<UV_WRITABLE>(loop, usb_device_get_fd(device));
        if (urbPoll == nullptr){
            ERR("Poll usb fail\n");
            goto fail_clean;
        }
        urbPoll->setData(this);
        urbPoll->start([](FdPoll<UV_WRITABLE>* p){
            auto *This = (CmdUsbWR *) p->getData();
            This->onUrbReady();
        });

        return doRead();

        fail_clean:

        freeUrb();
        usb_device_close(device);
        device = nullptr;
        return false;
    }
    void close() {
        if (urbPoll == nullptr){
            cleanAll();
            delete this;
            return;
        }
        urbPoll->close([](FdPoll<UV_WRITABLE> *p) {
            auto *This = (CmdUsbWR *) p->getData();
            This->cleanAll();
            delete (This);
        });
    }
    void sendCmd(const std::string &str, const std::function<void(CmdUsbWR *, bool)> &resultHandler ){
        if (!currCmd.empty()){
            ERR("The previous command has not been send yet!!!\n");
            resultHandler(this, false);
            return;
        }
        if (str.empty()){
            ERR("Got empty command\n");
            resultHandler(this, true);
            return;
        }
        this->resultHandler = resultHandler;
        this->currCmd = str;
        doSend();
    }

    void onCmdReaded(const std::function<void(CmdUsbWR *, std::string&)>& cmdReadedHandler){
        this->cmdReadedHandler = cmdReadedHandler;
    }

    const std::string& getUsbDevName(){
        return dev_name;
    }
private:
    void doSend(){
        if (currCmd.empty()){
            return;
        }
        urbWrite->buffer = (void *) currCmd.data();
        urbWrite->buffer_length = static_cast<int>(currCmd.length());
        if (urbWrite->buffer_length > urbWrite->max_packet_size){
            urbWrite->buffer_length = urbWrite->max_packet_size;
        }
        if (int res = usb_request_queue(urbWrite)){
            printf("queue read urb fail %d\n", res);
            currCmd.clear();
            resultHandler(this, false);
        }
    }
    bool doRead(){
        if (int res = usb_request_queue(urbRead)){
            printf("queue read urb fail %d\n", res);
            return false;
        }
        return true;
    }

    void onUrbReady(){
        if(device == nullptr){
            return;
        }
        if (usb_request *req= usb_request_wait(device)){
            if (req == urbWrite){
                printf("write finish, %d, %d\n", req->actual_length, req == urbWrite);
                if (req->actual_length > 0){
                    currCmd.erase(0, static_cast<unsigned long>(req->actual_length));
                    if (currCmd.empty()){
                        resultHandler(this, true);
                    } else {
                        doSend();
                    }
                } else{
                    ERR("Write USB error, actual_len=%d\n", req->actual_length);
                    currCmd.clear();
                    resultHandler(this, false);
                }
            }else if (req == urbRead){
                for (int i = 0; i <req->actual_length ; ++i) {
                    char ch = ((char *)req->buffer)[i];
                    if ( ch == '\0' || ch ==  '\n' || ch == '\r' ){
                        if (!recvBuf.empty()){
                            cmdReadedHandler(this, recvBuf);
                            recvBuf.clear();
                        }
                    } else {
                        recvBuf.push_back(ch);
                    }
                }

                if (req->actual_length <= 0 || !doRead()){
                    recvBuf.clear();
                    std::string err = "UsbReadError";
                    cmdReadedHandler(this, err);
                }
            }else{
                printf("unknown ep %d\n", req->endpoint);
            }
        }
    }

    void freeUrb(){
        if (urbRead){
            delete [](char *)urbRead->buffer;
            usb_request_free(urbRead);
            urbRead = nullptr;
        }
        if (urbWrite){
            usb_request_free(urbWrite);
            urbWrite = nullptr;
        }
    }
    void cleanAll(){
        if (urbWrite != nullptr){
            usb_request_cancel(urbWrite);
        }
        if (urbRead != nullptr){
            usb_request_cancel(urbRead);
        }
        freeUrb();
        if (device != nullptr){
            usb_device_close(device);
            device = nullptr;
        }
    }
private:
    Loop *loop;
    FdPoll<UV_WRITABLE> *urbPoll;
    std::string dev_name;
    struct usb_device *device;
    std::function<void(CmdUsbWR *, std::string&)> cmdReadedHandler;
    std::string currCmd;
    std::string recvBuf;
    std::function<void(CmdUsbWR *, bool)> resultHandler;
    struct usb_request *urbWrite;
    struct usb_request *urbRead;
};


IrdaMgr::IrdaMgr(Loop *loop) {
    this->loop = loop;
    this->data = nullptr;
    this->mbSendBusy = false;
    this->mDevMoniter = nullptr;
    usbIO = nullptr;
    usbHost = usb_host_init();
    if (usbHost == nullptr){
        ERR("init fail\n");
        return ;
    }
    auto device_added_cb = [](const char *dev_name, void *client_data)->int {
        ((IrdaMgr *)client_data)->onUsbDevAdd(dev_name);
        return false;
    };

    auto device_removed_cb = [](const char *dev_name, void *client_data)->int {
        ((IrdaMgr *)client_data)->onUsbDevRemove(dev_name);
        return false;
    };
    auto discovery_done_cb = [](void *client_data)->int {
        return false;
    };

    usb_host_load(usbHost,
                  device_added_cb,
                  device_removed_cb,
                  discovery_done_cb,
                  this);
    mDevMoniter = new FdPoll<UV_READABLE>(loop, usb_host_get_fd(usbHost));

    if (mDevMoniter == nullptr){
        ERR("Poll usb fail\n");
    }else{
        mDevMoniter->setData(this);
        mDevMoniter->start([](FdPoll<UV_READABLE>* p){
            auto This = reinterpret_cast<IrdaMgr *>(p->getData());
            usb_host_read_event(This->usbHost);
        });
    }
}

void IrdaMgr::onWebConnAdd(WebSocket<SERVER> *ws) {
    sendDevState(ws);
    mWebConns.push_back(ws);
    newUsbIO();
}

void IrdaMgr::onWebConnRemove(WebSocket<SERVER> *ws) {
    for (auto it = mWebConns.begin(); it != mWebConns.end();){
        if (*it == ws){
            it = mWebConns.erase(it);
        }else{
            ++it;
        }
    }
    if (mWebConns.empty() && usbIO != nullptr){
        usbIO->close();
        usbIO = nullptr;
    }
}

void IrdaMgr::onWebConnMsg(WebSocket<SERVER> *ws, const std::string &msg) {
    if (usbIO != nullptr){
        mPendingCmds.push_back(msg);
        doSendCmd();
    }
}

void IrdaMgr::close() {
    if (usbIO != nullptr){
        usbIO->close();
        usbIO = nullptr;
        broadcastDevState();
    }

    if (mDevMoniter != nullptr){
        mDevMoniter->close([](FdPoll<UV_READABLE> *p){
            auto This = reinterpret_cast<IrdaMgr *>(p->getData());
            This->clean();
        });
    }else{
        clean();
    }
}

void IrdaMgr::onUsbDevRemove(const char *dev_name) {
    for (auto it = mMatchedDeviceNames.begin(); it != mMatchedDeviceNames.end(); ) {
        if (*it == dev_name) {
            it = mMatchedDeviceNames.erase(it);
        } else {
            ++it;
        }
    }
    if (usbIO && usbIO->getUsbDevName() == dev_name){
        usbIO->close();
        usbIO = nullptr;
        broadcastDevState();
    }
    newUsbIO();
}

static bool is_vid_pid_match(const char *dev_name, uint16_t vid, uint16_t pid)
{
    struct usb_device *device = usb_device_open(dev_name);
    if (device == nullptr){
        printf("open device %s fail\n", dev_name);
        return false;
    }
    uint16_t usb_vid = usb_device_get_vendor_id(device);
    uint16_t usb_pid = usb_device_get_product_id(device);
    printf("%s, %04x:%04x, writeable=%d\n", dev_name, usb_vid, usb_pid, usb_device_is_writeable(device));
    usb_device_close(device);
    return usb_vid==vid && usb_pid==pid;
}

void IrdaMgr::onUsbDevAdd(const char *dev_name) {
    if (is_vid_pid_match(dev_name, IRDA_USB_DEV_VID, IRDA_USB_DEV_PID)){
        printf("New match device: %s\n", dev_name);
        mMatchedDeviceNames.emplace_back(dev_name);
        newUsbIO();
    }
}

void IrdaMgr::newUsbIO() {
    if (usbIO != nullptr || mWebConns.empty() || mMatchedDeviceNames.empty()){
        return;
    }
    auto IO = new CmdUsbWR(loop);
    if (IO != nullptr){

        IO->onCmdReaded([&](CmdUsbWR *io, std::string& rsp){
            if (io == usbIO){
                broadcastNewReaded(rsp);
            }
        });
        if (IO->start(mMatchedDeviceNames[0])){
            usbIO = IO;
            mbSendBusy = false;
            broadcastDevState();
        }else{
            IO->close();
        }
    }else{
        ERR("no memory\n");
    }
}

void IrdaMgr::broadcastDevState() {
    for (auto ws: mWebConns){
        sendDevState(ws);
    }
}

void IrdaMgr::broadcastNewReaded(const std::string &rsp) {
    if (rsp == "UsbReadError"){
        ERR("Read USB fail\n");
        usbIO->close();
        usbIO = nullptr;
        broadcastDevState();
        return;
    }
    for (auto ws: mWebConns){
        ws->send(rsp.c_str());
    }
}

void IrdaMgr::sendDevState(WebSocket<SERVER> *ws) {
    if (usbIO){
        ws->send("DevOnline");
    }else{
        ws->send("DevOffline");
        mPendingCmds.clear();
    }
}

void IrdaMgr::doSendCmd() {
    if (usbIO == nullptr || mbSendBusy || mPendingCmds.empty()){
        return;
    }

    mbSendBusy = true;
    std::string msg = mPendingCmds.front();
    mPendingCmds.pop_front();
    usbIO->sendCmd(msg, [&](CmdUsbWR *io, bool ok){
        if (ok){
            mbSendBusy = false;
            doSendCmd();
        } else {
            ERR("Write USB fail\n");
            usbIO->close();
            usbIO = nullptr;
            broadcastDevState();
        }
    });
}

void IrdaMgr::clean() {
    if (usbHost != nullptr){
        usb_host_cleanup(usbHost);
        usbHost = nullptr;
    }
    delete this;
}

