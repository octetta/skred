EXE = \
	skred \
	wav2data \
	scope \
	skode \
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

skode.o : skode.c
	gcc -DUSE_SEQ -DMAIN $(COPTS) -c $<

wav2data : wav2data.c miniwav.o
	gcc -g -D_GNU_SOURCE $^ -o $@

miniwav.o : miniwav.c miniwav.h
	gcc $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	gcc $(COPTS) -c $<

raylib-quickstart-main/Makefile :
	sh make-raylib
  
synth.o: synth.c synth.h synth-types.h
	gcc $(COPTS) -c $<

seq.o: seq.c seq.h
	gcc $(COPTS) -c $<

skode-seq.o: seq.c seq.h
	gcc -DSKODE -DUSE_SEQ $(COPTS) -c $< -o $@

udp.o: udp.c udp.h
	gcc $(COPTS) -c $<

wire.o: wire.c wire.h
	gcc $(COPTS) -c $<

skred.o: skred.c
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
  skode.o \
  miniwav.o \
  amysamples.o \
  synth.o \
  skode-seq.o \
  miniaudio.o \
	linenoise.o \
	skred-mem.o \
	util.o \
  #

skred : $(OBJS)
	gcc $(COPTS) $^ -o $@ $(LIB)

skode : $(SOBJS)
	gcc -DUSE_SEQ -DMAIN -g $^ -o $@ $(LIB)

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