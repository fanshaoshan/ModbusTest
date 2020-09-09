#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>


#include <applibs/log.h>
#include "utils.h"

static const char* error_name[] = {
    "DEVICE_OK",
    "DEVICE_E_INVALID",
    "DEVICE_E_IO",
    "DEVICE_E_BROKEN",
    "DEVICE_E_PROTOCOL",
    "DEVICE_E_TIMEOUT",
    "DEVICE_E_INTERNAL",
    "DEVICE_E_CONFIG"
};


const char* strerr(int err)
{
    static char *unknown = "E_UNKNOWN";
    if (err >= 0 || err <= DEVICE_E_CONFIG)
        return error_name[err];
    else
        return unknown;
}

// strcat with capability to grow
// msg - initial allocated buffer, can be NULL which will do the allocation
// psize - pointer to inital allocated buffer size, will be new buffer size on return
// inc - incremental for each growth
// token - null terminated string to concat.
// return point to result string, need to free by client
char* strcat_grow(char* msg, size_t *psize, size_t inc, const char* token)
{
    if (!token) return msg;

    size_t old_len = msg ? strlen(msg) : 0;
    size_t new_len = old_len + strlen(token) + 1;

    while (*psize < new_len)
        *psize += inc;

    char *msg2 = malloc(*psize);
    *msg2 = 0;
    if (msg) strcpy(msg2, msg);

    // start with old_len to improve performance
    strcat(msg2 + old_len, token);
    free(msg);
    return msg2;
}


// return current timestamp string, not thread safe.
const char* get_timestamp(time_t t)
{
    // timestamp
    static char timestamp[40];
    time_t s = 0;
    long ms = 0;

    if (t) {
        s = t;
        ms = 0;
    } else {
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        s = spec.tv_sec;
        ms = (spec.tv_nsec + 500000) / 1000000;
        if (ms > 999) { s++; ms = 0; }
    }
    struct tm *tm = gmtime(&s);
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, ms);
    return timestamp;
}

const char* format_timestamp(struct timespec spec)
{
    static char timestamp[40];

    time_t s = spec.tv_sec;
    long ms = (spec.tv_nsec + 500000) / 1000000;
    if (ms > 999) { s++; ms = 0; }

    struct tm *tm = gmtime(&s);
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, ms);
    return timestamp;
}

const char* hex(const unsigned char* data, size_t len)
{
    static char buf[400];
    int n = snprintf(buf, sizeof(buf), "[");
    for (size_t i = 0; i < len; i++)
        n += snprintf(buf + n, (size_t)(sizeof(buf) - n), "%02x ", data[i]);
    snprintf(buf + n, (size_t)(sizeof(buf) - n), "]");
    return buf;
}

const char* chr(const unsigned char* data, size_t len)
{
    static char buf[400];
    int n = snprintf(buf, sizeof(buf), "[");
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 32)
            n += snprintf(buf + n, (size_t)(sizeof(buf) - n), "\%d", data[i]);
        else
            n += snprintf(buf + n, (size_t)(sizeof(buf) - n), "%c", data[i]);
    }
    snprintf(buf + n, (size_t)(sizeof(buf) - n), "]");
    return buf;

}



char* trim(char * s)
{
    size_t l = strlen(s);

    while (l && isspace(s[l - 1])) --l;
    while (l && *s && isspace(*s)) ++s, --l;

    return l > 0 ? strndup(s, l) : NULL;
}


// start a stopwatch
inline void timer_stopwatch_start(struct timespec *s)
{
    clock_gettime(CLOCK_MONOTONIC, s);
}

// stop stopwatch and return ms
inline long timer_stopwatch_stop(struct timespec *s)
{
    struct timespec now = { 0, 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (now.tv_sec - s->tv_sec) * 1000
        + (now.tv_nsec - s->tv_nsec) / 1000000;
    return ms;
}

