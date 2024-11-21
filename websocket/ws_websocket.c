
#include "ws_core.h"
#include "ws_websocket.h"

static int ws_fetch_sec_key(const char *buf, char *seckey);
static void ws_websocket_uumask(char *data, int len, char *mask);

int ws_websocket_shake_hand(int fd)
{
    char buffer[WEBSOCKET_DATA_LEN_MAX] = {0};
    char header[WEBSOCKET_DATA_LEN_MAX] = {0};
    char *clientkey;
    char sha1temp[256] = {0};
    int i, n;
    static char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    ws_str_t src_str, dst_str;

    n = read(fd, buffer, WEBSOCKET_DATA_LEN_MAX);

    printf("%s\n", buffer);
    dst_str.data = malloc(256 * sizeof(char *));
    ws_memzero(dst_str.data, 256);

    clientkey = (char *)malloc(256);
    memset(clientkey, 0, 256);
    if (ws_fetch_sec_key(buffer, clientkey) != WS_OK)
        return WS_ERROR;

    if (!clientkey)
    {
        return WS_ERROR;
    }

    // 编码成SHA-1哈希值，并返回哈希的base64编码
    strcat(clientkey, GUID);
    ws_sha1(sha1temp, clientkey);

    src_str.data = malloc(256 * sizeof(char *));
    ws_memzero(src_str.data, 256);

    for (i = 0; i < strlen(sha1temp); i += 2)
    {
        src_str.data[i / 2] = ws_hextoi(sha1temp + i, 2);
    }
    src_str.len = strlen(src_str.data);
    ws_encode_base64(&dst_str, &src_str);

    sprintf(header, "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: %s\r\n"
                    "\r\n",
            dst_str.data);

    write(fd, header, strlen(header));

    free(clientkey);
    free(src_str.data);
    free(dst_str.data);
    return WS_OK;
}

static int ws_fetch_sec_key(const char *buf, char *seckey)
{
    char *keyBegin;
    static char flag[] = "Sec-WebSocket-Key: ";
    int i = 0;

    if (buf == NULL)
    {
        return WS_ERROR;
    }

    keyBegin = strstr(buf, flag);
    if (!keyBegin)
    {
        return WS_ERROR;
    }

    keyBegin += strlen(flag);

    for (i = 0; i < strlen(buf); i++)
    {
        if (keyBegin[i] == 0x0A || keyBegin[i] == 0x0D)
        {
            break;
        }
        seckey[i] = keyBegin[i];
    }

    return WS_OK;
}

static void ws_websocket_uumask(char *data, int len, char *mask)
{
    int i;
    for (i = 0; i < len; ++i)
    {
        *(data + i) ^= *(mask + (i % 4));
    }
}

int ws_websocket_parse(int fd, websocket_frame_t *head)
{
    char one_char;

    // read fin and op code
    if (read(fd, &one_char, 1) <= 0)
    {
        perror("read fin");
        return WS_ERROR;
    }

    head->fin = (one_char & 0x80) == 0x80;
    head->opcode = one_char & 0x0F;
    if (read(fd, &one_char, 1) <= 0)
    {
        perror("read mask");
        return WS_ERROR;
    }

    head->mask = (one_char & 0x80) == 0X80;

    // get payload length
    head->payload_length = one_char & 0x7F;

    if (head->payload_length == 126)
    {
        char extern_len[2];
        if (read(fd, extern_len, 2) <= 0)
        {
            perror("read extern_len");
            return WS_ERROR;
        }
        head->payload_length = (extern_len[0] & 0xFF) << 8 | (extern_len[1] & 0xFF);
    }
    else if (head->payload_length == 127)
    {
        char extern_len[8], temp;
        int i;
        if (read(fd, extern_len, 8) <= 0)
        {
            perror("read extern_len");
            return WS_ERROR;
        }
        for (i = 0; i < 4; i++)
        {
            temp = extern_len[i];
            extern_len[i] = extern_len[7 - i];
            extern_len[7 - i] = temp;
        }
        memcpy(&(head->payload_length), extern_len, 8);
    }

    // read masking-key
    if (read(fd, head->masking_key, 4) <= 0)
    {
        perror("read masking-key");
        return WS_ERROR;
    }

    // read payload data
    if (read(fd, head->payload_data, WEBSOCKET_DATA_LEN_MAX) <= 0)
    {
        perror("read payload data");
        return WS_ERROR;
    }

    ws_websocket_uumask(head->payload_data, strlen(head->payload_data), head->masking_key);
    printf("d--%s\n", head->payload_data);

    return 0;
}

