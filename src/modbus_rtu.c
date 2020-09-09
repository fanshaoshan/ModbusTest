#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define UART_STRUCTS_VERSION 1
#include <applibs/uart.h>
#include <applibs/log.h>
#include <applibs/gpio.h>

#include "modbus.h"
#include "modbus_rtu.h"
#include "utils.h"
#include "led.h"

#include <hw/sample_hardware.h>

#define TX_LED 0
#define RX_LED 1

#define MB_RTU_MAX_ADU_SIZE 256
#define MB_RTU_HEADER_SIZE 3

#define UART_read read
#define UART_write write
#define UART_close close


static int rtu_ensure_idle(modbus_rtu_t *self, int timeout)
{
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);

    struct pollfd fds[1];
    fds[0].fd = self->uart_fd;
    fds[0].events = POLLIN;

    bool quit = false;
    int elapse_ms;
    uint8_t garbage[MB_RTU_MAX_ADU_SIZE];
    while (!quit) {
        elapse_ms = timer_stopwatch_stop(&poll_sw);
        if (elapse_ms >= timeout) {
            quit = true;
            continue;
        }

        int nevents = poll(fds, 1, 0);
        if (nevents == 0) {
            // no data on rtu
            return 0;
        } else if (nevents == 1 && (fds[0].revents & POLLIN)) {
            // Read garbage data in uart buffer up to one frame of 256 bytes.
            Log_Debug("Consume garbage data: read %d byes of garbage on RTU\n",
                      UART_read(self->uart_fd, garbage, MB_RTU_MAX_ADU_SIZE));
        } else {
            Log_Debug("Uart poll error in rtu_ensure_idle: %s\n", strerror(errno));
            quit = true;
        }
    }

    return -1;
}

static uint16_t crc16(const uint8_t *buffer, int len)
{
    uint16_t temp, flag;
    temp = 0xFFFF;
    for (int i = 0; i < len; i++) {
        temp = (uint16_t)(temp ^ buffer[i]);
        for (int j = 1; j <= 8; j++) {
            flag = temp & 0x0001;
            temp >>= 1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    return temp & 0xFFFF;
}

#ifdef TX_ENABLE
static void tcdrain(modbus_rtu_t *self, size_t count)
{
    // assume 8N1 uart config
    double bytes_per_second = (double)(self->baud_rate) / 10;
    long tx_enable_us = (long)(1e6 * count / bytes_per_second);
    tx_enable_us += 200; // assume 200us processing time
    struct timespec delay;
    delay.tv_sec = (long)(tx_enable_us / 1e6);
    delay.tv_nsec = (tx_enable_us % 1000000) * 1000;
    nanosleep(&delay, NULL);
}
#endif

static int rtu_write_frame(modbus_rtu_t *self, uint8_t *buf, int count, int timeout)
{
#ifdef TX_ENABLE
    if (self->tx_enable_fd >= 0) {
        GPIO_SetValue(self->tx_enable_fd, GPIO_Value_High);
    }    
#endif

    int total = 0, uart_fd = self->uart_fd;
    struct pollfd fds[1];
    fds[0].fd = uart_fd;
    fds[0].events = POLLOUT;

    int result = DEVICE_OK, elapse_ms;
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);
    while (total < count) {
        elapse_ms = timer_stopwatch_stop(&poll_sw);
        if (elapse_ms >= timeout) {
            result = DEVICE_E_TIMEOUT;
            break;
        }

        int nevents = poll(fds, 1, timeout - elapse_ms);
        if (nevents < 0) {
            Log_Debug("uart poll out error:%s\n", strerror(errno));
            result = DEVICE_E_IO;
            break;
        } else if (nevents == 0) {
            Log_Debug("uart sending timeout\n");
            result = DEVICE_E_TIMEOUT;
            break;
        } else {
            if (fds[0].revents & POLLHUP) {
                Log_Debug("uart connection broken\n");
                result = DEVICE_E_BROKEN;
                break;
            } else if (fds[0].revents & POLLERR) {
                Log_Debug("uart poll error\n");
                result = DEVICE_E_IO;
                break;
            } else if (fds[0].revents & POLLOUT) {
                led_set_color(TX_LED, Led_Colors_Red);
                int nwrite = UART_write(uart_fd, buf + total, (size_t)(count - total));
                if (nwrite < 0) {
                    Log_Debug("uart write error:%s\n", strerror(errno));
                    result = DEVICE_E_IO;
                    break;
                }
                total += nwrite;
            }
        }
    }

#ifdef TX_ENABLE
    // wait for all sending bytes to be put on wire
    tcdrain(self, (size_t)count);
    if (self->tx_enable_fd >= 0) {
        GPIO_SetValue(self->tx_enable_fd, GPIO_Value_Low);
    }    
#endif

    const struct timespec ts_delay = {.tv_sec = 0, .tv_nsec=5*1000000};
    nanosleep(&ts_delay, NULL);
    led_set_color(TX_LED, Led_Colors_Off);                

    return result;
}


static int find_pdu_len(uint8_t *pdu)
{
    // it's error response, pdu is two bytes
    if (pdu[0] & 0x80)
        return 2;

    int length;
    switch (pdu[0]) {
    case FC_READ_COILS:
    case FC_READ_DISCRETE_INPUTS:
    case FC_READ_HOLDING_REGISTERS:
    case FC_READ_INPUT_REGISTERS:
        // 1 byte function code + 1 byte byte count + n bytes values
        length = 2 + pdu[1];
        break;
    case FC_WRITE_COILS:
    case FC_WRITE_HOLDING_REGISTERS:
    case FC_WRITE_SINGLE_COIL:
    case FC_WRITE_SINGLE_REGISTER:
        // 1 byte function code + 2 bytes start addr, 2 byte quantity
        length = 5;
        break;
    case FC_READ_EXCEPTION_STATUS:
        // 1 byte function code + 1 byte output data
        length = 2;
        break;
    case FC_DIAGNOSTICS:
        // 1 byte function code, 2 byte sub-function , 2 bytes data
        length = 5;
        break;
    case FC_GET_COMM_EVENT_COUNTER:
        // 1 byte function code, 2 byte status, 2 bytes event count
        length = 5;
        break;
    case FC_GET_COMM_EVENT_LOG:
        length = 2 + pdu[1];
        break;
    case FC_REPORT_SERVER_ID:
        length = 5;
        break;
    case FC_READ_FILE_RECORD:
    case FC_WRITE_FILE_RECORD:
        length = 2 + pdu[1];
        break;
    case FC_MASK_WRITE_REGISTER:
        length = 7;
        break;
    case FC_READ_WRITE_REGISTERS:
        length = 2 + pdu[1];
        break;
    case FC_READ_FIFO_QUEUE:
        length = 2 + pdu[1];
        break;
    case FC_MEI:
        // FC_MEI is not supported now as the pdu lenth cann't be found
        // in pdu and not fixed.
    default:
        length = -1;
        break;
    }

    return length;
}


static int rtu_read_bytes(modbus_rtu_t *self, uint8_t *buf, int count, int timeout)
{
    int result = DEVICE_OK, total = 0, elapse_ms;
    struct timespec poll_sw;
    struct pollfd fds[1];
    fds[0].fd = self->uart_fd;
    fds[0].events = POLLIN;
    timer_stopwatch_start(&poll_sw);

    while (total < count) {
        elapse_ms = timer_stopwatch_stop(&poll_sw);
        if (elapse_ms >= timeout) {
            result = DEVICE_E_TIMEOUT;
            break;
        }

        int nevents = poll(fds, 1, timeout - elapse_ms);
        if (nevents < 0) {
            Log_Debug("uart poll in err: %s\n", strerror(errno));
            result = DEVICE_E_IO;
            break;
        } else if (nevents == 0) {
            Log_Debug("uart receiving timeout\n");
            result = DEVICE_E_TIMEOUT;
            break;
        } else {
            if (fds[0].revents & POLLHUP) {
                Log_Debug("uart connection broken\n");
                result = DEVICE_E_BROKEN;
                break;
            } else if (fds[0].revents & POLLERR) {
                Log_Debug("uart poll error\n");
                result = DEVICE_E_IO;
                break;
            } else if (fds[0].revents & POLLIN) {
                led_set_color(RX_LED, Led_Colors_Red);
                int nread = UART_read(fds[0].fd, buf + total, (size_t)(count - total));
                if (nread < 0) {
                    Log_Debug("uart read error:%s\n", strerror(errno));
                    result = DEVICE_E_IO;
                    break;
                }
                total += nread;
            }
        }
    }

    const struct timespec ts_delay = {.tv_sec = 0, .tv_nsec=5*1000000};
    nanosleep(&ts_delay, NULL);

    led_set_color(RX_LED, Led_Colors_Off);
    return result;
}


static int rtu_read_frame(modbus_rtu_t *self, uint8_t *buf, int *fbytes, int timeout)
{
    // Find pdu len is tricky as we need to parse pdu to figure out
    // read first three bytes of adu
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);
    int err = rtu_read_bytes(self, buf, MB_RTU_HEADER_SIZE, timeout);
    if (err) {
        Log_Debug("Failed to read adu header:%s\n", strerr(err));
        return err;
    }

    int pdu_len = find_pdu_len(buf + 1);
    if (pdu_len <= 0 || pdu_len > MB_RTU_MAX_ADU_SIZE - 3) {
        Log_Debug("Invalid pdu len %d\n", pdu_len);
        return DEVICE_E_PROTOCOL;
    }

    // 1 byte slave_id + pdu + 2 bytes crc
    *fbytes = 1 + pdu_len + 2;
    int elapse_ms = timer_stopwatch_stop(&poll_sw);
    int result =  rtu_read_bytes(self, buf + 3, pdu_len, timeout - elapse_ms);
    return result;
}


// ------------------------ public interface --------------------------------


int modbus_rtu_open(modbus_rtu_t *self)
{
    // don't try to open again
    if (self->uart_fd >= 0)
        return 0;

    UART_Config config;
    UART_InitConfig(&config);
    config.blockingMode = UART_BlockingMode_NonBlocking;
    config.dataBits = UART_DataBits_Eight;
    config.parity = UART_Parity_None;
    config.stopBits = UART_StopBits_One;
    config.baudRate = self->baud_rate;

    self->uart_fd = UART_Open(self->uart_port, &config);
    if (self->uart_fd < 0) {
        Log_Debug("ERROR: Could not open UART: %s (%d).\n", strerror(errno), errno);
        return -1;
    }
#ifdef TX_ENABLE
    self->tx_enable_fd = GPIO_OpenAsOutput(SAMPLE_AILINK_UART_ENABLE, GPIO_OutputMode_PushPull, GPIO_Value_Low);

    if (self->tx_enable_fd < 0) {
        Log_Debug("ERROR: Could not open tx enable GPIO\n");
        return DEVICE_E_IO;
    }
#endif    

    return 0;
}

int modbus_rtu_close(modbus_rtu_t *self)
{
    int result = 0;
    if (self->uart_fd >= 0) {
        result = UART_close(self->uart_fd);
        self->uart_fd = -1;
    }
#ifdef TX_ENABLE
    if (self->tx_enable_fd >= 0) {
        result = close(self->tx_enable_fd);
        self->tx_enable_fd = -1;
    }
#endif    

    return result;
}

int modbus_rtu_send_request(modbus_rtu_t *self, uint8_t slave_id, const uint8_t *pdu, int pdu_len, int timeout)
{
    uint8_t adu[MB_RTU_MAX_ADU_SIZE];
    adu[0] = slave_id;
    memcpy(adu + 1, pdu, (size_t)pdu_len);

    // crc for the entrie adu
    uint16_t crc = crc16(adu, pdu_len + 1);
    adu[1 + pdu_len] = (uint8_t)(crc & 0xFF);
    adu[1 + pdu_len + 1] = (uint8_t)((crc >> 8) & 0xFF);

    int adu_len = pdu_len + 3; // 1 byte slave id + pdu + 2 bytes crc
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);

    if (rtu_ensure_idle(self, timeout) != 0) {
        return DEVICE_E_BUSY;
    }

    int elapse_ms = timer_stopwatch_stop(&poll_sw);
    int err = rtu_write_frame(self, adu, adu_len, timeout - elapse_ms);
    if (err) {
        Log_Debug("Failed to write request:%s\n", strerr(err));
        return err;
    }

    Log_Debug("ADU-->%s\n", hex(adu, (size_t)adu_len));
    return DEVICE_OK;
}


int modbus_rtu_recv_response(modbus_rtu_t *self, uint8_t slave_id, uint8_t *pdu, int *ppdu_len, int timeout)
{
    uint8_t adu[MB_RTU_MAX_ADU_SIZE];

    int adu_len = 0;
    // adu must have enough space to receive MB_RTU_MAX_ADU_SIZE bytes
    int err = rtu_read_frame(self, adu, &adu_len, timeout);
    if (err != 0) {
        return err;
    }

    Log_Debug("ADU<--%s\n", hex(adu, (size_t)adu_len));

    if (adu[0] != slave_id) {
        Log_Debug("Discard unexpected frame from slave %d, expected %d\n", adu[0], slave_id);
        return DEVICE_E_PROTOCOL;
    }

    uint16_t crc1 = (uint16_t)(adu[adu_len - 2] + (adu[adu_len - 1] << 8));
    uint16_t crc2 = crc16(adu, adu_len - 2);
    if (crc1 != crc2) {
        Log_Debug("CRC error: recv=%x calc=%x\n", crc1, crc2);
        return DEVICE_E_PROTOCOL;
    }

    // 1 byte slave_id + pdu + 2 bytes crc
    int pdu_len = adu_len - 3;
    memcpy(pdu, adu + 1, (size_t)pdu_len);
    *ppdu_len = pdu_len;
    return DEVICE_OK;
}


void modbus_rtu_destroy(modbus_rtu_t *self)
{
    modbus_rtu_close(self);
    free(self);
    led_close(TX_LED);
    led_close(RX_LED);
}


modbus_rtu_t *modbus_rtu_create(int uart, unsigned int baud_rate)
{
    modbus_rtu_t *rtu = (modbus_rtu_t *)calloc(1, sizeof(modbus_rtu_t));

    led_open(TX_LED);
    led_open(RX_LED);

    rtu->uart_port = uart;
    rtu->baud_rate = baud_rate;
    rtu->uart_fd = -1;
#ifdef TX_ENABLE
    rtu->tx_enable_fd = -1;
#endif        
    return rtu;
}
