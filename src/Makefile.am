bin_PROGRAMS = osmo-sip-connector

AM_CFLAGS=-Wall $(LIBOSMOCORE_CFLAGS) $(LIBOSMOVTY_CFLAGS) $(SOFIASIP_CFLAGS)

osmo_sip_connector_SOURCES = \
		evpoll.c \
		vty.c \
		main.c
osmo_sip_connector_LDADD = \
		$(SOFIASIP_LIBS) \
		$(LIBOSMOCORE_LIBS) \
		$(LIBOSMOVTY_LIBS)
