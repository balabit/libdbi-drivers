## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE3

if BUILDDOCS
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
htmldir = $(docdir)/dbd_sqlite3
doc_DATA = dbd_sqlite3.pdf
html_DATA = dbd_sqlite3/* 
else
docdir =
htmldir =
doc_DATA =
html_DATA =
endif

sqlite3_ltlibs = libdbdsqlite3.la
sqlite3_ldflags = -module -avoid-version
sqlite3_sources = dbd_sqlite3.c

else

docdir =
htmldir =
doc_DATA =
html_DATA =

sqlite3_ltlibs =
sqlite3_ldflags =
sqlite3_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @SQLITE3_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite3_ltlibs)
libdbdsqlite3_la_LDFLAGS = $(sqlite3_ldflags) @SQLITE3_LDFLAGS@ @SQLITE3_LIBS@ @LIBADD_LIBDBI@
libdbdsqlite3_la_SOURCES = $(sqlite3_sources)
libdbdsqlite3_la_DEPENDENCIES = dbd_sqlite3.h

EXTRA_DIST = dbd_sqlite3.c dbd_sqlite3.h AUTHORS TODO README dbd_sqlite3.sgml dbd_sqlite3.pdf dbd_sqlite3/*

# jadetex spews out tons of useless crap. shut it up:
dbd_sqlite3.pdf: dbd_sqlite3.sgml
	@echo "Converting dbd_sqlite3.sgml to PDF...";
	openjade -t tex -V tex-backend -d $(top_srcdir)/doc/include/doc-print.dsl dbd_sqlite3.sgml
# run jadetex three times to get the references right
	pdftex "&pdfjadetex" dbd_sqlite3.tex
	pdftex "&pdfjadetex" dbd_sqlite3.tex
	pdftex "&pdfjadetex" dbd_sqlite3.tex

dbd_sqlite3/*: dbd_sqlite3.sgml
	@echo "Converting dbd_sqlite3.sgml to HTML...";
	mkdir -p dbd_sqlite3 && cd dbd_sqlite3 && openjade -t sgml -d $(top_srcdir)/../doc/include/doc-html.dsl ../dbd_sqlite3.sgml


maintainer-clean: clean
	rm -f dbd_sqlite3.pdf dbd_sqlite3/*

