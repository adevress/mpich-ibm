AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies

libexec_PROGRAMS    = detect-upc
detect_upc_SOURCES  = detect-upc.c detect-upc.h $(SCANNER_SRC)
libexec_SCRIPTS   = upcppp

if CROSS_COMPILING
# Use HOST_CC for this program when cross-compiling, because it must run on the host
CFLAGS_STAMP=$(top_builddir)/detect-upc/.set-cflags

$(CFLAGS_STAMP): $(srcdir)/Makefile.in
	cd $(top_builddir) && CONFIG_HEADERS= CONFIG_FILES=detect-upc/Makefile ./config.status
	mv Makefile Makefile.bak && \
	@PERL@ -p -e 's|^\s*CFLAGS\s*=.*$$|CFLAGS=@HOST_CFLAGS@| ;  \
	              s|^\s*CC\s*=.*$$|CC=@HOST_CC@| ;              \
	              s|^\s*LDFLAGS\s*=.*$$|LDFLAGS=@HOST_LDFLAGS@| ;    \
	              s|^\s*LIBS\s*=.*$$|LIBS=@HOST_LIBS@| ; '        \
		     Makefile.bak >Makefile &&  \
	rm -f Makefile.bak
	touch $(CFLAGS_STAMP)

Makefile: $(CFLAGS_STAMP)
	touch Makefile
endif

# Install .pl in libexecdir, but w/o exec permissions
dotpldir = $(libexecdir)
dotpl_DATA = upcppp.pl

if UPCR_USE_LEX
# We are using a LEX
# Let automake do its normal thing to build.
# However, we also add the scanner.c to the files to clean.
SCANNER_SRC = scanner.l
scanner_clean = scanner.c
else
# We are NOT using a LEX
# Always use the prebuilt scanner.c, never deleting it.
# However, we issue warnings at build and refuse to distribute if out of date.
SCANNER_SRC = scanner.c
scanner.c: scanner.l detect-upc.h
	@if test \! -e "$(srcdir)/scanner.c"; then \
	    echo '**** ERROR: missing source file scanner.c cannot be built without a lex ****' >&2; \
	    exit 1; \
	 fi; \
	 echo '**** WARNING: scanner.c might be out-of-date but cannot be rebuilt without a lex ****' >&2; \
	 if test -n "$(DIST_HOOK_RUNNING)"; then \
	    echo '**** ERROR: refusing to distribute potentially out-of-date scanner.c ****' >&2; \
	    exit 1; \
	 fi
scanner_clean =
endif

# upcppp 
upcppp: $(top_builddir)/gasnet/other/perlstart

# Things to copy from srcdir to buildir when they differ
if BUILD_IS_SRC
files_we_copy =
else
files_we_copy = $(dotpl_DATA)
$(files_we_copy): force
	@srcfile="$(srcdir)/`basename $@`" ; \
         if test ! -f "$@" -o "`find $$srcfile -newer '$@' 2>&1`" ; then \
          echo cp -f "$$srcfile" . ;         \
          cp -f "$$srcfile" . ;              \
         fi
endif

# Things to conditionally clean
CLEANFILES = $(scanner_clean) $(CFLAGS_STAMP)
DISTCLEANFILES = $(files_we_copy)

# This rule is unnecessary but harmless when LEX is enabled.
# However, some automakes don't like dist-hook to be defined conditionally.
dist-hook:
	@$(MAKE) scanner.c DIST_HOOK_RUNNING=yes
# Make certain we distribute the same files regardless of using LEX or not
EXTRA_DIST	= scanner.c scanner.l $(dotpl_DATA)

.PHONY: force 

