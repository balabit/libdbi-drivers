AUTOMAKE_OPTIONS = foreign

if HAVE_FREETDS
if BUILDDOCS
# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
htmldir = $(docdir)dbd_freetds
#doc_DATA = dbd_freetds.pdf
#html_DATA =  dbd_freetds/* 
else
docdir =
htmldir =
endif

freetds_ltlibs = libdbdfreetds.la
freetds_ldflags = -module -avoid-version
freetds_sources = dbd_freetds.c

else

docdir =
htmldir =
doc_DATA =
html_DATA =

freetds_ltlibs =
freetds_ldflags =
freetds_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @FREETDS_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(freetds_ltlibs)
libdbdfreetds_la_LDFLAGS = $(freetds_ldflags) @FREETDS_LDFLAGS@ @FREETDS_LIBS@ @LIBADD_LIBDBI@
libdbdfreetds_la_SOURCES = $(freetds_sources)
libdbdfreetds_la_DEPENDENCIES = dbd_freetds.h

#EXTRA_DIST = dbd_freetds.c dbd_freetds.h AUTHORS TODO README dbd_freetds.pdf $(html_DATA)
EXTRA_DIST = dbd_freetds.c dbd_freetds.h

# jadetex spews out tons of useless crap. shut it up:
#dbd_freetds.pdf: dbd_freetds.sgml
#	@echo "Converting dbd_freetds.sgml to PDF...";
#	openjade -t tex -V tex-backend -d $(top_srcdir)/doc/include/doc-print.dsl dbd_freetds.sgml
# run jadetex three times to get the references right
#	pdftex "&pdfjadetex" dbd_freetds.tex
#	pdftex "&pdfjadetex" dbd_freetds.tex
#	pdftex "&pdfjadetex" dbd_freetds.tex

#dbd_freetds/*: dbd_freetds.sgml
#	@echo "Converting dbd_freetds.sgml to HTML...";
#	mkdir -p dbd_mysql && cd dbd_freetds && openjade -t sgml -d $(top_srcdir)/../doc/include/doc-html.dsl ../dbd_freetds.sgml




maintainer-clean: clean
#	rm -f dbd_mysql.pdf dbd_mysql/*

