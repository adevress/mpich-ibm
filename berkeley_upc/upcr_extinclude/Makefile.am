## Process this file with automake to produce Makefile.in       -*- makefile -*-

AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies

headers = 			\
	bupc_extensions.h	\
	bupc_wrappers.h		\
	pupc.h			\
	upc_collective.h	\
	upc_collective_bits.h	\
	upc_io.h		\
	upc_io_bits.h		\
	gcc-upc-lib.h

EXTRA_DIST = $(headers)

install-data-local: $(headers)
	$(mkinstalldirs) $(DESTDIR)$(includedir)/upcr_extinclude
	@list='$(headers)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    filename=`basename $$p`; \
	    echo " $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(includedir)/upcr_extinclude/$$filename"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(includedir)/upcr_extinclude/$$filename; \
	  else :; fi; \
	done

uninstall-local:
	rm -f $(DESTDIR)$(includedir)/upcr_extinclude/*.h

