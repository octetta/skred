EXE = \
	skred \
  rle1 \
  scanner

all : $(EXE)

ELIB = \
	linenoise.o

LIB = \
	-lm \
	-lasound \
  -pthread \
	-lpthread \
  -lrt

COPTS = \
  -D_GNU_SOURCE \
  -g

scanner : scanner.c $(ELIB)
	gcc $(COPTS) $^ -o $@

RLINC = -I raylib-quickstart-main/build/external/raylib-master/src
RLLIB = -L raylib-quickstart-main/bin/Debug

miniwav.o : miniwav.c miniwav.h
	gcc $(COPTS) -c $<

amysamples.o : amysamples.c amysamples.h
	gcc $(COPTS) -c $<

skred.o: skred.c
	gcc -DUSE_RAYLIB $(RLINC) $(COPTS) -c $<

seq.o: seq.c seq.h
	gcc $(COPTS) -c $<

motor.o: motor.c motor.h
	gcc $(COPTS) -c $<

skred : skred.o motor.o miniwav.o amysamples.o $(ELIB)
	gcc -DUSE_RAYLIB $(RLINC) $(RLLIB) $(COPTS) $^ -o $@ $(LIB) -lraylib

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

rle1: rle1.c
	gcc $(RLINC) $(RLLIB) rle1.c -o rle1 -lraylib -lm
 
clean :
	rm -f *.o
	rm -f $(EXE)