#!/usr/bin/env python

# (C) 2016 by Holger Hans Peter Freyther

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>


app_configs = {
    "osmo-sip-connector": ["doc/examples/osmo-sip-connector.cfg"]
}

apps = [
    (4256, "src/osmo-sip-connector", "OsmoSIPcon", "osmo-sip-connector")
]

vty_command = ["./src/osmo-sip-connector", "-c",
               "doc/examples/osmo-sip-connector.cfg"]
vty_app = apps[0]
