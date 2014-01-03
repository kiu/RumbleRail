#ifndef DIR_H
#define DIR_H

#include "global.h"
#include "lib/mmc/ff.h"

uint8_t dir_init(void);
uint8_t dir_mount(void);
uint8_t dir_umount(void);

FILINFO dir_get_file(void);

uint8_t dir_get_file_idx(void);
char* dir_get_file_name(uint8_t idx);

uint8_t dir_next(void);
uint8_t dir_prev(void);

#endif
