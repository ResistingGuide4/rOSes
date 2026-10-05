#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>

#include "vga.h"

#define VGA_WIDTH ((size_t)80)
#define VGA_HEIGHT ((size_t)25)
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;
static uint8_t line_length[VGA_HEIGHT];

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_move_cursor(size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(index & 0xFF));

	// Send high byte
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((index >> 8) & 0xFF));
}

void terminal_backspace() {
	if (terminal_column == 0) {
		if (terminal_row == 0) {
			return;
		}
		terminal_row--;
		terminal_column = line_length[terminal_row];
		terminal_move_cursor(terminal_column, terminal_row);
		return;
	}
	terminal_column--;
	terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
	terminal_move_cursor(terminal_column, terminal_row);
}

void terminal_putchar(char c) {
	if (terminal_buffer != (uint16_t *)0xB8000) {
		return;
	}
	if (c == '\n' || c == '\r') {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) {
			terminal_scrollup();
		}
		return;
	}

	unsigned char uc = c;

	terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		line_length[terminal_row]++;
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_scrollup();
	}
	line_length[terminal_row] = terminal_column;
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
	terminal_move_cursor(terminal_column, terminal_row);
}

void terminal_scrollup(void) {
	memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, VGA_WIDTH * (VGA_HEIGHT - 1) * sizeof(uint16_t));
	line_length[0] = 0;
	for (unsigned int i = 0; i < VGA_HEIGHT-1; i++) {
		uint8_t swap = line_length[i];
		line_length[i] = line_length[i + 1];
		line_length[i + 1] = swap;
	}

	for (size_t i = 0; i < VGA_WIDTH; i++) {
		terminal_putentryat(' ', terminal_color, i, VGA_HEIGHT - 1);
	}
	
	terminal_row--;
}

void terminal_eval_unicode(uint8_t c) {
	if (c == '\b') {
		terminal_backspace();
		return;
	}
	terminal_putchar(c);
	terminal_move_cursor(terminal_column, terminal_row);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}
