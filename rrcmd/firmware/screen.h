#ifndef SCREEN_H
#define SCREEN_H

#include "global.h"

enum SCREEN {SCR_BOOT, SCR_DISCOVER, SCR_DISCOVER_RESULT, SCR_INSERT, SCR_SCANNING, SCR_SELECT, SCR_PLAY};

void screen_init(void);
void screen_draw(uint8_t screen);

#endif
