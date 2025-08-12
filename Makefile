EXE = \
	sok1 \
	skred

all : $(EXE)

ELIB = \
	linenoise.o

LIB = \
	-lm \
	-lasound \
	-lpthread

sok1 : sok1.o $(ELIB)
	gcc $^ -o $@ $(LIB)

skred : skred.o $(ELIB)
	gcc -g $^ -o $@ $(LIB)

old :
	# gcc sok1.o linenoise.o -o sok1 $(LIB)

sok1.o: sok1.c	
	gcc -c $<
skred.o: skred.c
	gcc -g -c $<

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

clean :
	rm -f *.o
	rm -f $(EXE)