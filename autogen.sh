#!/bin/sh
# autogen.sh - generates configure using the autotools
# $Id$
aclocal
automake --add-missing
autoconf
autoheader