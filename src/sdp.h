#pragma once

#include <osmocom/core/utils.h>

enum sdp_mode {
	SDP_MODE_UNSET = 0,
	SDP_MODE_INACTIVE,
	SDP_MODE_SENDRECV,
	SDP_MODE_SENDONLY,
	SDP_MODE_RECVONLY,
	SDP_MODES_COUNT,
};

extern const struct value_string sdp_mode_names[];
static inline const char *sdp_mode_name(enum sdp_mode mode)
{ return get_value_string(sdp_mode_names, mode); }

const char *sdp_find_mode(const char *sdp_str, enum sdp_mode mode);
enum sdp_mode sdp_get_mode(const char *sdp_str, const char **found_at);
char *sdp_copy_and_set_mode(void *ctx, const char *sdp_str, enum sdp_mode mode);
