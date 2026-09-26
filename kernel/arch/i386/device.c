#include <kernel/device.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

bool PS2_port_one = true;
uint16_t PS2_device_one;

bool PS2_port_two = false;
uint16_t PS2_device_two;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %w1, %b0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
}

void send_command(uint8_t val) {
    uint32_t timeout = 100000;

    while ((inb(PS2_STATUS) & 0x2) == 0x2) {
        if (--timeout == 0) {
            // TODO: Add timeout error here
            return;
        }
    }
    outb(PS2_COMMAND, val);
}

void send_byte(uint8_t val, PS2_port_t port) {
    if (!PS2_port_one && port == PS2_PORT_ONE) {
        return;
    }

    uint32_t timeout = 100000;

    if (port == PS2_PORT_TWO) {
        if (!PS2_port_two) {
            return;
        }
        send_command(0xD4);
    }

    while ((inb(PS2_STATUS) & 0x2) == 0x2) {
        if (--timeout == 0) {
            // TODO: Add timeout error here
            return;
        }
    }
    outb(PS2_DATA, val);
}

uint8_t recieve_byte() {
    uint32_t timeout = 100000;

    while ((inb(PS2_STATUS) & 0x1) == 0) {
        if (--timeout == 0) {
            return 0xFF;
        }
    }
    return inb(PS2_DATA);
}

uint16_t identify_device(PS2_port_t port) {
    if ((port == PS2_PORT_ONE && !PS2_port_one) ||
        (port == PS2_PORT_TWO && !PS2_port_two)) {
        return 0xFFFF;
    }
        
    send_byte(0xF5, port);
    if (recieve_byte() != 0xFA) {
        return 0xFFFF;
    }

    send_byte(0xF2, port);
    if (recieve_byte() != 0xFA) {
        return 0xFFFF;
    }

    uint16_t id = 0;
    for (uint8_t val = recieve_byte(); val != 0xFF; val = recieve_byte()) {
        id <<= 8;
        id |= (uint16_t)val;
    }

    send_byte(0xF4, port);
    if (recieve_byte() != 0xFA) {
        return 0xFFFF;
    }


    return id;
}

static uint8_t read_config() {
    send_command(0x20);
    return recieve_byte();
}

static void write_config(uint8_t config) {
    send_command(0x60);
    send_byte(config, PS2_PORT_ONE);
}

void PS2_init() {
    send_command(0xAD); // Disable Port 1
    send_command(0xA7); // Disable Port 2
    recieve_byte(); // Flush Output Buffer

    uint8_t config_byte = read_config();
    config_byte &= 0xAE; // Clear translation and IRQ's and set clock for device port 1
    write_config(config_byte);

    send_command(0xAA); // PS2 controller self test
    if (recieve_byte() != 0x55) {
        // TODO: Add PS/2 Self Test Error here
        return;
    }
    write_config(config_byte); // Restore configuration byte in case it was changed

    send_command(0xA8); // Test for second port
    config_byte = read_config();
    if ((config_byte & 0x20) == 0) { // Check config bit 5
        PS2_port_two = true;
        send_command(0xA7); // Disable port 2

        config_byte = read_config();
        config_byte &= 0xDD;
        write_config(config_byte);

        send_command(0xA9);
        if (recieve_byte() != 0x0) {
            PS2_port_two = false;
        } else {
            send_command(0xA8); // Enable Port 2
            PS2_device_two = 0x0000;
            send_byte(0xFF, PS2_PORT_TWO); // Reset Device 2
            for (uint8_t val = recieve_byte(); val != 0xFF; val = recieve_byte()) {
                PS2_device_two <<= 8;
                PS2_device_two |= (val & 0xFF);
            }
            if (PS2_device_two == 0x0 || PS2_device_two == 0xFAFC) {
                PS2_device_two = 0xFFFF;
            } else {
                PS2_device_two = identify_device(PS2_PORT_TWO);
            }
        }
    }

    send_command(0xAB);
    if (recieve_byte() != 0x0) {
        PS2_port_one = false;
        if (!PS2_port_two) {
            abort();
            return;
        }
    } else {
        send_command(0xAE); // Enable Port 1
        PS2_device_one = 0x0000;
        send_byte(0xFF, PS2_PORT_ONE); // Reset Device 1
        for (uint8_t val = recieve_byte(); val != 0xFF; val = recieve_byte()) {
            PS2_device_one <<= 8;
            PS2_device_one |= (val & 0xFF);
        }
        if (PS2_device_one == 0x0 || PS2_device_one == 0xFAFC) {
            PS2_device_one = 0xFFFF;
        } else {
            PS2_device_one = identify_device(PS2_PORT_ONE);
        }
    }

    config_byte = read_config();
    config_byte |= (PS2_device_one != 0xFFFF ? 0x1 : 0x0) | (PS2_device_two != 0xFFFF ? 0x2 : 0x0); // Enable IRQ's
    write_config(config_byte);
}