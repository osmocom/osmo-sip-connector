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

AC_OUTPUT(
	src/Makefile
	tests/Makefile
	Makefile)
