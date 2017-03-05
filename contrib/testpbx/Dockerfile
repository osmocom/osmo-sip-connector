FROM debian:jessie

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends wget

# They use comodo.. it was hacked.. so don't bother trying to
# install the right root certificates...
RUN wget --no-check-certificate -O - https://files.freeswitch.org/repo/deb/debian/freeswitch_archive_g0.pub | apt-key add -
RUN echo "deb http://files.freeswitch.org/repo/deb/freeswitch-1.6/ jessie main" > /etc/apt/sources.list.d/freeswitch.list
RUN apt-get update && apt-get install -y freeswitch-meta-all


# Change the config...
COPY configs/vars.xml /etc/freeswitch/vars.xml
COPY configs/acl.conf.xml /etc/freeswitch/autoload_configs/acl.conf.xml
COPY configs/switch.conf.xml /etc/freeswitch/autoload_configs/switch.conf.xml
COPY configs/public.xml /etc/freeswitch/dialplan/public.xml
COPY configs/default.xml /etc/freeswitch/dialplan/default.xml
COPY configs/internal.xml /etc/freeswitch/sip_profiles/internal.xml

# Prepare to run
# Reduce the number of ports.. as otherwise we wait a long time
EXPOSE 6000-6020/udp
EXPOSE 5060/udp
CMD /usr/bin/freeswitch -nf
