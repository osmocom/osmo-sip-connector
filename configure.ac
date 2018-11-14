AC_INIT([osmo-sip-connector],
	m4_esyscmd([./git-version-gen .tarball-version]),
	[openbsc@lists.osmocom.org])

dnl *This* is the root dir, even if an install-sh exists in ../ or ../../
AC_CONFIG_AUX_DIR([.])

AM_INIT_AUTOMAKE([dist-bzip2])

dnl check for pkg-config (explained in detail in libosmocore/configure.ac)
AC_PATH_PROG(PKG_CONFIG_INSTALLED, pkg-config, no)
if test "x$PKG_CONFIG_INSTALLED" = "xno"; then
        AC_MSG_WARN([You need to install pkg-config])
fi
PKG_PROG_PKG_CONFIG([0.20])

dnl include release helper
RELMAKE='-include osmo-release.mk'
AC_SUBST([RELMAKE])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PROG_CC

PKG_CHECK_MODULES(LIBOSMOCORE, libosmocore >= 0.11.0)
PKG_CHECK_MODULES(LIBOSMOGSM, libosmogsm >= 0.11.0)
PKG_CHECK_MODULES(LIBOSMOVTY, libosmovty >= 0.11.0)
PKG_CHECK_MODULES(SOFIASIP, sofia-sip-ua-glib >= 1.12.0)

AC_ARG_ENABLE(sanitize,
	[AS_HELP_STRING(
		[--enable-sanitize],
		[Compile with address sanitizer enabled],
	)],
	[sanitize=$enableval], [sanitize="no"])
if test x"$sanitize" = x"yes"
then
	CFLAGS="$CFLAGS -fsanitize=address -fsanitize=undefined"
	CPPFLAGS="$CPPFLAGS -fsanitize=address -fsanitize=undefined"
fi

AC_ARG_ENABLE(werror,
	[AS_HELP_STRING(
		[--enable-werror],
		[Turn all compiler warnings into errors, with exceptions:
		 a) deprecation (allow upstream to mark deprecation without breaking builds);
		 b) "#warning" pragmas (allow to remind ourselves of errors without breaking builds)
		]
	)],
	[werror=$enableval], [werror="no"])
if test x"$werror" = x"yes"
then
	WERROR_FLAGS="-Werror"
	WERROR_FLAGS+=" -Wno-error=deprecated -Wno-error=deprecated-declarations"
	WERROR_FLAGS+=" -Wno-error=cpp" # "#warning"
	CFLAGS="$CFLAGS $WERROR_FLAGS"
	CPPFLAGS="$CPPFLAGS $WERROR_FLAGS"
fi

AC_ARG_ENABLE([vty_tests],
		AC_HELP_STRING([--enable-vty-tests],
				[Include the VTY/CTRL tests in make check (deprecated)
				[default=no]]),
		[enable_ext_tests="$enableval"],[enable_ext_tests="no"])
AC_ARG_ENABLE([external_tests],
		AC_HELP_STRING([--enable-external-tests],
				[Include the VTY/CTRL tests in make check [default=no]]),
		[enable_ext_tests="$enableval"],[enable_ext_tests="no"])
if test "x$enable_ext_tests" = "xyes" ; then
	AM_PATH_PYTHON
	AC_CHECK_PROG(OSMOTESTEXT_CHECK,osmotestvty.py,yes)
	 if test "x$OSMOTESTEXT_CHECK" != "xyes" ; then
		AC_MSG_ERROR([Please install osmocom-python to run the VTY/CTRL tests.])
	fi
fi
AC_MSG_CHECKING([whether to enable VTY/CTRL tests])
AC_MSG_RESULT([$enable_ext_tests])
AM_CONDITIONAL(ENABLE_EXT_TESTS, test "x$enable_ext_tests" = "xyes")

# Generate manuals
AC_ARG_ENABLE(manuals,
	[AS_HELP_STRING(
		[--enable-manuals],
		[Generate manual PDFs [default=no]],
	)],
	[osmo_ac_build_manuals=$enableval], [osmo_ac_build_manuals="no"])
AM_CONDITIONAL([BUILD_MANUALS], [test x"$osmo_ac_build_manuals" = x"yes"])
AC_ARG_VAR(OSMO_GSM_MANUALS_DIR, [path to common osmo-gsm-manuals files, overriding pkg-config and "../osmo-gsm-manuals"
	fallback])
if test x"$osmo_ac_build_manuals" = x"yes"
then
	# Find OSMO_GSM_MANUALS_DIR (env, pkg-conf, fallback)
	if test -n "$OSMO_GSM_MANUALS_DIR"; then
		echo "checking for OSMO_GSM_MANUALS_DIR... $OSMO_GSM_MANUALS_DIR (from env)"
	else
		OSMO_GSM_MANUALS_DIR="$($PKG_CONFIG osmo-gsm-manuals --variable=osmogsmmanualsdir 2>/dev/null)"
		if test -n "$OSMO_GSM_MANUALS_DIR"; then
			echo "checking for OSMO_GSM_MANUALS_DIR... $OSMO_GSM_MANUALS_DIR (from pkg-conf)"
		else
			OSMO_GSM_MANUALS_DIR="../osmo-gsm-manuals"
			echo "checking for OSMO_GSM_MANUALS_DIR... $OSMO_GSM_MANUALS_DIR (fallback)"
		fi
	fi
	if ! test -d "$OSMO_GSM_MANUALS_DIR"; then
		AC_MSG_ERROR("OSMO_GSM_MANUALS_DIR does not exist! Install osmo-gsm-manuals or set OSMO_GSM_MANUALS_DIR.")
	fi

	# Find and run check-depends
	CHECK_DEPENDS="$OSMO_GSM_MANUALS_DIR/check-depends.sh"
	if ! test -x "$CHECK_DEPENDS"; then
		CHECK_DEPENDS="osmo-gsm-manuals-check-depends"
	fi
	if ! $CHECK_DEPENDS; then
		AC_MSG_ERROR("missing dependencies for --enable-manuals")
	fi

	# Put in Makefile with absolute path
	OSMO_GSM_MANUALS_DIR="$(realpath "$OSMO_GSM_MANUALS_DIR")"
	AC_SUBST([OSMO_GSM_MANUALS_DIR])
fi

# https://www.freedesktop.org/software/systemd/man/daemon.html
AC_ARG_WITH([systemdsystemunitdir],
     [AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files])],,
     [with_systemdsystemunitdir=auto])
AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"], [
     def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)

     AS_IF([test "x$def_systemdsystemunitdir" = "x"],
   [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
    [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
    with_systemdsystemunitdir=no],
   [with_systemdsystemunitdir="$def_systemdsystemunitdir"])])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
      [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])

AC_MSG_RESULT([CPPFLAGS="$CPPFLAGS"])
AC_MSG_RESULT([CFLAGS="$CFLAGS"])
AC_MSG_RESULT([CXXFLAGS="$CXXFLAGS"])
AC_MSG_RESULT([LDFLAGS="$LDFLAGS"])

AC_OUTPUT(
	src/Makefile
	tests/Makefile
	doc/manuals/Makefile
	contrib/Makefile
	contrib/systemd/Makefile
	doc/Makefile
	doc/examples/Makefile
	Makefile)
