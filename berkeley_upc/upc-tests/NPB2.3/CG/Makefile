SHELL=/bin/sh
BENCHMARK=cg
BENCHMARKU=CG

include ../config/make.def

OBJS = cg.o mmio.o ${COMMON}/c_print_results.o ${COMMON}/c_${RAND}.o \
       ${COMMON}/c_timers.o ${COMMON}/c_wtime.o

include ../sys/make.common

${PROGRAM}: config ${OBJS}
	${CLINK} ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${C_LIB} -lcxml


cg.o:		cg.upc npbparams.h
	${UPCCOMPILE} cg.upc

mmio.o:		mmio.c
	${UPCCOMPILE} $<

clean:
	- rm -f *.o *~ *.w2c.* *startup* 
	- rm -f npbparams.h core



