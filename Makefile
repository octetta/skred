EXE = \
	skred \
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

miniwav.o : miniwav.c miniwav.h
	gcc $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	gcc $(COPTS) -c $<

raylib-quickstart-main/Makefile :
	sh make-raylib
  
skred.o: skred.c raylib-quickstart-main/Makefile
	gcc -DUSE_RAYLIB $(RLINC) $(COPTS) -c $<

seq.o: seq.c seq.h
	gcc $(COPTS) -c $<

skred : skred.o miniwav.o amysamples.o $(ELIB)
	gcc -DUSE_RAYLIB $(RLINC) $(RLLIB) $(COPTS) $^ -o $@ $(LIB) -lraylib

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

clean :
	rm -f *.o
	rm -f $(EXE)