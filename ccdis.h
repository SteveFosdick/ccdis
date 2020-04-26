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

extern unsigned mc_trace(const unsigned char *content, unsigned size, int16_t *glob_index, unsigned base_addr, unsigned addr, int *new_labels);
extern unsigned cc_trace(const unsigned char *content, unsigned size, int16_t *glob_index, unsigned addr, int *new_labels);
extern unsigned mc_disassemble(FILE *ofp, const unsigned char *content, unsigned size, int16_t *glob_index, unsigned base_addr, unsigned addr);
extern unsigned cc_disassemble(FILE *ofp, const unsigned char *content, unsigned size, int16_t *glob_index, unsigned base_addr, unsigned addr);

extern void print_label(FILE *ofp, int16_t *glob_index, unsigned base_addr, unsigned addr);
extern uint16_t print_data(FILE *ofp, const unsigned char *content, unsigned size, int16_t *glob_index, unsigned base_addr, unsigned addr);

#endif
