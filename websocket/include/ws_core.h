
/*
 * Copyright (C) K
 */

#ifndef _WS_CORE_H_
#define _WS_CORE_H_

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h> /* offsetof() */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>

#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sched.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* TCP_NODELAY, TCP_CORK */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

#include <time.h>   /* tzset() */
#include <malloc.h> /* memalign() */
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <crypt.h>
#include <sys/prctl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <semaphore.h>


typedef struct ws_str_s ws_str_t;
typedef struct websocket_frame_s websocket_frame_t;

#define WS_OK 0
#define WS_ERROR -1
#define WS_AGAIN -2
#define WS_BUSY -3
#define WS_DONE -4
#define WS_DECLINED -5
#define WS_ABORT -6

#include "ws_string.h"
#include "ws_sha1.h"
#include "ws_websocket.h"

#define LF (u_char)10
#define CR (u_char)13
#define CRLF "\x0d\x0a"

#define ws_abs(value) (((value) >= 0) ? (value) : -(value))

#endif /* _WS_CORE_H_INCLUDED_ */
