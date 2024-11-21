

#include "ws_server.h"
#include "ws_websocket.h"

extern int close_thread;

// 稍微精准的延时
#include <sys/time.h>
void ws_delayus(uint32_t us)
{
    struct timeval tim;
    tim.tv_sec = us / 1000000;
    tim.tv_usec = us % 1000000;
    select(0, NULL, NULL, NULL, &tim);
}

void ws_delayms(uint32_t ms)
{
    ws_delayus(ms * 1000);
}


int setnonblocking(int fd){
	int old_option = fcntl(fd,  F_GETFL);  // 获取文件描述符旧的状态标志
	int new_option = old_option | O_NONBLOCK; //设置非阻塞标志
	fcntl(fd, F_SETFL, new_option);  
	return old_option;  
}

int clrnonblocking(int fd){
	int old_option = fcntl(fd,  F_GETFL);  // 获取文件描述符旧的状态标志
	int new_option = old_option &  ~O_NONBLOCK; //设置非阻塞标志
	fcntl(fd, F_SETFL, new_option);  
	return old_option;  
}


// 服务器主线程,负责检测 新客户端接入
static void *server_thread(void *argv);
// 添加客户端
static void client_add(ws_server *wss, int fd, uint32_t ip, int port);
static void new_thread(void *obj, void *(*callback)(void *));
static void *server_task_th(void *argv);

ws_server *ws_server_create(
    int port,
    const char *path,
    void *priv,
    ws_on_message on_message,
    ws_on_exit on_exit)
{
    ws_server *wss = (ws_server *)calloc(1, sizeof(ws_server));
    wss->port = port;
    strcpy(wss->path, path ? path : "/");
    wss->priv = priv;
    wss->on_message = on_message;
    wss->on_exit = on_exit;
    new_thread(wss, &server_thread);
    return wss;
}

// 服务器主线程,负责检测 新客户端接入
static void *server_thread(void *argv)
{
    ws_server *wss = (ws_server *)argv;
    int count = 0;
    int fd_accept = 0;
    int optval;       // 整形的选项值
    socklen_t optlen; // 整形的选项值的长度

    socklen_t soc_addr_len;
    struct sockaddr_in accept_addr;
    struct sockaddr_in server_addr = {0};
    char str[INET_ADDRSTRLEN];
    server_addr.sin_family = AF_INET;         // 设置为IP通信
    server_addr.sin_addr.s_addr = INADDR_ANY; // 服务器IP地址
    server_addr.sin_port = htons(wss->port);  // 服务器端口号
    soc_addr_len = sizeof(struct sockaddr_in);

    // 创建socket
    if ((wss->fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
    {
        printf("create socket failed\r\n");
        return NULL;
    }

    // 地址可重用设置(有效避免bind超时)
    optval = 1;
    optlen = sizeof(optval);
    setsockopt(wss->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, optlen);

    struct linger ling = {0, 0};
    if (setsockopt(wss->fd, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling)) != 0)
    {
        return 0;
    }

    // 设置为非阻塞接收
    setnonblocking(wss->fd);

    // 绑定地址
    while (bind(wss->fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) != 0)
    {
        if (++count > WS_SERVER_BIND_TIMEOUT_MS)
        {
            printf("bind timeout %d 服务器端口占用中,请稍候再试\r\n", count);
            goto server_exit;
        }
        ws_delayms(1);
    }

    // 设置监听
    if (listen(wss->fd, BLOCK_SIZE) != 0)
    {
        printf("listen failed\r\n");
        goto server_exit;
    }

    pthread_mutex_init(&wss->lock, NULL);

    FD_ZERO(&wss->rset);
    FD_ZERO(&wss->wset);
    FD_SET(wss->fd, &wss->wset);

    wss->maxfd = wss->fd;
    // 正式开始
    printf("server ws://127.0.0.1:%d%s start \r\n", wss->port, wss->path);

    while (!wss->is_exit)
    {
        // 从可读文件描述符集合中选择就绪的套接字
        wss->rset = wss->wset;
        int nready = select(wss->maxfd + 1, &wss->rset, NULL, NULL, NULL);
        if (nready == -1)
        {
            printf("select errno = %d, %s\n", errno, strerror(errno));
            continue;
        }
    
        if (FD_ISSET(wss->fd, &wss->rset))//存在
        {

            //阻塞等待客户端连接--一旦有客户端连接就会解除阻塞
            fd_accept = accept(wss->fd, (struct sockaddr *)&accept_addr, &soc_addr_len);
            // 添加客户端
            if (fd_accept >= 0)
            {
                if (0 == ws_websocket_shake_hand(fd_accept))
                {
                    setnonblocking(fd_accept);
                    FD_SET(fd_accept, &wss->wset); //加入fd到集合
                    if (fd_accept > wss->maxfd)
                        wss->maxfd = fd_accept;

                    printf("From %s at PORT %d  %d\n",
                        inet_ntop(AF_INET, &accept_addr.sin_addr, str, sizeof(str)), ntohs(accept_addr.sin_port), fd_accept);
            
                    client_add(wss, fd_accept, accept_addr.sin_addr.s_addr, accept_addr.sin_port);
                }
            }   
        }
    }
    // 移除所有副线程
    wss->is_exit = true;
    pthread_mutex_destroy(&wss->lock);

server_exit:
    wss->is_exit = true;
    FD_CLR(wss->fd, &wss->rset);
    // 关闭socket
    close(wss->fd);
    wss->fd = 0;
    return NULL;
}

// 取得空闲,返回序号
static int client_get(ws_server *wss, int fd, uint32_t ip, int port)
{
    int i;
    for (i = 0; i < WS_SERVER_CLIENT; i++)
    {
        if (!wss->client[i].fd)
        {
            memset(&wss->client[i], 0, sizeof(ws_client));
            wss->client[i].fd = fd;
            *((uint32_t *)(wss->client[i].ip)) = ip;
            wss->client[i].port = port;
            wss->client[i].wss = wss;
            wss->client[i].priv = wss->priv;
            wss->client[i].index = ++wss->client_count;
            return i;
        }
    }
    printf("failed, out of range(%d) !!\r\n", WS_SERVER_CLIENT); // 满员
    return -1;
}

static void clear_client(ws_server *wss)
{
    int i = 0;
    for (i = 0; i < WS_SERVER_CLIENT; i++)
    {
        if (wss->client[i].fd)
        {
            close(wss->client[i].fd);
        }
    }
}

// 添加客户端
static void client_add(ws_server *wss, int fd, uint32_t ip, int port)
{
    int ret;
    ws_client *wsc;
    ws_thread *wst;

    pthread_mutex_lock(&wss->lock);

    // 取得空闲客户端指针
    ret = client_get(wss, fd, ip, port);
    if (ret < 0)
    {
        pthread_mutex_unlock(&wss->lock);
        return;
    }

    // 新增客户端及其匹配的线程
    wsc = &wss->client[ret];
    wst = &wss->thread[ret % WS_SERVER_CLIENT_OF_THREAD];

    // 线程已开启
    if (!wst->is_run) // 开启新线程
    {
        // 参数初始化
        wst->wss = wss;
        wst->wsc = wsc;

        wst->client_count += 1;
        // 开线程
        new_thread(wst, &server_task_th);
    }
    pthread_mutex_unlock(&wss->lock);
}

static void new_thread(void *obj, void *(*callback)(void *))
{
    pthread_t th;
    pthread_attr_t attr;
    int ret;
    // 禁用线程同步,线程运行结束后自动释放
    pthread_attr_init(&attr);                                    // 初始化一个线程属性对象
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // 分离状态启动线程
    // 抛出线程
    ret = pthread_create(&th, &attr, callback, (void *)obj);
    if (ret != 0)
        printf("pthread_create failed !! %s\r\n", strerror(ret));
    // attr destroy
    pthread_attr_destroy(&attr); // 销毁一个线程属性对象
}


//副线程检测异常客户端并移除
static int client_detect(void *argv)
{

    ws_thread *wst = (ws_thread *)argv;

    //有异常错误 || 就是要删除
    if(wst->wsc->exittype)
    {
        FD_CLR(wst->wsc->fd, &wst->wss->wset);

        //关闭描述符
        close(wst->wsc->fd);
        wst->client_count -= 1;
        wst->wss->client_count -= 1;

        if (wst->wss->on_exit)
            wst->wss->on_exit(wst->wsc, wst->wsc->exittype);

        return 1;
    }

    return 0; 
}


// 服务器副线程,负责检测 数据接收 和 客户端断开
// 只要还有一个客户端在维护就不会退出线程
static void *server_task_th(void *argv)
{
    ws_thread *wst = (ws_thread *)argv;
    int ret;
    char buf[WEBSOCKET_DATA_LEN_MAX] = {0};
    websocket_frame_t websocket_frame;

    char paysend_data[2000] = {0};    
    
    websocket_frame.payload_data = malloc(WEBSOCKET_DATA_LEN_MAX * sizeof(char *));
    while (!wst->wss->is_exit)
    {
        wst->is_run = true;
        if (FD_ISSET(wst->wsc->fd, &wst->wss->rset)) // 检查fd是否在这个集合里面, 
        {
            
            ws_memzero(websocket_frame.payload_data, WEBSOCKET_DATA_LEN_MAX);
            memset(buf, 0, WEBSOCKET_DATA_LEN_MAX);
            ret = recv(wst->wsc->fd, buf, WEBSOCKET_DATA_LEN_MAX, 0);
            if (ret == 0)
            {
                // 客户端断开连接
                wst->wsc->exittype = WET_DISCONNECT;
                break;
            }
            else if (ret > 0)
            {
                printf("---------------------\n");
                printf("fin=%d opcode=0x%X mask=%d payload_len=%llu\n",
                       websocket_frame.fin, websocket_frame.opcode,
                       websocket_frame.mask, websocket_frame.payload_length);

                FD_CLR(wst->wsc->fd, &wst->wss->wset);
                FD_SET(wst->wsc->fd, &wst->wss->wset);

                // 消息回调
                if (wst->wss->on_message)
                    wst->wss->on_message(wst->wsc, paysend_data, 1000, WDT_BINDATA);

                if (client_detect(wst))
                    break;
            }
        }
    }

    free(websocket_frame.payload_data);
    client_detect(wst);
    // 清空内存,下次使用
    memset(wst, 0, sizeof(ws_thread));
    return NULL;
}

void ws_server_release(ws_server *wss)
{
    if (wss)
    {
        clear_client(wss);
        wss->is_exit = true;
        close(wss->fd);
        free(wss);
        wss = NULL;
    }
}


/*******************************************************************************
 * 名称: ws_get_random_string
 * 功能: 生成随机字符串
 * 参数:
 *      buff: 随机字符串存储到
 *      len: 生成随机字符串长度
 * 返回: 无
 * 说明: 无
 ******************************************************************************/
static void ws_get_random_string(char *buff, uint32_t len)
{
    uint32_t i;
    uint8_t temp;
    srand((int32_t)time(0));
    for (i = 0; i < len; i++)
    {
        temp = (uint8_t)(rand() % 256);
        if (temp == 0) // 随机数不要0
            temp = 128;
        buff[i] = temp;
    }
}

/*******************************************************************************
 * 名称: ws_en_package
 * 功能: websocket数据收发阶段的数据打包, 通常client发server的数据都要mask(掩码)处理, 反之server到client却不用
 * 参数:
 *      data: 准备发出的数据
 *      data_len: 长度
 *      package: 打包后存储地址
 *      package_max_len: 存储地址可用长度
 *      mask: 是否使用掩码     1要   0 不要
 *      type: 数据类型, 由打包后第一个字节决定, 这里默认是数据传输, 即0x81
 * 返回: 打包后的长度(会比原数据长2~14个字节不等) <=0 打包失败
 * 说明: 无
 ******************************************************************************/
static int32_t ws_en_package(
    uint8_t *data,
    uint32_t data_len,
    uint8_t *package,
    uint32_t package_max_len,
    bool mask,
    ws_datatype type)
{
    uint32_t i, pkg_len = 0;
    // 掩码
    uint8_t mask_key[4] = {0};
    uint32_t maskCount = 0;
    // 最小长度检查
    if (package_max_len < 2)
        return -1;
    // 根据包类型设置头字节
    if (type == WDT_MINDATA)
        *package++ = 0x80;
    else if (type == WDT_TXTDATA)
        *package++ = 0x81;
    else if (type == WDT_BINDATA)   // 发送大的数据量
        *package++ = 0x82;
    else if (type == WDT_DISCONN)
        *package++ = 0x88;
    else if (type == WDT_PING)
        *package++ = 0x89;
    else if (type == WDT_PONG)
        *package++ = 0x8A;
    else
        return -1;
    pkg_len += 1;
    // 掩码位
    if (mask)
        *package = 0x80;
    // 半字节记录长度
    if (data_len < 126)
    {
        *package++ |= (data_len & 0x7F);
        pkg_len += 1;
    }
    // 2字节记录长度
    else if (data_len < 65536)
    {
        if (package_max_len < 4)
            return -1;
        *package++ |= 0x7E;
        *package++ = (uint8_t)((data_len >> 8) & 0xFF);
        *package++ = (uint8_t)((data_len >> 0) & 0xFF);
        pkg_len += 3;
    }
    // 8字节记录长度
    else
    {
        if (package_max_len < 10)
            return -1;
        *package++ |= 0x7F;
        *package++ = 0; // 数据长度变量是 uint32_t data_len, 暂时没有那么多数据
        *package++ = 0;
        *package++ = 0;
        *package++ = 0;
        *package++ = (uint8_t)((data_len >> 24) & 0xFF); // 到这里就够传4GB数据了
        *package++ = (uint8_t)((data_len >> 16) & 0xFF);
        *package++ = (uint8_t)((data_len >> 8) & 0xFF);
        *package++ = (uint8_t)((data_len >> 0) & 0xFF);
        pkg_len += 9;
    }
    // 数据使用掩码时,使用异或解码,mask_key[4]依次和数据异或运算,逻辑如下
    if (mask)
    {
        // 长度不足
        if (package_max_len < pkg_len + data_len + 4)
            return -1;
        // 随机生成掩码
        ws_get_random_string((char *)mask_key, sizeof(mask_key));
        *package++ = mask_key[0];
        *package++ = mask_key[1];
        *package++ = mask_key[2];
        *package++ = mask_key[3];
        pkg_len += 4;
        for (i = 0, maskCount = 0; i < data_len; i++, maskCount++)
        {
            // mask_key[4]循环使用
            if (maskCount == 4) // sizeof(mask_key))
                maskCount = 0;
            // 异或运算后得到数据
            *package++ = mask_key[maskCount] ^ data[i];
        }
        pkg_len += i;
        // 断尾
        *package = '\0';
    }
    // 数据没使用掩码, 直接复制数据段
    else
    {
        // 长度不足
        if (package_max_len < pkg_len + data_len)
            return -1;
        // 这种方法,data指针位置相近时拷贝异常
        //  memcpy(package, data, data_len);
        // 手动拷贝
        for (i = 0; i < data_len; i++)
            *package++ = data[i];
        pkg_len += i;
        // 断尾
        *package = '\0';
    }

    return pkg_len;
}

/*******************************************************************************
 * 名称: ws_send
 * 功能: websocket数据基本打包和发送
 * 参数:
 *      fd: 连接描述符
 *      *buff: 数据
 *      buff_len: 长度
 *      mask: 数据是否使用掩码, 客户端到服务器必须使用掩码模式
 *      type: 数据要要以什么识别头类型发送(txt, bin, ping, pong ...)
 * 返回: 调用send的返回
 * 说明: 无
 ******************************************************************************/
int32_t ws_send(int32_t fd, void *buff, int32_t buff_len, bool mask, ws_datatype type)
{
    uint8_t *ws_pkg = NULL;
    int32_t ret_len, ret;
    // 参数检查
    if (buff_len < 0)
        return 0;
    // 非包数据发送
    if (type == WDT_NULL)
        return send(fd, buff, buff_len, MSG_NOSIGNAL);
    // 数据打包 +14 预留类型、掩码、长度保存位
    ws_pkg = (uint8_t *)calloc(buff_len + 14, sizeof(uint8_t));
    ret_len = ws_en_package((uint8_t *)buff, buff_len, ws_pkg, (buff_len + 14), mask, type);
    if (ret_len <= 0)
    {
        free(ws_pkg);
        return 0;
    }

    ret = send(fd, ws_pkg, ret_len, MSG_NOSIGNAL);
    free(ws_pkg);
    return ret;
}