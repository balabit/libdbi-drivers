## Process this with automake to create Makefile.in

AUTOMAKE_OPTIONS = foreign

if HAVE_ORACLE

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

EXTRA_DIST = dbd_oracle.c dbd_oracle.h oracle_charsets.h
