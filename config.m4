PHP_ARG_WITH(gibson, for Gibson Client support,
[  --with-gibson             Include Gibson Client support])

if test "$PHP_GIBSON" != "no"; then
  SEARCH_PATH="/usr /usr/local /local"
  SEARCH_FOR="/include/gibson.h"

  if test "$PHP_GIBSON" = "yes"; then
    AC_MSG_CHECKING([for Gibson Client headers in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        GIBSON_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  else
    AC_MSG_CHECKING([for Gibson Client headers in $PHP_GIBSON])
    if test -r $PHP_GIBSON/$SEARCH_FOR; then
      GIBSON_DIR=$PHP_GIBSON
      AC_MSG_RESULT([found])
    fi
  fi

  if test -z "$GIBSON_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Cannot find Impala headers])
  fi

  PHP_ADD_INCLUDE($GIBSON_DIR/include)

  LIBNAME=gibsonclient
  LIBSYMBOL=gb_stats

  if test "x$PHP_LIBDIR" = "x"; then
    PHP_LIBDIR=lib
  fi

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $GIBSON_DIR/$PHP_LIBDIR, GIBSON_SHARED_LIBADD)
  ],[
    AC_MSG_ERROR([wrong Gibson Client version or lib not found])
  ],[
    -L$GIBSON_DIR/$PHP_LIBDIR
  ])

  AC_DEFINE(HAVE_GIBSON, 1, [Whether you have Gibson Client support])
  PHP_SUBST(GIBSON_SHARED_LIBADD)
  PHP_NEW_EXTENSION(gibson, gibson.c, $ext_shared)
fi
