AUTOMAKE_OPTIONS = foreign

if HAVE_FREETDS

# don't use docdir, doc_DATA. This way we keep the HTML in separate subdirs
# for each driver
doc_dir = $(datadir)/doc/$(PACKAGE)-$(VERSION)
html_subdir = dbd_freetds
#noinst_DATA = dbd_freetds.pdf dbd_freetds/* 
noinst_DATA = 

freetds_ltlibs = libfreetds.la
freetds_ldflags = -module -avoid-version
freetds_sources = dbd_freetds.c

else

doc_dir =
html_subdir =
noinst_DATA =

freetds_ltlibs =
freetds_ldflags =
freetds_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @FREETDS_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(freetds_ltlibs)
libfreetds_la_LDFLAGS = $(freetds_ldflags) @FREETDS_LDFLAGS@ @FREETDS_LIBS@
libfreetds_la_SOURCES = $(freetds_sources)
libfreetds_la_DEPENDENCIES = dbd_freetds.h

#EXTRA_DIST = dbd_freetds.c dbd_freetds.h AUTHORS TODO README dbd_freetds.pdf dbd_freetds/*
EXTRA_DIST = dbd_freetds.c dbd_freetds.h

# jadetex spews out tons of useless crap. shut it up:
#dbd_freetds.pdf:
#	@echo "Converting dbd_freetds.sgml to PostScript...";
#	sgmltools -b ps -s $(top_srcdir)/doc/include/doc-print.dsl dbd_freetds.sgml > /dev/null 2>&1
#	@echo "Converting dbd_freetds.ps to PDF...";
#	ps2pdf dbd_freetds.ps

#dbd_freetds/*:
#	@echo "Converting dbd_freetds.sgml to HTML...";
#	sgmltools -b html -s $(top_srcdir)/doc/include/doc-html.dsl dbd_freetds.sgml


distclean: clean
#	rm -f dbd_mysql.ps dbd_mysql.pdf dbd_mysql/*

# manually install the docs
install-data-local: 
if HAVE_FREETDS
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(doc_dir) $(DESTDIR)$(doc_dir)/$(html_subdir)
	@for f in $(noinst_DATA); do \
		$(INSTALL_DATA) $(srcdir)/$$f $(DESTDIR)$(doc_dir)/$$f; \
	done
endif