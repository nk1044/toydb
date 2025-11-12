# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Source and header files
SRC = buf.c hash.c pf.c
OBJ = buf.o hash.o pf.o
HDR = pftypes.h pf.h

# Default target
all: testpf testhash

# Combine all PF layer objects into a relocatable object
pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

# Build test executables
testpf: testpf.o pflayer.o
	$(CC) $(CFLAGS) -o testpf testpf.o pflayer.o

testhash: testhash.o pflayer.o
	$(CC) $(CFLAGS) -o testhash testhash.o pflayer.o

# Dependencies
$(OBJ): $(HDR)
testpf.o: $(HDR)
testhash.o: $(HDR)

# Optional lint check
lint:
	lint $(SRC)

# Install placeholder
install: pflayer.o

# Clean build artifacts
clean:
	rm -f *.o *.out file1 file2 testpf testhash pflayer.o
