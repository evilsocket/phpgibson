PHP_ARG_ENABLE(gibson, whether to enable Gibson Client support,[ --enable-gibson   Enable Gibson Client support])
if test "$PHP_GIBSON" = "yes"; then
  AC_DEFINE(HAVE_GIBSON, 1, [Whether you have Gibson Client])
  PHP_SUBST(GIBSON_SHARED_LIBADD)
  PHP_ADD_LIBRARY(gibsonclient, 1, GIBSON_SHARED_LIBADD)
  PHP_NEW_EXTENSION(gibson, gibson.c, $ext_shared)
fi
