# defaults for Linux

TARGET = linux
CC = gcc
LIB = -lm -lasound -pthread -lpthread -lrt
COPTS = -D_GNU_SOURCE -Wall -march=native -O3
# COPTS = -D_GNU_SOURCE -Wall -march=native -g # -O3

UNAME_S = $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
	CC = clang
	LIB = -lm -pthread -lpthread -framework CoreAudio -framework CoreFoundation 
	TARGET = darwin
	COPTS = -g -D_GNU_SOURCE -D_IS_OSX_ -Wall -arch arm64  -arch x86_64
	# COPTS = -D_GNU_SOURCE -D_IS_OSX_ -Wall -arch arm64 
	# DOPTS = -Wl,-stack_size,0x800000
endif

OUT = build/$(TARGET)
WINOUT = build/win

SKRED = $(OUT)/skred

SCOPE = $(OUT)/scope

all : $(SKRED)

basic : $(SKRED)

visual : $(SCOPE)

GUI = sk8r-$(TARGET) sk8-pad-$(TARGET) midi-sk8-$(TARGET)

gui : $(GUI)

$(OUT)/util.o : util.c
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skred-mem.o : skred-mem.c
	mkdir -p $(OUT)
	$(CC) $(COPTS) -c $< -o $@

$(SCOPE) :
	mkdir -p $(OUT)
	$(MAKE) -f Makefile.$(TARGET).scope

$(OUT)/wav2data : wav2data.c miniwav.o
	$(CC) $(COPTS) -D_GNU_SOURCE $^ -o $@

$(OUT)/skode : skode.c skode-example.c bestline.o
	$(CC) $(COPTS) -Wno-multichar skode.c skode-example.c bestline.o -o $@

$(OUT)/miniwav.o : miniwav.c miniwav.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/amysamples.o : amysamples.c amysamples.h
	$(CC) $(COPTS) -c $< -o $@

synth.def: skred.h

$(OUT)/retro.o: retro/retro.c retro/retro.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/synth.o: synth.c synth.h synth-types.h synth.def
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/seq.o: seq.c seq.h
	$(CC) $(QFLAGS) $(COPTS) -c $< -o $@

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
	$(OUT)/retro.o \
	$(OUT)/synth.o \
	$(OUT)/seq.o \
	$(OUT)/wire.o \
  $(OUT)/skode.o \
	$(OUT)/udp.o \
	$(OUT)/miniaudio.o \
	$(OUT)/skred-mem.o \
	$(OUT)/util.o \
  $(OUT)/bestline.o \
  $(OUT)/skqueue.o \
  $(OUT)/nsync_local.o \
	#

$(SKRED) : $(OBJS) | $(OUT)
	$(CC) $(COPTS) $(DOPTS) $(OBJS) -o $@ $(LIB)
	# $(CC) $(COPTS) $(OBJS) -o $(OUT)/skred $(LIB)

QFLAGS = -I. \
       -I./nsync_local/public \
       -I./nsync_local/internal \
       -I./nsync_local/platform/gcc \
       -I./nsync_local/platform/gcc_old

$(OUT)/skqueue.o :
	$(CC) $(COPTS) $(QFLAGS) -c skqueue.c -o $(OUT)/skqueue.o

$(OUT)/nsync_local.o :
	$(CC) $(COPTS) $(QFLAGS) -c nsync_local.c -o $(OUT)/nsync_local.o

$(OUT)/bestline.o: bestline.c bestline.h
	$(CC) -c $(COPTS) $< -o $@

$(OUT)/miniaudio.o: miniaudio.c miniaudio.h
	$(CC) $(COPTS) -c $< -o $@

# Linker Flags
# -s -w strips debug info for smaller binaries
# -H=windowsgui hides the terminal window on Windows launch
# -extldflags '-static...' ensures all C++ dependencies are bundled into the exe
WIN_LDFLAGS="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'"
LINUX_LDFLAGS="-s -w"

# go install fyne.io/tools/cmd/fyne@latest

sk8r-linux:
	mkdir -p $(OUT)
	cd sk8r && go build -o ../$(OUT)/sk8r-$(TARGET) .
	# cd sk8r && fyne package -exe ../$(OUT)/sk8r -os linux -tags flatpak -icon icon.png -app-id com.example.app -app-version 0.0.1

sk8r-darwin:
	mkdir -p $(OUT)
	cd sk8r && CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/sk8r-darwin-amd64 .
	cd sk8r && CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/sk8r-darwin-arm64 .
	lipo -create -output $(OUT)/sk8r-darwin-universal $(OUT)/sk8r-darwin-amd64 $(OUT)/sk8r-darwin-arm64
	cd sk8r && fyne package -exe ../$(OUT)/sk8r-darwin-universal -os darwin -icon icon.png -name ../$(OUT)/sk8r

win : sk8r-windows midi-sk8-windows sk8-pad-windows skred-windows repl-windows

skred-windows :
	mkdir -p $(WINOUT)
	$(MAKE) -f Makefile.win

GOWINENV = CGO_ENABLED=1 GOOS=windows GOARCH=amd64 CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
GOWINLD = -ldflags="-H=windowsgui -s -w -extldflags '-static -static-libgcc -static-libstdc++'"

repl-windows:
	mkdir -p $(WINOUT)
	cd repl && \
  $(GOWINENV) go build $(GOWINLD) -o skrepl.exe . && \
	fyne package --os windows --name "skrepl" --icon icon.png --appID com.octetta.skrepl && \
  cp skrepl.exe ../$(WINOUT)
 

sk8r-windows:
	mkdir -p $(WINOUT)
	cd sk8r && \
    $(GOWINENV) \
    fyne package --release -os windows && \
    cp sk8r.exe ../$(WINOUT)

xsk8r-windows: # keep note of the old way we built things...
	mkdir -p $(WINOUT)
	cd sk8r && \
  $(GOWINENV) go build $(GOWINLD) -o ../$(WINOUT)/sk8r.exe .

setup-midi:
	sudo dnf install mingw64-gcc mingw64-winpthreads-static alsa-lib-devel
	cd midi-sk8 && go mod tidy

midi-sk8-linux:
	mkdir -p $(OUT)
	cd midi-sk8 && go build -o ../$(OUT)/midi-sk8-linux .

midi-sk8-darwin:
	mkdir -p $(OUT)
	cd midi-sk8 && CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/midi-sk8-darwin-amd64 .
	cd midi-sk8 && CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/midi-sk8-darwin-arm64 .
	cd midi-sk8 && lipo -create -output ../$(OUT)/midi-sk8-darwin-universal ../$(OUT)/midi-sk8-darwin-amd64 ../$(OUT)/midi-sk8-darwin-arm64
	cd midi-sk8 && fyne package -exe ../$(OUT)/midi-sk8-darwin-universal -os darwin -icon icon.png -name ../$(OUT)/midi-sk8

midi-sk8-windows:
	mkdir -p $(WINOUT)
	cd midi-sk8 && \
    $(GOWINENV) \
    fyne package --release -os windows && \
    cp midi-sk8.exe ../$(WINOUT)


sk8-pad-linux:
	mkdir -p $(OUT)
	cd sk8-pad && go build -ldflags="-s -w" -o ../$(OUT)/sk8-pad-linux .

sk8-pad-darwin:
	mkdir -p $(OUT)
	cd sk8-pad && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=amd64 go build -o ../$(OUT)/sk8-pad-darwin-amd64 . && \
	CGO_ENABLED=1 GOOS=darwin GOARCH=arm64 go build -o ../$(OUT)/sk8-pad-darwin-arm64 . && \
	lipo -create -output ../$(OUT)/sk8-pad-darwin-universal ../$(OUT)/sk8-pad-darwin-amd64 ../$(OUT)/sk8-pad-darwin-arm64
	cd sk8-pad && fyne package -exe ../$(OUT)/sk8-pad-darwin-universal -os darwin -icon icon.png -name ../$(OUT)/sk8-pad

sk8-pad-windows:
	mkdir -p $(WINOUT)
	cd sk8-pad && \
    $(GOWINENV) \
    fyne package --release -os windows && \
    cp sk8-pad.exe ../$(WINOUT)


sk8-repl-linux:
	mkdir -p $(OUT)
	cd sk8-repl && go build -ldflags="-s -w" -o ../$(OUT)/sk8-repl .

sk8-repl-windows:
	mkdir -p $(WINOUT)
	cd sk8-repl && \
    CGO_ENABLED=1 GOOS=windows GOARCH=amd64 \
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    go build -ldflags="-H=windowsgui -s -w -extldflags '-static'" -o ../$(WINOUT)/sk8-repl.exe .

check : $(SKRED)
	valgrind --tool=memcheck --leak-check=full ./$(SKRED) -n

clean :
	rm -rf build
	cd raylib/src && make clean
	rm -rf bundle-skred-*
	find . -name '*.exe' -exec rm -f {} \;

REL = bundle-skred-windows-2026-01-20-002

install-win :
	rm -rf $(REL)
	mkdir -p $(REL)
	mkdir -p $(REL)/sk
	mkdir -p $(REL)/wav
	cd $(REL) ; \
    cp ../$(WINOUT)/skred.exe . ; \
    cp ../$(WINOUT)/scope.exe . ; \
    cp ../$(WINOUT)/sk8r.exe . ; \
    cp ../$(WINOUT)/midi-sk8.exe . ; \
    cp ../$(WINOUT)/sk8-pad.exe . ; \
    cp ../../sk/909.sk sk ; \
    cp ../../wav/24.wav wav ; \
    # upx --best *.exe
	rm -f $(REL).zip
	zip -r $(REL).zip $(REL)

install-darwin :
	rm -rf $(REL)
	mkdir -p $(REL)
	mkdir -p $(REL)/sk
	mkdir -p $(REL)/wav
	cd $(REL) ; \
    cp ../skred . ; \
    cp -r ../build/darwin/sk8r.app . ; \
    cp -r ../build/darwin/sk8-pad.app . ; \
    cp -r ../build/darwin/midi-sk8.app . ; \
    cp -r ../build/darwin/Skope.app . ; \
    cp ../sk/909.sk sk ; \
    cp ../wav/24.wav wav ; \
	rm -f $(REL).zip
	zip -r $(REL).zip $(REL)
