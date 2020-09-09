#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "modbus.h"
#include "utils.h"
#include <applibs/log.h>


static int parse_read_response(modbus_device_t *self, uint8_t *request, uint8_t *response, int len_rsp,
                               uint16_t *regs)
{
    // minimum 2 bytes, even in error case
    if (len_rsp < 2) {
        Log_Debug("response less than 2 bytes\n");
        return DEVICE_E_PROTOCOL;
    }

    uint8_t function_req = request[0];
    uint8_t function_rsp = response[0];
    uint16_t quantity_req = (uint16_t)((request[3] << 8) + request[4]);

    uint8_t function = response[0];

    // if succeed, server echo back function code
    if (function_rsp == function_req) {
        uint8_t byte_count = response[1];
        uint8_t *payload = response + 2;

        if (byte_count + 2 != len_rsp) {
            Log_Debug("byte count not match header\n");
            return DEVICE_E_PROTOCOL;
        }

        if (function == FC_READ_INPUT_REGISTERS || function == FC_READ_HOLDING_REGISTERS) {
            // check bytes received match requested
            if (byte_count != 2 * quantity_req) {
                Log_Debug("byte count not match requested\n");
                return DEVICE_E_PROTOCOL;
            }

            for (uint16_t i = 0; i < quantity_req; i++) {
                regs[i] = (uint16_t)((payload[i * 2] << 8) + payload[i * 2 + 1]);
            }
        } else if (function == FC_READ_COILS || function == FC_READ_DISCRETE_INPUTS) {
            // check bytes received match requested
            if (byte_count != (quantity_req + 7) / 8) {
                Log_Debug("byte count not match request\n");
                return DEVICE_E_PROTOCOL;
            }

            for (uint16_t i = 0; i < quantity_req; i++) {
                regs[i] = payload[i / 8] & (0x01u << (i % 8)) ? 1 : 0;
            }
        }
    } else if (function_rsp == (function_req | 0x80)) {
        // server echo back function code with LSB set when something wrong
        uint8_t exception_code = response[1];
        Log_Debug("Exception code: %d\n", exception_code);
        return DEVICE_E_PROTOCOL;
    } else {
        Log_Debug("Invalid server response: ADU:%s\n", hex(response, (size_t)len_rsp));
        return DEVICE_E_PROTOCOL;
    }

    return DEVICE_OK;
}

static int handle_read_request(modbus_device_t *self, uint8_t slave_id, uint8_t function_code, uint16_t addr,
                               uint16_t quantity, uint16_t *regs, int32_t timeout)
{
    uint8_t request[5];
    uint8_t response[MODBUS_MAX_PDU_SIZE];

    request[0] = function_code;          // MODBUS FUNCTION CODE
    request[1] = (uint8_t)((addr >> 8) & 0xFF);     // START REGISTER (Hi)
    request[2] = (uint8_t)(addr & 0xFF);            // START REGISTER (Lo)
    request[3] = (uint8_t)((quantity >> 8) & 0xFF); // NUMBER OF REGISTERS (Hi)
    request[4] = (uint8_t)(quantity & 0xFF);        // NUMBER OF REGISTERS (Lo)

    // send request
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);
    int err = modbus_rtu_send_request(self->rtu, slave_id, request, MODBUS_READ_REQUEST_FRAME_LENGTH, timeout);
    if (err) {
        Log_Debug("Failed to send request:%s\n", strerr(err));
        return err;
    }

    // recv response
    int len_rsp;
    // sending is not a blocked operation, so only use timeout for receiving
    int elapse_ms = timer_stopwatch_stop(&poll_sw);
    err = modbus_rtu_recv_response(self->rtu, slave_id, response, &len_rsp, timeout - elapse_ms);
    if (err) {
        Log_Debug("Failed to receive response:%s\n", strerr(err));
        return err;
    }

    // parse reponse
    return parse_read_response(self, request, response, len_rsp, regs);
}


static int parse_write_response(modbus_device_t *self, uint8_t *request, uint8_t *response, int len_rsp)
{
    // minimum 2 bytes, even in error case
    if (len_rsp < 2) {
        Log_Debug("reponse less than 2 bytes\n");
        return DEVICE_E_PROTOCOL;
    }

    uint8_t function_req = request[0];
    uint8_t function_rsp = response[0];

    // if succeed, server echo back function code
    if (function_rsp == function_req) {
        uint16_t addr_req = (uint16_t)((request[1] << 8) + request[2]);
        uint16_t addr_rsp = (uint16_t)((response[1] << 8) + response[2]);
        uint16_t quantity_req = (uint16_t)((request[3] << 8) + request[4]);
        uint16_t quantity_rsp = (uint16_t)((response[3] << 8) + response[4]);

        if (addr_req != addr_rsp || quantity_req != quantity_rsp) {
            Log_Debug("Invalid packet received\n");
            return DEVICE_E_PROTOCOL;
        }
    } else if (function_rsp == (function_req | 0x80)) {
        // server echo back error code
        uint8_t exception_code = response[1];
        Log_Debug("Exception code: %d\n", exception_code);
        return DEVICE_E_PROTOCOL;
    } else {
        Log_Debug("Don't understand server response\n");
        return DEVICE_E_PROTOCOL;
    }

    return DEVICE_OK;
}


static int handle_write_request(modbus_device_t *self, uint8_t slave_id, uint8_t function_code, uint16_t addr,
                                uint16_t quantity, uint16_t *regs, int32_t timeout)
{
    uint8_t request[MODBUS_MAX_PDU_SIZE];
    uint8_t response[MODBUS_MAX_PDU_SIZE];


    request[0] = function_code;          // MODBUS FUNCTION CODE
    request[1] = (uint8_t)((addr >> 8) & 0xFF);     // START REGISTER (Hi)
    request[2] = (uint8_t)(addr & 0xFF);            // START REGISTER (Lo)
    request[3] = (uint8_t)((quantity >> 8) & 0xFF); // NUMBER OF REGISTERS (Hi)
    request[4] = (uint8_t)(quantity & 0xFF);        // NUMBER OF REGISTERS (Lo)

    if (function_code == FC_WRITE_COILS) {
        request[5] = (uint8_t)((quantity + 7) / 8); // BYTE COUNT
        for (int i = 0; i < quantity; i++) {
            int byte_i = i / 8;
            int bit_i = i % 8;

            if (regs[i])
                request[6 + byte_i] |= (uint8_t)(1 << bit_i);
            else
                request[6 + byte_i] &= (uint8_t)(~(1 << bit_i));
        }
    } else if (function_code == FC_WRITE_HOLDING_REGISTERS) {
        request[5] = (uint8_t)(quantity * 2); // BYTE COUNT
        unsigned char *output = (request + 6);
        for (int i = 0; i < quantity; i++) {
            output[2 * i] = (uint8_t)(regs[i] >> 8);
            output[2 * i + 1] = (uint8_t)(regs[i] & 0xFF);
        }
    }

    // all write request has 6 bytes header plus addtional data
    struct timespec poll_sw;
    timer_stopwatch_start(&poll_sw);
    uint8_t len_req = (uint8_t)(6 + request[5]);
    int err = modbus_rtu_send_request(self->rtu, slave_id, request, len_req, timeout);
    if (err) {
        Log_Debug("Failed to send request:%s\n", strerr(err));
        return err;
    }

    int len_rsp;
    int elapse_ms = timer_stopwatch_stop(&poll_sw);
    err = modbus_rtu_recv_response(self->rtu, slave_id, response, &len_rsp, timeout - elapse_ms);
    if (err) {
        Log_Debug("Failed to receive response:%s\n", strerr(err));
        return err;
    }

    return parse_write_response(self, request, response, len_rsp);
}


// --------------------- public interface ---------------------------------------

int mb_read_register(modbus_device_t *self, uint8_t slave_id, uint8_t reg_type, uint16_t addr, uint16_t quantity,
                     uint16_t *regs, int32_t timeout)
{
    uint8_t fc = 0;
    switch (reg_type) {
    case COIL:
        fc = FC_READ_COILS;
        break;
    case DISCRETE_INPUT:
        fc = FC_READ_DISCRETE_INPUTS;
        break;
    case INPUT_REGISTER:
        fc = FC_READ_INPUT_REGISTERS;
        break;
    case HOLDING_REGISTER:
        fc = FC_READ_HOLDING_REGISTERS;
    }

    if (fc != 0)
        return handle_read_request(self, slave_id, fc, addr, quantity, regs, timeout);
    else
        return DEVICE_E_INVALID;
}

int mb_write_register(modbus_device_t *self, uint8_t slave_id, uint8_t reg_type, uint16_t addr, uint16_t quantity,
                      uint16_t *regs, int32_t timeout)
{
    uint8_t fc = 0;
    switch (reg_type) {
    case COIL:
        fc = FC_WRITE_COILS;
        break;
    case HOLDING_REGISTER:
        fc = FC_WRITE_HOLDING_REGISTERS;
        break;
    }
    if (fc != 0)
        return handle_write_request(self, slave_id, fc, addr, quantity, regs, timeout);
    else
        return DEVICE_E_INVALID;
}


int modbus_open(modbus_device_t *self, uint32_t slave_id, int timeout_ms)
{
    return modbus_rtu_open(self->rtu);
}


int modbus_close(modbus_device_t *self)
{
    return modbus_rtu_close(self->rtu);
}


void modbus_destroy_device(modbus_device_t *device)
{
    if (device) {
        if (device->rtu) {
            modbus_rtu_destroy(device->rtu);
        }
        free(device);
    }
}


modbus_device_t *modbus_create_device(int uart, unsigned int baud_rate)
{
    modbus_device_t *device = (modbus_device_t *)calloc(1, sizeof(modbus_device_t));

    device->rtu = modbus_rtu_create(uart, baud_rate);
    if (!device->rtu) {
        Log_Debug("Failed to create RTU\n");
        modbus_destroy_device(device);
        return NULL;
    }

    return device;
}