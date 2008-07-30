TARGET = BARNES-$(NETWORK)$(OPT)
FILES = code.upc code_io.upc load.upc grav.upc getparam.c util.c \
	code.h defs.h stdinc.h vectmath.h
CS = code.c code_io.c load.c grav.upc getparam.c util.c 
OBJS = code.o code_io.o load.o grav.o getparam.o util.o
CFLAGS =  
LDFLAGS =  
PTH = 
MACROS = ../../null_macros/c.m4.null
M4 = m4 -s -Ulen -Uindex
UPC_FLAGS = $(PTH) -T $(THREADS) $(OPT)
x = *
UPCC = upcc   
CC = gcc

$(TARGET): $(OBJS)
	$(UPCC) $(OBJS) $(CFLAGS) $(UPC_FLAGS) -o $(TARGET) $(LDFLAGS)

.SUFFIXES:
.SUFFIXES:	.o .upc .c .h 


.upc.o:
	$(UPCC) -c $(UPC_FLAGS) $*.upc

.c.o:
	$(CC) -c $(CFLAGS) $*.c

code.h: defs.h
defs.h: stdinc.h vectmath.h
vecmath.h:

code.o: code.upc code.h defs.h vecmath.h
code_io.o: code_io.upc code.h vecmath.h
getparam.o: getparam.c stdinc.h
grav.o: grav.upc code.h vecmath.h
load.o: load.upc code.h defs.h vecmath.h
util.o: util.c stdinc.h
clean:
	rm -f *.o BARNES-* *~ *.N *.B *.i *.t *.trans.c *o_temps
