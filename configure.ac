AC_PREREQ([2.69])
AC_INIT([flux-studio],[1.0.0])

AM_INIT_AUTOMAKE([
	-Wall
	dist-xz
	foreign
	tar-pax
	subdir-objects
])

AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS

# Check for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

AC_CHECK_LIB([pthread], pthread_create)
PKG_CHECK_MODULES([sdl2], [sdl2])
AC_SUBST([sdl2_CFLAGS])
AC_SUBST([sdl2_LIBS])

PKG_CHECK_MODULES([sdl2_gfx], [SDL2_gfx])
AC_SUBST([sdl2_gfx_CFLAGS])
AC_SUBST([sdl2_gfx_LIBS])

PKG_CHECK_MODULES([guile3], [guile-3.0])
AC_SUBST([guile3_CFLAGS])
AC_SUBST([guile3_LIBS])

AC_CHECK_HEADER_STDBOOL

AC_USE_SYSTEM_EXTENSIONS

AC_ARG_ENABLE(debug,
  AS_HELP_STRING(
    [--enable-debug],
    [enable debugging, default: no]),
    [case "${enableval}" in
      yes) debug=true ;;
      no)  debug=false ;;
      *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac],
    [debug=false])
  AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AM_COND_IF(DEBUG,
    AC_DEFINE(DEBUG, 1, [Define to 0 if this is a release build]),
    AC_DEFINE(DEBUG, 0, [Define to 1 or higher if this is a debug build]))

AC_CONFIG_FILES([Makefile src/Makefile src/core/Makefile])
AC_OUTPUT

AC_MSG_RESULT([
        $PACKAGE $VERSION
        =================

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}
        libdir:                 ${libdir}
        includedir:             ${includedir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS} ${sdl2_CFLAGS} ${sdl2_gfx_CFLAGS} ${guile3_CFLAGS}
        ldflags:                ${LIBS} ${sdl2_LIBS} ${sdl2_gfx_LIBS} ${guile3_LIBS}
])

