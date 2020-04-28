#ifndef CCDIS_INC
#define CCDIS_INC

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOC_CINTCODE 0x4000
#define LOC_M6502    0x8000
#define LOC_DATA     0xc000

#define LOC_USETYPE  0xc000

#define LOC_CALL     0x2000
#define LOC_GLOBAL   0x1000
#define LOC_LABEL    0x0800

#define LOC_GLOBMASK 0x07ff

#define MAX_FILE_SIZE 0x10000
#define MAX_GLOB_NO   0x0300

extern uint16_t loc_index[MAX_FILE_SIZE];

extern unsigned cc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels);
extern unsigned mc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels);

extern unsigned cc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);
extern unsigned mc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);

extern void print_label(FILE *ofp, unsigned addr);
extern void print_dest_addr(FILE *ofp, unsigned addr);
extern unsigned print_data(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);

#endif
