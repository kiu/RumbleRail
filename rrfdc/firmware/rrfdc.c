#include "rrfdc.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <util/delay.h>
#include <util/twi.h>

#include "lib/uart/uart.h"
#include "global.h"

const uint8_t FLOPPY_MAX_NOTE = 59;
const uint8_t FLOPPY_MAX_POS = 158;

enum MODE {INIT, IDLE, RUN, OFF};
volatile enum MODE mode = INIT;

volatile uint8_t floppy_pos;

volatile uint8_t led_i = 0;
volatile uint8_t led_r = 0;
volatile uint8_t led_g = 0;
volatile uint8_t led_b = 0;

char out[80];

static const uint16_t note_tick[] = {
/*
import math
tick = float(8000000) / 8
for note in range(0,128):
	freq = pow(2,(float(note)-69)/12) * 440 * 2
	ocr = int(tick / freq)
	print "\t%d, // Note %d" % (ocr, note)
*/
	61156, // Note 0
	57723, // Note 1
	54483, // Note 2
	51425, // Note 3
	48539, // Note 4
	45815, // Note 5
	43243, // Note 6
	40816, // Note 7
	38525, // Note 8
	36363, // Note 9
	34322, // Note 10
	32396, // Note 11
	30578, // Note 12
	28861, // Note 13
	27241, // Note 14
	25712, // Note 15
	24269, // Note 16
	22907, // Note 17
	21621, // Note 18
	20408, // Note 19
	19262, // Note 20
	18181, // Note 21
	17161, // Note 22
	16198, // Note 23
	15289, // Note 24
	14430, // Note 25
	13620, // Note 26
	12856, // Note 27
	12134, // Note 28
	11453, // Note 29
	10810, // Note 30
	10204, // Note 31
	9631, // Note 32
	9090, // Note 33
	8580, // Note 34
	8099, // Note 35
	7644, // Note 36
	7215, // Note 37
	6810, // Note 38
	6428, // Note 39
	6067, // Note 40
	5726, // Note 41
	5405, // Note 42
	5102, // Note 43
	4815, // Note 44
	4545, // Note 45
	4290, // Note 46
	4049, // Note 47
	3822, // Note 48
	3607, // Note 49
	3405, // Note 50
	3214, // Note 51
	3033, // Note 52
	2863, // Note 53
	2702, // Note 54
	2551, // Note 55
	2407, // Note 56
	2272, // Note 57
	2145, // Note 58
	2024, // Note 59
	1911, // Note 60
	1803, // Note 61
	1702, // Note 62
	1607, // Note 63
	1516, // Note 64
	1431, // Note 65
	1351, // Note 66
	1275, // Note 67
	1203, // Note 68
	1136, // Note 69
	1072, // Note 70
	1012, // Note 71
	955, // Note 72
	901, // Note 73
	851, // Note 74
	803, // Note 75
	758, // Note 76
	715, // Note 77
	675, // Note 78
	637, // Note 79
	601, // Note 80
	568, // Note 81
	536, // Note 82
	506, // Note 83
	477, // Note 84
	450, // Note 85
	425, // Note 86
	401, // Note 87
	379, // Note 88
	357, // Note 89
	337, // Note 90
	318, // Note 91
	300, // Note 92
	284, // Note 93
	268, // Note 94
	253, // Note 95
	238, // Note 96
	225, // Note 97
	212, // Note 98
	200, // Note 99
	189, // Note 100
	178, // Note 101
	168, // Note 102
	159, // Note 103
	150, // Note 104
	142, // Note 105
	134, // Note 106
	126, // Note 107
	119, // Note 108
	112, // Note 109
	106, // Note 110
	100, // Note 111
	94, // Note 112
	89, // Note 113
	84, // Note 114
	79, // Note 115
	75, // Note 116
	71, // Note 117
	67, // Note 118
	63, // Note 119
	59, // Note 120
	56, // Note 121
	53, // Note 122
	50, // Note 123
	47, // Note 124
	44, // Note 125
	42, // Note 126
	39, // Note 127

};

static const uint32_t  note_led[] = {
	// red
	0x00FF0000, // C
	0x00FF2222, // C#

	// orange
	0x00FFFF00, // D
	0x00FF6600, // D#

	// purple
	0x00FF00FF, // E

	// blue
	0x000000FF, // F
	0x002222FF, // F#

	// turqoise
	0x0000FFFF, // G
	0x0033DDDD, // G#

	// green
	0x0000FF00, // A
	0x0022FF22, // A#

	// white
	0x00FFFFFF // H
};

void set_led32(uint32_t color) {
	led_r = color >> 16;
	led_g = color >> 8;
	led_b = color >> 0;
}

void cmd(uint8_t cmd) {
	if (cmd == 0xFF) { 
		mode = OFF;
		return;
	}

	if (cmd == 0xFE) { 
		TIMSK |= (1 << OCIE2); // LED Timer On
		cbi(PORTB, PB1); // Enable motor
		mode = RUN;
		return;
	}

	if (cmd == 0xFD) { 
		mode = IDLE;
		set_led32(0x00000000);
		TIMSK &= ~(1 << OCIE1A); // Floppy Timer Off
		TIMSK &= ~(1 << OCIE2); // LED Timer Off
		sbi(PORTD, PD4); sbi(PORTD, PD5); sbi(PORTD, PD7); // LEDs off
		sbi(PORTB, PB1); // Disable motor
		return;
	}

	if (mode != RUN) {
		return;
	}

	// Note off
	if (cmd == 0x80) { 
		TIMSK &= ~(1 << OCIE1A);
		set_led32(0x00000000);
		return;
	}

	// Note on
	if (cmd <= FLOPPY_MAX_NOTE) {
		OCR1A = note_tick[cmd];
		TCNT1 = 0;
		TIMSK |= (1 << OCIE1A);
		set_led32(note_led[cmd % 12]);
		return;
	}
}

ISR(TWI_vect){
	// own address has been acknowledged
	if( (TWSR & 0xF8) == TW_SR_SLA_ACK ){  
		// prepare to receive next byte and acknowledge
		TWCR |= (1<<TWIE) | (1<<TWINT) | (1<<TWEA) | (1<<TWEN); 
	} else if ( (TWSR & 0xF8) == TW_SR_DATA_ACK ){
		// data has been received in slave receiver mode
		uint8_t data = TWDR;
		TWCR |= (1<<TWIE) | (1<<TWINT) | (0<<TWEA) | (1<<TWEN);	
		cmd(data);
	} else {
		// if none of the above apply prepare TWI to be addressed again
		TWCR |= (1<<TWIE) | (1<<TWEA) | (1<<TWEN);
	}
}

ISR(TIMER2_COMP_vect) {
	led_i++;

	if (led_i == 0 && led_r != 0) {
		cbi(PORTD, PD4);
	} 
	if (led_i == 0 && led_g != 0) {
		cbi(PORTD, PD5);
	} 
	if (led_i == 0 && led_b != 0) {
		cbi(PORTD, PD7);
	} 

	if (led_i == led_r) {
		sbi(PORTD, PD4);
	}
	if (led_i == led_g) {
		sbi(PORTD, PD5);
	}
	if (led_i == led_b) {
		sbi(PORTD, PD7);
	}

}

ISR(TIMER1_COMPA_vect) {
	if (floppy_pos == FLOPPY_MAX_POS) {
		sbi(PORTB, PB2); // Direction: Down
	}
	if (floppy_pos == 0) {
		cbi(PORTB, PB2); // Direction: Up
	}

	PORTB & (1 << PB2) ? floppy_pos-- : floppy_pos++;
	PORTB = PINB ^ (1 << PB3);
}

void head_move_start() {
	set_led32(0x00FF0000);
	uart_puts("Moving head to home position...");

	sbi(PORTB, PB2); // Direction: Down
	uint8_t i = FLOPPY_MAX_POS + 10;
	while (i > 0) {
		i--;		
		PORTB = PINB ^ (1 << PB3);
		_delay_ms(5);
	}
	cbi(PORTB, PB2); // Direction: up

	uart_puts("completed.\r\n");
	set_led32(0x0000FF00);
	_delay_ms(200);
	set_led32(0x00000000);
}

int main (void) {
	// Prepare LED port
	sbi(DDRD, PD4); // R
	sbi(PORTD, PD4);
	sbi(DDRD, PD5); // G
	sbi(PORTD, PD5);
	sbi(DDRD, PD7); // B
	sbi(PORTD, PD7);

	// Prepare I2C ADDR port
	PORTA = 0xFF;
	DDRA = 0x00;

	// Prepare FLOPPY port
	sbi(DDRB, PB1); // Motor
	cbi(PORTB, PB1); // Enable motor
	sbi(DDRB, PB2); // Direction
	sbi(DDRB, PB3); // Tick

	sei();

	// LED Timer
	OCR2 = 0x04;
	TCCR2 |= (1 << WGM21); // CTC
	TCCR2 |= (1 << CS22); // Prescaler 64
	TIMSK |= (1 << OCIE2);

	// Floppy Timer
	OCR1A = note_tick[127];
	TCCR1B |= (1 << WGM12); // CTC
	TCCR1B |= (1 << CS11); // Prescaler 8

	uart_init(UART_BAUD_SELECT_DOUBLE_SPEED(UART_BAUD_RATE, F_CPU)); 
	uart_puts("\r\nRRFDC http://schoar.de/tinkering/rumblerail\r\n");

	uint8_t addr = ~PINA & 0xFE;
	TWAR = addr;
	TWCR = (1<<TWIE) | (1<<TWEA) | (1<<TWINT) | (1<<TWEN);

	set_led32(0x000000FF);
	_delay_ms(400);

	sprintf(out, "I2C address is 0x%02x\r\n", addr);
    	uart_puts(out);

	head_move_start();

	uart_puts("Ready to rock!");

	cmd(0xFD); // Idle

	while(mode != OFF) {
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_mode();
	}

	uart_puts("Shutdown...\r\n");

	cli();

	sbi(PORTD, PD4); sbi(PORTD, PD5); sbi(PORTD, PD7); // LEDs off
	sbi(PORTB, PB1); // Disable motor

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_mode();

	return 0;
}
