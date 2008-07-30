SHELL=/bin/sh
BENCHMARK=mg
BENCHMARKU=MG
CLASS=S
NP=$(UPCTHREADS)

include ../config/make.def

OBJS = file_output.o \
       ${COMMON}/c_${RAND}.o ${COMMON}/c_print_results.o \
       ${COMMON}/c_timers.o ${COMMON}/c_wtime.o

include ../sys/make.common

ifdef VARIANT
OBJ-EXEC = ${BENCHMARK}.${CLASS}.${VARIANT}.${NP}.o
SOURCE = variants/${BENCHMARK}.${VARIANT}.c
EXEC = ${PROGRAM}.${VARIANT}.${NP}
CFLAGS += -I./
else
OBJ-EXEC = ${BENCHMARK}.${CLASS}.${NP}.o
SOURCE = ${BENCHMARK}.c
EXEC = ${PROGRAM}.${NP}
endif

CCOMPILE += -I$(COMMON) -o ${OBJ-EXEC}
CLINKFLAGS += 


${BENCHMARK}:
	echo "error: must compile with static threads"
	exit 1

${BENCHMARK}_st%:
	$(MAKE) clean
	$(MAKE) config $(OBJS) ${OBJ-EXEC}
	${CLINK} ${OBJ-EXEC} ${CLINKFLAGS} -o $@ ${OBJS} ${C_LIB}

all: $(PROGRAM)

${PROGRAM}: config ${OBJ-EXEC} ${OBJS}
	${CLINK} ${OBJ-EXEC} ${CLINKFLAGS} -o ${EXEC} ${OBJS} ${C_LIB}
#	mv ${EXEC} ${BINDIR}

${OBJ-EXEC}: ${SOURCE} npbparams.h file_output.h
	${CCOMPILE} ${SOURCE}

file_output.o: file_output.c file_output.h
	${CC} -c $(C_INC) $(CFLAGS) file_output.c

clean:
	rm -f *.o *~ ../common/*.o
	rm -f npbparams.h core ../sys/setconfig
