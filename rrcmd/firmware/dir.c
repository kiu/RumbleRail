#include "dir.h"

uint16_t file_count = 0;
char *files[FILES_PER_PAGE];

FATFS fat;

DIR dir;
FILINFO fi;
FILINFO fi_sel;
FRESULT ferr;

uint8_t page = 0;
uint8_t pip = 0; // position in page


uint8_t select_update(void) {
	ferr = f_opendir(&dir, "/");
	if (ferr != FR_OK) {
		return ferr;
	}

	for (uint8_t x = 0; x < page * FILES_PER_PAGE; x++) {
		ferr = f_readdir(&dir, &fi);
		if (ferr != FR_OK) {
			return ferr;
		}	
	}

	for (uint8_t x = 0; x < FILES_PER_PAGE; x++) {
		ferr = f_readdir(&dir, &fi);
		if (ferr != FR_OK) {
			return ferr;
		}

		free(files[x]);
		files[x] = malloc(strlen(fi.fname) + 1);
		strcpy(files[x], fi.fname);
					
		if (x == pip) {
			fi_sel = fi;
		}
	}

	return 0;
}

char* dir_get_file_name(uint8_t idx) {
	return files[idx];
}

uint8_t dir_get_file_idx() {
	return pip;
}

FILINFO dir_get_file() {
	select_update();
	return fi_sel;
}


uint8_t dir_file_count(uint16_t *count) {
	*count = 0;
	ferr = f_opendir(&dir, "/");
	if (ferr != FR_OK) {
		return ferr;
	}

	uint16_t c = 0;
	do {
		ferr = f_readdir(&dir, &fi);
		if (ferr != FR_OK) {
			return ferr;
		}
		c++;
	} while(fi.fname[0] != 0);

	*count = c - 1;

	return 0;
}

// --------------------------------------------------------

uint8_t dir_prev() {
	if (pip == 0) {
		if (page != 0) {
			pip = FILES_PER_PAGE - 1;
			page--;

			ferr = select_update();
			if (ferr != FR_OK) {
				return ferr;
			}
		}				
	} else {
		pip--;
	}

	return 0;
}

uint8_t dir_next() {
	if (pip + 1 == FILES_PER_PAGE) {
		if (page + 1 * FILES_PER_PAGE < file_count) {
			pip = 0;
			page++;

			ferr = select_update();
			if (ferr != FR_OK) {
				return ferr;
			}
		}				
	} else {
		if (page * FILES_PER_PAGE + pip + 1 < file_count) {
			pip++;
		}
	}

	return 0;
}

// --------------------------------------------------------

uint8_t dir_init() {
	return disk_initialize(0);
}

uint8_t dir_umount() {
	file_count = 0;
	page = 0;
	pip = 0;
	
	//ferr = f_mount(0, NULL);

	return 0;
}

uint8_t dir_mount() {
	dir_umount();
	
	ferr = f_mount(0, &fat);
	if (ferr != FR_OK) {
		return ferr;
	}

	ferr = dir_file_count(&file_count);
	if (ferr != FR_OK) {
		return ferr;
	}
	sprintf(out, "Found %d files\r\n", file_count);
	uart_puts(out);

	select_update();
	return 0;
}

