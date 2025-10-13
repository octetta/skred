EXE = \
	skred \
	scope-shared \
	scope \
  #

all : $(EXE)

ELIB = \
	linenoise.o \
  #

LIB = \
	-lm \
	-lasound \
  -pthread \
	-lpthread \
  -lrt \
  #

COPTS = \
  -D_GNU_SOURCE \
  -g

RLINC = -I raylib-quickstart-main/build/external/raylib-master/src
RLLIB = -L raylib-quickstart-main/bin/Debug

scope-shared.o : scope-shared.c scope-shared.h
	gcc $(COPTS) -c $<

scope-shared : scope-shared.c
	gcc -DSCOPE_SHARED_DEMO $^ -o $@

scope : scope.c scope-shared.o
	gcc -g -D_GNU_SOURCE -DUSE_RAYLIB $(RLINC) $(RLLIB) $^ -o $@ -lraylib -lm

miniwav.o : miniwav.c miniwav.h
	gcc $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	gcc $(COPTS) -c $<

raylib-quickstart-main/Makefile :
	sh make-raylib
  
synth.o: synth.c synth.h synth-types.h
	gcc $(COPTS) -c $<

wire.o: wire.c wire.h
	gcc $(COPTS) -c $<

skred.o: skred.c raylib-quickstart-main/Makefile
	gcc $(COPTS) -c $<

skred : skred.o miniwav.o amysamples.o synth.o wire.o $(ELIB) scope-shared.o miniaudio.o
	gcc $(COPTS) $^ -o $@ $(LIB)

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

miniaudio.o: miniaudio.c miniaudio.h
	gcc -c $<

clean :
	rm -f *.o
	rm -f $(EXE)