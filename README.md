osmo-sip-connector - Osmocom SIP connector
==========================================

This implements an interface between the MNCC (Mobile Network Call Control)
interface of OsmoMSC (and also previously OsmoNITB) and SIP.

Call identities can be either the MSISDN or the IMSI of the subscriber.

Requirements of Equipment
-------------------------

* DTMF need to be sent using SIP INFO messages. DTMF in RTP is not
supported.
* BTS+PBX and SIP connector+PBX  must be in the same network (UDP must be
able to flow directly between these elements)
* No handover support.
* IP based BTS (e.g. Sysmocom sysmoBTS but no Siemens BS11)
* No emergency calls

Limitations
-----------

* PT of RTP needs to match the one used by the BTS. E.g. AMR needs to use
the same PT as the BTS. This is because rtp_payload2 is not yet supported
by the osmo-bts software.

* AMR SDP file doesn't include the mode-set params and allowed codec modes.
This needs to be configured in some way.

Homepage
--------

You can find the osmo-sip-connector issue tracker and wiki online at
<https://osmocom.org/projects/osmo-sip-conector> and <https://osmocom.org/projects/osmo-sip-conector/wiki>


GIT Repository
--------------

You can clone from the official osmo-msc.git repository using

        git clone https://gitea.osmocom.org/cellular-infrastructure/osmo-sip-connector

There is a web interface at <https://gitea.osmocom.org/cellular-infrastructure/osmo-sip-connector>


Documentation
-------------

User Manuals and VTY reference manuals are [optionally] built in PDF form
as part of the build process.

Pre-rendered PDF version of the current "master" can be found at
[User Manual](https://downloads.osmocom.org/docs/latest/osmosipconnector-usermanual.pdf)
as well as the [VTY Reference Manual](https://downloads.osmocom.org/docs/latest/osmosipconnector-vty-reference.pdf)


Mailing List
------------

Discussions related to osmo-sip-connector are happening on the
openbsc@lists.osmocom.org mailing list, please see
<https://lists.osmocom.org/mailman/listinfo/openbsc> for subscription
options and the list archive.

Please observe the [Osmocom Mailing List
Rules](https://osmocom.org/projects/cellular-infrastructure/wiki/Mailing_List_Rules)
when posting.

Contributing
------------

Our coding standards are described at
<https://osmocom.org/projects/cellular-infrastructure/wiki/Coding_standards>

We us a gerrit based patch submission/review process for managing
contributions.  Please see
<https://osmocom.org/projects/cellular-infrastructure/wiki/Gerrit> for
more details

The current patch queue for osmo-sip-connector can be seen at
<https://gerrit.osmocom.org/#/q/project:osmo-sip-connector+status:open>
