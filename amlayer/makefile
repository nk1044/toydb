CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu89

# Common object files
AM_OBJS = am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ambulkload.o misc.o
PF_OBJ = ../pflayer/pflayer.o

# ===== Final Executable =====
a.out : $(AM_OBJS) $(PF_OBJ) main.o
	$(CC) $(CFLAGS) $(AM_OBJS) $(PF_OBJ) main.o -o a.out

# ===== Combined Object for AM Layer =====
amlayer.o : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ambulkload.o
	ld -r am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ambulkload.o -o amlayer.o

# ===== Individual Compilation Rules =====
am.o : am.c am.h pf.h
	$(CC) $(CFLAGS) -c am.c

amfns.o : amfns.c am.h pf.h
	$(CC) $(CFLAGS) -c amfns.c

amsearch.o : amsearch.c am.h pf.h
	$(CC) $(CFLAGS) -c amsearch.c

aminsert.o : aminsert.c am.h pf.h
	$(CC) $(CFLAGS) -c aminsert.c

amscan.o : amscan.c am.h pf.h
	$(CC) $(CFLAGS) -c amscan.c

amstack.o : amstack.c am.h pf.h
	$(CC) $(CFLAGS) -c amstack.c

amglobals.o : amglobals.c am.h
	$(CC) $(CFLAGS) -c amglobals.c

amprint.o : amprint.c am.h pf.h
	$(CC) $(CFLAGS) -c amprint.c

ambulkload.o : ambulkload.c am.h pf.h
	$(CC) $(CFLAGS) -c ambulkload.c

misc.o : misc.c am.h pf.h
	$(CC) $(CFLAGS) -c misc.c

main.o : main.c am.h pf.h
	$(CC) $(CFLAGS) -c main.c

# ===== TEST TARGETS =====
# Each test links against full AM + PF layers

test1 : test1.o $(AM_OBJS) $(PF_OBJ)
	$(CC) $(CFLAGS) test1.o $(AM_OBJS) $(PF_OBJ) -o test1

test1.o : test1.c am.h pf.h
	$(CC) $(CFLAGS) -c test1.c

test2 : test2.o $(AM_OBJS) $(PF_OBJ)
	$(CC) $(CFLAGS) test2.o $(AM_OBJS) $(PF_OBJ) -o test2

test2.o : test2.c am.h pf.h
	$(CC) $(CFLAGS) -c test2.c

test3 : test3.o $(AM_OBJS) $(PF_OBJ)
	$(CC) $(CFLAGS) test3.o $(AM_OBJS) $(PF_OBJ) -o test3

test3.o : test3.c am.h pf.h
	$(CC) $(CFLAGS) -c test3.c

test4 : test4_indextest.o $(AM_OBJS) $(PF_OBJ)
	$(CC) $(CFLAGS) test4_indextest.o $(AM_OBJS) $(PF_OBJ) -o test4

test4_indextest.o : test4_indextest.c am.h pf.h
	$(CC) $(CFLAGS) -c test4_indextest.c

# ===== New: index comparison test =====
# This will be your new index build comparison (existing/incremental/bulkload)
indextest : indextest.o $(AM_OBJS) $(PF_OBJ)
	$(CC) $(CFLAGS) indextest.o $(AM_OBJS) $(PF_OBJ) -o indextest

indextest.o : indextest.c am.h pf.h
	$(CC) $(CFLAGS) -c indextest.c

# ===== Clean =====
clean:
	rm -f *.o
	rm -f *.out
	rm -f test1 test2 test3 test4 indextest a.out
