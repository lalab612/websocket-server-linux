
/*
 * Copyright (C) $K
 */

#ifndef _WS_STRING_H_
#define _WS_STRING_H_

#include "ws_core.h"

struct ws_str_s
{
    size_t len;
    u_char *data;
};

#define break_uint32(var, ByteNum) \
    (u_char)((uint32_t)(((var) >> ((ByteNum) * 8)) & 0x00FF))

#define build_uint32(Byte0, Byte1, Byte2, Byte3) \
    ((u_char)((uint32_t)((Byte0) & 0x00FF) + ((uint32_t)((Byte1) & 0x00FF) << 8) + ((uint32_t)((Byte2) & 0x00FF) << 16) + ((uint32_t)((Byte3) & 0x00FF) << 24)))

#define build_uint16(loByte, hiByte) \
    ((u_short)(((loByte) & 0x00FF) + (((hiByte) & 0x00FF) << 8)))

#define hi_uint16(a) (((a) >> 8) & 0xFF)
#define lo_uint16(a) ((a) & 0xFF)

#define build_uint8(hiByte, loByte) \
    ((u_char)(((loByte) & 0x0F) + (((hiByte) & 0x0F) << 4)))

#define hi_uint8(a) (((a) >> 4) & 0x0F)
#define lo_uint8(a) ((a) & 0x0F)

#define ws_string(str) {sizeof(str) - 1, (u_char *)str}
#define ws_null_string {0, NULL}

#define ws_tolower(c) (u_char)((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define ws_toupper(c) (u_char)((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

#define ws_strncasecmp(s1, s2, n) \
    strncasecmp((const char *)s1, (const char *)s2, n)
#define ws_strcasecmp(s1, s2) \
    strcasecmp((const char *)s1, (const char *)s2)

#define ws_strncmp(s1, s2, n) strncmp((const char *)s1, (const char *)s2, n)

#define ws_strcmp(s1, s2) strcmp((const char *)s1, (const char *)s2)

#define ws_strstr(s1, s2) strstr((const char *)s1, (const char *)s2)
#define ws_strlen(s) strlen((const char *)s)

#define ws_memzero(buf, n) (void)memset(buf, 0, n)
#define ws_memset(buf, c, n) (void)memset(buf, c, n)

#define ws_cpymem(dst, src, n) ((u_char *)memcpy(dst, src, n)) + (n)

#define ws_memcmp memcmp

u_char *ws_cpystrn(u_char *dst, u_char *src, size_t n);

int ws_rstrncmp(u_char *s1, u_char *s2, size_t n);
int ws_rstrncasecmp(u_char *s1, u_char *s2, size_t n);

int ws_atoi(u_char *line, size_t n);
ssize_t ws_atosz(u_char *line, size_t n);
off_t ws_atoof(u_char *line, size_t n);
time_t ws_atotm(u_char *line, size_t n);
int ws_hextoi(u_char *line, size_t n);

void ws_md5_text(u_char *text, u_char *md5);

#define ws_base64_encoded_length(len) (((len + 2) / 3) * 4)
#define ws_base64_decoded_length(len) (((len + 3) / 4) * 3)

void ws_encode_base64(ws_str_t *dst, ws_str_t *src);
int ws_decode_base64(ws_str_t *dst, ws_str_t *src);

size_t ws_utf_length(ws_str_t *utf);
u_char *ws_utf_cpystrn(u_char *dst, u_char *src, size_t n);

#define ws_qsort qsort

#define ws_value_helper(n) #n
#define ws_value(n) ws_value_helper(n)

#endif /* _WS_STRING_H_INCLUDED_ */
