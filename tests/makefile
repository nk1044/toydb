CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
INCLUDES = -I../pflayer -I../hfLayer -I../amlayer -Itests

# Object files
PFOBJS = ../pflayer/pf.o ../pflayer/buf.o ../pflayer/hash.o
HF_OBJS = ../hfLayer/hf.o
AM_OBJS = ../amlayer/am.o ../amlayer/amfns.o ../amlayer/amsearch.o ../amlayer/aminsert.o \
          ../amlayer/amstack.o ../amlayer/amglobals.o ../amlayer/amscan.o ../amlayer/amprint.o ../amlayer/misc.o

# Default target
all: test1 test2

########################
#   TEST EXECUTABLES   #
########################

# test1 → compiles pf_layer_test.c
test1: pf_layer_test.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# test2 → compiles hf_layer_test.c
test2: hf_layer_test.c $(HF_OBJS) $(PFOBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

test3: am_layer_test.c $(HF_OBJS) $(PFOBJS) $(AM_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^


########################
#   PATTERN RULES      #
########################

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f test1 test2 *.o *.hf *.bin *.tbl *.txt *.db \
	      ../pflayer/*.o ../hfLayer/*.o ../amlayer/*.o 
