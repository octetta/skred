EXE = \
	sok1 \
	skred \
  rle1

all : $(EXE)

ELIB = \
	linenoise.o

LIB = \
	-lm \
	-lasound \
  -pthread \
	-lpthread

COPTS = \
  -D_GNU_SOURCE \
  -g

RLINC = -I raylib-quickstart-main/build/external/raylib-master/src
RLLIB = -L raylib-quickstart-main/bin/Debug

sok1 : sok1.o $(ELIB)
	gcc $(COPTS) $^ -o $@ $(LIB)

#skred : skred.o $(ELIB)
#	gcc $(COPTS) $^ -o $@ $(LIB)

#skred.o: skred.c
#	gcc $(COPTS) -c $<

skred.o: skred.c
	gcc -DUSE_RAYLIB $(RLINC) $(COPTS) -c $<

seq.o: seq.c
	gcc $(COPTS) -c $<

skred : skred.o seq.o $(ELIB)
	gcc -DUSE_RAYLIB $(RLINC) $(RLLIB) $(COPTS) $^ -o $@ $(LIB) -lraylib

sok1.o: sok1.c	
	gcc $(COPTS) -c $<

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

rle1: rle1.c
	gcc $(RLINC) $(RLLIB) rle1.c -o rle1 -lraylib -lm
 
clean :
	rm -f *.o
	rm -f $(EXE)