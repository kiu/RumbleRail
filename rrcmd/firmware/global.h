#ifndef GLOBAL_H
#define GLOBAL_H

#include <avr/io.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define F_CPU 20000000UL
#define UART_BAUD_RATE 9600

#define MAX_TRACKS 32
#define MAX_FLOPPIES 112
#define FILES_PER_PAGE 16

#define LCD_PWM 128;

//#define DEBUG

char out[80];

#ifndef cbi
	#define cbi(sfr, bit)     (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
	#define sbi(sfr, bit)     (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#endif
