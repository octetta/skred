EXE = \
	skred \
	scope-shared \
	wav2data \
	# scope \
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

scope-shared.o : scope-shared.c scope-shared.h
	gcc $(COPTS) -c $<

scope-shared : scope-shared.c
	gcc -DSCOPE_SHARED_DEMO $^ -o $@

scope : scope.c scope-shared.o raylib-quickstart-main/Makefile
	gcc -g -D_GNU_SOURCE -DUSE_RAYLIB $(RLINC) $(RLLIB) $^ -o $@ -lraylib -lm

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
  scope-shared.o \
  miniaudio.o \
	linenoise.o \
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