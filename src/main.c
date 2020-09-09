
#define UART_STRUCTS_VERSION 1
#define NETWORKING_STRUCTS_VERSION 1

#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <applibs/log.h>
#include <applibs/networking.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "led.h"
#include "modbus.h"

#include <hw/sample_hardware.h>


#define BAUD_RATE    19200
#define TIMEOUT_MS   1000
#define SLAVE_ID     1
#define REGISTER_ADDR 1840
#define REGISTER_COUNT 100

#define UART_PORT    SAMPLE_AILINK_UART1

int test_modbus(void)
{
    Log_Debug("\n\nmodbus test\n");
    struct modbus_device_t *device = modbus_create_device(UART_PORT, BAUD_RATE);

    if (!device) {
        Log_Debug("Failed to create modbus device\n");
        return -1;
    }

    if (modbus_open(device, SLAVE_ID, TIMEOUT_MS) != 0) {
        Log_Debug("Failed to open modbus device\n");
        modbus_destroy_device(device);
        return -1;
    }

    for (uint16_t i = REGISTER_ADDR; i < REGISTER_ADDR + REGISTER_COUNT; i++) {
        uint16_t reg;
        mb_read_register(device, SLAVE_ID, HOLDING_REGISTER, i, 1, &reg, TIMEOUT_MS);
    }

    modbus_close(device);
    modbus_destroy_device(device);
    return 0;
}


int main(void )
{
    Log_Debug("Application starting 2\n");

    while (1) {
		test_modbus();
        sleep(5);
    }
    return 0;
}