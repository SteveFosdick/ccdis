#ifndef CCDIS_INC
#define CCDIS_INC

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool asm_mode;

#define LOC_CINTCODE 0x4000
#define LOC_M6502    0x8000
#define LOC_DATA     0xc000

#define LOC_USETYPE  0xc000

#define LOC_CALL     0x2000

#define LOC_STRING   0x2000
#define LOC_WORD     0x1000

#define LOC_GLOBAL   0x0800
#define LOC_LABEL    0x0400

#define LOC_GLOBMASK 0x03ff

#define MAX_FILE_SIZE 0x10000
#define MAX_GLOB_NO   0x02ff

typedef struct {
    uint16_t addr;
    uint16_t indx;
} label_entry;

extern label_entry *label_entries;
extern size_t label_entries_used;
extern unsigned label_width;

#define MAX_LABEL_LEN 15

extern char (*label_names)[MAX_LABEL_LEN+1];
extern size_t label_names_used;

extern void labels_apply(void);
extern int labels_load(const char *fn);

extern uint16_t loc_index[MAX_FILE_SIZE];

extern unsigned cc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels);
extern unsigned mc_trace(const unsigned char *content, unsigned base_addr, unsigned addr, unsigned max_addr, unsigned *new_labels);

extern unsigned cc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);
extern unsigned mc_disassemble(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);

typedef struct {
    const char lab[8];
    const char byte[8];
    const char word[8];
    const char imm[8];
    const char acc[8];
    const char equ[8];
    const char org[8];
    const char dfb[8];
    const char dfw[8];
    unsigned (*str)(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);
} print_cfg;

extern const print_cfg pf_beebasm;
extern const print_cfg pf_lancs;
extern const print_cfg pf_ca65;
extern const print_cfg *pf_current;

extern void print_label(FILE *ofp, unsigned addr);
extern void print_asm_hdr(FILE *ofp, unsigned start_addr, unsigned size);
extern void print_dest_addr_nonl(FILE *ofp, unsigned addr);
extern void print_dest_addr(FILE *ofp, unsigned addr);
extern unsigned print_data(FILE *ofp, const unsigned char *content, unsigned addr, unsigned max_addr);

#endif
