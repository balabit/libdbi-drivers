AUTOMAKE_OPTIONS = foreign

if HAVE_SQLITE

sqlite_ltlibs = libsqlite_dbi.la
sqlite_ldflags = -export-dynamic -avoid-version
sqlite_sources = dbd_sqlite_dbi.c

else

sqlite_ltlibs =
sqlite_ldflags =
sqlite_sources =

endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include @SQLITE_INCLUDE@

libdir = $(driverdir)
lib_LTLIBRARIES = $(sqlite_ltlibs)
libsqlite_dbi_la_LDFLAGS = $(sqlite_ldflags) @SQLITE_LFLAGS@ @SQLITE_LIBS@
libsqlite_dbi_la_SOURCES = $(sqlite_sources)
libsqlite_dbi_la_DEPENDENCIES = sqlite-stuff.h

EXTRA_DIST = sqlite-stuff.h
