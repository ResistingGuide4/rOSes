#ifndef DEVICE_H
#define DEVICE_H 1

#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

typedef enum {
    PS2_PORT_ONE = 1,
    PS2_PORT_TWO = 2
} PS2_port_t;

void send_byte(uint8_t value, PS2_port_t port);
uint8_t recieve_byte();

#endif