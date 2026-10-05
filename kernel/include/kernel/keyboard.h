#ifndef KEYBOARD_H
#define KEYBOARD_H 1

#include <stdbool.h>
#include <stdint.h>

#include <kernel/device.h>

uint32_t PS2_key_handler();
void keyboard_init(PS2_port_t port);

#endif