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

    printf("done");
    usb_host_cleanup(usb_ctx);
    free_list(&head);
    return 0;
}
