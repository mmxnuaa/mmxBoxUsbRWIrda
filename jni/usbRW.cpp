//
// Created by mmx on 2018/4/1.
//

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

static void test_target(const char *dev_name)
{
    struct usb_device *device = usb_device_open(dev_name);
    if (device == nullptr){
        printf("open device %s fail\n", dev_name);
        return ;
    }

    const char testStr[]= "sdjflksdjsjfj1234";
    int cnt = usb_device_bulk_transfer(device, 0x1, (void *) testStr, static_cast<int>(strlen(testStr)), 1000);
    printf("write result: %d\n", cnt);
    char read[256]= {0};
    cnt = usb_device_bulk_transfer(device, 0x81, (void *) read, 200, 1000);
    printf("read result: %d, %s\n", cnt, read);

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

