CC = gcc

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
	CC := clang
	LIB := \
	-lm \
	-pthread \
	-lpthread \
	-framework CoreAudio \
	-framework CoreFoundation \
	#

	TARGET := darwin

	COPTS := \
	  -D_GNU_SOURCE \
	  -D_IS_OSX_ \
	  -Wall \
	  -arch arm64 \
	  -arch x86_64 \
	  #
else
	LIB := \
	-lm \
	-lasound \
	-pthread \
	-lpthread \
	-lrt \
	#

	TARGET := linux

	COPTS := \
	-D_GNU_SOURCE \
	-Wall \
	-march=native \
	-O3 \
	#

endif

OUT := build/$(TARGET)

EXE = \
	$(OUT)/skred \
	#

$(OUT):
	mkdir -p $@

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

# Build macos version
sk8r-macos:
	cd $(SK8R_DIR) && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../build/sk8r-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../build/sk8r-macos-arm64 . && \
	lipo -create -output ../build/sk8r-macos-universal ../build/sk8r-macos-amd64 ../build/sk8r-macos-arm64
	cd $(SK8R_DIR) && fyne package -exe ../build/sk8r-macos-universal -os darwin -icon icon.png -name ../build/sk8r

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

midi-macos:
	mkdir -p $(BUILD_DIR)
	cd $(MIDI_DIR) && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../build/midi-sk8-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../build/midi-sk8-macos-arm64 . && \
	lipo -create -output ../build/midi-sk8-macos-universal ../build/midi-sk8-macos-amd64 ../build/midi-sk8-macos-arm64
	cd $(MIDI_DIR) && fyne package -exe ../build/midi-sk8-macos-universal -os darwin -icon icon.png -name ../build/midi-sk8

midi-windows:
	mkdir -p build
	cd midi-sk8 && CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
	CC=x86_64-w64-mingw32-gcc \
	CXX=x86_64-w64-mingw32-g++ \
	go build -ldflags="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'" -o ../build/midi-sk8.exe .

pad-linux:
	mkdir -p $(BUILD_DIR)
	cd sk8-pad && go build -ldflags="-s -w" -o ../build/sk8-pad-linux .

pad-macos:
	mkdir -p $(BUILD_DIR)
	cd sk8-pad && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../build/sk8-pad-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../build/sk8-pad-macos-arm64 . && \
	lipo -create -output ../build/sk8-pad-macos-universal ../build/sk8-pad-macos-amd64 ../build/sk8-pad-macos-arm64
	cd sk8-pad && fyne package -exe ../build/sk8-pad-macos-universal -os darwin -icon icon.png -name ../build/sk8-pad

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

all : $(OUT) $(EXE)

$(OUT)/util.o : util.c
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skred-mem.o : skred-mem.c
	$(CC) $(COPTS) -c $< -o $@

futex-compat.o : futex-compat.c futex-compat.h
	$(CC) $(COPTS) -c $<

build/linux/libraylib.a :
	mkdir -p build/linux
	cd raylib/src && make clean && \
	make PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC && \
	cp libraylib.a ../../build/linux/

scope : scope.c skred-mem.o build/linux/libraylib.a
	$(CC) $(COPTS) -D_GNU_SOURCE -DUSE_RAYLIB -L build/linux -I raylib/src $^ -o $@ -lraylib -lm

$(OUT)/wav2data : wav2data.c miniwav.o
	$(CC) $(COPTS) -D_GNU_SOURCE $^ -o $@

$(OUT)/skode : skode.c skode-example.c bestline.o
	$(CC) $(COPTS) -Wall -Wno-multichar skode.c skode-example.c bestline.o -o $@

$(OUT)/miniwav.o : miniwav.c miniwav.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/amysamples.o : amysamples.c amysamples.h
	$(CC) $(COPTS) -c $< -o $@

raylib-quickstart-main/Makefile :
	sh make-raylib
	
synth.def: skred.h

$(OUT)/synth.o: synth.c synth.h synth-types.h synth.def
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/seq.o: seq.c seq.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/udp.o: udp.c udp.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skode.o: skode.c skode.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/wire.o: wire.c wire.h synth.def skode.h $(OUT)/skode.o
	$(CC) $(COPTS) -Wno-multichar -c $< -o $@

$(OUT)/skred.o: skred.c skred.h synth.def
	$(CC) $(COPTS) -c $< -o $@

OBJS = \
	$(OUT)/skred.o \
	$(OUT)/miniwav.o \
	$(OUT)/amysamples.o \
	$(OUT)/synth.o \
	$(OUT)/seq.o \
	$(OUT)/wire.o \
  $(OUT)/skode.o \
	$(OUT)/udp.o \
	$(OUT)/miniaudio.o \
	$(OUT)/bestline.o \
	$(OUT)/skred-mem.o \
	$(OUT)/util.o \
	#

$(OUT)/skred : $(OBJS)
	$(CC) $(COPTS) $(OBJS) -o $(OUT)/skred $(LIB)

$(OUT)/bestline.o: bestline.c bestline.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/miniaudio.o: miniaudio.c miniaudio.h
	$(CC) $(COPTS) -c $< -o $@

check : $(OUT)/skred
	valgrind --tool=memcheck --leak-check=full ./$(OUT)/skred -n

clean :
	rm -f *.o
	rm -f $(EXE)
	rm -rf build
	cd raylib/src && make clean

REL = bundle-skred-2026-01-03-005

install-win :
	rm -rf $(REL)
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
    upx --best *.exe
	rm -f $(REL).zip
	zip -r $(REL).zip $(REL)

install-macos :
	rm -rf $(REL)
	mkdir -p $(REL)
	mkdir -p $(REL)/sk
	mkdir -p $(REL)/wav
	cd $(REL) ; \
    cp ../skred . ; \
    cp -r ../build/sk8r.app . ; \
    cp -r ../build/sk8-pad.app . ; \
    cp -r ../build/midi-sk8.app . ; \
    cp ../sk/909.sk sk ; \
    cp ../wav/24.wav wav ; \
	rm -f $(REL).zip
	zip -r $(REL).zip $(REL)

test-windows :
	x86_64-w64-mingw32-gcc test-windows-audio.c -o tone.exe
	wine tone.exe