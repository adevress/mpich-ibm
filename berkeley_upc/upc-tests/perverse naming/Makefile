
UPCC   = upcc
UPCC_FLAGS = 

p1 p1_st%: 
	USER="crazy username" $(UPCC) -o $@ $(UPCC_FLAGS) '-Ireally perverse' 'perverse one.upc'

p2 p2_st%: 
	tar xf p2.tar
	mv quiteperverse 'quite\ ~`\!@#$$%^&*)(}{][;:\\\"'\''?><.,perverse'
	mv p2.upc 'per *!$$?,verse.upc'
	USER='crazy username~`\!@#$$%^&*)(}{][;:\' $(UPCC) -o $@ $(UPCC_FLAGS) '-Ireally perverse' 'quite\ ~`\!@#$$%^&*)(}{][;:\\\"'\''?><.,perverse'/'totally ~`!@#$$%^&*)(}{][;:\"'\''?><.,perverse.upc' 'per *!$$?,verse.upc'

clean:
	-rm -f p1 p1_st* p2 p2_st* *.out *.err
