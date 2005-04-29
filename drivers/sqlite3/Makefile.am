## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE3

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_sqlite3
noinst_DATA = dbd_sqlite3.pdf dbd_sqlite3/* 

sqlite3_ltlibs = libsqlite3.la
sqlite3_ldflags = -module -avoid-version
sqlite3_sources = dbd_sqlite3.c

else

doc_dir =
html_subdir =
noinst_DATA =

sqlite3_ltlibs =
sqlite3_ldflags =
sqlite3_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @SQLITE3_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite3_ltlibs)
libsqlite3_la_LDFLAGS = $(sqlite3_ldflags) @SQLITE3_LDFLAGS@ @SQLITE3_LIBS@
libsqlite3_la_SOURCES = $(sqlite3_sources)
libsqlite3_la_DEPENDENCIES = dbd_sqlite3.h

EXTRA_DIST = dbd_sqlite3.c dbd_sqlite3.h AUTHORS TODO README dbd_sqlite3.pdf dbd_sqlite3/*

# jadetex spews out tons of useless crap. shut it up:
dbd_sqlite3.pdf:
	@echo "Converting dbd_sqlite3.sgml to PostScript...";
	sgmltools -b ps -s $(top_srcdir)/doc/include/doc-print.dsl dbd_sqlite3.sgml > /dev/null 2>&1
	@echo "Converting dbd_sqlite3.ps to PDF...";
	ps2pdf dbd_sqlite3.ps

dbd_sqlite3/*:
	@echo "Converting dbd_sqlite3.sgml to HTML...";
	sgmltools -b html -s $(top_srcdir)/doc/include/doc-html.dsl dbd_sqlite3.sgml


distclean: clean
	rm -f dbd_sqlite3.ps dbd_sqlite3.pdf dbd_sqlite3/*

# manually install the docs
install-data-local: 
if HAVE_SQLITE3
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
endif