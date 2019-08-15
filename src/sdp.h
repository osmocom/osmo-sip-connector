#pragma once

#include <sofia-sip/sip.h>
#include <sofia-sip/sdp.h>

#include <stdbool.h>

struct sip_call_leg;
struct call_leg;

enum sdp_mode_e {
	SDP_MODE_INVALID = -1,
	SDP_MODE_INACTIVE = 0,
	SDP_MODE_SENDONLY = 1,
	SDP_MODE_RECVONLY = 2,
	SDP_MODE_SENDRECV = 3,
};

bool sdp_get_sdp_mode(const sip_t *sip, sdp_mode_t *mode);
bool sdp_screen_sdp(const sip_t *sip);
bool sdp_extract_sdp(struct sip_call_leg *leg, const sip_t *sip, bool any_codec);
char *sdp_create_file(struct sip_call_leg *leg, struct call_leg *other, sdp_mode_t mode);
