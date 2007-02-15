## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE

if BUILDDOCS
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
htmldir = $(docdir)/dbd_sqlite
doc_DATA = dbd_sqlite.pdf
html_DATA = dbd_sqlite/* 
else
docdir =
htmldir =
doc_DATA =
html_DATA =
endif

sqlite_ltlibs = libdbdsqlite.la
sqlite_ldflags = -module -avoid-version
sqlite_sources = dbd_sqlite.c

else

docdir =
htmldir =
doc_DATA =
html_DATA =

sqlite_ltlibs =
sqlite_ldflags =
sqlite_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @SQLITE_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite_ltlibs)
libdbdsqlite_la_LDFLAGS = $(sqlite_ldflags) @SQLITE_LDFLAGS@ @SQLITE_LIBS@ @LIBADD_LIBDBI@
libdbdsqlite_la_SOURCES = $(sqlite_sources)
libdbdsqlite_la_DEPENDENCIES = dbd_sqlite.h

EXTRA_DIST = dbd_sqlite.c dbd_sqlite.h AUTHORS TODO README dbd_sqlite.sgml dbd_sqlite.pdf dbd_sqlite/*

# jadetex spews out tons of useless crap. shut it up:
dbd_sqlite.pdf: dbd_sqlite.sgml
	@echo "Converting dbd_sqlite.sgml to PDF...";
	openjade -t tex -V tex-backend -d $(top_srcdir)/doc/include/doc-print.dsl dbd_sqlite.sgml
# run jadetex three times to get the references right
	pdftex "&pdfjadetex" dbd_sqlite.tex
	pdftex "&pdfjadetex" dbd_sqlite.tex
	pdftex "&pdfjadetex" dbd_sqlite.tex

dbd_sqlite/*: dbd_sqlite.sgml
	@echo "Converting dbd_sqlite.sgml to HTML...";
	mkdir -p dbd_sqlite && cd dbd_sqlite && openjade -t sgml -d $(top_srcdir)/../doc/include/doc-html.dsl ../dbd_sqlite.sgml


maintainer-clean: clean
	rm -f dbd_sqlite.pdf dbd_sqlite/*

