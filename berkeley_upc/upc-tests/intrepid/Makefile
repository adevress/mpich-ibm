UPCC = upcc
THREADS = 
UPC_FLAGS =  
SINGLE_UNIT_TESTS = test00 test01 test02 test03 test04 test05 test06 \
           test09 test10 test11 test12 \
	   test13 test14 test15 test16 test17 test18 test19 test20
SEP_UNIT_TESTS = test07 test08
UPC_TESTS = test00 test01 test02 test03 test04 test05 test06 \
	   test07 test08 \
           test09 test10 test11 test12 \
	   test13 test14 test15 test16 test17 test18 test19 test20
RUN = upcrun
PROCS = -n 2


.SUFFIXES: .upc .upo
default: $(UPC_TESTS)

.upc.o: 
	$(UPCC) -c $(UPC_FLAGS) -o $@ $<


$(SINGLE_UNIT_TESTS): %: %.upc
	$(UPCC)  $(UPC_FLAGS) -o $@ $<

runall:
	for t in $(UPC_TESTS); do\
	$(RUN) $(PROCS) $$t;\
	done

test07: test07.o test07_sep.o
	$(UPCC) -o test07 test07.o test07_sep.o  

test08: test08.o test08_sep.o
	$(UPCC) -o test08 test08.o test08_sep.o  

clean:
	-rm -f *.upo  whirl2c.h *.w2c.c *.w2c.h $(UPC_TESTS) \
	    *startup* *.o *global* *_symbols* *.B *.N *.t *.i *.upc.c *.o
