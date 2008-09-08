## Process this file with automake to produce Makefile.in       -*- makefile -*-

AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies

SUBDIRS = sys

headers = 

built_headers = 	\
	alloca.h.in	\
	fenv.h.in	\
	inttypes.h.in	\
	limits.h.in	\
	math.h.in	\
	time.h.in	\
	stdarg.h.in	\
	stdbool.h.in	\
	stddef.h.in	\
	stdint.h.in 	\
	stdlib.h.in	\
	stdio.h.in	\
	string.h.in

EXTRA_DIST = $(headers) $(built_headers)

all: *.h

builddir = $(top_builddir)/$(subdir)

install-data-local: $(headers) $(built_headers)
	$(mkinstalldirs) $(DESTDIR)$(includedir)/upcr_geninclude
	@list='$(headers)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    filename=`basename $$p`; \
	    echo " $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(includedir)/upcr_geninclude/$$filename"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(includedir)/upcr_geninclude/$$filename; \
	  else :; fi; \
	done
	@list='$(built_headers)'; for p in $$list; do \
	  q=`basename $$p .in`; \
	  if test -f $(builddir)/$$q; then \
	    filename=`basename $$q`; \
	    echo " $(INSTALL_DATA) $(builddir)/$$q $(DESTDIR)$(includedir)/upcr_geninclude/$$filename"; \
	    $(INSTALL_DATA) $(builddir)/$$q $(DESTDIR)$(includedir)/upcr_geninclude/$$filename; \
	  else :; fi; \
	done

uninstall-local:
	rm -f $(DESTDIR)$(includedir)/upcr_geninclude/*.h

