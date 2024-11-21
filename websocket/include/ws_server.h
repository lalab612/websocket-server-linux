
#ifndef _WS_SERVER_H_
#define _WS_SERVER_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/select.h>


#define BLOCK_SIZE 10
#define WS_SERVER_THREAD 10
// 每个副线程维护客户端最大数量
#define WS_SERVER_CLIENT_OF_THREAD 100
#define WS_SERVER_CLIENT (WS_SERVER_THREAD * WS_SERVER_CLIENT_OF_THREAD) // 10*100=1000

// bind超时,通常为服务器端口被占用,或者有客户端还连着上次的服务器
#define WS_SERVER_BIND_TIMEOUT_MS 1000
//接入客户端数量超出这个数时不在 onMessage 打印,避免卡顿
#define WS_SERVER_CLIENT_OF_PRINTF 10

typedef enum
{
    WDT_NULL = 0, // 非标准数据包
    WDT_MINDATA,  // 0x0：中间数据包
    WDT_TXTDATA,  // 0x1：txt类型数据包
    WDT_BINDATA,  // 0x2：bin类型数据包
    WDT_DISCONN,  // 0x8：断开连接类型数据包 收到后需手动 close(fd)
    WDT_PING,     // 0x8：ping类型数据包 ws_recv 函数内自动回复pong
    WDT_PONG,     // 0xA：pong类型数据包
} ws_datatype;

// 断连原因
typedef enum
{
    WET_NONE = 0,
    WET_SEND,          // 发送失败
    WET_LOGIN,         // websocket握手检查失败(http请求格式错误或者path值不一致)
    WET_LOGIN_TIMEOUT, // 连接后迟迟不发起websocket握手
    WET_DISCONNECT,    // 收到断开协议包
} ws_exittype;

typedef struct _ws_client ws_client;
typedef struct _ws_thread ws_thread;
typedef struct _ws_server ws_server;

// 客户端事件回调函数原型
typedef void (*ws_on_message)(ws_client *wsc, char *msg, int msgLen, ws_datatype datatype);
typedef void (*ws_on_exit)(ws_client *wsc, ws_exittype exittype);

// 客户端使用的参数结构体
struct _ws_client
{
    int fd;                 // accept之后得到的客户端连接描述符
    uint8_t ip[4];          // 接入客户端的ip(accpet阶段获得)
    int port;               // 接入客户端的端口
    ws_exittype exittype;   // 断连标志
    uint32_t index;         // 接入客户端的历史序号(从1数起)
    void *priv;             // 用户私有指针
    ws_server *wss;         // 所在服务器指针
};

// 副线程结构体(只要还有一个客户端在维护就不会退出线程)
struct _ws_thread
{
    int client_count; // 该线程正在维护的客户端数量
    bool is_run;      // 线程运行状况
    ws_server *wss;
    ws_client *wsc; 
};

// 服务器主线程使用的参数结构体
struct _ws_server
{
    int fd; // 服务器描述符
    int maxfd;
    fd_set rset, wset;
    int port;         // 服务器端口
    char path[256];   // 服务器路径
    void *priv;       // 用户私有指针
    int client_count; // 当前接入客户端总数
    bool is_exit;     // 线程结束标志
    pthread_mutex_t lock;
    ws_on_message on_message;
    ws_on_exit on_exit;
    ws_thread thread[WS_SERVER_CLIENT_OF_THREAD]; // 副线程数组
    ws_client client[WS_SERVER_CLIENT]; // 全体客户端列表
};

// 服务器创建和回收
ws_server *ws_server_create(
    int port,                 // 服务器端口
    const char *path,         // 服务器路径
    void *priv,               // 用户私有指针,回调函数里使用 wsc->priv 取回
    ws_on_message on_message, // 收到客户端数据
    ws_on_exit on_exit);      // 客户端断开时(已断开)

void ws_server_release(ws_server *wss);

int32_t ws_send(int32_t fd, void* buff, int32_t buff_len, bool mask, ws_datatype type);

void ws_delayms(uint32_t ms);

#endif
