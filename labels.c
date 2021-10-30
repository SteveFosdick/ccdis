#define _GNU_SOURCE
#include "ccdis.h"
#include "cintcode_tabs.h"
#include <errno.h>
#include <error.h>

label_entry *label_entries = NULL;
size_t label_entries_used = 0;
static size_t label_entries_alloc = 0;

char (*label_names)[MAX_LABEL_LEN+1] = NULL;
size_t label_names_used  = 0;
static size_t label_names_alloc = 0;

#define LABEL_BUMP_SIZE 16

void labels_apply(void)
{
    label_entry *ptr = label_entries;
    label_entry *end = ptr + label_entries_used;
    while (ptr < end) {
        loc_index[ptr->addr] = ptr->indx;
        ptr++;
    }
}

static int one_label(const char *fn, unsigned line_no, char *line)
{
    char *ptr;
    char *asc_addr = strtok_r(line, " \t\n", &ptr);
    if (!asc_addr || *asc_addr == '#')
        return 0;
    char *end;
    unsigned bin_addr = strtoul(asc_addr, &end, 0);
    if (*end) {
        error_at_line(0, errno, fn, line_no, "'%s' is not a valid address", asc_addr);
        return EINVAL;
    }
    char *op = strtok_r(NULL, " \t\n", &ptr);
    if (!op) {
        error_at_line(0, 0, fn, line_no, "missing directive");
        return EINVAL;
    }
    char *value = strtok_r(NULL, " \t\n", &ptr);
    char genlab[6];
    if (!value) {
        snprintf(genlab, sizeof(genlab), "L%04X", bin_addr);
        value = genlab;
    }

    unsigned flags;
    if (!strcasecmp(op, "cintcode"))
        flags = LOC_CINTCODE|LOC_LABEL;
    else if (!strcasecmp(op, "cintcall"))
        flags = LOC_CINTCODE|LOC_CALL|LOC_LABEL;
    else if (!strcasecmp(op, "mcode"))
        flags = LOC_M6502|LOC_LABEL;
    else if (!strcasecmp(op, "mcall"))
        flags = LOC_M6502|LOC_CALL|LOC_LABEL;
    else if (!strcasecmp(op, "data"))
        flags = LOC_DATA|LOC_LABEL;
    else if (!strcasecmp(op, "word"))
        flags = LOC_DATA|LOC_LABEL|LOC_WORD;
    else if (!strcasecmp(op, "dbyte"))
        flags = LOC_DATA|LOC_LABEL|LOC_DBYTE;
    else if (!strcasecmp(op, "string"))
        flags = LOC_DATA|LOC_LABEL|LOC_STRING;
    else {
        error_at_line(0, errno, fn, line_no, "invalid directive '%s'", op);
        return EINVAL;
    }

    if (label_entries_used >= label_entries_alloc) {
        size_t new_alloc = label_entries_alloc + LABEL_BUMP_SIZE;
        label_entry *new_tab = realloc(label_entries, new_alloc * sizeof(label_entry));
        if (!new_tab) {
            error_at_line(0, errno, fn, line_no, "unable to allocate label entry");
            return errno;
        }
        label_entries = new_tab;
        label_entries_alloc = new_alloc;
    }
    if (flags & LOC_GLOBAL) {
        char *end;
        unsigned globno = strtoul(value, &end, 0);
        if (*end) {
            error_at_line(0, errno, fn, line_no, "'%s' is not a valid integer", value);
            return EINVAL;
        }
        if (globno > MAX_GLOB_NO) {
            error_at_line(0, errno, fn, line_no, "global number %d is too big", globno);
            return EINVAL;
        }
        label_entries[label_entries_used].indx = flags|globno;
    }
    else {
        size_t len = strlen(value);
        if (len > MAX_LABEL_LEN) {
            error_at_line(0, errno, fn, line_no, "label '%s' (len %ld, max %d) is too long", value, len, MAX_LABEL_LEN);
            return EINVAL;
        }
        if (label_names_used >= label_names_alloc) {
            size_t new_alloc = label_names_alloc + LABEL_BUMP_SIZE;
            char (*new_tab)[MAX_LABEL_LEN+1] = realloc(label_names, new_alloc * (MAX_LABEL_LEN+1));
            if (!new_tab) {
                error_at_line(0, errno, fn, line_no, "unable to allocate label name");
                return errno;
            }
            label_names = new_tab;
            label_names_alloc = new_alloc;
        }
        strcpy(label_names[label_names_used], value);
        label_entries[label_entries_used].indx = flags|label_names_used++;
    }
    label_entries[label_entries_used++].addr = bin_addr;
    return 0;
}

int labels_load(const char *fn)
{
    int status = 0;

    FILE *ifp = fopen(fn, "r");
    if (ifp) {
        unsigned line_no = 1;
        char line[80];
        while (fgets(line, sizeof(line), ifp))
            if ((status = one_label(fn, line_no++, line)))
                break;
        fclose(ifp);
    }
    else {
        error(0, errno, "unable to open '%s' for input", fn);
        status = errno;
    }
    return status;
}
