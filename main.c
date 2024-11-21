


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ws_server.h"
#include "ws_core.h"


ws_server *wss;

void fun_sig(int sig)
{
    ws_server_release(wss);
    signal(sig, SIG_DFL);
}

/*
 *  接收数据回调
 *  参数:
 *      wsc: 客户端信息结构体指针
 *      msg: 接收数据内容
 *      msg_len: >0时为websocket数据包
 *      type： websocket包类型
 */
void on_message(ws_client *wsc, char *msg, int msg_len, ws_datatype type)
{
    int ret = 0;
    // 正常 websocket 数据包
    if (msg_len > 0)
    {
        // 客户端过多时不再打印,避免卡顿
        if (wsc->wss->client_count <= WS_SERVER_CLIENT_OF_PRINTF)
            printf("onMessage: fd/%03d index/%03d total/%03d %d\r\n", wsc->fd, wsc->index, wsc->wss->client_count, msg_len);

        char test[32] = "hello 00000";
        ret = ws_send(wsc->fd, test, strlen(test), false, type);
        //发送失败,标记异常(后续会被自动回收)
        if (ret < 0)
            wsc->exittype = WET_SEND;
    }

}


void _on_exit(ws_client *wsc, ws_exittype exittype)
{
    // 断开原因
    switch (exittype)
    {
    case WET_SEND:
        printf("_on_exit: fd/%03d index/%03d total/%03d disconnect by send\r\n", wsc->fd, wsc->index, wsc->wss->client_count);
        break;
    case WET_LOGIN:
        printf("_on_exit: fd/%03d index/%03d total/%03d disconnect by login failed \r\n", wsc->fd, wsc->index, wsc->wss->client_count);
        break;
    case WET_LOGIN_TIMEOUT:
        printf("_on_exit: fd/%03d index/%03d total/%03d disconnect by login timeout \r\n", wsc->fd, wsc->index, wsc->wss->client_count);
        break;
    case WET_DISCONNECT:
        printf("_on_exit: fd/%03d index/%03d total/%03d disconnect by disconnect \r\n", wsc->fd, wsc->index, wsc->wss->client_count);
        break;
    default:
        printf("_on_exit: fd/%03d index/%03d total/%03d disconnect by unknow \r\n", wsc->fd, wsc->index, wsc->wss->client_count);
    }
}


int main(int argc, char **argv)
{

    //服务器必须参数
    wss = ws_server_create(argc > 1 ? atoi(argv[1]) : 9997, //服务器端口
                            argc > 2 ? argv[2] : "/",        //路径为空)
                            NULL,       
                            &on_message, 
                            &_on_exit);   

    signal(SIGINT, fun_sig);	

    while (!wss->is_exit)
    {
      
    }

    return 0;
}