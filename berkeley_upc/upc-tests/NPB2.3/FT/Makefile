SHELL=/bin/sh
BENCHMARK=ft
BENCHMARKU=FT

UPC_COLL_INC = ../../../collective/include
UPC_COLL_LIB = ../../../collective/lib

include ../config/make.def

OBJS = ${COMMON}/c_${RAND}.o ${COMMON}/c_print_results.o \
       ${COMMON}/c_timers.o ${COMMON}/c_wtime.o #../omp-prof.o

include ../sys/make.common

CCOMPILE += -I$(COMMON) -DDEBUG # -I$(UPC_COLL_INC) #-DDEBUG_OUTPUT
#CLINKFLAGS += -L$(UPC_COLL_LIB) # -lupc_coll-$(NP)

all: $(PROGRAM) 
#$(BINDIR)/ft-seq

${PROGRAM}: config ft.o ${OBJS}
	${CLINK} ft.o ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${C_LIB}

$(BINDIR)/ft-seq: config ft-seq.o $(OBJS)
	cc $(C_LIB) ft-seq.o $(OBJS) -o $@

ft.o: ft.upc global.h npbparams.h
	${UPCCOMPILE} $< 

ft-seq.o: ft-seq.c global-seq.h npbparams.h
	cc -c $(C_INC) ft-seq.c -o $@

ft-seq.c: ft.c global-seq.h
	cat $< | $(UPC2C) > $@
	cat $@ | sed -es'/"\(\w\+\).h"/"\1-seq.h"/g' > $@.tmp
	mv $@.tmp $@

global-seq.h: global.h
	cat $< | $(UPC2C) > $@

clean:
	-rm -f *.o *~ mputil*
	-rm -f ft npbparams.h core
	-rm -f ft-seq.[och]
