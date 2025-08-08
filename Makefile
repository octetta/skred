EXE = \
	sok1 \
	sok2

all : $(EXE)

ELIB = \
	linenoise.o

LIB = \
	-lm \
	-lasound \
	-lpthread

sok1 : sok1.o $(ELIB)
	gcc $^ -o $@ $(LIB)

sok2 : sok2.o $(ELIB)
	gcc $^ -o $@ $(LIB)

old :
	# gcc sok1.o linenoise.o -o sok1 $(LIB)

sok1.o: sok1.c	
	gcc -c $<

linenoise.o: linenoise.c linenoise.h
	gcc -c $<

clean :
	rm -f *.o
	rm -f $(EXE)