all: cpi mcpi

cpi_trans cpi_st%_trans:
	$(UPCC) $(UPCC_FLAGS) -trans cpi.upc

mcpi_trans mcpi_st%_trans:
	$(UPCC) $(UPCC_FLAGS) -trans mcpi.upc

cpi cpi_st%: cpi.upc
	$(UPCC) $(UPCC_FLAGS) -o $@ cpi.upc

mcpi mcpi_st%: mcpi.upc
	$(UPCC) $(UPCC_FLAGS) -o $@ mcpi.upc

run: cpi mcpi
	$(UPCRUN) -np $(THREADS) ./cpi
	$(UPCRUN) -np $(THREADS) ./mcpi
clean:
	rm -f cpi cpi_st* mcpi mcpi_st* *.trans.c *.B *.upc.c \
	*.global_data.c *.i *.t \
	*.w2c.* *.upc_symbols upcc-sizes *.N *.o *startup_tmp.c *~
