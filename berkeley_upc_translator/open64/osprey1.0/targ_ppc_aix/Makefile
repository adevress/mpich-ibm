DIRS = driver.nue gccfe be whirl2c wopt ipl lno


all:
	OBJECT_MODE=32 ; export OBJECT_MODE ; \
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d CC="$(CC) -maix32" CXX="$(CXX) -maix32" || exit 1; \
	done 
	if [ ! -f driver.nue/sgiupc ]; then (cd driver.nue && ln -s driver sgiupc); fi
#	test -f driver/sgiupc || ( cd driver && ln -s driver sgiupc )

clean:
	for d in $(DIRS) ; do \
	 $(MAKE) -C $$d clean; rm -fr CVS || exit 1; \
	done

.PHONY: all clean

