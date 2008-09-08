AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies

SUBDIRS       = .

scripts       = harness.pl runjobs.pl harness_util.pl
gen_scripts   = harness runjobs checkfail
docs          = README

spec_files = \
	cray-upc.spec		\
	gcc-upc.spec		\
	gcc-upc-3.spec		\
	hp-upc.spec		\
	ibm-upc.spec 		\
	upc-wrapper

gen_spec_files = \
	compiler.spec		\
	gcc-upc-upcr.spec	\
	gcc-upc-3-upcr.spec

EXTRA_DIST    = $(docs)			\
		sysconfs		\
		$(spec_files)		\
		$(scripts)

$(top_builddir)/gasnet/other/perlstart: $(top_srcdir)/gasnet/other/perlstart.in
	@$(MAKE) -C $(top_builddir)/gasnet/other perlstart
harness: $(top_builddir)/gasnet/other/perlstart
runjobs: $(top_builddir)/gasnet/other/perlstart

# make sure certain files get rebuilt if their source files changed.
all-local: $(scripts) $(gen_scripts) $(gen_spec_files)

# Conditionally copy files if srcdir and builddir differ
if BUILD_IS_SRC
files_we_copy =
else
files_we_copy =  $(scripts)
$(files_we_copy): force
	@srcfile="$(srcdir)/`basename $@`" ; \
         if test ! -f "$@" -o "`find $$srcfile -newer '$@' 2>&1`" ; then \
          echo cp -f "$$srcfile" . ;         \
          cp -f "$$srcfile" . ;              \
         fi
endif
DISTCLEANFILES = $(files_we_copy)

force:

.PHONY: force 

