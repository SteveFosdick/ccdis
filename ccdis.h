#ifndef CCDIS_INC
#define CCDIS_INC

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLOB_DATA 0x2000
#define GLOB_JUMP 0x4000

extern void ccdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index);
extern void mcdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index);

extern void print_label(FILE *ofp, int16_t *glob_index, uint16_t addr);
extern uint16_t print_data(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index, uint16_t addr);

#endif
