bin_PROGRAMS = osmo-sip-connector

AM_CFLAGS=-Wall $(LIBOSMOCORE_CFLAGS) $(LIBOSMOVTY_CFLAGS) $(SOFIASIP_CFLAGS)

noinst_HEADERS = \
	evpoll.h vty.h mncc_protocol.h app.h mncc.h sip.h

osmo_sip_connector_SOURCES = \
		sip.c \
		mncc.c \
		evpoll.c \
		vty.c \
		main.c
osmo_sip_connector_LDADD = \
		$(SOFIASIP_LIBS) \
		$(LIBOSMOCORE_LIBS) \
		$(LIBOSMOVTY_LIBS)
