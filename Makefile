CC     = gcc
CFLAGS = -O2 -g -Wall

all: ccdis

MODULES	= main.o ccdis.o mcdis.o cintcode_tabs.o print.o rom_tab.o labels.o

$(MODULES): ccdis.h
ccdis.o print.o: cintcode_tabs.h

ccdis: $(MODULES)
