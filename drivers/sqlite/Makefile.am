## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_sqlite
noinst_DATA = dbd_sqlite.pdf dbd_sqlite/* 

sqlite_ltlibs = libsqlite.la
sqlite_ldflags = -export-dynamic -avoid-version
sqlite_sources = dbd_sqlite.c

else

doc_dir =
html_subdir =
noinst_DATA =

sqlite_ltlibs =
sqlite_ldflags =
sqlite_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @SQLITE_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite_ltlibs)
libsqlite_la_LDFLAGS = $(sqlite_ldflags) @SQLITE_LDFLAGS@ @SQLITE_LIBS@
libsqlite_la_SOURCES = $(sqlite_sources)
libsqlite_la_DEPENDENCIES = dbd_sqlite.h

EXTRA_DIST = dbd_sqlite.c dbd_sqlite.h AUTHORS TODO README $(noinst_DATA)

# jadetex spews out tons of useless crap. shut it up:
dbd_sqlite.pdf:
	@echo "Converting dbd_sqlite.sgml to PostScript...";
	sgmltools -b ps -s $(top_srcdir)/doc/include/doc-print.dsl dbd_sqlite.sgml > /dev/null 2>&1
	@echo "Converting dbd_sqlite.ps to PDF...";
	ps2pdf dbd_sqlite.ps

dbd_sqlite/*:
	@echo "Converting dbd_sqlite.sgml to HTML...";
	sgmltools -b html -s $(top_srcdir)/doc/include/doc-html.dsl dbd_sqlite.sgml


distclean: clean
	rm -f dbd_sqlite.ps dbd_sqlite.pdf dbd_sqlite/*

# manually install the docs
install-data-local: 
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
