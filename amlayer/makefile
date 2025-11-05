CC = gcc
CFLAGS = -Wall -Wextra -std=gnu89

# Final executable
a.out : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o misc.o
	$(CC) $(CFLAGS) am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o misc.o -o a.out

# Combined object for AM layer
amlayer.o : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o
	ld -r am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o -o amlayer.o

# Individual compilation rules
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

misc.o : misc.c am.h pf.h
	$(CC) $(CFLAGS) -c misc.c

main.o : main.c am.h pf.h
	$(CC) $(CFLAGS) -c main.c


test1 : test1.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) test1.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o -o test1

test1.o : test1.c am.h pf.h
	$(CC) $(CFLAGS) -c test1.c

test2 : test2.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) test2.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o -o test2

test2.o : test2.c am.h pf.h
	$(CC) $(CFLAGS) -c test2.c

test3 : test3.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) test3.o misc.o am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ../pflayer/pflayer.o -o test3

test3.o : test3.c am.h pf.h
	$(CC) $(CFLAGS) -c test3.c



clean:
	rm -f *.o
	rm -f *.out
