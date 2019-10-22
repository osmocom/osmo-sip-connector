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

static const struct value_string sdp_mode_attr[] = {
	{ SDP_MODE_UNSET, ""},
	{ SDP_MODE_INACTIVE, "a=inactive\r\n"},
	{ SDP_MODE_SENDRECV, "a=sendrecv\r\n"},
	{ SDP_MODE_SENDONLY, "a=sendonly\r\n"},
	{ SDP_MODE_RECVONLY, "a=recvonly\r\n"},
	{}
};

const struct value_string sdp_mode_names[] = {
	{ SDP_MODE_UNSET, "unset"},
	{ SDP_MODE_INACTIVE, "inactive"},
	{ SDP_MODE_SENDRECV, "sendrecv"},
	{ SDP_MODE_SENDONLY, "sendonly"},
	{ SDP_MODE_RECVONLY, "recvonly"},
	{}
};

/* Return location of a given SDP sendrecv mode attribute, if any. */
const char *sdp_find_mode(const char *sdp_str, enum sdp_mode mode)
{
	const char *found;
	if (mode == SDP_MODE_UNSET)
		return NULL;

	found = strstr(sdp_str, get_value_string(sdp_mode_attr, mode));
	if (!found)
		return NULL;

	/* At the start of the line? */
	if (found > sdp_str && *(found-1) != '\n')
		return NULL;

	return found;
}

/* Return the last SDP sendrecv mode attribute, or SDP_MODE_UNSET */
enum sdp_mode sdp_get_mode(const char *sdp_str, const char **found_at)
{
	const char *at = NULL;
	enum sdp_mode mode = SDP_MODE_UNSET;
	enum sdp_mode i;
	for (i = 0; i < SDP_MODES_COUNT; i++) {
		const char *found = sdp_find_mode(sdp_str, i);
		if (!found)
			continue;
		/* If more than one sendrecv attrib are in the SDP string, let the last one win */
		if (at > found)
			continue;
		at = found;
		mode = i;
	}

	if (found_at)
		*found_at = at;
	return mode;
}

/* Remove all pre-existing occurences of an SDP mode attribute, and then add the given mode attribute (if not
 * SDP_MODE_UNSET). Allocate the resulting string from ctx (talloc). */
char *sdp_copy_and_set_mode(void *ctx, const char *sdp_str, enum sdp_mode mode)
{
	char *removed = talloc_strdup(ctx, sdp_str);
	char *added;
	char *found_at;

	/* Remove */
	while (sdp_get_mode(removed, (const char**)&found_at) != SDP_MODE_UNSET) {
		char *next_line = strchr(found_at, '\n');
		if (!next_line)
			*found_at = '\0';
		else
			strcpy(found_at, next_line);
	}

	/* Add */
	if (mode == SDP_MODE_UNSET)
		return removed;

	added = talloc_asprintf(ctx, "%s%s", removed, get_value_string(sdp_mode_attr, mode));
	talloc_free(removed);
	return added;
}
