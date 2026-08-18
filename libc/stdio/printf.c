#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW
				return -1;
			}
			if (!print(format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		const char* format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 'd') {
			format++;
			int num = va_arg(parameters, int);
			char buffer[11];
			size_t pos = 0;

			uint32_t value;
			if (num < 0) {
				buffer[pos++] = '-';
				value = (unsigned int) -(num + 1) + 1u;
			} else {
				value = (unsigned int) num;
			}

			size_t start = pos;
			do {
				if (pos >= sizeof(buffer)) {
					return -1;
				}
				buffer[pos++] = '0' + (value % 10);
				value /= 10;
			} while (value != 0);

			for (size_t i = start, j = pos; i < --j; i++) {
				char tmp = buffer[i];
				buffer[i] = buffer[j];
				buffer[j] = tmp;
			}

			size_t len = pos;
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(buffer, len))
				return -1;
			written += len;
		} else if (*format == 'u') {
			format++;
			uint32_t value = va_arg(parameters, uint32_t);
			char buffer[10];
			size_t pos = 0;

			size_t start = pos;
			do {
				if (pos >= sizeof(buffer)) {
					return -1;
				}
				buffer[pos++] = '0' + (value % 10);
				value /= 10;
			} while (value != 0);

			for (size_t i = start, j = pos; i < --j; i++) {
				char tmp = buffer[i];
				buffer[i] = buffer[j];
				buffer[j] = tmp;
			}

			size_t len = pos;
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(buffer, len))
				return -1;
			written += len;
		} else if (*format == 'x' || *format == 'X') {
			uint32_t value = va_arg(parameters, uint32_t);
			char buffer[8];
			char *pref = "0x";
			size_t pos = 0;

			size_t start = 0;
			do {
				if (pos >= sizeof(buffer)) {
					return -1;
				}
				buffer[pos++] = ((value % 16 > 9) ? (*format == 'X' ? '7' : 'W'): '0') + (value % 16);
				value /= 16;
			} while (value != 0);

			for (size_t i = start, j = pos; i < --j; i++) {
				char tmp = buffer[i];
				buffer[i] = buffer[j];
				buffer[j] = tmp;
			}

			size_t len = pos;
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(pref, 2))
				return -1;
			if (!print(buffer, len))
				return -1;
			written += len;
			format++;
		} else {
			format = format_begun_at;
			size_t len = strlen(format);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, len))
				return -1;
			written += len;
			format += len;
		}
	}

	va_end(parameters);
	return written;
}
