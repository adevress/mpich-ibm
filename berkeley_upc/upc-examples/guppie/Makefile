
MYTARGET = guppie

SRCS   = $(MYTARGET).upc
OBJS   = $(SRCS:.upc=.o)

UPCC   = upcc
UPCC_FLAGS = 

$(MYTARGET) $(MYTARGET)_st%: 
	$(UPCC) -o $@ -DLTABSIZE=15L $(UPCC_FLAGS) $(SRCS)

run: $(MYTARGET)
	./$(MYTARGET) 

clean:
	-rm -f $(OBJS) $(MYTARGET) $(MYTARGET)_st* *.out *.err
