#include "rrcmd.h"


#include "midi.h"
#include "dir.h"
#include "screen.h"
//#include "screen_dummy.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lib/i2c/i2cmaster.h"
#include "lib/uart/uart.h"

uint8_t floppy_count = 0;
uint8_t floppy_addr[MAX_FLOPPIES];

volatile uint8_t btn_state = 0;
volatile uint8_t e_btn = 0;
volatile uint8_t e_sd = 0;

unsigned char i2c_ret;
uint8_t ferr;

// ---------------------------------------

uint8_t get_floppy_count() {
	return floppy_count;
}

// ---------------------------------------

uint8_t sendI2C_addr(uint8_t addr, uint8_t cmd) {
#ifdef DEBUG
	sprintf(out, "S: %02x %02x\r\n", addr, cmd);
	uart_puts(out);
#endif

	i2c_ret = i2c_start(addr + I2C_WRITE);

	if (i2c_ret == 0) {
		i2c_ret = i2c_write(cmd);
	}

	i2c_stop();
	return i2c_ret;
}

uint8_t sendI2C_id(uint8_t id, uint8_t cmd) {
	if (id >= floppy_count) {
		return 1;
	}
	return sendI2C_addr(floppy_addr[id], cmd);
}

void scanI2C(void) {
	uart_puts("I2C device discovery:");

	for (uint8_t addr = 2; addr != 0xFE; addr += 2) {
		if (sendI2C_addr(addr, 0x80) == 0) {
			floppy_addr[floppy_count] = addr;
			floppy_count++;
			sprintf(out, " %02x", addr);
			uart_puts(out);
		}
	}
	uart_puts("\r\n");
}

// ---------------------------------------

void song_stop(void) {
	TIMSK1 &= ~(1 << OCIE1A);
	for (uint8_t i = 0; i < floppy_count; i++) {
		sendI2C_id(i, 0xFD);	
	}
}

void song_start(void) {
	for (uint8_t i = 0; i < floppy_count; i++) {
		sendI2C_id(i, 0xFE);	
	}

	FILINFO fiplay = dir_get_file();
	ferr = midi_init(fiplay);
	if (ferr != FR_OK) {
		song_stop();
		return;
	}
	
	screen_draw(SCR_PLAY);

	TIMSK1 |= (1 << OCIE1A);
}

// ---------------------------------------

ISR(TIMER1_COMPA_vect) {
	if (midi_tick()) {
		song_stop();
	}
}

ISR (PCINT0_vect) {
	if (PINA & (1 << PA1)) {
		e_sd = 1; // SD inserted
	} else {
		e_sd = 2; // SD removed
	}
}

ISR (PCINT2_vect) {
	uint8_t pin = PINC;
	uint8_t lft = pin & (1<<PC7);
	uint8_t act = pin & (1<<PC6);
	uint8_t rgt = pin & (1<<PC5);

	if (!lft && !rgt) {
		btn_state = 0;
	}
	if (btn_state) {
		return;
	}

	if (act) {
		e_btn = 1;
	}

	if (lft && !rgt) {
		btn_state = 1;
		e_btn = 2;
	}


	if (rgt && !lft) {
		btn_state = 1;
		e_btn = 3;
	}

}

// ---------------------------------------

int main (void) {
	// Prepare PWR
	sbi(DDRA, PA0); // EN_3V3
	sbi(PORTA, PA0);

	// Prepare SPI
	sbi(DDRB, PB7);  // SCK
	cbi(PORTB, PB6); // DI
	cbi(DDRB, PB6);  // DI
	sbi(DDRB, PB5);  // DO
	sbi(DDRB, PB4);  // SS

	//SPCR = (1<<MSTR) | (1<<SPR1) | (1<<SPR0) | (1<<SPE); // clk/64
	//SPSR |= (1<<SPI2X);

	// Prepare display
	sbi(DDRA, PA5);  // A0
	sbi(DDRA, PA4);  // Reset
	sbi(DDRA, PA3);  // CS
	sbi(DDRD, PD7);  // Backlight

	// Prepare MMC
	sbi(DDRA, PA2); // CS
	cbi(PORTA, PA1); // CD
	cbi(DDRA, PA1); // CD
	PCMSK0 |= (1 << PCINT1);
	PCICR |= (1 << PCIE0);

	// Prepare buttons
	cbi(PORTC, PC5); // Previous
	cbi(DDRC, PC5);	
	PCMSK2 |= (1 << PCINT21);

	cbi(PORTC, PC6); // Action
	cbi(DDRC, PC6);
	PCMSK2 |= (1 << PCINT22);

	cbi(PORTC, PC7); // Next
	cbi(DDRC, PC7);
	PCMSK2 |= (1 << PCINT23);

	PCICR |= (1 << PCIE2);

	sei();

	// Init backlight: Fast PWM, clear, clk/8
	OCR2A = LCD_PWM;
	TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20);
	TCCR2B = (1 << CS21);
	sbi(PORTD, PD7); // Backlight

	OCR1A = 0x00A2;
	TCCR1B |= (1 << WGM12); // CTC
	TCCR1B |= (1 << CS12) | (1 << CS10); // Prescaler 1024

	uart_init(UART_BAUD_SELECT_DOUBLE_SPEED(UART_BAUD_RATE, F_CPU)); 
	uart_puts("\r\nfdsp http://schoar.de/tinkering/xxx\r\n");

	screen_init();
	_delay_ms(2000);

	screen_draw(SCR_BOOT);
	_delay_ms(2000);

	screen_draw(SCR_DISCOVER);
	_delay_ms(2000);

	i2c_init();
	scanI2C();

	screen_draw(SCR_DISCOVER_RESULT);
	_delay_ms(2000);

	e_sd = 1;

	while (1) {

		if (e_sd != 0) {
			if (e_sd == 1) {
				if (dir_init() == 0 && dir_mount() == 0) {
					screen_draw(SCR_SELECT);
					uart_puts("SD Card inserted\r\n");
				} else {
					e_sd = 2;
				}
			}
			if (e_sd == 2) {
				song_stop();
				dir_umount();
				screen_draw(SCR_INSERT);
				uart_puts("Insert SD Card\r\n");
			}
			_delay_ms(20);
			e_sd = 0;
		}

		if (e_btn != 0) {
			if (e_btn == 1) {
				song_stop();				

				song_start();
			}
			if (e_btn == 2) {
				song_stop();
				dir_prev();
				screen_draw(SCR_SELECT);
			} 
			if (e_btn == 3) {
				song_stop();
				dir_next();
				screen_draw(SCR_SELECT);
			}
			e_btn = 0;
		}

	}

	uart_puts("Shutdown...\r\n");

	for (uint8_t i = 0; i < floppy_count; i++) {
		sendI2C_id(i, 0xFF);	
	}

	cbi(PORTD, PD7); // Backlight Off
	cbi(PORTA, PA0); // 3v3 off

	cli();

	return 0;
}

