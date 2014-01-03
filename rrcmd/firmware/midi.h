#ifndef MIDI_H
#define MIDI_H

#include "global.h"
#include "lib/mmc/ff.h"

enum PLAY_TYPE {PM_TRACK, PM_TRACK_MOD, PM_CHANNEL, PM_CHANNEL_MOD, PM_SINGLE};
enum PLAY_TYPE play_type;

uint8_t midi_init(FILINFO tfi);
uint8_t midi_tick(void);

char *midi_get_row1(void);
char *midi_get_row2(void);
char *midi_get_row3(void);

#endif
