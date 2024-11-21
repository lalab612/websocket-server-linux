#ifndef _WS_WEBSOCKET_H_
#define _WS_WEBSOCKET_H_

#include "ws_core.h"
#include "ws_string.h"

#define WEBSOCKET_SERVER_PORT 9997

#define WEBSOCKET_DATA_LEN_MAX 1024

struct websocket_frame_s
{
    char fin;
    char opcode;
    char mask;
    unsigned long long payload_length;
    char masking_key[4];
    char *payload_data;
};

int ws_websocket_shake_hand(int fd);
int ws_websocket_parse(int fd, websocket_frame_t *head);

#endif
