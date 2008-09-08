
MYTARGET = gups-barrier-handle

SRCS   = $(MYTARGET).upc
OBJS   = $(SRCS:.upc=.o)
INCLS  = -I.
UPCC   = upcc
UPCC_FLAGS =  #what's the default name for testing?

$(MYTARGET) $(MYTARGET)_st%: 
	$(UPCC) -o $@ $(UPCC_FLAGS) $(SRCS) $(INCLS)

run: $(MYTARGET)
	./$(MYTARGET)   

clean:
	-rm -f $(OBJS) $(MYTARGET) $(MYTARGET)_st* *.out *.err
