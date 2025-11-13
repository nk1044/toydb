CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
INCLUDES = -I../pflayer -I../hfLayer -I../amlayer -Itests

PFOBJS = ../pflayer/pf.o ../pflayer/buf.o ../pflayer/hash.o
HF_OBJS = ../hfLayer/hf.o
AM_OBJS = ../amlayer/am.o ../amlayer/amfns.o ../amlayer/amsearch.o ../amlayer/aminsert.o \
          ../amlayer/amstack.o ../amlayer/amglobals.o ../amlayer/amscan.o ../amlayer/amprint.o ../amlayer/misc.o

all: test1 test2


test1: pf_layer_test.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test2: hf_layer_test.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test3: test2 am_layer_test.c $(HF_OBJS) $(PFOBJS) $(AM_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f test1 test2 *.o *.hf *.bin *.tbl *.txt *.db \
	      ../pflayer/*.o ../hfLayer/*.o ../amlayer/*.o 
