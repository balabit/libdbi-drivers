AUTOMAKE_OPTIONS = foreign

if HAVE_PGSQL

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_pgsql
noinst_DATA = dbd_pgsql.pdf dbd_pgsql/* 

pgsql_ltlibs = libpgsql.la
pgsql_ldflags = -module -avoid-version
pgsql_sources = dbd_pgsql.c

else

doc_dir =
html_subdir =
noinst_DATA =

pgsql_ltlibs =
pgsql_ldflags =
pgsql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @PGSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(pgsql_ltlibs)
libpgsql_la_LDFLAGS = $(pgsql_ldflags) @PGSQL_LDFLAGS@ @PGSQL_LIBS@
libpgsql_la_SOURCES = $(pgsql_sources)
libpgsql_la_DEPENDENCIES = dbd_pgsql.h

EXTRA_DIST = dbd_pgsql.c dbd_pgsql.h AUTHORS TODO README dbd_pgsql.pdf dbd_pgsql/*

# jadetex spews out tons of useless crap. shut it up:
dbd_pgsql.pdf:
	@echo "Converting dbd_pgsql.sgml to PostScript...";
	sgmltools -b ps -s $(top_srcdir)/doc/include/doc-print.dsl dbd_pgsql.sgml > /dev/null 2>&1
	@echo "Converting dbd_pgsql.ps to PDF...";
	ps2pdf dbd_pgsql.ps

dbd_pgsql/*:
	@echo "Converting dbd_pgsql.sgml to HTML...";
	sgmltools -b html -s $(top_srcdir)/doc/include/doc-html.dsl dbd_pgsql.sgml


distclean: clean
	rm -f dbd_pgsql.ps dbd_pgsql.pdf dbd_pgsql/*

# manually install the docs
install-data-local: 
if HAVE_PGSQL
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
endif