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

## SQLITE

AC_DEFUN(AC_CHECK_SQLITE,
[
ac_sqlite="NO"
ac_sqlite_incdir="NO"
ac_sqlite_libdir="NO"

# exported variables
SQLITE_LIBS=""
SQLITE_LFLAGS=""
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
	SQLITE_LFLAGS=-L$ac_sqlite_libdir
	
	AC_SUBST(SQLITE_LIBS)
	AC_SUBST(SQLITE_INCLUDE)
	AC_SUBST(SQLITE_LFLAGS)
else
	AC_MSG_RESULT(no)
fi
])
