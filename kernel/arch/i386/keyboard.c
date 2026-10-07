#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/thread.h>

#include <stdio.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %w1, %b0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
}

/*
    This table translates PS/2 Scan Set 1 into keycodes
    First 3 bits represent the row and last 5 bits represent the index in row of each key on the Dell Inspiron 1550 keyboard
    Codes for function key alternatives range from 0x13 - 0x1F (Includes Menu Key on R. Ctrl)
    Not Present Keys (Row 6): Scroll Lock, Prev. Track, Next Track, Multimedia Stop, WWW Home, Right GUI, Sleep, Wake, WWW Search, WWW Favorites, WWW Refresh, WWW Stop, WWW Forward, WWW Back, My Computer, Email, Media Select, Pause/Break
*/
uint8_t keycodes[] = {
    0x00, 0x00, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x40,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x6C, 0xA0, 0x61, 0x62,
    0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x20, 0x80, 0x4D, 0x81, 0x82, 0x83, 0x84,
    0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x30, 0xA3, 0xA4, 0x60, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x2E, 0xC0, 0x4E, 0x4F, 0x50, 0x31, 0x6D, 0x6E, 0x6F, 0x51, 0x8C,
    0x8D, 0x8E, 0xAD, 0xAE, 0x00, 0x00, 0x00, 0x0B, 0x0C, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC2, 0x00, 0x00, 0x8F, 0xA6, 0x00, 0x00, 0x13, 0x0F, 0x16, 0x00, 0xC3, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x14, 0x00, 0x15, 0x00, 0xC4, 0x00, 0x00, 0x2F, 0x00,
    0x1C, 0xA5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD1, 0x00,
    0x1D, 0xA9, 0xA7, 0x00, 0xA8, 0x00, 0xAC, 0x00, 0x1E, 0xAA, 0xAB, 0x0D, 0x0E, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xA2, 0xC5, 0x1F, 0x12, 0xC6, 0x00, 0x00, 0x00, 0xC7, 0x00, 0xC8, 0xC9,
    0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0
};

uint8_t us_querty[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    '`',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b', 0x00, '/',  '*',  '-',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\\', '7',  '8',  '9',  '+',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 'a',  's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '\r', '4',  '5',  '6',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 'z',  'x',  'c',  'v',  'b',  'n',  'm',  ',',  '.',  '/',  0x00, '1',  '2',  '3',  '\r', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, ' ',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '0',  '.',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Begins at unicode 0x28
uint8_t us_querty_shift[] = {
    '"', '(', ')', '+', '<', '_', '>', '?',
    ')', '!', '@', '#', '$', '%', '^', '&', '*', '(', ':', ':', '<', '=', '>', '?',
    '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '^', '_',
    '~', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '~', 0x7f
};

PS2_port_t PS2_port = PS2_PORT_ONE;
static uint8_t extended = 0;
uint8_t press_keys = 0; // Bits: 0 - Reserved, 1 - L. Ctrl, 2 - L. Alt, 3 - L. Shift, 4 - R. Ctrl, 5 - R. Alt, 6 - R. Shift
uint8_t toggle_keys = 0; // Bits: 0 - Scroll Lock, 1 - Num Lock, 2 - Caps Lock

static uint16_t keycode_stack[16];
static uint16_t *keycode_stack_top = keycode_stack - 1;

// Currently PS2 Only
static uint16_t scan_to_key() {
    uint8_t scan_code = inb(0x60);
    bool released = false; // Flag
    if ((scan_code & 0x80) == 0x80) {
        released = true;
    } else {
        released = false;
    }
    if (extended > 1) {
        extended--;
        return 0xFF;
    }
    if (extended == 0) {
        if (scan_code == 0xE1) {
            extended = 5;
            return 0xFF;
        } else if (scan_code == 0xE0) {
            extended = 1;
            return 0xFF;
        }
        scan_code &= ~0x80;
    } else if (extended == 1) {
        scan_code &= ~0x80;
        scan_code += 0x49;
        extended--;
    }
    
    return (uint16_t)released << 8 | (uint16_t)keycodes[scan_code];
}

void keyboard_init(PS2_port_t new_port) {
    PS2_port = new_port;
    for (int i = 0; i < 3; i++) {
        send_byte(0xF0, new_port);
        send_byte(0x02, new_port); // Set Keyboard to Scan Code Two
        if (recieve_byte() == 0xFA) {
            break;
        }
    }
    for (int i = 0; i < 3; i++) {
        send_byte(0xED, new_port);
        send_byte(0x00, new_port); // Disable Lock Key Lights
        if (recieve_byte() == 0xFA) {
            break;
        }
    }
}

void keyboard_update_leds() {
    for (int i = 0; i < 3; i++) {
        send_byte(0xED, PS2_PORT_TWO);
        send_byte(toggle_keys, PS2_PORT_TWO);
        if (recieve_byte() == 0xFA) {
            break;
        }
    }
}

/*  The value sent by this function indicates as follows:
    Bits 0-7:               The keycode of the keypress
    Bits 8-15:              The unicode value of the keypress if it has one
    Bit  16:                0 if released; 1 if pressed
    Bits 17-22 (ascending): L. Ctrl, L. Alt, L. Shift, R. Ctrl, R. Alt, R. Shift
    Bit  23:                Reserved
    Bits 24-26 (ascending): Caps Lock, Scroll Lock, Num Lock
    Bits 27-31:             Reserved
*/

uint32_t PS2_key_handler() {
    uint16_t keycode = scan_to_key();

    if (keycode == 0xFF) {
        return 0xFFFFFFFF;
    }

    keycode_stack_top++;
    *keycode_stack_top = keycode;
}

uint32_t keycode_handler(void) {
    while (true) {
        while (keycode_stack_top < keycode_stack) {
            switch_thread(false);
        }
        
        uint8_t keycode = (uint8_t)(*keycode_stack_top);
        bool released = (bool)((*keycode_stack_top) >> 8) & 0xFF;
        keycode_stack_top--;

        uint8_t unicode = us_querty[keycode];
        if ((toggle_keys & 0x4) && unicode >= 0x61) {
            unicode = us_querty_shift[unicode - 0x28];
        }
        if ((press_keys & 0x8 || press_keys & 0x40) && unicode >= 0x28) {
            unicode = us_querty_shift[unicode - 0x28];
        }

        if ((press_keys & 0x2 || press_keys & 0x10)) {
            if (unicode >= 'a' && unicode <= '}') {
                unicode -= 0x20;
            }
            if (unicode >= 0x40 && unicode < 0x60) {
                unicode &= 0x1F;
            }
        }

        if (
            (toggle_keys & 0x2) == 0 && 
            keycode > 0x40 && keycode < 0xC0 && 
            /*Is it a keypad num*/(((unsigned)(0x12 - keycode/32 - keycode % 32) < 3) || (keycode == 0xAE))
        ) {
            unicode = 0;
        }

        if (released) {
            if (keycode == 0xA0) {
                press_keys &= ~0x2;
            } else if (keycode == 0xA3) {
                press_keys &= ~0x4;
            } else if (keycode == 0x80) {
                press_keys &= ~0x8;
            } else if (keycode == 0xA6) {
                press_keys &= ~0x10;
            } else if (keycode == 0xA5) {
                press_keys &= ~0x20;
            } else if (keycode == 0x8B) {
                press_keys &= ~0x40;
            }
        } else {
            if (keycode == 0xA0) {
                press_keys |= 0x2;
            } else if (keycode == 0xA3) {
                press_keys |= 0x4;
            } else if (keycode == 0x80) {
                press_keys |= 0x8;
            } else if (keycode == 0xA6) {
                press_keys |= 0x10;
            } else if (keycode == 0xA5) {
                press_keys |= 0x20;
            } else if (keycode == 0x8B) {
                press_keys |= 0x40;
            } else if (keycode == 0xC0) {
                toggle_keys ^= 0x1;
                keyboard_update_leds();
            } else if (keycode == 0x2E) {
                toggle_keys ^= 0x2;
                keyboard_update_leds();
            } else if (keycode == 0x60) {
                toggle_keys ^= 0x4;
                keyboard_update_leds();
            } else if (unicode != 0x0) {
                terminal_eval_unicode(unicode);
            }
        }

        //return ((uint32_t)toggle_keys & 0x7) << 24 | ((uint32_t)press_keys & 0xFE) << 16 | ((uint32_t)released & 0x1) << 16 | ((uint32_t)unicode & 0xFF) << 8 | (uint32_t)keycode & 0xFF;
    }
}