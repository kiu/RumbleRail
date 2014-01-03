#include "screen_dummy.h"

#include "dir.h"

int freeRam() {
	extern int __heap_start, *__brkval; 
	int v; 
	return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void print_memory() {
	sprintf(out, "Mem: %04d\r\n", freeRam());
	uart_puts(out);
}

void screen_draw(uint8_t screen) {
	sprintf(out, "SCREEN draw: %02d\r\n", screen);
	uart_puts(out);

	print_memory();

	if (screen == SCR_SELECT) {
		uint8_t match = dir_get_file_idx();
		for (uint8_t i; i < FILES_PER_PAGE; i++) {
			if (match == i) {
				sprintf(out, "A: %s\r\n", dir_get_file_name(i));
			} else {
				sprintf(out, "S: %s\r\n", dir_get_file_name(i));
			}
			uart_puts(out);
		}
	}
}

void screen_init() {
	uart_puts("SCREEN init\r\n");
	print_memory();
}

