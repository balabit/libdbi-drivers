## Process this with automake to create Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_MSQL

doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_msql
noinst_DATA = dbd_msql.pdf dbd_msql/* 


msql_ltlibs = libmsql.la
msql_ldflags = -export-dynamic -avoid-version
msql_sources = dbd_msql.c

else

msql_ltlibs =
msql_ldflags =
msql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @MSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(msql_ltlibs)
libmsql_la_LDFLAGS = $(msql_ldflags) @MSQL_LDFLAGS@ @MSQL_LIBS@
libmsql_la_SOURCES = $(msql_sources)
libmsql_la_DEPENDENCIES = dbd_msql.h

EXTRA_DIST = dbd_msql.c dbd_msql.h dbd_msql.sgml dbd_msql.pdf dbd_msql/*

dbd_msql.pdf:
	@echo "Converting dbd_msql.sgml to PostScript...";
	sgmltools -b ps -s $(top_srcdir)/doc/include/doc-print.dsl dbd_msql.sgml > /dev/null 2>&1
	@echo "Converting dbd_msql.ps to PDF...";
	ps2pdf dbd_msql.ps

dbd_msql/*:
	@echo "Converting dbd_msql.sgml to HTML...";
	sgmltools -b html -s $(top_srcdir)/doc/include/doc-html.dsl dbd_msql.sgml

distclean: clean
	rm -f dbd_msql.ps dbd_msql.pdf dbd_msql/*


# manually install the docs
install-data-local:
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
