CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
INCLUDES = -I../pflayer -I../spLayer -I../amlayer -Itests

# Common object sets
PFOBJS = ../pflayer/pf.o ../pflayer/buf.o ../pflayer/pageOperation.o ../pflayer/hash.o
HF_OBJS = ../spLayer/hf.o
AM_OBJS = ../amlayer/am.o ../amlayer/amfns.o ../amlayer/amsearch.o ../amlayer/aminsert.o \
          ../amlayer/amstack.o ../amlayer/amglobals.o ../amlayer/amscan.o ../amlayer/amprint.o ../amlayer/misc.o

# Default target
all: test1


# PF-layer buffer + HF dataset test
test1: test_pf_rw_mix.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test2: test2.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Pattern rules for .o
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f test1 test_pf_rw_mix *.o ../pflayer/*.o ../spLayer/*.o ../amlayer/*.o courses.hf courses_test.hf 
