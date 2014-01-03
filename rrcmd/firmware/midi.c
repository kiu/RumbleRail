#include "midi.h"

#include "rrcmd.h"

const uint8_t FLOPPY_MAX_NOTE = 59;

FIL f;
FRESULT ferr;
UINT re;

uint32_t offset;

uint16_t midi_format_type;
uint16_t midi_track_count;
uint16_t midi_time_division;

uint32_t midi_event_pos[MAX_TRACKS];
uint8_t midi_running_status_type[MAX_TRACKS];
uint8_t midi_running_status_channel[MAX_TRACKS];

uint32_t buf_tick[MAX_TRACKS];
uint32_t buf_cmd[MAX_TRACKS];

uint8_t note_last[MAX_FLOPPIES];
uint8_t floppy_count;

char* row1;
char* row2;
char* row3;

// ---------------------------------------------------------

uint8_t seek(uint32_t pos) {
	ferr = f_lseek(&f, pos);
	offset = pos;
	return ferr;
}

uint8_t readDWord(uint32_t *val) {
	ferr = f_read(&f, val, 4, &re);
	offset += 4;
	return ferr;
}

uint8_t readWord(uint16_t *val) {
	ferr = f_read(&f, val, 2, &re);
	offset += 2;
	return ferr;
}

uint8_t readByte(uint8_t *val) {
	ferr = f_read(&f, val, 1, &re);
	offset += 1;
	return ferr;
}

uint8_t parse_variable_length(uint32_t *len) {
	uint8_t tmp = 0;
	uint32_t value = 0;
	do {
		ferr = readByte(&tmp);
		if (ferr != FR_OK) {
			return ferr;
		}
		value = (value << 7) + (tmp & 0x7F);
	} while (tmp & 0x80);
	*len = value;
	return 0;
}

// ---------------------------------------------------------

uint16_t swap_uint16(uint16_t val) {
	uint16_t in = val;
	return (in << 8) | (in >> 8);
}

int16_t swap_int16(int16_t val) {
	int16_t in = val;
	return (in << 8) | ((in >> 8) & 0xFF);
}

uint32_t swap_uint32(uint32_t val) {
	uint32_t in = val;
	in = ((in << 8) & 0xFF00FF00) | ((in >> 8) & 0xFF00FF); 
	return (in << 16) | (in >> 16);
}

int32_t swap_int32(int32_t val) {
	int32_t in = val;
	in = ((in << 8) & 0xFF00FF00) | ((in >> 8) & 0xFF00FF); 
	return (in << 16) | ((in >> 16) & 0xFFFF);
}

// ---------------------------------------------------------

uint16_t midi_get_track_count(void) {
	return midi_track_count;
}

uint8_t midi_is_active(uint8_t track) {
	return midi_event_pos[track] != 0;
}

uint16_t midi_time_base(uint32_t mpqn) {
	double ms = (double)mpqn / (double)midi_time_division;
	double tick =  51.2; // 1 / (20.0 / 1024.0)
	uint16_t timer = (uint16_t)(ms / tick);
	return timer;
}

char* midi_get_row1() {
	return row1;
}
char* midi_get_row2() {
	return row2;
}
char* midi_get_row3() {
	return row3;
}

// ---------------------------------------------------------

uint8_t midi_parse_head(void) {
	ferr = seek(0x08);
	if (ferr != FR_OK) {
		return ferr;
	}

	ferr = readWord(&midi_format_type);
	if (ferr != FR_OK) {
		return ferr;
	}
	midi_format_type = swap_uint16(midi_format_type);

	ferr = readWord(&midi_track_count);
	if (ferr != FR_OK) {
		return ferr;
	}
	midi_track_count = swap_uint16(midi_track_count);
	if (midi_track_count > MAX_TRACKS) {
		midi_track_count = MAX_TRACKS;
	}

	ferr = readWord(&midi_time_division);
	if (ferr != FR_OK) {
		return ferr;
	}
	midi_time_division = swap_uint16(midi_time_division);

	return 0;
}

uint8_t midi_parse_tracks(void) {
	ferr = seek(0x0E);
	if (ferr != FR_OK) {
		return ferr;
	}

	uint32_t track_size = 0;

	for (uint16_t track = 1; track <= midi_track_count; track++) {
		ferr = seek(offset + 4);
		if (ferr != FR_OK) {
			return ferr;
		}

		ferr = readDWord(&track_size);
		if (ferr != FR_OK) {
			return ferr;
		}
		track_size = swap_uint32(track_size);

		midi_event_pos[track] = offset;

		sprintf(out, "Track: #%02x Size %04lx Offset %04lx\r\n", track, track_size, offset);
		uart_puts(out);

		offset += track_size;
		seek(offset);
	}

	return 0;
}

// ---------------------------------------------------------

uint8_t midi_next(uint8_t track, uint32_t *tick, uint32_t *cmd) {
	ferr = seek(midi_event_pos[track]);
	if (ferr != FR_OK) {
		return ferr;
	}
	
	uint32_t event_time = 0;
	uint8_t event_type = 0;

	ferr = parse_variable_length(&event_time);
	if (ferr != FR_OK) {
		return ferr;
	}
	*tick = event_time;
	*cmd = 0;
	
	ferr = readByte(&event_type);
	if (ferr != FR_OK) {
		return ferr;
	}

	uint32_t tmp = 0;

	switch (event_type) {

		case 0xFF: // Meta Event
			;
			uint8_t meta_type = 0;
			uint32_t meta_length = 0;
			//ignore meta_data

			ferr = readByte(&meta_type);
			if (ferr != FR_OK) {
				return ferr;
			}

			ferr = parse_variable_length(&meta_length);
			if (ferr != FR_OK) {
				return ferr;
			}

			if (meta_type == 0x51) { // Tempo
				tmp = 0x51000000;

				uint8_t rrr;
				ferr = readByte(&rrr);
				if (ferr != FR_OK) {
					return ferr;
				}
				tmp |= ((uint32_t)rrr) << 16;

				ferr = readByte(&rrr);
				if (ferr != FR_OK) {
					return ferr;
				}
				tmp |= ((uint32_t)rrr) << 8;

				ferr = readByte(&rrr);
				if (ferr != FR_OK) {
					return ferr;
				}
				tmp |= ((uint32_t)rrr);

				*cmd = tmp;
			} else {
				offset += meta_length;
				seek(offset);
			}

			midi_event_pos[track] = offset;

			if (meta_type == 0x2F) { // End of Track
				midi_event_pos[track] = 0;
			}

			break;

		case 0xF0: // System Exclusive
		case 0xF7: // System Exclusive
			;
			uint32_t sys_length = 0;
			//ignore sys_data

			ferr = parse_variable_length(&sys_length);
			if (ferr != FR_OK) {
				return ferr;
			}

			offset += sys_length;
			ferr = seek(offset);
			if (ferr != FR_OK) {
				return ferr;
			}

			midi_event_pos[track] = offset;

			break;

		default: // Channel Event			
			;
			uint8_t channel_type = 0;
			uint8_t channel_channel = 0;
			uint8_t channel_param1 = 0;
			uint8_t channel_param2 = 0;		

			if (event_type & 0x80) {
				// New status
				channel_type = event_type >> 4;
				channel_channel = event_type & 0x0F;
				midi_running_status_type[track] = channel_type;
				midi_running_status_channel[track] = channel_channel;
				
				ferr = readByte(&channel_param1);
				if (ferr != FR_OK) {
					return ferr;
				}

			} else {
				// Running status
				channel_type = midi_running_status_type[track];
				channel_channel = midi_running_status_channel[track];
				
				channel_param1 = event_type;
			}

			if (channel_type != 0x0C && channel_type != 0x0D) {
				ferr = readByte(&channel_param2);
				if (ferr != FR_OK) {
					return ferr;
				}
			}

			midi_event_pos[track] = offset;
			
			if (channel_type == 0x09 && channel_param2 == 0) {
				// Zero velocity means Note off
				channel_type = 0x08;
			}

			if (channel_type == 0x08 || channel_type == 0x09) {
				tmp = ((uint32_t)channel_type) << 24;
				tmp |= ((uint32_t)channel_param1) << 8;
				tmp |= ((uint32_t)channel_channel);
				*cmd = tmp;
			}

			break;
	}

	return 0;
}
void exec_cmd_single(uint8_t cmd, uint8_t note) {
	if (note > FLOPPY_MAX_NOTE) {
		return;
	}

	if (cmd == 0x08) {
		for (uint8_t floppy = 0; floppy < MAX_FLOPPIES; floppy++) {
			if (note_last[floppy] == note) {
				note_last[floppy] = 0x80;
				sendI2C_id(floppy, 0x80);
				return;
			}
		}
	}
	if (cmd == 0x09) {
		for (uint8_t floppy = 0; floppy < MAX_FLOPPIES; floppy++) {
			if (note_last[floppy] == 0x80) {
				note_last[floppy] = note;
				sendI2C_id(floppy, note);
				return;
			}
		}
	}
}

void exec_cmd_note(uint8_t cmd, uint8_t note, uint8_t idx) {
	if (note > FLOPPY_MAX_NOTE) {
		return;
	}

	if (idx >= floppy_count) {
		return;
	}

	if (cmd == 0x08) {
		if (note_last[idx] == note) {
			note_last[idx] = 0x80;
			sendI2C_id(idx, 0x80);
		}
	}
	if (cmd == 0x09) {
		note_last[idx] = note;
		sendI2C_id(idx, note);
	}
}

uint8_t midi_tick() {
	uint8_t active = 1;

	uint8_t cmd;
	uint8_t p1;
	uint8_t p2;
	uint8_t p3;

	for (uint8_t track = 1; track <= midi_get_track_count(); track++) {
		if (midi_is_active(track)) {
			active = 0;
		}

		while (midi_is_active(track) && buf_tick[track] == 0) {
			cmd = buf_cmd[track] >> 24;	// Note On/Off, Tempo
			p1 = buf_cmd[track] >> 16;	// -
			p2 = buf_cmd[track] >> 8;	// Note
			p3 = buf_cmd[track];		// Channel

			if (cmd == 0x51) {
				OCR1A = midi_time_base(buf_cmd[track] & 0x00FFFFFF);
			}

			if (cmd == 0x08 || cmd == 0x09) {
				switch (play_type) {

					case PM_SINGLE:
						exec_cmd_single(cmd, p2);
						break;

					case PM_TRACK_MOD:
						exec_cmd_note(cmd, p2, track % floppy_count);
						break;

					case PM_TRACK:
						exec_cmd_note(cmd, p2, track);
						break;
						
					case PM_CHANNEL_MOD:
						exec_cmd_note(cmd, p2, p3 % floppy_count);
						break;

					case PM_CHANNEL:
					default:
						exec_cmd_note(cmd, p2, p3);
						break;

				 }	
			}

			if (midi_next(track, &buf_tick[track], &buf_cmd[track])) {
				midi_event_pos[track] = 0;
			}
		}

		buf_tick[track]--;
	}

	return active;
}

// ---------------------------------------------------------

uint8_t midi_init(FILINFO fi) {
	f_close(&f);
	floppy_count = get_floppy_count();

	for (uint8_t track = 0; track < MAX_TRACKS; track++) {
		buf_tick[track] = 0;
		buf_cmd[track] = 0;
	}

	for (uint8_t floppy = 0; floppy < MAX_FLOPPIES; floppy++) {
		note_last[floppy] = 0x80;
	}

	ferr = f_open(&f, fi.fname, FA_READ);
	sprintf(out, "Open: %02x %s\r\n", ferr, fi.fname);
	uart_puts(out);
	if (ferr != FR_OK) {
		return ferr;
	}

	switch (fi.fname[strlen(fi.fname) - 1]) {
		case 'X':
			play_type = PM_TRACK_MOD;
			break;
		case 'M':
			play_type = PM_CHANNEL_MOD;
			break;
		case 'T':
			play_type = PM_TRACK;
			break;
		case 'C':
			play_type = PM_CHANNEL;
			break;
		case 'S':
			play_type = PM_SINGLE;
			break;
		default:
			play_type = PM_SINGLE;
			break;
	}
	
	sprintf(out, "Details: Size %4lx Play Mode %02x\r\n", f_size(&f), play_type);
	uart_puts(out);

	ferr = midi_parse_head();
	sprintf(out, "Header: Type %02x Tracks %02x Time %02x\r\n", midi_format_type, midi_track_count, midi_time_division);
	uart_puts(out);

	sprintf(out, "  Play Mode %02x", play_type);
	free(row1);
	row1 = malloc(strlen(out) + 1);
	strcpy(row1, out);

	sprintf(out, "Size %04lx Type %02x", f_size(&f), midi_format_type);
	free(row2);
	row2 = malloc(strlen(out) + 1);
	strcpy(row2, out);

	sprintf(out, "Tracks %02x Time %02x", midi_track_count, midi_time_division);
	free(row3);
	row3 = malloc(strlen(out) + 1);
	strcpy(row3, out);

	if (ferr != FR_OK) {
		return ferr;
	}

	ferr = midi_parse_tracks();
	if (ferr != FR_OK) {
		return ferr;
	}

	OCR1A = midi_time_base(500000);

	return 0;
}

