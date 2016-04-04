#pragma once

#include <sofia-sip/su_wait.h>
#include <sofia-sip/url.h>
#include <sofia-sip/sip.h>
#include <sofia-sip/nua_tag.h>
#include <sofia-sip/su_glib.h>
#include <sofia-sip/nua.h>

struct app_config;
struct call;

struct sip_agent {
	struct app_config	*app;
	su_home_t		home;
	su_root_t		*root;

	nua_t			*nua;
};

void sip_agent_init(struct sip_agent *agent, struct app_config *app);
int sip_agent_start(struct sip_agent *agent);

int sip_create_remote_leg(struct sip_agent *agent, struct call *call);
