##
## originally by Christian Hammers <ch@westend.com>,
## who borrowed ideas from Stephan Kulow.
## then this file was tweaked by David Parker for libdbi.
## and finally it was ripped out and modified by Markus
## Hoenicka who then used for the libdbi-drivers project
##

AC_DEFUN([AC_FIND_FILE],
[
$3=no
for i in $2; do
	for j in $1; do
		if test -r "$i/$j"; then
			$3=$i
			break 2
		fi
	done
done
])

## DBI

AC_DEFUN([AC_CHECK_DBI],
[
AM_CONDITIONAL(HAVE_DBI, false)
ac_dbi_incdir="no"

# exported variables
DBI_INCLUDE=""

AC_MSG_CHECKING(for libdbi framework)

AC_ARG_WITH(dbi-incdir,
	[  --with-dbi-incdir       Specifies where the libdbi include files are.],
	[  ac_dbi_incdir="$withval" ])

if test "$ac_dbi_incdir" = "no"; then
	dbi_incdirs="/usr/include /usr/local/include /sw/include"
	AC_FIND_FILE(dbi/dbi.h, $dbi_incdirs, ac_dbi_incdir)
	if test "$ac_dbi_incdir" = "no"; then
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([Invalid libdbi directory - include files not found.])
	fi
fi
AC_MSG_RESULT([yes: headers in $ac_dbi_incdir])
AM_CONDITIONAL(HAVE_DBI, true)
	
DBI_INCLUDE=-I$ac_dbi_incdir
	
AC_SUBST(DBI_INCLUDE)
])

## MYSQL

AC_DEFUN([AC_CHECK_MYSQL],
[
AM_CONDITIONAL(HAVE_MYSQL, false)
ac_mysql="no"
ac_mysql_incdir="no"
ac_mysql_libdir="no"

# exported variables
MYSQL_LIBS=""
MYSQL_LDFLAGS=""
MYSQL_INCLUDE=""

AC_MSG_CHECKING(for MySQL support)

AC_ARG_WITH(mysql,
	[  --with-mysql            Include MySQL support.],
	[  ac_mysql="$withval" ])
AC_ARG_WITH(mysql-dir,
	[  --with-mysql-dir        Specifies the MySQL root directory.],
	[  ac_mysql_incdir="$withval"/include
	   ac_mysql_libdir="$withval"/lib ])
AC_ARG_WITH(mysql-incdir,
	[  --with-mysql-incdir     Specifies where the MySQL include files are.],
	[  ac_mysql_incdir="$withval" ])
AC_ARG_WITH(mysql-libdir,
	[  --with-mysql-libdir     Specifies where the MySQL libraries are.],
	[  ac_mysql_libdir="$withval" ])

if test "$ac_mysql" = "yes"; then
	if test "$ac_mysql_incdir" = "no" || test "$ac_mysql_libs" = "no"; then
		mysql_incdirs="/usr/include /usr/local/include /usr/include/mysql /usr/local/include/mysql /usr/local/mysql/include /usr/local/mysql/include/mysql /opt/mysql/include/mysql /sw/include/mysql"
		AC_FIND_FILE(mysql/mysql.h, $mysql_incdirs, ac_mysql_incdir)
		mysql_libdirs="/usr/lib /usr/local/lib /usr/lib/mysql /usr/local/lib/mysql /usr/local/mysql/lib /usr/local/mysql/lib/mysql /opt/mysql/lib/mysql /sw/lib/mysql"
		mysql_libs="libmysqlclient.so libmysqlclient.a"
		AC_FIND_FILE($mysql_libs, $mysql_libdirs, ac_mysql_libdir)
		if test "$ac_mysql_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid MySQL directory - include files not found.])
		fi
		if test "$ac_mysql_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid MySQL directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_mysql_libdir, headers in $ac_mysql_incdir])
	AM_CONDITIONAL(HAVE_MYSQL, true)
	
	MYSQL_LIBS=-lmysqlclient
	MYSQL_INCLUDE=-I$ac_mysql_incdir
	MYSQL_LDFLAGS=-L$ac_mysql_libdir
	
	AC_SUBST(MYSQL_LIBS)
	AC_SUBST(MYSQL_INCLUDE)
	AC_SUBST(MYSQL_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## PGSQL

AC_DEFUN([AC_CHECK_PGSQL],
[
AM_CONDITIONAL(HAVE_PGSQL, false)
ac_pgsql="no"
ac_pgsql_incdir="no"
ac_pgsql_libdir="no"

# exported variables
PGSQL_LIBS=""
PGSQL_LDFLAGS=""
PGSQL_INCLUDE=""

AC_MSG_CHECKING(for PostgreSQL support)

AC_ARG_WITH(pgsql,
	[  --with-pgsql            Include PostgreSQL support.],
	[  ac_pgsql="$withval" ])
AC_ARG_WITH(pgsql-dir,
	[  --with-pgsql-dir        Specifies the PostgreSQL root directory.],
	[  ac_pgsql_incdir="$withval"/include
	   ac_pgsql_libdir="$withval"/lib ])
AC_ARG_WITH(pgsql-incdir,
	[  --with-pgsql-incdir     Specifies where the PostgreSQL include files are.],
	[  ac_pgsql_incdir="$withval" ])
AC_ARG_WITH(pgsql-libdir,
	[  --with-pgsql-libdir     Specifies where the PostgreSQL libraries are.],
	[  ac_pgsql_libdir="$withval" ])

if test "$ac_pgsql" = "yes"; then
	if test "$ac_pgsql_incdir" = "no" || test "$ac_pgsql_libs" = "no"; then
		pgsql_incdirs="/usr/include /usr/local/include /usr/include/pgsql /usr/local/include/pgsql /usr/local/pgsql/include /usr/include/postgresql /usr/local/postgresql/include /opt/pgsql/include"
		AC_FIND_FILE(libpq-fe.h, $pgsql_incdirs, ac_pgsql_incdir)
		pgsql_libdirs="/usr/lib /usr/local/lib /usr/lib/pgsql /usr/local/lib/pgsql /usr/local/pgsql/lib /opt/pgsql/lib"
		pgsql_libs="libpq.so libpq.a"
		AC_FIND_FILE($pgsql_libs, $pgsql_libdirs, ac_pgsql_libdir)
		if test "$ac_pgsql_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid PostgreSQL directory - include files not found.])
		fi
		if test "$ac_pgsql_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid PostgreSQL directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_pgsql_libdir, headers in $ac_pgsql_incdir])
	AM_CONDITIONAL(HAVE_PGSQL, true)
	
	PGSQL_LIBS=-lpq
	PGSQL_INCLUDE=-I$ac_pgsql_incdir
	PGSQL_LDFLAGS=-L$ac_pgsql_libdir
	
	AC_SUBST(PGSQL_LIBS)
	AC_SUBST(PGSQL_INCLUDE)
	AC_SUBST(PGSQL_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## SQLITE

AC_DEFUN([AC_CHECK_SQLITE],
[
AM_CONDITIONAL(HAVE_SQLITE, false)
ac_sqlite="no"
ac_sqlite_incdir="no"
ac_sqlite_libdir="no"

# exported variables
SQLITE_LIBS=""
SQLITE_LDFLAGS=""
SQLITE_INCLUDE=""

AC_MSG_CHECKING(for SQLite support)

AC_ARG_WITH(sqlite,
	[  --with-sqlite           Include SQLite support.],
	[  ac_sqlite="$withval" ])
AC_ARG_WITH(sqlite-dir,
	[  --with-sqlite-dir       Specifies the SQLite root directory.],
	[  ac_sqlite_incdir="$withval"/include
	   ac_sqlite_libdir="$withval"/lib ])
AC_ARG_WITH(sqlite-incdir,
	[  --with-sqlite-incdir    Specifies where the SQLite include files are.],
	[  ac_sqlite_incdir="$withval" ])
AC_ARG_WITH(sqlite-libdir,
	[  --with-sqlite-libdir    Specifies where the SQLite libraries are.],
	[  ac_sqlite_libdir="$withval" ])

if test "$ac_sqlite" = "yes"; then
	if test "$ac_sqlite_incdir" = "no" || test "$ac_sqlite_libs" = "no"; then
		sqlite_incdirs="/usr/include /usr/local/include /usr/include/sqlite /usr/local/include/sqlite /usr/local/sqlite/include /opt/sqlite/include"
		AC_FIND_FILE(sqlite.h, $sqlite_incdirs, ac_sqlite_incdir)
		sqlite_libdirs="/usr/lib /usr/local/lib /usr/lib/sqlite /usr/local/lib/sqlite /usr/local/sqlite/lib /opt/sqlite/lib"
		sqlite_libs="libsqlite.so libsqlite.a"
		AC_FIND_FILE($sqlite_libs, $sqlite_libdirs, ac_sqlite_libdir)
		if test "$ac_sqlite_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid SQLite directory - include files not found.])
		fi
		if test "$ac_sqlite_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid SQLite directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_sqlite_libdir, headers in $ac_sqlite_incdir])
	AM_CONDITIONAL(HAVE_SQLITE, true)
	
	SQLITE_LIBS=-lsqlite
	SQLITE_INCLUDE=-I$ac_sqlite_incdir
	SQLITE_LDFLAGS=-L$ac_sqlite_libdir
	
	AC_SUBST(SQLITE_LIBS)
	AC_SUBST(SQLITE_INCLUDE)
	AC_SUBST(SQLITE_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## SQLITE3

AC_DEFUN([AC_CHECK_SQLITE3],
[
AM_CONDITIONAL(HAVE_SQLITE3, false)
ac_sqlite3="no"
ac_sqlite3_incdir="no"
ac_sqlite3_libdir="no"

# exported variables
SQLITE3_LIBS=""
SQLITE3_LDFLAGS=""
SQLITE3_INCLUDE=""

AC_MSG_CHECKING(for SQLite3 support)

AC_ARG_WITH(sqlite3,
	[  --with-sqlite3          Include SQLite3 support.],
	[  ac_sqlite3="$withval" ])
AC_ARG_WITH(sqlite3-dir,
	[  --with-sqlite3-dir      Specifies the SQLite3 root directory.],
	[  ac_sqlite3_incdir="$withval"/include
	   ac_sqlite3_libdir="$withval"/lib ])
AC_ARG_WITH(sqlite3-incdir,
	[  --with-sqlite3-incdir   Specifies where the SQLite3 include files are.],
	[  ac_sqlite3_incdir="$withval" ])
AC_ARG_WITH(sqlite3-libdir,
	[  --with-sqlite3-libdir   Specifies where the SQLite3 libraries are.],
	[  ac_sqlite3_libdir="$withval" ])

if test "$ac_sqlite3" = "yes"; then
	if test "$ac_sqlite3_incdir" = "no" || test "$ac_sqlite3_libs" = "no"; then
		sqlite3_incdirs="/usr/include /usr/local/include /usr/include/sqlite3 /usr/local/include/sqlite3 /usr/local/sqlite3/include /opt/sqlite3/include"
		AC_FIND_FILE(sqlite3.h, $sqlite3_incdirs, ac_sqlite3_incdir)
		sqlite3_libdirs="/usr/lib /usr/local/lib /usr/lib/sqlite3 /usr/local/lib/sqlite3 /usr/local/sqlite3/lib /opt/sqlite3/lib"
		sqlite3_libs="libsqlite3.so libsqlite3.a"
		AC_FIND_FILE($sqlite3_libs, $sqlite3_libdirs, ac_sqlite3_libdir)
		if test "$ac_sqlite3_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid SQLite3 directory - include files not found.])
		fi
		if test "$ac_sqlite3_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid SQLite3 directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_sqlite3_libdir, headers in $ac_sqlite3_incdir])
	AM_CONDITIONAL(HAVE_SQLITE3, true)
	
	SQLITE3_LIBS=-lsqlite3
	SQLITE3_INCLUDE=-I$ac_sqlite3_incdir
	SQLITE3_LDFLAGS=-L$ac_sqlite3_libdir
	
	AC_SUBST(SQLITE3_LIBS)
	AC_SUBST(SQLITE3_INCLUDE)
	AC_SUBST(SQLITE3_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## mSQL

AC_DEFUN([AC_CHECK_MSQL],
[
AM_CONDITIONAL(HAVE_MSQL, false)
ac_msql="no"
ac_msql_incdir="no"
ac_msql_libdir="no"

# exported variables
MSQL_LIBS=""
MSQL_LDFLAGS=""
MSQL_INCLUDE=""

AC_MSG_CHECKING(for MiniSQL (mSQL) support)

AC_ARG_WITH(msql,
	[  --with-msql             Include mSQL support.],
	[  ac_msql="$withval" ])
AC_ARG_WITH(msql-dir,
	[  --with-msql-dir         Specifies the mSQL root directory.],
	[  ac_msql_incdir="$withval"/include
	   ac_msql_libdir="$withval"/lib ])
AC_ARG_WITH(msql-incdir,
	[  --with-msql-incdir      Specifies where the mSQL include files are.],
	[  ac_msql_incdir="$withval" ])
AC_ARG_WITH(msql-libdir,
	[  --with-msql-libdir      Specifies where the mSQL libraries are.],
	[  ac_msql_libdir="$withval" ])

if test "$ac_msql" = "yes"; then
	if test "$ac_msql_incdir" = "no" || test "$ac_msql_libs" = "no"; then
		msql_incdirs="/usr/local/msql3/include /usr/msql3/include /opt/msql3/include/ /usr/include /usr/local/include"
		AC_FIND_FILE(msql.h, $msql_incdirs, ac_msql_incdir)
		msql_libdirs="/usr/lib /usr/local/lib /usr/lib/msql3 /usr/local/lib/msql /usr/local/msql3/lib /opt/msql3/lib"
		AC_FIND_FILE(libmsql.a, $msql_libdirs, ac_msql_libdir)
		if test "$ac_msql_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid mSQL directory - include files not found.])
		fi
		if test "$ac_msql_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid mSQL directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_msql_libdir, headers in $ac_msql_incdir])
	AM_CONDITIONAL(HAVE_MSQL, true)
	
	MSQL_LIBS=-lmsql
	MSQL_INCLUDE=-I$ac_msql_incdir
	MSQL_LDFLAGS=-L$ac_msql_libdir
	
	AC_SUBST(MSQL_LIBS)
	AC_SUBST(MSQL_INCLUDE)
	AC_SUBST(MSQL_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## Oracle

AC_DEFUN([AC_CHECK_ORACLE],
[
AM_CONDITIONAL(HAVE_ORACLE, false)
ac_oracle="no"
ac_oracle_incdir="no"
ac_oracle_libdir="no"

# exported variables
ORACLE_LIBS=""
ORACLE_LDFLAGS=""
ORACLE_INCLUDE=""

AC_MSG_CHECKING(for Oracle support)

AC_ARG_WITH(oracle,
	[  --with-oracle           Include Oracle support.],
	[  ac_oracle="$withval" ])
AC_ARG_WITH(oracle-dir,
	[  --with-oracle-dir       Specifies ORACLE_HOME.],
	[  ac_oracle_incdir="$withval"/rdbms/demo
	   ac_oracle_libdir="$withval"/lib ])
AC_ARG_WITH(oracle-incdir,
	[  --with-oracle-incdir    Specifies where the Oracle include files are.],
	[  ac_oracle_incdir="$withval" ])
AC_ARG_WITH(oracle-libdir,
	[  --with-oracle-libdir    Specifies where the Oracle libraries are.],
	[  ac_oracle_libdir="$withval" ])

if test "$ac_oracle" = "yes"; then
	if test "$ac_oracle_incdir" = "no" || test "$ac_oracle_libs" = "no"; then
		oracle_incdirs="$ORACLE_HOME/rdbms/demo $ORACLE_HOME/rdbms/public"
		AC_FIND_FILE(oci.h, $oracle_incdirs, ac_oracle_incdir)
		oracle_libdirs="$ORACLE_HOME/lib"
		AC_FIND_FILE(libclntsh.so, $oracle_libdirs, ac_oracle_libdir)
		if test "$ac_oracle_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Oracle directory - include files not found.])
		fi
		if test "$ac_oracle_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Oracle directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_oracle_libdir, headers in $ac_oracle_incdir])
	AM_CONDITIONAL(HAVE_ORACLE, true)
	
	ORACLE_LIBS=-lclntsh
	ORACLE_INCLUDE="-I$ac_oracle_incdir -I$ORACLE_HOME/rdbms/public"
	ORACLE_LDFLAGS=-L$ac_oracle_libdir
	
	AC_SUBST(ORACLE_LIBS)
	AC_SUBST(ORACLE_INCLUDE)
	AC_SUBST(ORACLE_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## Firebird

AC_DEFUN([AC_CHECK_FIREBIRD_INTERBASE],
[
AM_CONDITIONAL(HAVE_FIREBIRD_INTERBASE, false)
ac_firebird="no"
ac_firebird_incdir="no"
ac_firebird_libdir="no"

# exported variables
FIREBIRD_LIBS=""
FIREBIRD_LDFLAGS=""
FIREBIRD_INCLUDE=""

AC_MSG_CHECKING(for Firebird/Interbase support)

AC_ARG_WITH(firebird,
	[  --with-firebird         Include Firebird support.],
	[  ac_firebird="$withval" ])
AC_ARG_WITH(firebird-dir,
	[  --with-firebird-dir     Specifies Firebird directory.],
	[  ac_firebird_incdir="$withval"/include
	   ac_firebird_libdir="$withval"/lib ])
AC_ARG_WITH(firebird-incdir,
	[  --with-firebird-incdir  Specifies where Firebird/Interbase include files are.],
	[  ac_firebird_incdir="$withval" ])
AC_ARG_WITH(firebird-libdir,
	[  --with-firebird-libdir  Specifies where Firebird/Interbase libraries are.],
	[  ac_firebird_libdir="$withval" ])

if test "$ac_firebird" = "yes"; then
	if test "$ac_firebird_incdir" = "no" || test "$ac_firebird_libs" = "no"; then
		firebird_incdirs="/opt/firebird/include /usr/include /usr/local/include /usr/include/firebird /usr/local/include/firebird /usr/local/firebird/include /opt/firebird/include"
		AC_FIND_FILE(ibase.h, $firebird_incdirs, ac_firebird_incdir)
		firebird_libdirs="/opt/firebird/lib /usr/lib /usr/local/lib /usr/lib/firebird /usr/local/lib/firebird /usr/local/firebird/lib /opt/firebird/lib"
		AC_FIND_FILE(libfbclient.so, $firebird_libdirs, ac_firebird_libdir)
		if test "$ac_firebird_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Firebird/Interbase directory - include files not found.])
		fi
		if test "$ac_firebird_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Firebird/Interbase directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_firebird_libdir, headers in $ac_firebird_incdir])
	AM_CONDITIONAL(HAVE_FIREBIRD_INTERBASE, true)
	
	dnl libfbclient needs pthreads
	AC_SEARCH_LIBS(pthread_create, c_r pthread,,)

	FIREBIRD_LIBS="-lfbclient"
	FIREBIRD_INCLUDE="-I$ac_firebird_incdir"
	FIREBIRD_LDFLAGS=-L$ac_firebird_libdir
	
	AC_SUBST(FIREBIRD_LIBS)
	AC_SUBST(FIREBIRD_INCLUDE)
	AC_SUBST(FIREBIRD_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## Freetds

AC_DEFUN([AC_CHECK_FREETDS],
[
AM_CONDITIONAL(HAVE_FREETDS, false)
ac_freetds="no"
ac_freetds_incdir="no"
ac_freetds_libdir="no"

# exported variables
FREETDS_LIBS=""
FREETDS_LDFLAGS=""
FREETDS_INCLUDE=""

AC_MSG_CHECKING(for Freetds support)

AC_ARG_WITH(freetds,
	[  --with-freetds          Include Freetds support.],
	[  ac_freetds="$withval" ])
AC_ARG_WITH(freetds-dir,
	[  --with-freetds-dir      Specifies FREETDS_HOME.],
	[  ac_freetds_incdir="$withval"/include
	   ac_freetds_libdir="$withval"/lib ])
AC_ARG_WITH(freetds-incdir,
	[  --with-freetds-incdir   Specifies where the Freetds include files are.],
	[  ac_freetds_incdir="$withval" ])
AC_ARG_WITH(freetds-libdir,
	[  --with-freetds-libdir   Specifies where the Freetds libraries are.],
	[  ac_freetds_libdir="$withval" ])

if test "$ac_freetds" = "yes"; then
	if test "$ac_freetds_incdir" = "no" || test "$ac_freetds_libs" = "no"; then
		freetds_incdirs="/usr/include /usr/local/include"
		AC_FIND_FILE(tds.h, $freetds_incdirs, ac_freetds_incdir)
		freetds_libdirs="/usr/lib /usr/local/lib"
		AC_FIND_FILE(libtds.so, $freetds_libdirs, ac_freetds_libdir)
		if test "$ac_freetds_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Freetds directory - include files not found.])
		fi
		if test "$ac_freetds_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Freetds directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_freetds_libdir, headers in $ac_freetds_incdir])
	AM_CONDITIONAL(HAVE_FREETDS, true)
	
	FREETDS_LIBS=-lct
	FREETDS_INCLUDE=-I$ac_freetds_incdir
	FREETDS_LDFLAGS=-L$ac_freetds_libdir
	
	AC_SUBST(FREETDS_LIBS)
	AC_SUBST(FREETDS_INCLUDE)
	AC_SUBST(FREETDS_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])

## Ingres

AC_DEFUN([AC_CHECK_INGRES],
[
AM_CONDITIONAL(HAVE_INGRES, false)
ac_ingres="no"
ac_ingres_incdir="no"
ac_ingres_libdir="no"

# exported variables
INGRES_LIBS=""
INGRES_LDFLAGS=""
INGRES_INCLUDE=""

AC_MSG_CHECKING(for Ingres support)

AC_ARG_WITH(ingres,
	[  --with-ingres           Include Ingres support.],
	[  ac_ingres="$withval" ])
AC_ARG_WITH(ingres-dir,
	[  --with-ingres-dir       Specifies II_SYSTEM.],
	[  ac_ingres_incdir="$withval"/ingres/files
	   ac_ingres_libdir="$withval"/ingres/lib ])
AC_ARG_WITH(ingres-incdir,
	[  --with-ingres-incdir    Specifies where the Ingres include files are.],
	[  ac_ingres_incdir="$withval" ])
AC_ARG_WITH(ingres-libdir,
	[  --with-ingres-libdir    Specifies where the Ingres libraries are.],
	[  ac_ingres_libdir="$withval" ])

if test "$ac_ingres" = "yes"; then
	if test "$ac_ingres_incdir" = "no" || test "$ac_ingres_libs" = "no"; then
		ingres_incdirs="/usr/include /usr/local/include /opt/Ingres/IngresII/ingres/files"
		AC_FIND_FILE(iiapi.h, $ingres_incdirs, ac_ingres_incdir)
		ingres_libdirs="/usr/lib /usr/local/lib /opt/Ingres/IngresII/ingres/lib"
		AC_FIND_FILE(libingres.a, $ingres_libdirs, ac_ingres_libdir)
		if test "$ac_ingres_incdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Ingres directory - include files not found.])
		fi
		if test "$ac_ingres_libdir" = "no"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Ingres directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_ingres_libdir, headers in $ac_ingres_incdir])
	AM_CONDITIONAL(HAVE_INGRES, true)
	
	INGRES_LIBS="-lingres -lpthread -ldl -lm -lcrypt"
	INGRES_INCLUDE=-I$ac_ingres_incdir
	INGRES_LDFLAGS=-L$ac_ingres_libdir
	
	AC_SUBST(INGRES_LIBS)
	AC_SUBST(INGRES_INCLUDE)
	AC_SUBST(INGRES_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])
