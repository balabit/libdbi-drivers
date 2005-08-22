## Process this with automake to create Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_ORACLE

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_oracle
noinst_DATA = dbd_oracle.pdf dbd_oracle/* 

oracle_ltlibs = liboracle.la
oracle_ldflags = -module -avoid-version
oracle_sources = dbd_oracle.c

else

oracle_ltlibs =
oracle_ldflags =
oracle_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @ORACLE_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(oracle_ltlibs)
liboracle_la_LDFLAGS = $(oracle_ldflags) @ORACLE_LDFLAGS@ @ORACLE_LIBS@
liboracle_la_SOURCES = $(oracle_sources)
liboracle_la_DEPENDENCIES = dbd_oracle.h

EXTRA_DIST = dbd_oracle.c dbd_oracle.h oracle_charsets.h AUTHORS TODO README dbd_oracle.sgml dbd_oracle.pdf dbd_oracle/*

# jadetex spews out tons of useless crap. shut it up:
dbd_oracle.pdf: dbd_oracle.sgml
	@echo "Converting dbd_oracle.sgml to PDF...";
	openjade -t tex -V tex-backend -d $(top_srcdir)/doc/include/doc-print.dsl dbd_oracle.sgml
# run jadetex three times to get the references right
	pdftex "&pdfjadetex" dbd_oracle.tex
	pdftex "&pdfjadetex" dbd_oracle.tex
	pdftex "&pdfjadetex" dbd_oracle.tex

dbd_oracle/*: dbd_oracle.sgml
	@echo "Converting dbd_oracle.sgml to HTML...";
	mkdir -p dbd_oracle && cd dbd_oracle && openjade -t sgml -d $(top_srcdir)/../doc/include/doc-html.dsl ../dbd_oracle.sgml


distclean: clean
	rm -f dbd_oracle.pdf dbd_oracle/*

# manually install the docs
install-data-local: 
if HAVE_ORACLE
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
endif
