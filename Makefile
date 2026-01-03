CC = gcc

EXE = \
	skred \
	skode \
	scope \
	#

SK8R_DIR := sk8r
MIDI_DIR := midi-sk8
BUILD_DIR := build
WIN_CC   := x86_64-w64-mingw32-gcc

# Linker Flags
# -s -w strips debug info for smaller binaries
# -H=windowsgui hides the terminal window on Windows launch
# -extldflags '-static...' ensures all C++ dependencies are bundled into the exe
WIN_LDFLAGS="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'"
LINUX_LDFLAGS="-s -w"

.PHONY: all sk8r-linux sk8r-windows midi-linux midi-windows setup-midi clean

# Build Linux version
sk8r-linux:
	cd $(SK8R_DIR) && go build -o ../build/sk8r-linux .

# Build Windows version (Docker-free cross-compile)
sk8r-windows:
	mkdir -p build
	cd $(SK8R_DIR) && CGO_ENABLED=1 GOOS=windows GOARCH=amd64 CC=$(WIN_CC) \
	go build -ldflags="-H=windowsgui" -o ../build/sk8r.exe .

# 1. Install system dependencies & tidy modules (Run this once)
setup-midi:
	sudo dnf install mingw64-gcc mingw64-winpthreads-static alsa-lib-devel
	cd $(MIDI_DIR) && go mod tidy

# 2. Build for Linux (Native Fedora)
midi-linux:
	mkdir -p $(BUILD_DIR)
	cd $(MIDI_DIR) && go build -o ../$(BUILD_DIR)/midi-sk8-linux .

midi-windows:
	mkdir -p build
	cd midi-sk8 && CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
	CC=x86_64-w64-mingw32-gcc \
	CXX=x86_64-w64-mingw32-g++ \
	go build -ldflags="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'" -o ../build/midi-sk8.exe .

pad-linux:
	mkdir -p $(BUILD_DIR)
	cd sk8-pad && go build -ldflags="-s -w" -o ../build/sk8-pad-linux .

pad-windows:
	mkdir -p build
	cd sk8-pad && \
    CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    go build -ldflags="-H=windowsgui -s -w -extldflags '-static'" -o ../build/sk8-pad.exe .

repl-linux:
	mkdir -p $(BUILD_DIR)
	cd sk8-repl && go build -ldflags="-s -w" -o ../build/sk8-repl .

repl-windows:
	mkdir -p build
	cd sk8-repl && \
    CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    go build -ldflags="-H=windowsgui -s -w -extldflags '-static'" -o ../build/sk8-repl.exe .

EXTRA = \
	wav2data \
	#

all : $(EXE)

LIB = \
	-lm \
	-lasound \
	-pthread \
	-lpthread \
	-lrt \
	#

COPTS = \
	-D_GNU_SOURCE \
	-Wall \
	-march=native \
	-O3 \
	#
NOPTS = \
	-g

util.o : util.c
	$(CC) $(COPTS) -c $<

skred-mem.o : skred-mem.c
	$(CC) $(COPTS) -c $<

futex-compat.o : futex-compat.c futex-compat.h
	$(CC) $(COPTS) -c $<

build/linux/libraylib.a :
	mkdir -p build/linux
	cd raylib/src && make clean && \
	make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC && \
	cp libraylib.a ../../build/linux/

scope : scope.c skred-mem.o build/linux/libraylib.a
	$(CC) -D_GNU_SOURCE -DUSE_RAYLIB -L build/linux -I raylib/src $^ -o $@ -lraylib -lm

wav2data : wav2data.c miniwav.o
	$(CC) -D_GNU_SOURCE $^ -o $@

skode : skode.c skode-example.c bestline.o
	$(CC) -Wall -Wno-multichar skode.c skode-example.c bestline.o -o $@

smidi : cmex2.c crossmidi.c crossmidi.h udpmini.c udpmini.h
	$(CC) cmex2.c crossmidi.c udpmini.c -o smidi -lasound

skmidi : skmidi.c
	$(CC) skmidi.c -o skmidi -lasound

miniwav.o : miniwav.c miniwav.h
	$(CC) $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	$(CC) $(COPTS) -c $<

raylib-quickstart-main/Makefile :
	sh make-raylib
	
synth.def: skred.h

synth.o: synth.c synth.h synth-types.h synth.def
	$(CC) $(COPTS) -c $<

seq.o: seq.c seq.h
	$(CC) $(COPTS) -c $<

udp.o: udp.c udp.h
	$(CC) $(COPTS) -c $<

skode.o: skode.c skode.h
	$(CC) $(COPTS) -c $<

wire.o: wire.c wire.h synth.def skode.h skode.o
	$(CC) $(COPTS) -Wno-multichar -c $<

skred.o: skred.c skred.h synth.def
	$(CC) $(COPTS) -c $<

OBJS = \
	skred.o \
	miniwav.o \
	amysamples.o \
	synth.o \
	seq.o \
	wire.o skode.o \
	udp.o \
	miniaudio.o \
	bestline.o \
	skred-mem.o \
	util.o \
	#

skred : $(OBJS)
	$(CC) $(COPTS) $^ -o $@ $(LIB)

bestline.o: bestline.c bestline.h
	$(CC) -c $<

miniaudio.o: miniaudio.c miniaudio.h
	$(CC) -c $<

check : skred
	valgrind --tool=memcheck --leak-check=full ./skred

clean :
	rm -f *.o
	rm -f $(EXE)
	rm -rf build
	cd raylib/src && make clean

REL = bundle-skred-2026-01-03-002

install-win :
	mkdir -p $(REL)
	mkdir -p $(REL)/sk
	mkdir -p $(REL)/wav
	cd $(REL) ; \
    cp ../win/skred.exe . ; \
    cp ../win/scope.exe . ; \
    cp ../build/sk8r.exe . ; \
    cp ../build/midi-sk8.exe . ; \
    cp ../build/sk8-pad.exe . ; \
    cp ../sk/909.sk sk ; \
    cp ../wav/24.wav wav ; \
  #
	rm -f $(REL).zip
	zip -r $(REL).zip $(REL)

test-windows :
	x86_64-w64-mingw32-gcc test-windows-audio.c -o tone.exe
	wine tone.exe