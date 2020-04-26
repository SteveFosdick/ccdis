#ifndef CCDIS_INC
#define CCDIS_INC

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE 0x10000

#define GLOB_CINTCODE 0x2000
#define GLOB_MC6502   0x2001
#define GLOB_DATA     0x2002

extern size_t mc_trace(const unsigned char *content, size_t size, int16_t *glob_index, size_t addr, int *new_labels);
extern size_t cc_trace(const unsigned char *content, size_t size, int16_t *glob_index, size_t addr, int *new_labels);
extern size_t mc_disassemble(FILE *ofp, const unsigned char *content, size_t size, int16_t *glob_index, size_t addr);
extern size_t cc_disassemble(FILE *ofp, const unsigned char *content, size_t size, int16_t *glob_index, size_t addr);

extern void print_label(FILE *ofp, int16_t *glob_index, uint16_t addr);
extern uint16_t print_data(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index, uint16_t addr);

#endif
