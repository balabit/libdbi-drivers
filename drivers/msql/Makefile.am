AUTOMAKE_OPTIONS = foreign

if HAVE_MSQL

msql_ltlibs = libmsql.la
msql_ldflags = -export-dynamic -avoid-version
msql_sources = dbd_msql.c

else

msql_ltlibs =
msql_ldflags =
msql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @MSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(msql_ltlibs)
libmsql_la_LDFLAGS = $(msql_ldflags) @MSQL_LDFLAGS@ @MSQL_LIBS@
libmsql_la_SOURCES = $(msql_sources)
libmsql_la_DEPENDENCIES = dbd_msql.h

EXTRA_DIST = dbd_myql.c dbd_msql.h
