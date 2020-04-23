#ifndef CCDIS_INC
#define CCDIS_INC

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void ccdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index);
extern void mcdis(FILE *ofp, const unsigned char *content, uint16_t code_size, int16_t *glob_index);

#endif
