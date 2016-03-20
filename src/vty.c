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

#include "vty.h"

static int mncc_vty_go_parent(struct vty *vty);
static int mncc_vty_is_config_node(struct vty *vty, int node);

static struct vty_app_info vty_info = {
	.name		= "OsmoMNCC",
	.version	= PACKAGE_VERSION,
	.go_parent_cb	= mncc_vty_go_parent,
	.is_config_node	= mncc_vty_is_config_node,
	.copyright	= "GNU AGPLv3+\n",
};


static int mncc_vty_go_parent(struct vty *vty)
{
	return 0;
}

static int mncc_vty_is_config_node(struct vty *vty, int node)
{
	return 0;
}

void mncc_sip_vty_init(void)
{
	vty_init(&vty_info);
}
