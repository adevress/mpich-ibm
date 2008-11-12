DIRS = driver.nue gccfe be whirl2c wopt ipl lno targ_info libiberty libcomutil

all:
	
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d || exit 1; \
	done 
	test -f driver.nue/sgiupc || ( cd driver.nue && ln -s driver sgiupc )

clean:
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d clean || exit 1; \
	done

.PHONY: all clean

