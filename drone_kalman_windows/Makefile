# Makefile for the Windows implementation (MinGW-w64).
#   On Linux (cross-compile, to check that everything builds):  make CC=x86_64-w64-mingw32-gcc
#   On Windows with MSYS2 / w64devkit:                           make
# On plain Windows (cmd), use build.bat instead.
CC     = gcc
CFLAGS = -std=gnu11 -O2 -Wall -Wextra -Iinclude
LIBS   = -ladvapi32 -lpsapi
LIB    = $(wildcard src/*.c)
BIN    = bin

APPS  = $(BIN)/kalman_win.exe $(BIN)/validate.exe $(BIN)/bench_win.exe
DEMOS = $(BIN)/demo_fileio.exe $(BIN)/demo_process.exe $(BIN)/demo_memory.exe $(BIN)/demo_ipc.exe $(BIN)/demo_perms.exe
TOOLS = $(BIN)/gen_test_data.exe

all: $(BIN) $(APPS) $(DEMOS) $(TOOLS)

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%.exe: app/%.c $(LIB) $(wildcard include/*.h)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LIBS)

$(BIN)/%.exe: demos/%.c $(LIB) $(wildcard include/*.h)
	$(CC) $(CFLAGS) $< $(LIB) -o $@ $(LIBS)

$(BIN)/gen_test_data.exe: tools/gen_test_data.c include/drn_format.h
	$(CC) $(CFLAGS) $< -o $@ -lm

clean:
	rm -f $(BIN)/*.exe

.PHONY: all clean
