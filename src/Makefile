# defaults for Linux

TARGET = linux
CC = gcc
LIB = -lm -lasound -pthread -lpthread -lrt
COPTS = -D_GNU_SOURCE -Wall -march=native -O3

UNAME_S = $(shell uname -s)

ifeq ($(UNAME_S), Darwin)
	CC = clang
	LIB = -lm -pthread -lpthread -framework CoreAudio -framework CoreFoundation 
	TARGET = darwin
	COPTS = -g -D_GNU_SOURCE -D_IS_OSX_ -Wall -arch arm64  -arch x86_64
endif

OUT = build/$(TARGET)
WINOUT = build/win

SKRED = $(OUT)/skred

all : $(SKRED)

test :
	gcc -pthread -o test_queue test_queue.c skqueue.c -I.
	gcc -pthread -o test_heap test_heap.c skqueue.c -I.

$(OUT)/util.o : util.c
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skred-mem.o : skred-mem.c
	mkdir -p $(OUT)
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/wav2data : wav2data.c miniwav.o
	$(CC) $(COPTS) -D_GNU_SOURCE $^ -o $@

ands-example : ands.c ands-example.c bestline.o
	$(CC) $(COPTS) -Wno-multichar ands.c ands-example.c bestline.o -o $@

$(OUT)/miniwav.o : miniwav.c miniwav.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/amysamples.o : amysamples.c amysamples.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/retro.o: retro/retro.c retro/retro.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/synth.o: synth.c synth.h synth-types.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/seq.o: seq.c seq.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/udp.o: udp.c udp.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/ands.o: ands.c ands.h
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/synth-alloc.o: synth-alloc.c
	$(CC) $(COPTS) -c $< -o $@

$(OUT)/skode.o: skode.c skode.h ands.h synth.c seq.c $(OUT)/ands.o
	$(CC) $(COPTS) -Wno-multichar -c $< -o $@

$(OUT)/skred.o: skred.c skred.h
	mkdir -p $(OUT)
	$(CC) $(COPTS) -c $< -o $@

OBJS = \
	$(OUT)/skred.o \
	$(OUT)/miniwav.o \
	$(OUT)/amysamples.o \
	$(OUT)/retro.o \
	$(OUT)/synth.o \
	$(OUT)/seq.o \
	$(OUT)/skode.o \
  $(OUT)/ands.o \
	$(OUT)/udp.o \
	$(OUT)/miniaudio.o \
	$(OUT)/skred-mem.o \
	$(OUT)/util.o \
  $(OUT)/bestline.o \
  $(OUT)/skqueue.o \
  $(OUT)/synth-alloc.o \
	#

$(SKRED) : $(OBJS) | $(OUT)
	$(CC) $(COPTS) $(DOPTS) $(OBJS) -o $@ $(LIB)

mini-skred : mini-skred.c $(OBJS)
	$(CC) \
    -DMINI \
    -Wno-multichar \
    $(COPTS) \
    $(OUT)/miniwav.o \
    $(OUT)/amysamples.o \
    $(OUT)/retro.o \
    $(OUT)/synth.o \
    $(OUT)/seq.o \
    $(OUT)/ands.o \
    $(OUT)/miniaudio.o \
    $(OUT)/util.o \
    $(OUT)/skqueue.o \
    -o mini-skred mini-skred.c skode.c $(LIB)

$(OUT)/skqueue.o : skqueue.c skqueue.h
	$(CC) $(COPTS) -c skqueue.c -o $(OUT)/skqueue.o

$(OUT)/bestline.o: bestline.c bestline.h
	$(CC) -c $(COPTS) $< -o $@

$(OUT)/miniaudio.o: miniaudio.c miniaudio.h
	$(CC) $(COPTS) -c $< -o $@

win : skred-windows

skred-windows :
	mkdir -p $(WINOUT)
	$(MAKE) -f Makefile.win

check : $(SKRED)
	valgrind --tool=memcheck --leak-check=full ./$(SKRED) -n

clean :
	rm -rf build
	find . -name '*.exe' -exec rm -f {} \;
