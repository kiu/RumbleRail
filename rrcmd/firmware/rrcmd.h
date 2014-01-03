#ifndef RRCMD_H
#define RRCMD_H

#include "global.h"

uint8_t get_floppy_count(void);
uint8_t sendI2C_id(uint8_t id, uint8_t cmd);

#endif
