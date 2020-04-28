/* Table of ROM addresses with the BBC Microcomputer BCPL ROM */

#ifndef ROM_TAB_INC
#define ROM_TAB_INC

typedef struct {
    uint16_t romaddr;
    uint16_t locindx;
} rom_entry;

#define ROM_TAB_SIZE 92

extern rom_entry rom_table[ROM_TAB_SIZE];

#endif
