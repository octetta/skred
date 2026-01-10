# defaults for Linux

TARGET := linux
CC = gcc
LIB := -lm -lasound -pthread -lpthread -lrt
COPTS := -D_GNU_SOURCE -Wall -march=native -O3 

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
	CC := clang
	LIB := -lm -pthread -lpthread -framework CoreAudio -framework CoreFoundation 
	TARGET := darwin
	COPTS := -D_GNU_SOURCE -D_IS_OSX_ -Wall -arch arm64 
endif

OUT := build/$(TARGET)

SKRED = $(OUT)/skred

SCOPE = $(OUT)/scope

all : $(SKRED)

basic : $(SKRED)

visual : $(SCOPE)

$(OUT)/util.o : util.c
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skred-mem.o : skred-mem.c
	mkdir -p $(OUT)
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/libraylib.a :
	mkdir -p $(OUT)
	cd raylib/src && $(MAKE) clean
	cd raylib/src && $(MAKE) CC=$(CC) PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	cp raylib/src/libraylib.a $(OUT)/

$(SCOPE) : scope.c $(OUT)/skred-mem.o $(OUT)/libraylib.a
	mkdir -p $(OUT)
	$(CC) $(COPTS) -D_GNU_SOURCE -DUSE_RAYLIB -L $(OUT) -I raylib/src $^ -o $@ -lraylib $(LIB)

$(OUT)/wav2data : wav2data.c miniwav.o
	$(CC) $(COPTS) -D_GNU_SOURCE $^ -o $@

$(OUT)/skode : skode.c skode-example.c bestline.o
	$(CC) $(COPTS) -Wall -Wno-multichar skode.c skode-example.c bestline.o -o $@

$(OUT)/miniwav.o : miniwav.c miniwav.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/amysamples.o : amysamples.c amysamples.h
	$(CC) $(COPTS) -c $< -o $@

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
	mkdir -p $(OUT)
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

$(SKRED) : $(OBJS) | $(OUT)
	$(CC) $(COPTS) $(OBJS) -o $@ $(LIB)
	# $(CC) $(COPTS) $(OBJS) -o $(OUT)/skred $(LIB)

$(OUT)/bestline.o: bestline.c bestline.h
	$(CC) -c $< -o $@

$(OUT)/miniaudio.o: miniaudio.c miniaudio.h
	$(CC) -c $< -o $@

### GUI tooling

TOOLS = \
  sk8r-$(TARGET) \
  sk8-pad-$(TARGET) \
  midi-sk8-$(TARGET) \
  #

HIDE = \
  $(OUT)/midi-sk8 \
  #

tools : $(TOOLS)

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

# go install fyne.io/tools/cmd/fyne@latest

sk8r-linux:
	cd $(SK8R_DIR) && go build -o ../$(OUT)/sk8r .
	# cd $(SK8R_DIR) && fyne package -exe ../$(OUT)/sk8r -os linux -tags flatpak -icon icon.png -app-id com.example.app -app-version 0.0.1

sk8r-macos:
	cd $(SK8R_DIR) && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/sk8r-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/sk8r-macos-arm64 . && \
	lipo -create -output ../build/sk8r-macos-universal ../$(OUT)/sk8r-macos-amd64 ../$(OUT)/sk8r-macos-arm64
	cd $(SK8R_DIR) && fyne package -exe ../$(OUT)/sk8r-macos-universal -os darwin -icon icon.png -name ../$(OUT)/sk8r

win: sk8r-windows midi-sk8-windows sk8-pad-windows

sk8r-windows:
	mkdir -p build/win
	cd $(SK8R_DIR) && CGO_ENABLED=1 GOOS=windows GOARCH=amd64 CC=$(WIN_CC) \
	go build -ldflags="-H=windowsgui" -o ../build/win/sk8r.exe .

# 1. Install system dependencies & tidy modules (Run this once)
setup-midi:
	sudo dnf install mingw64-gcc mingw64-winpthreads-static alsa-lib-devel
	cd $(MIDI_DIR) && go mod tidy

# 2. Build for Linux (Native Fedora)
midi-sk8-linux:
	cd $(MIDI_DIR) && go build -o ../$(OUT)/midi-sk8-linux .

midi-macos:
	mkdir -p $(BUILD_DIR)
	cd $(MIDI_DIR) && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/midi-sk8-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/midi-sk8-macos-arm64 . && \
	lipo -create -output ../$(OUT)/midi-sk8-macos-universal ../$(OUT)/midi-sk8-macos-amd64 ../$(OUT)/midi-sk8-macos-arm64
	cd $(MIDI_DIR) && fyne package -exe ../$(OUT)/midi-sk8-macos-universal -os darwin -icon icon.png -name ../$(OUT)/midi-sk8

midi-sk8-windows:
	mkdir -p build/win
	cd midi-sk8 && CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
	CC=x86_64-w64-mingw32-gcc \
	CXX=x86_64-w64-mingw32-g++ \
	go build -ldflags="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'" -o ../build/win/midi-sk8.exe .

sk8-pad-linux:
	mkdir -p $(BUILD_DIR)
	cd sk8-pad && go build -ldflags="-s -w" -o ../$(OUT)/sk8-pad-linux .

pad-macos:
	mkdir -p $(BUILD_DIR)
	cd sk8-pad && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/sk8-pad-macos-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/sk8-pad-macos-arm64 . && \
	lipo -create -output ../$(OUT)/sk8-pad-macos-universal ../$(OUT)/sk8-pad-macos-amd64 ../build/sk8-pad-macos-arm64
	cd sk8-pad && fyne package -exe ../$(OUT)/sk8-pad-macos-universal -os darwin -icon icon.png -name ../$(OUT)/sk8-pad

sk8-pad-windows:
	mkdir -p build/win
	cd sk8-pad && \
    CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    go build -ldflags="-H=windowsgui -s -w -extldflags '-static'" -o ../build/win/sk8-pad.exe .

repl-linux:
	mkdir -p $(BUILD_DIR)
	cd sk8-repl && go build -ldflags="-s -w" -o ../$(OUT)/sk8-repl .

repl-windows:
	mkdir -p build/win
	cd sk8-repl && \
    CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    go build -ldflags="-H=windowsgui -s -w -extldflags '-static'" -o ../build/win/sk8-repl.exe .

check : $(SKRED)
	valgrind --tool=memcheck --leak-check=full ./$(SKRED) -n

clean :
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
    cp ../build/win/sk8r.exe . ; \
    cp ../build/win/midi-sk8.exe . ; \
    cp ../build/win/sk8-pad.exe . ; \
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