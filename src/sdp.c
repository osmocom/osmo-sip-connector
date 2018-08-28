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
	return false;

success:
	sdp_parser_free(parser);
	return true;
}

bool sdp_extract_sdp(struct sip_call_leg *leg, const sip_t *sip, bool any_codec)
{
	sdp_connection_t *conn;
	sdp_session_t *sdp;
	sdp_parser_t *parser;
	sdp_media_t *media;
	const char *sdp_data;
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
		struct in_addr addr;

		if (conn->c_addrtype != sdp_addr_ip4)
			continue;
		inet_aton(conn->c_address, &addr);
		leg->base.ip = addr.s_addr;
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
			if (!any_codec && strcasecmp(map->rm_encoding, leg->wanted_codec) != 0)
				continue;

			leg->base.port = media->m_port;
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
		return false;
	}

	sdp_parser_free(parser);
	return true;
}

char *sdp_create_file(struct sip_call_leg *leg, struct call_leg *other)
{
	struct in_addr net = { .s_addr = htonl(other->ip) };
	char *fmtp_str = NULL, *sdp;

	leg->wanted_codec = app_media_name(other->payload_msg_type);

	if (strcmp(leg->wanted_codec, "AMR") == 0)
		fmtp_str = talloc_asprintf(leg, "a=fmtp:%d octet-align=1\r\n", other->payload_type);

	sdp = talloc_asprintf(leg,
				"v=0\r\n"
				"o=Osmocom 0 0 IN IP4 %s\r\n"
				"s=GSM Call\r\n"
				"c=IN IP4 %s\r\n"
				"t=0 0\r\n"
				"m=audio %d RTP/AVP %d\r\n"
				"%s"
				"a=rtpmap:%d %s/8000\r\n",
				inet_ntoa(net), inet_ntoa(net), /* never use diff. addr! */
				other->port, other->payload_type,
				fmtp_str ? fmtp_str : "",
				other->payload_type,
				leg->wanted_codec);
	talloc_free(fmtp_str);
	return sdp;
}
