#pragma once
#include <stddef.h> /* size_t */
#include <time.h>
#include <applibs/log.h>

enum {
    DEVICE_OK,
    DEVICE_E_INVALID,    // invalid operation
    DEVICE_E_IO,         // IO error
    DEVICE_E_BROKEN,     // connection broken
    DEVICE_E_PROTOCOL,   // protocol error, bad pdu, out of sync
    DEVICE_E_TIMEOUT,    // timeout
    DEVICE_E_INTERNAL,   // internal logic error, assert
    DEVICE_E_CONFIG,     // device configuration error
    DEVICE_E_BUSY,       // Garbage data on link
};


const char *strerr(int err);



char* strcat_grow(char* msg, size_t *msg_size, size_t inc_size, const char* token);
const char* get_timestamp(time_t t);
const char* format_timestamp(struct timespec spec);

const char*  hex(const unsigned char* data, size_t len);
const char*  chr(const unsigned char* data, size_t len);


// start a stopwatch
void timer_stopwatch_start(struct timespec *s);

// stop stopwatch and return ms
long timer_stopwatch_stop(struct timespec *s);

// none inplace trim, caller need to release memory
char* trim(char * s);

