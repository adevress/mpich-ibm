# $Id: Makefile.am,v 1.17 2006/02/01 21:13:25 bonachea Exp $
AUTOMAKE_OPTIONS  = foreign 1.4 no-dependencies

manfiles            = upcc.1 upcrun.1 upc_trace.1 upcdecl.1
htmlfiles           = $(manfiles:.1=.html)

man_MANS            = $(manfiles) 

# delete man page sources during 'clean', since they're generated files.
CLEANFILES          = $(manfiles)

EXTRA_DIST          = upcc.extraman upcrun.extraman upc_trace.extraman help2man

# For developers: this target just adds a man1 directory and puts manpage in it,
# so 'man upcc' work without install (assuming you've got '.' in your MANPATH)
dev: $(manfiles)
	if test ! -d man1; then mkdir man1; fi
	cp $(manfiles) man1

help2man = @PERL@ $(top_srcdir)/man/help2man
standard_options = -I $< -N -o $@ -s 1 -S "Berkeley UPC" -v -h2mversion=@VERSION@ -h=-h2mhelp

upcc.1: $(top_srcdir)/man/upcc.extraman $(top_srcdir)/upcc.pl
	@$(MAKE) -C $(top_builddir) upcc.pl upcc
	$(help2man) -n "the Berkeley UPC compiler, version @VERSION@" \
	$(standard_options) $(top_builddir)/upcc

upcrun.1: $(top_srcdir)/man/upcrun.extraman $(top_srcdir)/upcrun.pl
	@$(MAKE) -C $(top_builddir) upcrun.pl upcrun
	$(help2man) -n "a portable parallel job launcher for UPC programs, version @VERSION@" \
	$(standard_options) $(top_builddir)/upcrun

upc_trace.1: $(top_srcdir)/man/upc_trace.extraman $(top_srcdir)/gasnet/other/contrib/gasnet_trace.pl
	@$(MAKE) -C $(top_builddir) upc_trace
	$(help2man) -n "the UPC/GASNet trace summarization tool, version @VERSION@" \
	$(standard_options) $(top_builddir)/upc_trace

upcdecl_dir = contrib/upcdecl
upcdecl.1: $(top_srcdir)/$(upcdecl_dir)/upcdecl.extraman 
	@$(MAKE) -C $(top_builddir)/$(upcdecl_dir) upcdecl
	version=`$(top_builddir)/$(upcdecl_dir)/upcdecl -V | @PERL@ -pe 's/\s*upcdecl\s*//'` ; \
	@PERL@ $(top_srcdir)/man/help2man -n "UPC type explanation and construction tool, $$version" \
	$(standard_options) -v -V -h -H $(top_builddir)/$(upcdecl_dir)/upcdecl

# targets to build HTML pages from man: for now insert these manually into the
# 'upc-webpage' tree before building the release tarball
html: $(htmlfiles)

.1.html:
	$(top_srcdir)/man/manServer_107.pl $< > $@

dev-clean: 
	rm -rf man1 $(manfiles) $(htmlfiles)
