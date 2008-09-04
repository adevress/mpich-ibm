TARGET = RandomAccess_UPC

SRCS   = $(TARGET).upc
OBJS   = $(SRCS:.upc=.o)

UPCC   = upcc
UPCC_FLAGS = -Iinclude

$(TARGET) $(TARGET)_st%:
	$(UPCC) -o $@ $(UPCC_FLAGS) $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	-rm -f $(OBJS) $(TARGET) $(TARGET)_st* *.out *.err

