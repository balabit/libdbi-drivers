AUTOMAKE_OPTIONS = foreign

if HAVE_MYSQL

if BUILDDOCS
# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
docdir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
htmldir = $(docdir)/dbd_mysql
doc_DATA = dbd_mysql.pdf
html_DATA = dbd_mysql/* 
else
docdir =
htmldir =
doc_DATA =
html_DATA =
endif

mysql_ltlibs = libdbdmysql.la
mysql_ldflags = -module -avoid-version
mysql_sources = dbd_mysql.c

else

docdir =
htmldir =
doc_DATA =
html_DATA =

mysql_ltlibs =
mysql_ldflags =
mysql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @MYSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(mysql_ltlibs)
libdbdmysql_la_LDFLAGS = $(mysql_ldflags) @MYSQL_LDFLAGS@ @MYSQL_LIBS@ @LIBADD_LIBDBI@
libdbdmysql_la_SOURCES = $(mysql_sources)
libdbdmysql_la_DEPENDENCIES = dbd_mysql.h

EXTRA_DIST = dbd_mysql.c dbd_mysql.h AUTHORS TODO README dbd_mysql.sgml dbd_mysql.pdf $(html_DATA)

# jadetex spews out tons of useless crap. shut it up:
dbd_mysql.pdf: dbd_mysql.sgml
	@echo "Converting dbd_mysql.sgml to PDF...";
	openjade -t tex -V tex-backend -d $(top_srcdir)/doc/include/doc-print.dsl dbd_mysql.sgml
# run jadetex three times to get the references right
	pdftex "&pdfjadetex" dbd_mysql.tex
	pdftex "&pdfjadetex" dbd_mysql.tex
	pdftex "&pdfjadetex" dbd_mysql.tex

dbd_mysql/*: dbd_mysql.sgml
	@echo "Converting dbd_mysql.sgml to HTML...";
	mkdir -p dbd_mysql && cd dbd_mysql && openjade -t sgml -d $(top_srcdir)/../doc/include/doc-html.dsl ../dbd_mysql.sgml


maintainer-clean: clean
	rm -f dbd_mysql.pdf dbd_mysql/*

