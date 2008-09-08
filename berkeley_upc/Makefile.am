AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies
ACLOCAL_AMFLAGS   = -I gasnet

# Note: current directory '.' must come before totalview directory.
SUBDIRS       = gasnet umalloc upcr_geninclude upcr_preinclude upcr_extinclude \
		upcr_postinclude man detect-upc contrib harness config-aux \
		docs profile . @TOTALVIEW_SUBDIR@

# .pl files to install in bindir, but w/o exec permissions
dotpldir = $(bindir)
dotpl_DATA = upcc.pl upcrun.pl

makefile_fragments = upcr.mak upcc.mak
conf_files    = upcc.conf upcrun.conf
bin_SCRIPTS   = upcc upcrun
noinst_SCRIPTS= gcc_as_cc.pl
config_DATA   = $(CONFIG_HEADER)
configdir     = $(includedir)
docs          = ChangeLog README README.developers 
EXTRA_DIST    = $(docs)                         \
		.autom4te.cfg                   \
                Bootstrap                       \
                unBootstrap                     \
                README                          \
                LICENSE.TXT                     \
                upc-examples                    \
                upc-tests                       \
                gcc_as_cc.pl                    \
		Makefile.multiconf              \
		multiconf.conf.in               \
		multiconf.pl                    \
		upcc_multi.in                   \
		upcc_multi.pl                   \
		$(dotpl_DATA)

# .pl libraries needed by $(dotpl_DATA)
perl_libs = \
	upcr_ctuple.pl \
	upcr_getopt.pl \
	upcr_util.pl

libupcr_hdrs= upcr.h upcr_alloc.h upcr_barrier.h                    	\
                 upcr_err.h upcr_handler_decls.h upcr_handlers.h       	\
                 upcr_init.h upcr_internal.h upcr_locks.h              	\
                 upcr_threads.h upcr_shaccess.h upcr_sptr.h            	\
                 upcr_util.h upcr_translator_tld.h       		\
                 upcr_memcpy.h upcr_collective.h        		\
                 bupc_extern.h upcr_io.h upcr_gccupc.h upcr_syshdrs.h   \
                 upcr_sem.h upcr_profile.h upcr_atomic.h

include_HEADERS= $(perl_libs) $(libupcr_hdrs)

#
# Each version of the runtime library is built via a recursive make call--we
# need this since each requires different CFLAGS settings, GASNET libs, etc.  
#
# We include upcr.mak, which figures out which GASNet .mak fragment has the
# correct settings for the version of the runtime library we want to build.
# We have to do this in a separate file since we can't put GNU make conditionals
# in this file (or automake will barf).
#
# Silly: automake uses 'include' for its own purposes, so use an autoconf
# variable to sneak one past it to regular GNU make...
@MAKE_INCLUDE@ upcr.mak

libupcr_srcs = upcr_alloc.c upcr_barrier.c upcr_err.c upcr_init.c \
               upcr_locks.c upcr_broadcast.c upcr_handlers.c \
               upcr_sptr.c upcr_threads.c upcr_util.c upcr_globfiles.c \
               upcr_gccupc.c upcr_memcpy.c upcr_collective.c \
               upcr_coll_templates.c upcr_extern.c upcr_io.c upcr_sem.c \
               upcr_sizes.c upcr_totalview.c upcr_profile.c upcr_atomic.c

# Because of tracing/stats macros we must recompile when GASNet headers change.
# We avoid needing to keep the list current by using overkill.
# Note that using $(shell find ...) makes automake very unhappy.
gasnet_hdrs = gasnet/gasnet_config.h  \
              $(srcdir)/gasnet/*.h    \
              $(srcdir)/gasnet/*/*.h  \
              $(srcdir)/gasnet/*/*/*.h

libupcr_deps = $(libupcr_srcs) $(libupcr_hdrs) $(gasnet_hdrs) acconfig.h \
	       upcr_config.h upcr_preinclude/upc_collective_bits.h \
	       upcr_preinclude/upc_io_bits.h upcr_preinclude/upc_bits.h \
               $(srcdir)/umalloc/*.h

# List of all libraries that can be built: needed by autoconf
EXTRA_LIBRARIES     = libupcr-mpi-seq.a   libupcr-mpi-par.a  libupcr-mpi-tv.a   \
                      libupcr-udp-seq.a   libupcr-udp-par.a  libupcr-udp-tv.a   \
                      libupcr-elan-seq.a  libupcr-elan-par.a libupcr-elan-tv.a  \
                      libupcr-lapi-seq.a  libupcr-lapi-par.a libupcr-lapi-tv.a  \
                      libupcr-gm-seq.a    libupcr-gm-par.a   libupcr-gm-tv.a    \
                      libupcr-vapi-seq.a  libupcr-vapi-par.a libupcr-vapi-tv.a  \
                      libupcr-ibv-seq.a   libupcr-ibv-par.a  libupcr-ibv-tv.a  \
                      libupcr-shmem-seq.a                                       \
                      libupcr-sci-seq.a   libupcr-sci-par.a  libupcr-sci-tv.a   \
                      libupcr-smp-seq.a   libupcr-smp-par.a  libupcr-smp-tv.a   \
                      libupcr-portals-seq.a libupcr-portals-par.a libupcr-portals-tv.a \
                      libupcr-dcmf-seq.a libupcr-dcmf-par.a 

# list of libraries to actually build, determined at configure time
lib_LIBRARIES       = @TARGETLIBS@

LIBTESTS=$(lib_LIBRARIES:.a=-test)

# Each version of the library is built via a recursive make call with
# different parameters

libupcr_mpi_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-mpi-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=mpi UPCR_PARSEQ=seq build_lib

libupcr_mpi_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-mpi-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=mpi UPCR_PARSEQ=par build_lib

libupcr_mpi_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-mpi-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=mpi UPCR_PARSEQ=tv build_lib

libupcr_udp_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-udp-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=udp UPCR_PARSEQ=seq build_lib

libupcr_udp_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-udp-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=udp UPCR_PARSEQ=par build_lib

libupcr_udp_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-udp-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=udp UPCR_PARSEQ=tv build_lib

libupcr_elan_seq_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-elan-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=elan UPCR_PARSEQ=seq build_lib

libupcr_elan_par_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-elan-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=elan UPCR_PARSEQ=par build_lib

libupcr_elan_tv_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-elan-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=elan UPCR_PARSEQ=tv build_lib

libupcr_lapi_seq_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-lapi-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=lapi UPCR_PARSEQ=seq build_lib

libupcr_lapi_par_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-lapi-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=lapi UPCR_PARSEQ=par build_lib

libupcr_lapi_tv_a_SOURCES  = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-lapi-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=lapi UPCR_PARSEQ=tv build_lib

libupcr_gm_seq_a_SOURCES    = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-gm-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=gm UPCR_PARSEQ=seq build_lib

libupcr_gm_par_a_SOURCES    = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-gm-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=gm UPCR_PARSEQ=par build_lib

libupcr_gm_tv_a_SOURCES    = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-gm-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=gm UPCR_PARSEQ=tv build_lib

libupcr_smp_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-smp-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=smp UPCR_PARSEQ=seq build_lib

libupcr_smp_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-smp-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=smp UPCR_PARSEQ=par build_lib

libupcr_smp_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-smp-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=smp UPCR_PARSEQ=tv build_lib

libupcr_sci_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-sci-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=sci UPCR_PARSEQ=seq build_lib

libupcr_sci_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-sci-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=sci UPCR_PARSEQ=par build_lib

libupcr_sci_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-sci-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=sci UPCR_PARSEQ=tv build_lib

libupcr_shmem_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-shmem-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=shmem UPCR_PARSEQ=seq build_lib

libupcr_vapi_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-vapi-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=vapi UPCR_PARSEQ=seq build_lib

libupcr_vapi_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-vapi-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=vapi UPCR_PARSEQ=par build_lib

libupcr_vapi_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-vapi-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=vapi UPCR_PARSEQ=tv build_lib

libupcr_ibv_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-ibv-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=ibv UPCR_PARSEQ=seq build_lib

libupcr_ibv_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-ibv-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=ibv UPCR_PARSEQ=par build_lib

libupcr_ibv_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-ibv-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=ibv UPCR_PARSEQ=tv build_lib

libupcr_portals_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-portals-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=portals UPCR_PARSEQ=seq build_lib

libupcr_portals_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-portals-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=portals UPCR_PARSEQ=par build_lib

libupcr_portals_tv_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-portals-tv.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=portals UPCR_PARSEQ=tv build_lib

libupcr_dcmf_seq_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-dcmf-seq.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=dcmf UPCR_PARSEQ=seq build_lib

libupcr_dcmf_par_a_SOURCES   = $(libupcr_srcs) $(libupcr_hdrs)
libupcr-dcmf-par.a: $(libupcr_deps) 
	$(MAKE) UPCR_CONDUIT=dcmf UPCR_PARSEQ=par build_lib



if UPCR_OBJECT_STRINGS
else
# dirty hack to make our sizes strings scannable on platforms like X1, 
# where character arrays are stored compressed in the object file
# build and link a minimal GASNet program including our strings, 
# then scan them into a header where libupcr can compile them in using GASNETI_IDENT
UPCR_SIZES_HEADER=upcr_sizes-$(UPCR_CONDUIT)-$(GASNET_PARSEQ).h
UPCR_SIZES_ACTIVATE=-DUPCRI_SIZES_H=$(UPCR_SIZES_HEADER)
UPCR_SIZES_BASE=upcr_sizes-$(UPCR_CONDUIT)-$(GASNET_PARSEQ)
UPCR_SIZES_CLEAN=upcr_sizes-*-*.h
$(UPCR_SIZES_HEADER): $(libupcr_deps)
	$(UPCR_CC) -c $(EXTRA) $(UPCR_CPPFLAGS) -DUPCRI_SIZES_DUMMY $(UPCR_CFLAGS) \
          -o $(UPCR_SIZES_BASE).o $(srcdir)/upcr_sizes.c 
	$(UPCR_LD) $(UPCR_LDFLAGS) -o $(UPCR_SIZES_BASE) $(UPCR_SIZES_BASE).o $(GASNET_LIBS)
	@PERL@ -044 -ne 'if (/^(UPCRSizeof: [^\$$]+\$$)/) { \
           print "GASNETT_IDENT(upcri_IdentString_sizes_".++$$i.",\"\$$$$1\");\n";}' \
           $(UPCR_SIZES_BASE) > $@
	test -s $@
	rm -f $(UPCR_SIZES_BASE) $(UPCR_SIZES_BASE).o
endif

if BUILDCONFIG_GASP
  UPCR_GASP_CPPFLAGS=-DUPCRI_INST_UPCCFLAG=1
endif

# templatized library build 
$(gasnet_makfile):
	$(MAKE) -C gasnet/$(UPCR_CONDUIT)-conduit $(UPCR_CONDUIT)-$(GASNET_PARSEQ).mak

build_lib: $(gasnet_makfile) $(UPCR_SIZES_HEADER)
	$(UPCR_CC) -c $(EXTRA) $(UPCR_CPPFLAGS) $(UPCR_GASP_CPPFLAGS) $(UPCR_SIZES_ACTIVATE) $(UPCR_CFLAGS) \
          -o upcr_globfiles-$(UPCR_CONDUIT)-$(UPCR_PARSEQ).o $(srcdir)/upcr_globfiles.c 
	$(AR) cru libupcr-$(UPCR_CONDUIT)-$(UPCR_PARSEQ).a upcr_globfiles-$(UPCR_CONDUIT)-$(UPCR_PARSEQ).o
	$(RANLIB) libupcr-$(UPCR_CONDUIT)-$(UPCR_PARSEQ).a
	-rm -f upcr_globfiles-$(UPCR_CONDUIT)-$(UPCR_PARSEQ).o

# make sure certain files get rebuilt if their source files changed.
all-local: $(makefile_fragments) $(conf_files) $(bin_SCRIPTS) \
	$(perl_libs) $(lib_LIBRARIES) upc_trace

# Things to clean
CLEANFILES = $(LIBTESTS) $(UPCR_SIZES_CLEAN)
DISTCLEANFILES = config.status.bak upc_trace $(files_we_copy)
distclean-local:
	rm -Rf .subst_text

$(top_builddir)/gasnet/other/perlstart: $(top_srcdir)/gasnet/other/perlstart.in
	@$(MAKE) -C $(top_builddir)/gasnet/other perlstart
upcc: $(top_builddir)/gasnet/other/perlstart upcc.pl $(perl_libs)
upcrun: $(top_builddir)/gasnet/other/perlstart upcrun.pl $(perl_libs)

upc_trace: force
	@$(MAKE) -C $(top_builddir)/gasnet/other/contrib gasnet_trace
	@rm -f $@
	ln -s $(top_builddir)/gasnet/other/contrib/gasnet_trace $@

# Things to copy from srcdir to buildir when they differ
if BUILD_IS_SRC
files_we_copy =
else
files_we_copy =  $(dotpl_DATA) $(perl_libs)
$(files_we_copy): force
	@srcfile="$(srcdir)/`basename $@`" ; \
         if test ! -f "$@" -o "`find $$srcfile -newer '$@' 2>&1`" ; then \
          echo cp -f "$$srcfile" . ;         \
          cp -f "$$srcfile" . ;              \
         fi
endif

install-data-local: $(makefile_fragments) $(conf_files)
	$(mkinstalldirs) $(DESTDIR)$(includedir)
	@list='$(makefile_fragments)'; for p in $$list; do \
	  if test -f $$p; then \
	    filename=`basename $$p`; \
	    echo "@GASNET_INSTALL_CMD@ < $$p > $(DESTDIR)$(includedir)/$$filename"; \
	    @GASNET_INSTALL_CMD@ < $$p > $(DESTDIR)$(includedir)/$$filename || exit $$? ; \
	  else :; fi; \
	done 
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	@list='$(conf_files)'; for p in $$list; do \
	  if test -f $$p; then \
	    filename=`basename $$p`; \
	    echo "@GASNET_INSTALL_CMD@ < $$p > $(DESTDIR)$(sysconfdir)/$$filename"; \
	    @GASNET_INSTALL_CMD@ < $$p > $(DESTDIR)$(sysconfdir)/$$filename || exit $$? ; \
	  else :; fi; \
	done 

install-exec-local:
	@rm -f $(DESTDIR)$(bindir)/upc_trace
	ln -s gasnet_trace $(DESTDIR)$(bindir)/upc_trace

uninstall-local:
	@rm -f $(DESTDIR)$(bindir)/upc_trace
	@list='$(makefile_fragments)'; for p in $$list; do \
	  if test -f $$p; then \
	    filename=`basename $$p`; \
	    echo " rm -f $(DESTDIR)$(includedir)/$$filename;"; \
	    rm -f $(DESTDIR)$(includedir)/$$filename || exit $$? ; \
	  else :; fi; \
	done
	@list='$(conf_files)'; for p in $$list; do \
	  if test -f $$p; then \
	    filename=`basename $$p`; \
	    echo " rm -f $(DESTDIR)$(sysconfdir)/$$filename;"; \
	    rm -f $(DESTDIR)$(sysconfdir)/$$filename || exit $$? ; \
	  else :; fi; \
	done

dist-hook:
	@if grep 'UPCRI_CONFIGURE_ARGS.*--enable-totalview' upcr_config.h; then :; else \
	  echo "TO 'make dist' YOU MUST CONFIGURE with --enable-totalview"; exit -1; fi
	find $(distdir) -depth -name CVS -type d -exec rm -Rf '{}' \;
	find $(distdir) -name .#\* -type f -exec rm -f '{}' \;

# If we've disabled LEX, ensure it stays disabled for 'make distcheck'
if UPCR_USE_LEX
UPCR_LEX_CONFIG_FLAG=
else
UPCR_LEX_CONFIG_FLAG=--without-lex
endif
DISTCHECK_CONFIGURE_FLAGS = $(UPCR_LEX_CONFIG_FLAG) --enable-totalview --without-multiconf

force:

if BUILDCONFIG_GASP
  UPCC_TEST=$(top_builddir)/profile/dump/upcc-dump -inst
  TESTPROGRAM=$(top_srcdir)/upc-tests/benchmarks/gasp_test.upc 
else
  # tests backend compile only
  # TESTPROGRAM=$(top_srcdir)/upc-tests/upcr-tests/test-compiles/compiles.trans.c
  # tests translate and backend compile
  TESTPROGRAM=$(top_srcdir)/upc-tests/bugzilla/bug87.upc

  UPCC_TEST=$(top_builddir)/upcc
endif

# targets conduitname-{seq,par,tv,all} build everything required 
# to build and run a working UPC program using that conduit config
%-seq %-par %-tv: force 
	@conduit=`echo '$@' | $(PERL) -pe 's/-[^-]+$$//'` ; 		\
	 conduitcfg=`echo '$@' | $(PERL) -pe 's/[^-]+-//; s/tv/par/'` ;	\
         $(MAKE) -C gasnet/$$conduit-conduit $$conduitcfg
	@$(MAKE) -C profile all
	@$(MAKE) libupcr-$@.a
	@$(MAKE) -C detect-upc all
	@$(MAKE) -C umalloc all
	@$(MAKE) upcc upcrun

%-all: force
	@conduit=`echo '$@' | $(PERL) -pe 's/-[^-]+$$//'` ; 	\
	$(MAKE) $$conduit-seq || exit $? ; 			\
	$(MAKE) $$conduit-par || exit $? ; 			\
        if test "$(TOTALVIEW_ENABLED)" = "yes"; then 		\
	  $(MAKE) $$conduit-tv || exit $? ; 			\
        fi

libupcr-%-seq-test: all
	$(UPCC_TEST) -o $@ -network $* -nopthreads $(TESTPROGRAM)

libupcr-%-par-test: all
	$(UPCC_TEST) -o $@ -network $* -pthreads -nolink-cache $(TESTPROGRAM)

libupcr-%-tv-test: all
	$(UPCC_TEST) -o $@ -network $* -tv -nolink-cache $(TESTPROGRAM)

tests-local:
	$(MAKE) $(LIBTESTS)

tests-hello:
	@$(MAKE) $(LIBTESTS) TESTPROGRAM=$(top_srcdir)/upc-examples/hello.upc
	@echo ' '
	@echo '       ==== UPC "HELLO WORLD" TESTS SUCCESSFULLY COMPILED ===='
	@echo ' '
	@echo 'Your UPC compilation setup appears to be working for all detected networks:'
	@echo ' '
	@echo '      $(CONDUITS)'
	@echo ' '
	@echo 'You should now test UPC runtime operation for each network of interest. '
	@if test -x libupcr-smp-par-test ; then \
	 echo 'Start by testing the single-node smp/pthreaded network, with a command like:' ;  \
	 echo ' ' ; \
	 echo "  ./upcrun -n 2 libupcr-smp-par-test" ;  \
	fi
	@if test "@DEFAULT_CONDUIT@" != "smp" ; then \
	 echo ' ' ; \
	 echo 'Next, ensure you are in a proper environment for launching parallel jobs' ; \
	 echo '(eg batch system session, if necessary) and then issue a command like:' ; \
	 echo ' ' ; \
	 echo "  ./upcrun -n 2 libupcr-@DEFAULT_CONDUIT@-seq-test" ; \
	 echo ' ' ; \
	 echo 'Any similarly for any other networks listed above.' ; \
	fi
	@echo ' '

tests-clean:
	-rm -f $(LIBTESTS)
	$(MAKE) -C $(top_builddir)/gasnet $@

tests tests-seq tests-par tests-parsync run-tests run-tests-seq run-tests-par run-tests-parsync: tests-local
	$(MAKE) -C $(top_builddir)/gasnet $@

test: tests

check: tests-local check-exports

if USING_GCCUPC
GCCUPC_EXPORTS = -e ' [\._]*upc_' -e ' [\._]*gccupc_' \
                 -e ' [\._]*THREADS' -e ' [\._]*MYTHREAD' \
		 -e ' [\.]*___*put' -e ' [\.]*___*get'  \
		 -e ' [\.]*___*cvtaddr' -e ' [\.]*__copy'
endif

if GNU_NM
check-exports: $(lib_LIBRARIES)
	@echo Checking libupcr exports...
	@for lib in $(lib_LIBRARIES) ; do                                           \
          failed=0 ;                                                                \
          echo $$lib: ;                                                             \
          echo ;                                                                    \
          @NM@ --defined-only $$lib |                                               \
            grep -v -e ' [\._]*upcr_' -e ' [\._]*upcri_' -e ' [\._]*bupc_'          \
                    -e ' [\._]*pupc_'                                               \
                    -e ' [\._]*gasneti_' -e ' [\._]*gasnete_' -e ' [\._]*gasnetc_'  \
                    -e ' [\._]*gasnet_' -e ' [\._]*gasnett_'                        \
                    -e __FUNCTION__ -e __PRETTY_FUNCTION__ -e ' [\._]*DWinfo'       \
                    -e ' [\._]*TV_s2s_' -e ' [\._]*MPIR_'                           \
                    -e ' [\._]*debug_info_seg' -e ' [\._]*debug_abbrev_seg'         \
                    -e ' [\._]*debug_frame_seg' -e ' [\._]*debug_line_seg'          \
                    -e ' [\._]*stab' -e ' [\._]*gnu_dev_'                           \
                    $(GCCUPC_EXPORTS)  |                                            \
            @PERL@ -n -e 'print if /^[0-9a-fA-F]+\s+[A-Z]\s+/' > .$$lib.exp;        \
          if test -s .$$lib.exp ; then                                              \
            cat .$$lib.exp ;                                                        \
            echo FAILED ;                                                           \
            failed=1 ;                                                              \
          else                                                                      \
            echo PASSED ;                                                           \
          fi ;                                                                      \
          rm -f .$$lib.exp ;                                                        \
        done ; exit $$failed
else
check-exports: $(lib_LIBRARIES)
	@echo check-exports test SKIPPED
endif

.PHONY: force test tests tests-local build_lib dist-hook check-exports

