##
## originally by Christian Hammers <ch@westend.com>,
## who borrowed ideas from Stephan Kulow.
## then this file was tweaked by David Parker for libdbi.
## and finally it was ripped out and modified by Markus
## Hoenicka who then used for the libdbi-drivers project
##

AC_DEFUN(AC_FIND_FILE,
[
$3=NO
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

AC_DEFUN(AC_CHECK_DBI,
[
AM_CONDITIONAL(HAVE_DBI, false)
ac_dbi_incdir="NO"

# exported variables
DBI_INCLUDE=""

AC_MSG_CHECKING(for libdbi framework)

AC_ARG_WITH(dbi-incdir,
	[  --with-dbi-incdir	  Specifies where the libdbi include files are.],
	[  ac_dbi_incdir="$withval" ])

if test "$ac_dbi_incdir" = "NO"; then
	dbi_incdirs="/usr/include /usr/local/include /sw/include/"
	AC_FIND_FILE(dbi/dbi.h, $dbi_incdirs, ac_dbi_incdir)
	if test "$ac_dbi_incdir" = "NO"; then
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

AC_DEFUN(AC_CHECK_MYSQL,
[
AM_CONDITIONAL(HAVE_MYSQL, false)
ac_mysql="NO"
ac_mysql_incdir="NO"
ac_mysql_libdir="NO"

# exported variables
MYSQL_LIBS=""
MYSQL_LDFLAGS=""
MYSQL_INCLUDE=""

AC_MSG_CHECKING(for MySQL support)

AC_ARG_WITH(mysql,
	[  --with-mysql		  Include MySQL support.],
	[  ac_mysql="YES" ])
AC_ARG_WITH(mysql-dir,
	[  --with-mysql-dir	  Specifies the MySQL root directory.],
	[  ac_mysql_incdir="$withval"/include
	   ac_mysql_libdir="$withval"/lib ])
AC_ARG_WITH(mysql-incdir,
	[  --with-mysql-incdir	  Specifies where the MySQL include files are.],
	[  ac_mysql_incdir="$withval" ])
AC_ARG_WITH(mysql-libdir,
	[  --with-mysql-libdir	  Specifies where the MySQL libraries are.],
	[  ac_mysql_libdir="$withval" ])

if test "$ac_mysql" = "YES"; then
	if test "$ac_mysql_incdir" = "NO" || test "$ac_mysql_libs" = "NO"; then
		mysql_incdirs="/usr/include /usr/local/include /usr/include/mysql /usr/local/include/mysql /usr/local/mysql/include /usr/local/mysql/include/mysql /opt/mysql/include/mysql /sw/include/mysql"
		AC_FIND_FILE(mysql/mysql.h, $mysql_incdirs, ac_mysql_incdir)
		mysql_libdirs="/usr/lib /usr/local/lib /usr/lib/mysql /usr/local/lib/mysql /usr/local/mysql/lib /usr/local/mysql/lib/mysql /opt/mysql/lib/mysql /sw/lib/mysql"
		mysql_libs="libmysqlclient.so libmysqlclient.a"
		AC_FIND_FILE($mysql_libs, $mysql_libdirs, ac_mysql_libdir)
		if test "$ac_mysql_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid MySQL directory - include files not found.])
		fi
		if test "$ac_mysql_libdir" = "NO"; then
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

AC_DEFUN(AC_CHECK_PGSQL,
[
AM_CONDITIONAL(HAVE_PGSQL, false)
ac_pgsql="NO"
ac_pgsql_incdir="NO"
ac_pgsql_libdir="NO"

# exported variables
PGSQL_LIBS=""
PGSQL_LDFLAGS=""
PGSQL_INCLUDE=""

AC_MSG_CHECKING(for PostgreSQL support)

AC_ARG_WITH(pgsql,
	[  --with-pgsql		  Include PostgreSQL support.],
	[  ac_pgsql="YES" ])
AC_ARG_WITH(pgsql-dir,
	[  --with-pgsql-dir	  Specifies the PostgreSQL root directory.],
	[  ac_pgsql_incdir="$withval"/include
	   ac_pgsql_libdir="$withval"/lib ])
AC_ARG_WITH(pgsql-incdir,
	[  --with-pgsql-incdir	  Specifies where the PostgreSQL include files are.],
	[  ac_pgsql_incdir="$withval" ])
AC_ARG_WITH(pgsql-libdir,
	[  --with-pgsql-libdir	  Specifies where the PostgreSQL libraries are.],
	[  ac_pgsql_libdir="$withval" ])

if test "$ac_pgsql" = "YES"; then
	if test "$ac_pgsql_incdir" = "NO" || test "$ac_pgsql_libs" = "NO"; then
		pgsql_incdirs="/usr/include /usr/local/include /usr/include/pgsql /usr/local/include/pgsql /usr/local/pgsql/include /usr/include/postgresql /usr/local/postgresql/include /opt/pgsql/include"
		AC_FIND_FILE(libpq-fe.h, $pgsql_incdirs, ac_pgsql_incdir)
		pgsql_libdirs="/usr/lib /usr/local/lib /usr/lib/pgsql /usr/local/lib/pgsql /usr/local/pgsql/lib /opt/pgsql/lib"
		pgsql_libs="libpq.so libpq.a"
		AC_FIND_FILE($pgsql_libs, $pgsql_libdirs, ac_pgsql_libdir)
		if test "$ac_pgsql_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid PostgreSQL directory - include files not found.])
		fi
		if test "$ac_pgsql_libdir" = "NO"; then
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

AC_DEFUN(AC_CHECK_SQLITE,
[
AM_CONDITIONAL(HAVE_SQLITE, false)
ac_sqlite="NO"
ac_sqlite_incdir="NO"
ac_sqlite_libdir="NO"

# exported variables
SQLITE_LIBS=""
SQLITE_LDFLAGS=""
SQLITE_INCLUDE=""

AC_MSG_CHECKING(for SQLite support)

AC_ARG_WITH(sqlite,
	[  --with-sqlite		  Include SQLite support.],
	[  ac_sqlite="YES" ])
AC_ARG_WITH(sqlite-dir,
	[  --with-sqlite-dir	  Specifies the SQLite root directory.],
	[  ac_sqlite_incdir="$withval"/include
	   ac_sqlite_libdir="$withval"/lib ])
AC_ARG_WITH(sqlite-incdir,
	[  --with-sqlite-incdir	  Specifies where the SQLite include files are.],
	[  ac_sqlite_incdir="$withval" ])
AC_ARG_WITH(sqlite-libdir,
	[  --with-sqlite-libdir	  Specifies where the SQLite libraries are.],
	[  ac_sqlite_libdir="$withval" ])

if test "$ac_sqlite" = "YES"; then
	if test "$ac_sqlite_incdir" = "NO" || test "$ac_sqlite_libs" = "NO"; then
		sqlite_incdirs="/usr/include /usr/local/include /usr/include/sqlite /usr/local/include/sqlite /usr/local/sqlite/include /opt/sqlite/include"
		AC_FIND_FILE(sqlite.h, $sqlite_incdirs, ac_sqlite_incdir)
		sqlite_libdirs="/usr/lib /usr/local/lib /usr/lib/sqlite /usr/local/lib/sqlite /usr/local/sqlite/lib /opt/sqlite/lib"
		sqlite_libs="libsqlite.so libsqlite.a"
		AC_FIND_FILE($sqlite_libs, $sqlite_libdirs, ac_sqlite_libdir)
		if test "$ac_sqlite_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid SQLite directory - include files not found.])
		fi
		if test "$ac_sqlite_libdir" = "NO"; then
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

## mSQL

AC_DEFUN(AC_CHECK_MSQL,
[
AM_CONDITIONAL(HAVE_MSQL, false)
ac_msql="NO"
ac_msql_incdir="NO"
ac_msql_libdir="NO"

# exported variables
MSQL_LIBS=""
MSQL_LDFLAGS=""
MSQL_INCLUDE=""

AC_MSG_CHECKING(for MiniSQL (mSQL) support)

AC_ARG_WITH(msql,
	[  --with-msql		  Include mSQL support.],
	[  ac_msql="YES" ])
AC_ARG_WITH(msql-dir,
	[  --with-msql-dir	  Specifies the mSQL root directory.],
	[  ac_msql_incdir="$withval"/include
	   ac_msql_libdir="$withval"/lib ])
AC_ARG_WITH(msql-incdir,
	[  --with-msql-incdir	  Specifies where the mSQL include files are.],
	[  ac_msql_incdir="$withval" ])
AC_ARG_WITH(msql-libdir,
	[  --with-msql-libdir	  Specifies where the mSQL libraries are.],
	[  ac_msql_libdir="$withval" ])

if test "$ac_msql" = "YES"; then
	if test "$ac_msql_incdir" = "NO" || test "$ac_msql_libs" = "NO"; then
		msql_incdirs="/usr/local/msql3/include /usr/msql3/include /opt/msql3/include/ /usr/include /usr/local/include"
		AC_FIND_FILE(msql.h, $msql_incdirs, ac_msql_incdir)
		msql_libdirs="/usr/lib /usr/local/lib /usr/lib/msql3 /usr/local/lib/msql /usr/local/msql3/lib /opt/msql3/lib"
		AC_FIND_FILE(libmsql.a, $msql_libdirs, ac_msql_libdir)
		if test "$ac_msql_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid mSQL directory - include files not found.])
		fi
		if test "$ac_msql_libdir" = "NO"; then
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

AC_DEFUN(AC_CHECK_ORACLE,
[
AM_CONDITIONAL(HAVE_ORACLE, false)
ac_oracle="NO"
ac_oracle_incdir="NO"
ac_oracle_libdir="NO"

# exported variables
ORACLE_LIBS=""
ORACLE_LDFLAGS=""
ORACLE_INCLUDE=""

AC_MSG_CHECKING(for Oracle support)

AC_ARG_WITH(oracle,
	[  --with-oracle		  Include Oracle support.],
	[  ac_oracle="YES" ])
AC_ARG_WITH(oracle-dir,
	[  --with-oracle-dir	  Specifies ORACLE_HOME.],
	[  ac_oracle_incdir="$withval"/rdbms/demo
	   ac_oracle_libdir="$withval"/lib ])
AC_ARG_WITH(oracle-incdir,
	[  --with-oracle-incdir	  Specifies where the Oracle include files are.],
	[  ac_oracle_incdir="$withval" ])
AC_ARG_WITH(oracle-libdir,
	[  --with-oracle-libdir	  Specifies where the Oracle libraries are.],
	[  ac_oracle_libdir="$withval" ])

if test "$ac_oracle" = "YES"; then
	if test "$ac_oracle_incdir" = "NO" || test "$ac_oracle_libs" = "NO"; then
		oracle_incdirs="$ORACLE_HOME/rdbms/demo $ORACLE_HOME/rdbms/public"
		AC_FIND_FILE(oci.h, $oracle_incdirs, ac_oracle_incdir)
		oracle_libdirs="$ORACLE_HOME/lib"
		AC_FIND_FILE(libclntsh.so, $oracle_libdirs, ac_oracle_libdir)
		if test "$ac_oracle_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Oracle directory - include files not found.])
		fi
		if test "$ac_oracle_libdir" = "NO"; then
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
AC_DEFUN(AC_CHECK_FIREBIRD_INTERBASE,
[
AM_CONDITIONAL(HAVE_FIREBIRD_INTERBASE, false)
ac_firebird="NO"
ac_firebird_incdir="NO"
ac_firebird_libdir="NO"

# exported variables
FIREBIRD_LIBS=""
FIREBIRD_LDFLAGS=""
FIREBIRD_INCLUDE=""

AC_MSG_CHECKING(for Firebird/Interbase support)

AC_ARG_WITH(firebird,
	[  --with-firebird		  Include Firebird support.],
	[  ac_firebird="YES" ])
AC_ARG_WITH(firebird-dir,
	[  --with-firebird-dir	  Specifies Firebird directory.],
	[  ac_firebird_incdir="$withval"/include
	   ac_firebird_libdir="$withval"/lib ])
AC_ARG_WITH(oracle-incdir,
	[  --with-firebird-incdir	  Specifies where the Firebird/Interbase include files are.],
	[  ac_firebird_incdir="$withval" ])
AC_ARG_WITH(firebird-libdir,
	[  --with-firebird-libdir	  Specifies where the Firebird/Interbase libraries are.],
	[  ac_firebird_libdir="$withval" ])

if test "$ac_firebird" = "YES"; then
	if test "$ac_firebird_incdir" = "NO" || test "$ac_firebird_libs" = "NO"; then
		firebird_incdirs="/opt/firebird/include"
		AC_FIND_FILE(ibase.h, $firebird_incdirs, ac_firebird_incdir)
		firebird_libdirs="/opt/firebird/lib"
		AC_FIND_FILE(libfbclient.so, $firebird_libdirs, ac_firebird_libdir)
		if test "$ac_firebird_incdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Firebird/Interbase directory - include files not found.])
		fi
		if test "$ac_firebird_libdir" = "NO"; then
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid Firebird/Interbase directory - libraries not found.])
		fi
	fi
	AC_MSG_RESULT([yes: libs in $ac_firebird_libdir, headers in $ac_firebird_incdir])
	AM_CONDITIONAL(HAVE_FIREBIRD_INTERBASE, true)
	
	FIREBIRD_LIBS=-lfbclient
	FIREBIRD_INCLUDE="-I$ac_firebird_incdir"
	FIREBIRD_LDFLAGS=-L$ac_firebird_libdir
	
	AC_SUBST(FIREBIRD_LIBS)
	AC_SUBST(FIREBIRD_INCLUDE)
	AC_SUBST(FIREBIRD_LDFLAGS)
else
	AC_MSG_RESULT(no)
fi
])
