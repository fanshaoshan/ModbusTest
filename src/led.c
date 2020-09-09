#include <unistd.h>
#include <string.h>

#include <applibs/gpio.h>
#include <applibs/log.h>
#include "led.h"

#include <hw/sample_hardware.h>

static const GPIO_Id led_pins[NUM_LEDS][3] = {
    {SAMPLE_AILINK_LED1_RED, SAMPLE_AILINK_LED1_GREEN, SAMPLE_AILINK_LED1_BLUE},
    {SAMPLE_AILINK_LED2_RED, SAMPLE_AILINK_LED2_GREEN, SAMPLE_AILINK_LED2_BLUE}
};


static struct led_t leds[NUM_LEDS];

int led_open(int i)
{
    if (i >= NUM_LEDS) return -1;

    for (int c = 0; c < NUM_CHANNELS; c++) {
        leds[i].channel[c] =
            GPIO_OpenAsOutput(led_pins[i][c], GPIO_OutputMode_PushPull, LED_OFF);
        if (leds[i].channel[c] < 0) {
            Log_Debug("ERROR: Could not open LED.\n");
            return -1;
        }
    }
    return 0;
}

int led_set_color(int i, int color)
{
    int result;

    for (int c = 0; c < NUM_CHANNELS; c++) {
        bool isOn = (int)color & (0x1 << c);

        result =
            GPIO_SetValue(leds[i].channel[c], isOn ? LED_ON : LED_OFF);
        if (result != 0) {
            Log_Debug("ERROR: Cannot change RGB LED %d color.\n", i);
        }
    }
    return result;
}

void led_close(int i)
{
    if (i >= NUM_LEDS) return;

    for (int c = 0; c < NUM_CHANNELS; c++) {
        int ledFd = leds[i].channel[c];
        if (ledFd >= 0) {
            GPIO_SetValue(ledFd, LED_OFF); // off
            close(ledFd);
        }
    }
}