# C and C++ compilers to use for building the translator
CC = gcc
CXX = g++

# Default location for installation
PREFIX = /usr/local/bupc_translator

# override default build directory (empty for auto-detect)
BUILDDIR=

# override if your version of copy doesn't like these flags
CPYDIR = cp -R -p

#-------------------------------------------------------
# You should not need to modify anything below this line

all clean install:
	@echo "------------------------------------------------------------" ; \
	if test "$(BUILDDIR)" ; then			\
	  echo "WARNING: forcing BUILDDIR=$(BUILDDIR)"; \
	  builddir="$(BUILDDIR)" ;			\
	else						\
	  CC="$(CC)"; export CC;			\
	  CXX="$(CXX)"; export CXX;			\
	  builddir=`config-aux/config-builddir` ;	\
	fi ; 						\
	if test "$$builddir" ; then			\
	  echo "Making $@ in BUILDDIR=$$builddir" ;	\
	  echo " $(CC) is: `$(CC) --version 2>&1 | head -1`" ; \
	  echo " $(CXX) is: `$(CXX) --version 2>&1 | head -1`" ; \
	  echo "------------------------------------------------------------" ; \
	  if test "$$builddir" = "targ_g4_osx" ; then   \
	    strip=NOSTRIP=1 ; export strip ;            \
	  fi ;                                          \
	  $(MAKE) BUILDDIR="$$builddir" $$strip $@-internal ;	\
	else						\
	  exit 1;					\
	fi

all-internal:
	$(MAKE) -C open64/osprey1.0/$(BUILDDIR) all CC="$(CC)" CXX="$(CXX)"
	@echo "------------------------------------------------------------" 
	@echo " *** Build complete! *** "
	@echo ""
	@echo "You may now install the compiler with:"
	@echo ""
	@echo "  $(MAKE) install PREFIX=/prefix/to/use"
	@echo ""
	@echo "Alternatively, to use this translator directly from the build tree:"
	@echo ""
	@echo "for LOCAL compilations, add the following path in the 'translator'"
	@echo " setting of the runtime's upcc.conf or private ~/.upccrc file, "
	@echo " or pass it to 'upcc -translator=...'"
	@echo ""
	@echo " translator = `pwd`/open64/osprey1.0/$(BUILDDIR)"
	@echo ""
	@echo "for REMOTE (ssh-based) compilations, use the path:"
	@echo ""
	@echo " translator = `whoami`@`hostname`:`pwd`/open64/osprey1.0/$(BUILDDIR)"
	@echo ""
	@echo "------------------------------------------------------------" 

clean-internal:
	$(MAKE) -C open64/osprey1.0/$(BUILDDIR) clean

# Files to install
INSTALL_LIST = \
	libupc/upcr_trans_extra.c		\
	libupc/upcr_trans_extra.w2c.h		\
	$(BUILDDIR)/be/be			\
	$(BUILDDIR)/driver.nue/driver		\
	$(BUILDDIR)/driver.nue/sgiupc		\
	$(BUILDDIR)/gccfe/gfec			\
	$(BUILDDIR)/whirl2c/whirl2c		\
	$(BUILDDIR)/whirl2c/whirl2c_be		\
	`find $(BUILDDIR) -name '*.so'`		

install-internal:
	@mkdir -p "$(PREFIX)" || exit $? ; 				\
	prefix=`cd "$(PREFIX)" ; pwd` ; 				\
	cd open64/osprey1.0 ; 						\
	for file in $(INSTALL_LIST) ; do 				\
	  fdir=`dirname "$$file" | perl -pe 's@$(BUILDDIR)@targ@'` ; 	\
	  fbase=`basename "$$file"` ; 					\
	  instdir="$$prefix/$$fdir" ; 					\
	  mkdir -p "$$instdir" || exit $? ; 				\
	  echo "Installing: $$instdir/$$fbase" ; 			\
	  $(CPYDIR) "$$file" "$$instdir/$$fbase" || exit $? ; 		\
	  if test -x "$$instdir/$$fbase" &&                             \
	     test "$(NOSTRIP)" = "" ; then                              \
	    echo "Stripping: $$instdir/$$fbase" ; 			\
	    strip "$$instdir/$$fbase" || echo "  FAILURE IGNORED" ;     \
	  fi ;                                                          \
	done ; 								\
	echo "------------------------------------------------------------------------------" ; \
	echo " *** Installation complete! ***" ; 						\
	echo " " ; 										\
	echo " To use this translator LOCALLY, insert the following pathname:" ;		\
	echo " " ; 										\
	echo "    $$prefix/targ" ; 								\
	echo " " ; 										\
	echo " in the 'translator' setting of upcc.conf or ~/.upccrc, " ; 			\
	echo " or pass it to upcc -translator=path" ; 						\
	echo " " ; 										\
	echo " for REMOTE (ssh-based) compilations, use the path:" ; 				\
	echo " " ; 										\
	echo " translator = `whoami`@`hostname`:$$prefix/targ" ; 				\
	echo " " ; 										\
	echo "------------------------------------------------------------------------------" 


# Files and directory trees to always distribute
EXTRA_DIST = \
	Makefile \
	LICENSE.TXT \
	README \
	config-aux \
	open64/osprey1.0/libupc/upcr_trans_extra.c \
	open64/osprey1.0/libupc/upcr_trans_extra.w2c.h \
	open64/osprey1.0/gccfe/gnu/gstab.h \
	open64/osprey1.0/gccfe/gnu/stab.def \
	open64/osprey1.0/targia32_ia64_nodebug \
	open64/osprey1.0/build_ia64 \
	open64/osprey1.0/targ_alpha_tru64 \
	open64/osprey1.0/targ_g4_osx \
	open64/osprey1.0/targ_ppc_aix \
	open64/osprey1.0/be/be/so_locations \
	open64/osprey1.0/driver/sort_options.*

# Files/directories to never distribute
NO_DIST = \
	./clean-stamp \
	./nodist-list

nodist-list: 
	$(MAKE) update-nodist-list

# re-create the list of files to prune for distribution
update-nodist-list:
	$(MAKE) clean
	touch clean-stamp
	sleep 60 # sleep to ensure timestamp update
	$(MAKE) all # build everything
	# now touch anything that needs to be kept for EXTRA_DIST
	for file in $(EXTRA_DIST) ; do \
	  if test -f $$file ; then \
	    head $$file > /dev/null ; \
	  else \
	    find $$file -type f -exec head {} \; > /dev/null ; \
	  fi ; \
        done
	sleep 60 # sleep to ensure timestamp update
	find -type f \! -anewer clean-stamp -print > nodist-list # scan for untouched files
	find -type d -name CVS -print >> nodist-list # remove CVS goop
	# remove hard-coded nodist files
	for file in $(NO_DIST) ; do \
	  echo $$file >> nodist-list ; \
	done
	# filter out a few known patterns of extra-dist files, and produce a canonical list
	mv nodist-list nodist-list.old
	cat nodist-list.old | \
	    grep -v README | \
            grep -v Exported | \
            sort | uniq > nodist-list
	rm -f nodist-list.old

TARBALL_NAME = berkeley_upc_translator.tar.gz

# create a pruned translator distribution
dist: nodist-list
	@echo --- Checking source tree...
	@if test "`find . -name '*.o' -print`" != "" ; then \
	  echo ERROR: make dist should only be run on a CLEAN CVS checkout of the translator source tree ; \
	  exit 1 ; \
        fi
	@echo --- Pruning source tree...
	for file in `cat ./nodist-list` ; do \
	  rm -Rf $$file ; \
	done
	@echo --- Preparing translator distribution tarball...
	rm -f $(TARBALL_NAME)
	dir=`pwd` ; dir=`basename $$dir` ; cd .. ; \
        tar cvpf - $$dir | gzip -9 -c > $(TARBALL_NAME) ; \
	mv -f $(TARBALL_NAME) $$dir
	@echo $(TARBALL_NAME) is ready for testing and distribution.

force:

.PHONY: all clean force dist update-nodist-list

