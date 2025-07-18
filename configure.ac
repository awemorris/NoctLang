AC_INIT([noct], [0.1], [awe@noctvm.io])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_RANLIB

AC_CONFIG_HEADERS([config.h])
AC_CHECK_HEADERS([stdbool.h stdint.h inttypes.h sys/types.h])
AC_CONFIG_FILES([Makefile src/Makefile po/Makefile.in man/Makefile])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.21])

AC_ARG_ENABLE([nls],
  [AS_HELP_STRING([--disable-nls], [build without gettext support])],
  [],
  [enable_nls=yes]
)
AM_CONDITIONAL([USE_NLS], [test "x$enable_nls" = "xyes"])

AC_OUTPUT
