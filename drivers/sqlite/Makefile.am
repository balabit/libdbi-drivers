AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE

sqlite_ltlibs = libsqlite.la
sqlite_ldflags = -export-dynamic -avoid-version
sqlite_sources = dbd_sqlite.c

else

sqlite_ltlibs =
sqlite_ldflags =
sqlite_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @DBI_INCLUDE@ @SQLITE_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite_ltlibs)
libsqlite_la_LDFLAGS = $(sqlite_ldflags) @SQLITE_LDFLAGS@ @SQLITE_LIBS@
libsqlite_la_SOURCES = $(sqlite_sources)
libsqlite_la_DEPENDENCIES = dbd_sqlite.h

EXTRA_DIST = dbd_sqlite.c dbd_sqlite.h AUTHORS TODO README
