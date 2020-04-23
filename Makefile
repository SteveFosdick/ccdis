CC     = gcc
CFLAGS = -Og -g -Wall

ccdis: main.o ccdis.o mcdis.o cintcode_tabs.o
