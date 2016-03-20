AC_INIT([osmo-sip-connector],
	m4_esyscmd([./git-version-gen .tarball-version]),
	[openbsc@lists.osmocom.org])
AM_INIT_AUTOMAKE([dist-bzip2])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PROG_CC

PKG_CHECK_MODULES(LIBOSMOCORE, libosmocore >= 0.8.0)
PKG_CHECK_MODULES(LIBOSMOVTY, libosmovty)
PKG_CHECK_MODULES(SOFIASIP, sofia-sip-ua-glib >= 1.12.0)


AC_OUTPUT(
	src/Makefile
	Makefile)
