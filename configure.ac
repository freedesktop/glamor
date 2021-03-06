#  Copyright 2011 Zhigang Gong.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Process this file with autoconf to produce a configure script


# Initialize Autoconf
AC_PREREQ([2.63])
AC_INIT([glamor-egl],
        [0.6.0],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        [glamor-egl])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

# Initialize X.Org macros 1.8 or later for MAN_SUBSTS set by XORG_MANPAGE_SECTIONS
m4_ifndef([XORG_MACROS_VERSION], 
          [m4_fatal([must install xorg-macros 1.8 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

# Checks for programs.
# AC_PROG_LN_S

# Obtain compiler/linker options for dependencies

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(XORG, [xorg-server >= 1.10])
PKG_CHECK_MODULES(DRI2, [dri2proto >= 2.6])

LIBDRM="libdrm >= 2.4.23"
LIBGL="gl >= 7.1.0"
LIBPIXMAN="pixman-1 >= 0.21.8"
LIBEGL="egl"
LIBGLESV2="glesv2"
LIBGBM="gbm"
LIBGBMv9="gbm >= 9"

# Define a configure option for an alternate input module directory
AC_ARG_WITH(xorg-module-dir,
            AS_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])

AC_SUBST([moduledir])

# Define a configure option for an alternate X Server configuration directory
sysconfigdir=`$PKG_CONFIG --variable=sysconfigdir xorg-server`
AC_ARG_WITH(xorg-conf-dir,
            AS_HELP_STRING([--with-xorg-conf-dir=DIR],
                           [Default xorg.conf.d directory [[default=from $PKG_CONFIG xorg-server]]]),
            [configdir="$withval"],
            [configdir="$sysconfigdir"])
AC_SUBST(configdir)
AM_CONDITIONAL(HAS_XORG_CONF_DIR, [test "x$sysconfigdir" != "x"])

# X Server SDK location is required to install evdev header files
# This location is also relayed in the xorg-evdev.pc file
sdkdir=`$PKG_CONFIG --variable=sdkdir xorg-server`
AC_SUBST([sdkdir])

AC_MSG_CHECKING([whether to include GLAMOR_GLES2 support])
AC_ARG_ENABLE(glamor-gles2,         AS_HELP_STRING([--enable-glamor-gles2], [Build glamor based on gles2 (default: no)]), [GLAMOR_GLES2="$enableval"], [GLAMOR_GLES2=no])
AC_MSG_RESULT([$GLAMOR_GLES2])

AC_MSG_CHECKING([whether to include GLAMOR Xv support])
AC_ARG_ENABLE(xv,         AS_HELP_STRING([--disable-xv], [Disable glamor Xv (default: enabled)]), [GLAMOR_XV="$enableval"], [GLAMOR_XV=yes])
AC_MSG_RESULT([$GLAMOR_XV])
AM_CONDITIONAL([GLAMOR_XV], [test "x$GLAMOR_XV" != xno])

AC_MSG_CHECKING([whether to enable DEBUG])
AC_ARG_ENABLE(debug,         AS_HELP_STRING([--enable-debug], [Build debug version glamor (default: no)]), [DEBUG="$enableval"], [DEBUG=no])
AC_MSG_RESULT([$DEBUG])
if test "x$DEBUG" = xyes; then
   AC_DEFINE(DEBUG, 1, [Enable DEBUG])
fi

if test "x$GLAMOR_GLES2" = xyes; then
   AC_DEFINE(GLAMOR_GLES2,1,[Build glamor over GLES2])
   PKG_CHECK_MODULES(GLESV2, $LIBGLESV2)
   REQUIRED_LIBS="$REQUIRED_LIBS $LIBGLESV2"
   GLAMOR_GL_CFLAGS="$GLESV2_CFLAGS -DGLAMOR_GLES2"
else 
   AC_DEFINE(GLAMOR_GL,1,[Build glamor over GL])
   PKG_CHECK_MODULES(GL, $LIBGL)
   REQUIRED_LIBS="$REQUIRED_LIBS $LIBGL"
   GLAMOR_GL_CFLAGS="$GL_CFLAGS -DGLAMOR_GL"
fi

AC_SUBST([GLAMOR_GL_CFLAGS])

AM_CONDITIONAL([GLAMOR_GLES2], [test "x$GLAMOR_GLES2" = xyes])
if test "x$GLAMOR_XV" = xyes; then
   AC_DEFINE(GLAMOR_XV,1,[Build Xv support])
fi

PKG_CHECK_MODULES([LIBDRM], $LIBDRM)
PKG_CHECK_MODULES(EGL, $LIBEGL, [EGL=yes], [EGL=no])
AM_CONDITIONAL([EGL], [test "x$EGL" = xyes])

if test "x$EGL = xyes"; then
   PKG_CHECK_MODULES(GBM, $LIBGBM, [GBM=yes], [GBM=no])
   if test "x$GBM" = xyes; then
     AC_DEFINE(GLAMOR_HAS_GBM, 1, [Use GBM.])
     AC_DEFINE(GLX_USE_SHARED_DISPATCH, 1, [GLX and GLAMOR share the glapi dispatch table.])
   fi
fi

PKG_CHECK_MODULES(GBMv9, $LIBGBMv9, [GBMv9=yes], [GBMv9=no])

AC_MSG_CHECKING([Enable Glamor Dri3 helpers])
AC_ARG_ENABLE(glamor-dri3, AS_HELP_STRING([--enable-glamor-dri3], [Build glamor Dri3 helpers (default: yes if libgbm >= 9 is detected)]), [GLAMOR_DRI3_HELPERS="$enableval"], [GLAMOR_DRI3_HELPERS=yes])

if test "x$GLAMOR_DRI3_HELPERS" = xyes -a "x$GBMv9" = xno; then
   GLAMOR_DRI3_HELPERS=no
fi

AC_MSG_RESULT([$GLAMOR_DRI3_HELPERS])

if test "x$GLAMOR_DRI3_HELPERS" = xyes; then
    AC_DEFINE(GLAMOR_HAS_DRI3_SUPPORT, 1, [Enable Dri3 helpers])
fi

dnl
dnl TLS detection
dnl
AC_MSG_CHECKING(for thread local storage (TLS) support)
AC_CACHE_VAL(ac_cv_tls, [
    ac_cv_tls=none
    keywords="__thread __declspec(thread)"
    for kw in $keywords ; do
        AC_TRY_COMPILE([int $kw test;], [], ac_cv_tls=$kw)
    done
])
AC_MSG_RESULT($ac_cv_tls)

if test "$ac_cv_tls" != "none"; then
    AC_MSG_CHECKING(for tls_model attribute support)
    AC_CACHE_VAL(ac_cv_tls_model, [
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $STRICT_CFLAGS"
        AC_TRY_COMPILE([int $ac_cv_tls __attribute__((tls_model("initial-exec"))) test;], [],
                       ac_cv_tls_model=yes, ac_cv_tls_model=no)
        CFLAGS="$save_CFLAGS"
    ])
    AC_MSG_RESULT($ac_cv_tls_model)

    if test "x$ac_cv_tls_model" = "xyes" ; then
        mesa_tls=$ac_cv_tls' __attribute__((tls_model("initial-exec")))'
    else
        mesa_tls=$ac_cv_tls
    fi

    AC_DEFINE_UNQUOTED([TLS], $mesa_tls, [The compiler supported TLS storage class, prefering initial-exec if tls_model is supported])
fi

AC_ARG_ENABLE([glx-tls],
    [AS_HELP_STRING([--enable-glx-tls],
        [enable TLS support in GLX @<:@default=disabled@:>@])],
    [GLX_USE_TLS=$enableval
     if test "x$GLX_USE_TLS" = "xyes" && test "${ac_cv_tls}" = "none" ; then
        AC_MSG_ERROR([GLX with TLS support requested, but the compiler does not support it.])
     fi],
    [GLX_USE_TLS=no
     if test "${ac_cv_tls}" != "none" ; then
        GLX_USE_TLS=yes
     fi])
AC_SUBST(GLX_TLS, ${GLX_USE_TLS})

if test "x$GLX_USE_TLS" = xyes ; then
        GLX_DEFINES="-DGLX_USE_TLS -DPTHREADS"
        GLX_SYS_LIBS="$GLX_SYS_LIBS -lpthread"
fi
AC_SUBST([GLX_DEFINES])
AC_SUBST([GLX_SYS_LIBS])

AC_CONFIG_FILES([Makefile
           glamor-egl.pc
           glamor.pc
           src/Makefile
	   conf/Makefile])
AC_OUTPUT
