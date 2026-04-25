CC ?= cc
CXX ?= c++
CFLAGS ?= -std=c11 -Wall -Wextra -O3 -march=native
LDFLAGS ?=
PKG_CONFIG ?= pkg-config

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3 2>/dev/null || $(PKG_CONFIG) --cflags SDL3 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl3 2>/dev/null || $(PKG_CONFIG) --libs SDL3 2>/dev/null)

UNAME_S := $(shell uname -s)
VINEMU_OBJS := vinemu.o vimana.o
ifeq ($(UNAME_S),Darwin)
VINEMU_OBJS += cocoa_autoreleasepool.o
COCOA_LIBS := -framework Foundation -framework AppKit
endif

.PHONY: all clean patterns

all: vinasm vinemu

vinasm: vinasm.c vinrom.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ vinasm.c

vinemu: $(VINEMU_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(VINEMU_OBJS) $(SDL_LIBS) $(COCOA_LIBS) -lm
	rm -f $(VINEMU_OBJS)

vinemu.o: vinemu.c vinrom.h ../src/c/vimana.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I../src/c -c -o $@ vinemu.c

vimana.o: ../src/c/vimana.c ../src/c/vimana.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -I../src/c -c -o $@ ../src/c/vimana.c

cocoa_autoreleasepool.o: ../src/objc/cocoa_autoreleasepool.mm
	$(CXX) $(SDL_CFLAGS) -c -o $@ ../src/objc/cocoa_autoreleasepool.mm

clean:
	rm -f vinasm vinemu $(VINEMU_OBJS)

patterns:
	python3 scan_extras.py ../extras
