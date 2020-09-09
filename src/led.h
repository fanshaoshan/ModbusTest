#pragma once

#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <applibs/gpio.h>

#define NUM_CHANNELS 3

#define NUM_LEDS 2

#define LED_ON GPIO_Value_High
#define LED_OFF GPIO_Value_Low

struct led_t {
    int channel[NUM_CHANNELS];
};


#define RGBLED_INIT_VALUE               \
    {                                   \
        .channel = { -1, -1, -1 } \
    }


enum {
    Led_Colors_Off = 0,     // 000 binary
    Led_Colors_White = 7,   // 111 binary
    Led_Colors_Red = 1,     // 001 binary
    Led_Colors_Green = 2,   // 010 binary
    Led_Colors_Blue = 4,    // 100 binary
    Led_Colors_Cyan = 6,    // 110 binary
    Led_Colors_Magenta = 5, // 101 binary
    Led_Colors_Yellow = 3,  // 011 binary
    Led_Colors_Unknown = 8  // 1000 binary
};


int led_open(int i);

void led_close(int i);

int led_set_color(int i, int color);
