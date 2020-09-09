#pragma once
#include <stdint.h>

// #define TX_ENABLE

#define MODBUS_MAX_PDU_SIZE 253
#define MODBUS_MAX_ADU_SIZE 260

#define MODBUS_READ_REQUEST_FRAME_LENGTH 5

typedef struct modbus_rtu_t modbus_rtu_t;
struct modbus_rtu_t {
    int uart_fd;
    int uart_port;
    unsigned int baud_rate;
#ifdef TX_ENABLE
    int tx_enable_fd;
#endif    
};


modbus_rtu_t *modbus_rtu_create(int uart, unsigned int baud_rate);
void modbus_rtu_destroy(modbus_rtu_t *self);
int modbus_rtu_open(modbus_rtu_t *self);
int modbus_rtu_close(modbus_rtu_t *self);
void modbus_rtu_destroy(modbus_rtu_t *self);

int modbus_rtu_send_request(modbus_rtu_t *self, uint8_t slave_id, const uint8_t *pdu, int pdu_len, int timeout);
int modbus_rtu_recv_response(modbus_rtu_t *self, uint8_t slave_id, uint8_t *pdu, int *ppdu_len, int timeout);
