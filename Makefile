EXE = \
	skred \
  skode \
  #

EXTRA = \
	wav2data \
	scope \
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
  -g

RLINC = -I raylib-quickstart-main/build/external/raylib-master/src
RLLIB = -L raylib-quickstart-main/bin/Debug

util.o : util.c
	gcc $(COPTS) -c $<

skred-mem.o : skred-mem.c
	gcc $(COPTS) -c $<

scope : scope.c skred-mem.o # raylib-quickstart-main/Makefile
	gcc -g -D_GNU_SOURCE -DUSE_RAYLIB $(RLINC) $(RLLIB) $^ -o $@ -lraylib -lm

wav2data : wav2data.c miniwav.o
	gcc -g -D_GNU_SOURCE $^ -o $@

skode : skode.c linenoise.o
	gcc -DDEMO -Wall -Wno-multichar skode.c linenoise.o -o $@

smidi : cmex2.c crossmidi.c crossmidi.h udpmini.c udpmini.h
	gcc cmex2.c crossmidi.c udpmini.c -o smidi -lasound

miniwav.o : miniwav.c miniwav.h
	gcc $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	gcc $(COPTS) -c $<

raylib-quickstart-main/Makefile :
	sh make-raylib
  
synth.def: skred.h

synth.o: synth.c synth.h synth-types.h synth.def
	gcc $(COPTS) -c $<

seq.o: seq.c seq.h
	gcc $(COPTS) -c $<

udp.o: udp.c udp.h
	gcc $(COPTS) -c $<

wire.o: wire.c wire.h synth.def
	gcc $(COPTS) -c $<

skred.o: skred.c skred.h synth.def
	gcc $(COPTS) -c $<

OBJS = \
  skred.o \
  miniwav.o \
  amysamples.o \
  synth.o \
  seq.o \
  wire.o \
  udp.o \
  miniaudio.o \
	linenoise.o \
	skred-mem.o \
	util.o \
  #

SOBJS = \
  miniwav.o \
  amysamples.o \
  synth.o \
  miniaudio.o \
	linenoise.o \
	skred-mem.o \
	util.o \
  #

skred : $(OBJS)
	gcc $(COPTS) $^ -o $@ $(LIB)

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

miniaudio.o: miniaudio.c miniaudio.h
	gcc -c $<

check : skred
	valgrind --tool=memcheck --leak-check=full ./skred

clean :
	rm -f *.o
	rm -f $(EXE)

test-windows :
	x86_64-w64-mingw32-gcc test-windows-audio.c -o tone.exe
	wine tone.exe