CC = gcc

EXE = \
	skred \
  skode \
  #

EXTRA = \
	wav2data \
	scope \
	scope2 \
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

RLINC = -I raylib-quickstart-main/build/external/raylib-master/src
RLLIB = -L raylib-quickstart-main/bin/Debug

util.o : util.c
	$(CC) $(COPTS) -c $<

skred-mem.o : skred-mem.c
	$(CC) $(COPTS) -c $<

scope : scope.c skred-mem.o # raylib-quickstart-main/Makefile
	$(CC) -D_GNU_SOURCE -DUSE_RAYLIB $(RLINC) $(RLLIB) $^ -o $@ -lraylib -lm

scope2 : scope2.c skred-mem.o # raylib-quickstart-main/Makefile
	$(CC) -D_GNU_SOURCE -DUSE_RAYLIB $(RLINC) $(RLLIB) $^ -o $@ -lraylib -lm

wav2data : wav2data.c miniwav.o
	$(CC) -D_GNU_SOURCE $^ -o $@

skode : skode.c skode-example.c bestline.o
	$(CC) -Wall -Wno-multichar skode.c skode-example.c bestline.o -o $@

smidi : cmex2.c crossmidi.c crossmidi.h udpmini.c udpmini.h
	$(CC) cmex2.c crossmidi.c udpmini.c -o smidi -lasound

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

SOBJS = \
  miniwav.o \
  amysamples.o \
  synth.o \
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

test-windows :
	x86_64-w64-mingw32-gcc test-windows-audio.c -o tone.exe
	wine tone.exe