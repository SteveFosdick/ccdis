/* User defined labels */

#ifndef LABELS_INC
#define LABELS_INC

typedef struct {
    uint16_t addr;
    uint16_t indx;
} label_entry;

extern label_entry label_entries;
extern size_t label_entries_size;
extern size_t label_entries_alloc;

#define MAX_LABEL_LEN 15

char (*label_names)[MAX_LABEL_LEN+1];
extern size_t label_names_size;
extern size_t label_names_alloc;

#endif
