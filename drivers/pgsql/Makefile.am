AUTOMAKE_OPTIONS = foreign

if HAVE_PGSQL

pgsql_ltlibs = libpgsql.la
pgsql_ldflags = -module -avoid-version
pgsql_sources = dbd_pgsql.c

else

pgsql_ltlibs =
pgsql_ldflags =
pgsql_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @PGSQL_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(pgsql_ltlibs)
libpgsql_la_LDFLAGS = $(pgsql_ldflags) @PGSQL_LDFLAGS@ @PGSQL_LIBS@
libpgsql_la_SOURCES = $(pgsql_sources)
libpgsql_la_DEPENDENCIES = dbd_pgsql.h

EXTRA_DIST = dbd_pgsql.c dbd_pgsql.h
