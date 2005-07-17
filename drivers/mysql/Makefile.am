AUTOMAKE_OPTIONS = foreign

if HAVE_MYSQL

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_mysql
noinst_DATA = dbd_mysql.pdf dbd_mysql/* 

mysql_ltlibs = libmysql.la
mysql_ldflags = -module -avoid-version
mysql_sources = dbd_mysql.c

else

doc_dir =
html_subdir =
noinst_DATA =

mysql_ltlibs =
mysql_ldflags =
mysql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @MYSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(mysql_ltlibs)
libmysql_la_LDFLAGS = $(mysql_ldflags) @MYSQL_LDFLAGS@ @MYSQL_LIBS@
libmysql_la_SOURCES = $(mysql_sources)
libmysql_la_DEPENDENCIES = dbd_mysql.h

EXTRA_DIST = dbd_mysql.c dbd_mysql.h AUTHORS TODO README dbd_mysql.pdf dbd_mysql/*

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


distclean: clean
	rm -f dbd_mysql.pdf dbd_mysql/*

# manually install the docs
install-data-local: 
if HAVE_MYSQL
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
endif