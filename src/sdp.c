/*
 * (C) 2016 by Holger Hans Peter Freyther
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "sdp.h"
#include "call.h"
#include "logging.h"
#include "app.h"

#include <talloc.h>

#include <sofia-sip/sdp.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#include <osmocom/core/socket.h>

/*
 * Check if the media mode attribute exists in SDP, in this
 * case update the passed pointer with the media mode
 */
bool sdp_get_sdp_mode(const sip_t *sip, sdp_mode_t *mode) {

	const char *sdp_data;
	sdp_parser_t *parser;
	sdp_session_t *sdp;

	if (!sip->sip_payload || !sip->sip_payload->pl_data) {
		LOGP(DSIP, LOGL_ERROR, "No SDP file\n");
		return false;
	}

	sdp_data = sip->sip_payload->pl_data;
	parser = sdp_parse(NULL, sdp_data, strlen(sdp_data), sdp_f_mode_0000);
	if (!parser) {
		LOGP(DSIP, LOGL_ERROR, "Failed to parse SDP\n");
		return false;
	}

	sdp = sdp_session(parser);
	if (!sdp) {
		LOGP(DSIP, LOGL_ERROR, "No sdp session\n");
		sdp_parser_free(parser);
		return false;
	}

	if (!sdp->sdp_media || !sdp->sdp_media->m_mode) {
		sdp_parser_free(parser);
		return false;
	}

	*mode = sdp->sdp_media->m_mode;
	sdp_parser_free(parser);
	return true;
}

/*
 * We want to decide on the audio codec later but we need to see
 * if it is even including some of the supported ones.
 */
bool sdp_screen_sdp(const sip_t *sip)
{
	const char *sdp_data;
	sdp_parser_t *parser;
	sdp_session_t *sdp;
	sdp_media_t *media;

	if (!sip->sip_payload || !sip->sip_payload->pl_data) {
		LOGP(DSIP, LOGL_ERROR, "No SDP file\n");
		return false;
	}

	sdp_data = sip->sip_payload->pl_data;
	parser = sdp_parse(NULL, sdp_data, strlen(sdp_data), 0);
	if (!parser) {
		LOGP(DSIP, LOGL_ERROR, "Failed to parse SDP\n");
		return false;
	}

	sdp = sdp_session(parser);
	if (!sdp) {
		LOGP(DSIP, LOGL_ERROR, "No sdp session\n");
		sdp_parser_free(parser);
		return false;
	}

	for (media = sdp->sdp_media; media; media = media->m_next) {
		sdp_rtpmap_t *map;

		if (media->m_proto != sdp_proto_rtp)
			continue;
		if (media->m_type != sdp_media_audio)
			continue;

		for (map = media->m_rtpmaps; map; map = map->rm_next) {
			if (strcasecmp(map->rm_encoding, "GSM") == 0)
				goto success;
			if (strcasecmp(map->rm_encoding, "GSM-EFR") == 0)
				goto success;
			if (strcasecmp(map->rm_encoding, "GSM-HR-08") == 0)
				goto success;
			if (strcasecmp(map->rm_encoding, "AMR") == 0)
				goto success;
		}
	}

	sdp_parser_free(parser);
	/* FIXME: osmo-sip-connector should not interfere in codecs at all */
	return false;

success:
	sdp_parser_free(parser);
	return true;
}

/* Extract RTP address, port and payload type from SDP received in SIP message, in order to populate the legacy MNCC
 * fields, for backwards compatibility. osmo-sip-connector now always sends the entire SDP info unchanged via MNCC,
 * which obsoletes the legacy fields. But for backwards compatibility, still populate the legacy fields. */
bool sdp_extract_sdp(struct sip_call_leg *leg, const sip_t *sip, bool any_codec)
{
	sdp_connection_t *conn;
	sdp_session_t *sdp;
	sdp_parser_t *parser;
	sdp_media_t *media;
	const char *sdp_data;
	uint16_t port;
	bool found_conn = false, found_map = false;

	if (!sip->sip_payload || !sip->sip_payload->pl_data) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) but no SDP file\n", leg);
		return false;
	}

	sdp_data = sip->sip_payload->pl_data;
	parser = sdp_parse(NULL, sdp_data, strlen(sdp_data), 0);
	if (!parser) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) failed to parse SDP\n",
			leg);
		return false;
	}

	sdp = sdp_session(parser);
	if (!sdp) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) no sdp session\n", leg);
		sdp_parser_free(parser);
		return false;
	}

	for (conn = sdp->sdp_connection; conn; conn = conn->c_next) {
		switch (conn->c_addrtype) {
		case sdp_addr_ip4:
			if (inet_pton(AF_INET, conn->c_address,
				      &((struct sockaddr_in*)&leg->base.addr)->sin_addr) != 1)
				continue;
			leg->base.addr.ss_family = AF_INET;
			break;
		case sdp_addr_ip6:
			if (inet_pton(AF_INET6, conn->c_address,
				      &((struct sockaddr_in6*)&leg->base.addr)->sin6_addr) != 1)
				continue;
			leg->base.addr.ss_family = AF_INET6;
			break;
		default:
			continue;
		}
		found_conn = true;
		break;
	}

	for (media = sdp->sdp_media; media; media = media->m_next) {
		sdp_rtpmap_t *map;

		if (media->m_proto != sdp_proto_rtp)
			continue;
		if (media->m_type != sdp_media_audio)
			continue;

		for (map = media->m_rtpmaps; map; map = map->rm_next) {
			if (!any_codec
			    && leg->wanted_codec
			    && strcasecmp(map->rm_encoding, leg->wanted_codec) != 0)
				continue;

			port = media->m_port;
			leg->base.payload_type = map->rm_pt;
			found_map = true;
			break;
		}

		if (found_map)
			break;
	}

	if (!found_conn || !found_map) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) did not find %d/%d\n",
			leg, found_conn, found_map);
		sdp_parser_free(parser);
		/* FIXME: osmo-sip-connector should not interfere in codecs at all */
		return false;
	}

	switch (leg->base.addr.ss_family) {
	case AF_INET:
		((struct sockaddr_in*)&leg->base.addr)->sin_port = htons(port);
		break;
	case AF_INET6:
		((struct sockaddr_in6*)&leg->base.addr)->sin6_port = htons(port);
		break;
	default:
		OSMO_ASSERT(0);
	}

	sdp_parser_free(parser);
	return true;
}

/* One leg has sent a SIP or MNCC message, which is now translated/forwarded to the counterpart MNCC or SIP.
 * Take as much from the source's SDP as possible, but make sure the connection mode reflects the 'mode' arg (sendrecv,
 * recvonly, sendonly, inactive).
 * For example, if the MSC sent an MNCC_SETUP_IND, the SDP from the MNCC is found in 'other', while 'leg' reflects the
 * SIP side that should receive this SDP in the SIP Invite that is being composed by the caller of this function.
 * \param leg  The target for which the returned SDP is intended.
 * \param other  The source of which we are to reflect the SDP.
 * \return  SDP string, using 'leg' as talloc ctx.
 */
char *sdp_create_file(struct sip_call_leg *leg, struct call_leg *other, sdp_mode_t mode)
{
	sdp_parser_t *parser;
	sdp_session_t *sdp;
	sdp_media_t *media;
	const char *sdp_data;
	sdp_printer_t *printer;
	char buf[1024];
	const char *sdp_str;
	char *ret;

	sdp_data = other->rx_sdp;

	if (!*sdp_data) {
		/* Legacy compat: We have not received any SDP from the other call leg. Compose some original SDP from
		 * the RTP information we have. */
		char *fmtp_str = NULL;
		char *mode_attribute;
		char ip_addr[INET6_ADDRSTRLEN];
		char ipv;

		osmo_sockaddr_ntop((const struct sockaddr *)&other->addr, ip_addr);
		ipv = other->addr.ss_family == AF_INET6 ? '6' : '4';
		leg->wanted_codec = app_media_name(other->payload_msg_type);
		if (strcmp(leg->wanted_codec, "AMR") == 0)
			fmtp_str = talloc_asprintf(leg, "a=fmtp:%d octet-align=1\r\n", other->payload_type);

		switch (mode) {
		case sdp_inactive:
			mode_attribute = "a=inactive\r\n";
			break;
		case sdp_sendrecv:
			mode_attribute = "a=sendrecv\r\n";
			break;
		case sdp_sendonly:
			mode_attribute = "a=sendonly\r\n";
			break;
		case sdp_recvonly:
			mode_attribute = "a=recvonly\r\n";
			break;
		default:
			OSMO_ASSERT(false);
			break;
		}

		return talloc_asprintf(leg,
				       "v=0\r\n"
				       "o=Osmocom 0 0 IN IP%c %s\r\n"
				       "s=GSM Call\r\n"
				       "c=IN IP%c %s\r\n"
				       "t=0 0\r\n"
				       "m=audio %d RTP/AVP %d\r\n"
				       "%s"
				       "a=rtpmap:%d %s/8000\r\n"
				       "%s",
				       ipv, ip_addr, ipv, ip_addr,
				       osmo_sockaddr_port((const struct sockaddr *)&other->addr),
				       other->payload_type,
				       fmtp_str ? fmtp_str : "",
				       other->payload_type,
				       leg->wanted_codec,
				       mode_attribute);
	}

	/* We have received SDP from the other call leg. Forward this as-is, only apply the mode the caller requests:
	 * parse SDP, set media mode, recompose. */
	/* TODO: currently, we often detect the media mode from parsing SDP, and then forward the same SDP, applying the
	 * same mode to it below. It may make sense to completely skip parsing and composition: the mode is usually
	 * already in the received SDP.
	 * However, there are some invocations of this function (sdp_create_file()) with hardcoded modes. Take a look if
	 * that is really necessary, and if not, just drop below parsing + recomposition and use the sdp_data as is.
	 */
	parser = sdp_parse(NULL, sdp_data, strlen(sdp_data), 0);
	if (!parser) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) failed to parse SDP\n", other);
		return talloc_strdup(leg, sdp_data);
	}

	sdp = sdp_session(parser);
	if (!sdp) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) no sdp session\n", other);
		sdp_parser_free(parser);
		return talloc_strdup(leg, sdp_data);
	}

	for (media = sdp->sdp_media; media; media = media->m_next)
		media->m_mode = mode;

	printer = sdp_print(NULL, sdp, buf, sizeof(buf), sdp_f_mode_always);
	if (!printer) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) failed to print SDP\n", other);
		sdp_parser_free(parser);
		return talloc_strdup(leg, sdp_data);
	}

	sdp_str = sdp_message(printer);
	if (!sdp_str) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) failed to print SDP: %s\n", other, sdp_printing_error(printer));
		sdp_str = sdp_data;
	}

	ret = talloc_strdup(leg, sdp_str);

	sdp_parser_free(parser);
	sdp_printer_free(printer);
	return ret;
}
