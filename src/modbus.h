#pragma once
#include "modbus_rtu.h"
#include <stdint.h>


enum { COIL = 0, DISCRETE_INPUT = 1, INVALID = 2, INPUT_REGISTER = 3, HOLDING_REGISTER = 4 };

enum {
    FC_READ_COILS = 0x01,
    FC_READ_DISCRETE_INPUTS = 0x02,
    FC_READ_HOLDING_REGISTERS = 0x03,
    FC_READ_INPUT_REGISTERS = 0x04,

    FC_WRITE_SINGLE_COIL = 0x05,
    FC_WRITE_SINGLE_REGISTER = 0x06,
    FC_READ_EXCEPTION_STATUS = 0x07,
    FC_DIAGNOSTICS = 0x08,
    FC_GET_COMM_EVENT_COUNTER = 0x0B,
    FC_GET_COMM_EVENT_LOG = 0x0C,
    FC_WRITE_COILS = 0x0F,
    FC_WRITE_HOLDING_REGISTERS = 0x10,
    FC_REPORT_SERVER_ID = 0x11,
    FC_READ_FILE_RECORD = 0x14,
    FC_WRITE_FILE_RECORD = 0x15,
    FC_MASK_WRITE_REGISTER = 0x16,
    FC_READ_WRITE_REGISTERS = 0x17,
    FC_READ_FIFO_QUEUE = 0x18,
    FC_MEI = 0x2B,
    FC_READ_DEVICE_IDENTITY = 0x2B
};
  
#define MODBUS_MAX_COIL_PER_READ 0x7D0
#define MODBUS_MAX_DISCRETE_PER_READ 0x7D0
#define MODBUS_MAX_INPUT_PER_READ 0x7D
#define MODBUS_MAX_HOLDING_PER_READ 0x7D
#define MODBUS_MAX_COIL_PER_WRITE 0x7B0
#define MODBUS_MAX_HOLDING_PER_WRITE 0x7B


typedef struct modbus_device_t modbus_device_t;
struct modbus_device_t {
    modbus_rtu_t *rtu;
};


struct modbus_device_t *modbus_create_device(int uart, unsigned int baud_rate);

int modbus_open(struct modbus_device_t *self, uint32_t slave_id, int timeout_ms);

int modbus_close(struct modbus_device_t *self);

void modbus_destroy_device(struct modbus_device_t *self);

int mb_read_register(struct modbus_device_t *self, uint8_t slave_id, uint8_t reg_type, uint16_t addr, uint16_t quantity,
                     uint16_t *buf, int32_t timeout_ms);

int mb_write_register(struct modbus_device_t *self, uint8_t slave_id, uint8_t reg_type, uint16_t addr, uint16_t quantity,
                      uint16_t *buf, int32_t timeout_ms);
