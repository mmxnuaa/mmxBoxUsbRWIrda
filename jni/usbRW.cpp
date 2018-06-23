//
// Created by mmx on 2018/4/1.
//

#include <uWS.h>
using namespace uWS;
using namespace uS;
#include <cstdio>
#include <malloc.h>
#include <string.h>
#include "usbhost.h"

typedef struct DevInfoNode{
    const char *dev_name;
    DevInfoNode *next;
}DevInfoNode;

static int device_added_cb(const char *dev_name, void *client_data)
{
    printf("new device %s\n", dev_name);
    auto **pH = static_cast<DevInfoNode **>(client_data);
    auto *node = static_cast<DevInfoNode *>(malloc(sizeof(DevInfoNode)));
    node->dev_name = strdup(dev_name);
    node->next = *pH;
    *pH = node;
    return false;
}

static int device_removed_cb(const char *dev_name, void *client_data)
{
    printf("remove device %s\n", dev_name);
    auto **pH = static_cast<DevInfoNode **>(client_data);

    DevInfoNode **pNextAddr = pH;
    DevInfoNode *nextVal = *pH;
    while (nextVal != nullptr){
        if (strcmp(nextVal->dev_name, dev_name) == 0){
            break;
        }
        pNextAddr = &nextVal->next;
        nextVal = *pNextAddr;
    }
    if (nextVal != nullptr){
        *pNextAddr = nextVal->next;
        free((void *) nextVal->dev_name);
        free(nextVal);
    }
    return false;
}

static void free_list(DevInfoNode **pH)
{
    while (*pH != nullptr){
        DevInfoNode *cur = *pH;
        *pH = cur->next;
        free((void *) cur->dev_name);
        free(cur);
    }
}

static int discovery_done_cb(void *client_data)
{
    return true;
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

struct USBReady : Poll {
    void (*cb)(USBReady *);
    Loop *loop;
    void *data;

    USBReady(Loop *loop, int usbFd) : Poll(loop, usbFd) {
        this->loop = loop;
    }

    void start(void (*cb)(USBReady *)) {
        this->cb = cb;
        Poll::setCb([](Poll *p, int, int) {
//            uint64_t val;
//            if (::read(((USBReady *) p)->state.fd, &val, 8) == 8) {
                ((USBReady *) p)->cb((USBReady *) p);
//            }
        });
//        Poll::start(loop, this, UV_READABLE);
        Poll::start(loop, this, UV_WRITABLE);
    }

    void close() {
        Poll::stop(loop);
//        ::close(state.fd);
        Poll::close(loop, [](Poll *p) {
            delete (USBReady *) p;
        });
    }

    void setData(void *data) {
        this->data = data;
    }

    void *getData() {
        return data;
    }
};

static void test_target(const char *dev_name)
{
    struct usb_device *device = usb_device_open(dev_name);
    if (device == nullptr){
        printf("open device %s fail\n", dev_name);
        return ;
    }

    const char *cmd = "IrdaReceiveStart\n";
//    const char *cmd = "HowAreYou\n";
//    const char testStr[]= "sdjflksdjsjfj1234";
//    char read[256]= {0};
//    int cnt = usb_device_bulk_transfer(device, 0x1, (void *) cmd, static_cast<int>(strlen(cmd)), 1000);
//    printf("write result: %d\n", cnt);
//    while (true){
//        cnt = usb_device_bulk_transfer(device, 0x81, (void *) read, 200, 1000);
//        read[cnt] = '\0';
//        printf("read result: %d, %s\n", cnt, read);
//    }
    static struct usb_request *urbWrite = nullptr, *urbRead = nullptr;
//    struct usb_endpoint_descriptor epdesc;
//    urbRead = usb_request_new(device, )
    usb_descriptor_iter iter;
    usb_descriptor_iter_init(device, &iter);
    printf("description dump begin\n");
    while (struct usb_descriptor_header *h = usb_descriptor_iter_next(&iter)){
        printf("+++ type = %d\n", h->bDescriptorType);
        if (h->bDescriptorType == USB_DT_ENDPOINT){
            struct usb_endpoint_descriptor *epdesc = reinterpret_cast<usb_endpoint_descriptor *>(h);
            printf("---endpoint 0x%02X, maxpacketsize=%d, attr=%d\n", epdesc->bEndpointAddress, epdesc->wMaxPacketSize, epdesc->bmAttributes);
            if (epdesc->bEndpointAddress == 0x01 && urbWrite == nullptr){
                urbWrite = usb_request_new(device, epdesc);
            }else if (epdesc->bEndpointAddress == 0x81 && urbRead == nullptr){
                urbRead = usb_request_new(device, epdesc);
                if (urbRead){
                    urbRead->buffer_length = epdesc->wMaxPacketSize;
                    urbRead->buffer = new char[epdesc->wMaxPacketSize+1];
                }
            }
        }
    }
    static bool error = false;
    uS::Loop *loop = uS::Loop::createLoop();
    USBReady *usbEvt = new USBReady(loop, usb_device_get_fd(device));
    static auto t_start = std::chrono::high_resolution_clock::now();
    printf("====dump end===\n");
    if (urbRead == nullptr || urbWrite == nullptr){
        printf("new URB fail\n");
        goto done;
    }

    urbWrite->buffer = (void *) cmd;
    urbWrite->buffer_length = static_cast<int>(strlen(cmd));
    if (int res = usb_request_queue(urbWrite)){
        printf("queue write urb fail %d\n", res);
        goto done;
    }

    if (int res = usb_request_queue(urbWrite)){
        printf("queue write urb fail %d\n", res);
        goto done;
    }
    if (int res = usb_request_queue(urbRead)){
        printf("queue read urb fail %d\n", res);
        goto done;
    }
//    printf("now=%d\n", t_start);
//    while (usb_request *req= usb_request_wait(device)){
//        auto t_end = std::chrono::high_resolution_clock::now();
//        printf("diff=%f\n", std::chrono::duration<double, std::milli>(t_end-t_start).count());
//        t_start = t_end;
//        if (req->endpoint == 0x01){
//            printf("write finish, %d, %d\n", req->actual_length, req == urbWrite);
//        }else if (req->endpoint == 0x81){
//            ((char *)req->buffer)[req->actual_length] = '\0';
//            printf("receive %d, %d : %s\n", req == urbRead, req->actual_length, static_cast<char *>(req->buffer));
//            if (int res = usb_request_queue(req)){
//                printf("queue read urb fail %d\n", res);
//                goto done;
//            }
//        }else{
//            printf("unknown ep %d\n", req->endpoint);
//        }
//    }

    usbEvt->setData(device);
    usbEvt->start([](USBReady* evt){
        printf("event in\n");
        usb_device *dev = reinterpret_cast<usb_device *>(evt->getData());
        if (usb_request *req= usb_request_wait(dev)){
            auto t_end = std::chrono::high_resolution_clock::now();
            printf("diff=%f\n", std::chrono::duration<double, std::milli>(t_end-t_start).count());
            t_start = t_end;
            if (req->endpoint == 0x01){
                printf("write finish, %d, %d\n", req->actual_length, req == urbWrite);
            }else if (req->endpoint == 0x81){
                ((char *)req->buffer)[req->actual_length] = '\0';
                printf("receive %d, %d : %s\n", req == urbRead, req->actual_length, static_cast<char *>(req->buffer));
                if (int res = usb_request_queue(req)){
                    printf("queue read urb fail %d\n", res);
                    error = true;
//                    goto done;
                }
            }else{
                printf("unknown ep %d\n", req->endpoint);
            }
        }
        printf("event out\n");
    });


    while (true){
        loop->doEpoll(3000);
        printf("loop\n");
        if (error){
            printf("error happen\n");
            break;
        }
    }

done:
    if (urbRead){
        usb_request_cancel(urbRead);
        delete []((char *)urbRead->buffer);
        usb_request_free(urbRead);
    }
    if (urbWrite){
        usb_request_cancel(urbWrite);
        usb_request_free(urbWrite);
    }
    usb_device_close(device);
}

int main()
{
    printf("start\n");
    struct usb_host_context *usb_ctx = usb_host_init();
    if (usb_ctx == nullptr){
        printf("init fail\n");
        return -1;
    }

    DevInfoNode *head = nullptr;
    usb_host_load(usb_ctx,
                      device_added_cb,
                      device_removed_cb,
                      discovery_done_cb,
                      &head);

    printf("done query\n");
    usb_host_cleanup(usb_ctx);

    DevInfoNode *Node = head;
    const char *target = nullptr;
    while (Node != nullptr){
        if (is_vid_pid_match(Node->dev_name, 0x0483, 0x5740)){
            printf("match: %s\n", Node->dev_name);
            target = Node->dev_name;
        }
        Node = Node->next;
    }

    if (target != nullptr){
        test_target(target);
    }

    free_list(&head);
    return 0;
}

