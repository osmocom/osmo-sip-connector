#pragma once
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/stats.h>

enum {
	SIP_CTR_CALL_INITIATED,
	SIP_CTR_CALL_FAILED,
	SIP_CTR_CALL_CONNECTED,
	SIP_CTR_CALL_RELEASED,
};

static const struct rate_ctr_desc sip_ctr_description[] = {
	[SIP_CTR_CALL_INITIATED] = {"call:initiated", "Call(s) initiated."},
	[SIP_CTR_CALL_FAILED] = {"call:failed", "Call(s) failed."},
	[SIP_CTR_CALL_CONNECTED] = {"call:connected", "Call(s) connected."},
	[SIP_CTR_CALL_RELEASED] = {"call:released", "Call(s) released."},
};

static const struct rate_ctr_group_desc sip_ctrg_desc = {
	"sip-connector",
	"SIP connector",
	OSMO_STATS_CLASS_GLOBAL,
	ARRAY_SIZE(sip_ctr_description),
	sip_ctr_description
};
