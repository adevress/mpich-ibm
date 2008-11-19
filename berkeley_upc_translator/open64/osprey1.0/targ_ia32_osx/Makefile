DIRS = driver.nue gccfe be whirl2c wopt ipl lno

all:
	
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d || exit 1; \
	done 
	if [ ! -f driver.nue/sgiupc ]; then (cd driver.nue && ln -s driver sgiupc); fi
#	test -f driver/sgiupc || ( cd driver && ln -s driver sgiupc )

clean:
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d clean; rm -fr CVS || exit 1; \
	done

.PHONY: all clean

