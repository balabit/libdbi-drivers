## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

TESTS = test_dbi
check_PROGRAMS = test_dbi
test_dbi_SOURCES = test_dbi.c
test_dbi_LDADD = -L@libdir@ -lm -ldbi @LIBADD_DL@

INCLUDES = -I@includedir@
CFLAGS = -g -DDBI_DRIVER_DIR=\"@driverdir@\"

debug:
	$(MAKE) all CFLAGS="@DEBUG@"

profile:
	$(MAKE) all CFLAGS="@PROFILE@"
