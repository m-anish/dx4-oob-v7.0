# from the liveuser script
# and add /ofw
cat >> /etc/fstab <<EOF
/tmp            /tmp            tmpfs         rw          0 0
varcacheyum     /var/cache/yum  tmpfs         rw,mode=755 0 0
vartmp          /var/tmp        tmpfs         rw          0 0
none            /ofw            promfs		  defaults    0 0
EOF

mkdir /ofw
mkdir /bootpart

# fix /usr/share/hal/fdi/policy/10osvendor/05-olpc-detect.fdi
sed -i -e "s/OLPC HGPK ALPS HGPK/OLPC ALPS HGPK/;" /usr/share/hal/fdi/policy/10osvendor/05-olpc-detect.fdi

# create olpc user
# Provide access to ttyUSB nodes (#8434)
/usr/sbin/useradd -m -c "olpc user" -G audio,wheel,uucp,video olpc
/usr/bin/passwd -d olpc

# make sure to own home directory and relax permissions a little
chown olpc:olpc /home/olpc
chmod 755 /home/olpc

# move /root to /home/root (#6700)
mv /root /home/root
ln -sT home/root /root

# allow sudo for olpc user
echo "%wheel ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# Only allow su access to those in the wheel group (#5537)
sed -i -e '1,6s/^#auth/auth/' /etc/pam.d/su

# Here we deal with a conflict between kickstart and olpc-configure
# olpc-configure creates /etc/sysconfig/keyboard based on mfg data on first
# boot, but kickstart also creates this file for the initial image.
# therefore a live system will always be different from any OS image built
# here :(
# solution: don't ship this file in the image, then it won't get checked
rm -f /etc/sysconfig/keyboard

cat >/etc/sysconfig/i18n <<__EOF__
# LANG=C considerably speeds up boot (measured 5sec) -- bernie
LANG=C
SYSFONT=sun12x22
__EOF__

# Restore VT layout and behaviour from OLPC 8.2 builds (#9517)
echo "start on started prefdm" >> /etc/event.d/tty1
sed -i -e 's/mingetty/mingetty --loginpause --autologin root --noclear/' \
	/etc/event.d/tty[12]
rm -f /etc/event.d/tty[3-6]

# Enable tmpfs mounts dictated by rwtab (#9637)
mkdir -p /security/state
sed -i -e 's/TEMPORARY_STATE=no/TEMPORARY_STATE=yes/' \
	-e 's:STATE_MOUNT=.*:STATE_MOUNT=/security/state:' \
	/etc/sysconfig/readonly-root

# Remove resolv.conf from rwtab so that it can be updated atomically (#2748)
sed -i -e "/resolv.conf/d" /etc/rwtab

# Things to store separately in persistent storage
# This means these files can be writable at runtime without breaking the
# pristine-ness of /versions/pristine
cat >/etc/statetab.d/olpc <<EOF
/etc/ssh
/etc/sysconfig/i18n
/etc/timezone
/var/lib/dbus
/var/lib/random-seed
EOF

#prefdm tweaks
sed -i -e 's/respawn limit 10/respawn limit 3/' /etc/event.d/prefdm

cat >>/etc/event.d/prefdm <<EOF
# make sure dcon is unfrozen if something goes wrong.
post-stop script
  echo 0 > /sys/devices/platform/dcon/freeze
end script
EOF

# prevent NM from setting the hostname and modifying /etc/hosts to reflect this
# this allows olpc-configure to set a hostname based on the MAC address
echo "NM_IGNORE_HOSTNAME_LOCALHOST=yes" >> /etc/sysconfig/network

# FIXME: fix this bug and remove this hack
cat >/etc/NetworkManager/dispatcher.d/adhoc-route <<EOF
#!/bin/sh
# workaround for http://dev.laptop.org/ticket/9669
[ "\$1" = "eth0" ] || exit 0
if [ "\$2" = "down" ]; then
	/sbin/ip route del 224.0.0.0/4 dev eth0 &>/dev/null
else
	/sbin/iwconfig \$1 | /bin/grep -q "Mode:Ad-Hoc" && \
		/sbin/ip route add 224.0.0.0/4 dev eth0 &>/dev/null
fi
exit 0
EOF
chmod a+x /etc/NetworkManager/dispatcher.d/adhoc-route

# OLPC CA certificate (#9624)
# this is used by wget, but the Web activity uses cert8.db in its own
# installation (which is then copied into user's profile dir)
# FIXME: move this into olpc-utils - it can be installed into
# /etc/pki/tls/certs and then it can regenerate cert.pem during %post
cat >> /etc/pki/tls/cert.pem <<EOF

-----BEGIN CERTIFICATE-----
MIIHPDCCBSSgAwIBAgIJANxRsiqV2zHqMA0GCSqGSIb3DQEBBQUAMIHEMTowOAYD
VQQDEzFPbmUgTGFwdG9wIFBlciBDaGlsZCBSb290IENlcnRpZmljYXRpb24gQXV0
aG9yaXR5MQswCQYDVQQGEwJVUzEvMC0GA1UEChMmT25lIExhcHRvcCBQZXIgQ2hp
bGQgQXNzb2NpYXRpb24sIEluYy4xFjAUBgNVBAgTDU1hc3NhY2h1c2V0dHMxEjAQ
BgNVBAcTCUNhbWJyaWRnZTEcMBoGCSqGSIb3DQEJARYNY2FAbGFwdG9wLm9yZzAe
Fw0wNzExMjMxODM5NDVaFw0xMjExMjIxODM5NDVaMIHEMTowOAYDVQQDEzFPbmUg
TGFwdG9wIFBlciBDaGlsZCBSb290IENlcnRpZmljYXRpb24gQXV0aG9yaXR5MQsw
CQYDVQQGEwJVUzEvMC0GA1UEChMmT25lIExhcHRvcCBQZXIgQ2hpbGQgQXNzb2Np
YXRpb24sIEluYy4xFjAUBgNVBAgTDU1hc3NhY2h1c2V0dHMxEjAQBgNVBAcTCUNh
bWJyaWRnZTEcMBoGCSqGSIb3DQEJARYNY2FAbGFwdG9wLm9yZzCCAiIwDQYJKoZI
hvcNAQEBBQADggIPADCCAgoCggIBAKN96sjTagQ5G8Piy5RumOXH9x/4U9fAmFhX
+eF7W4r57W/rqjuG8lyPbLOxmm8WP9pLH5wfksKforQSzF36DBe7RxoRTEk6UT7i
zz/KJEiVCYNeLySOpfNdBOETeAoun8xnaw4w7Ng55YMaMx3Tlm6yQ1NDWnbbhPkh
RvbR26SDsWp/AGFpHkEwQcnQFg/yNn6uBlyN/6hEkFqhl+jJsvizKGSkGI1DdmHy
cMchMb4NFQZcB4j/dqe0nOYfn9nb5UXTucUF4y3PDo3QGrYoZ1+a6n+ZwIWlDDCf
N1+Kp0K0G8MECeVYwc0ydnCSXOzeLF2p1Q+IeCH7e3BIVgltEYQgFWKHufnqvwiI
JVYvbY1kiv7KtTgBgsYj8ciis+X0ULcsDcfZSKcYXwQBJ9jA2Qwim8lOCZXx+rCZ
nRLf2oKShs6TVW6uoHgnNq2p3dzzQpzv5miJEB8JlZRg+O8JxBC593rTBq3SuRH2
J4N5nHAI0PaKoirfJ7wRKzpECBNtQLzkRaeyPQUO5VrivCouOqwEUILph0toiM6v
ZcLfvBLP1JLY2CaPMhGlIBv1Bk0/XfOJF+3CXiRrQ7jLcBYrySJawmTMU4B2k3y6
aBPyP4xx/FrMdi/0NxDIV+DDImButZmv6QxOw6MEFKOl3NjnyqsNgW8EZeTGBcUD
YbYtwyJFAgMBAAGjggEtMIIBKTAdBgNVHQ4EFgQU91un4/npByzkHrrIxahc6Gdo
YXkwgfkGA1UdIwSB8TCB7oAU91un4/npByzkHrrIxahc6GdoYXmhgcqkgccwgcQx
OjA4BgNVBAMTMU9uZSBMYXB0b3AgUGVyIENoaWxkIFJvb3QgQ2VydGlmaWNhdGlv
biBBdXRob3JpdHkxCzAJBgNVBAYTAlVTMS8wLQYDVQQKEyZPbmUgTGFwdG9wIFBl
ciBDaGlsZCBBc3NvY2lhdGlvbiwgSW5jLjEWMBQGA1UECBMNTWFzc2FjaHVzZXR0
czESMBAGA1UEBxMJQ2FtYnJpZGdlMRwwGgYJKoZIhvcNAQkBFg1jYUBsYXB0b3Au
b3JnggkA3FGyKpXbMeowDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOCAgEA
ARhXvy/LncRXS6p6omskGSKR0KzJdZIYyGTrc+zQFabBArwULxVJyrCPNoO4DGRr
sqIJ8U0rkPMQPMhgZuaxgXq7cd6CF/+v/A+IWvM2ocxnmLJ+KOhqiKkRFms6M5mi
N6V46cibyNGYZxT/GL5n98cxnSP1/M+MBgdqa/ddb6eSeZHd1sCY/gFcp0iHCMs/
/EKb0/fSopRNNNbKfNwYPDHTYQr5pL6HZeuJrvhYklfP3+JJXaZFF/Zc4Kex3W7J
fUZjD1uf5b/QMCl4gFelFR8MITskIRm+ufcL/FMelLhDK22FNJKefEP4KcQcEaib
GVsBbZtJv36Gudr1PLvYRZseS6opVNhGlRby7BbKPuLsxP16JGDSD9D2hM5l29SE
qBplJ+XRYXNG50Z7k+ItYRI0n2caEMVCCF39zMVSTIeYLKCoy8Ob1s9j6KNPee/j
8Dggl2qznWin2s2VUnp78Gw42uXb037dXTM8FD+8GPmRIxPHiUuNS5HZ6FjvU4qm
NF8iK9vDtOdUUfugohBD4tGhxsBWc7HWSOYlbT2ZK0M7GQyxe/zZ1ilqdBCo92o8
VBLjX53goylEBj2hyPFmIcnU1gzFol5fFkEQ9FQIAK9gqDmNWZ/+UQcUhNS0ZCiJ
DmGtqUGxKcyVBHKCbGemJaLmiIKya//U7ARDvC7kqfc=
-----END CERTIFICATE-----
EOF

# avahi is a bit annoying in that it modifies /etc/avahi/etc/localtime at
# runtime (copied from /etc/localtime at the start of the init script).
# this breaks contents manifest. olpc-update already has a workaround, but
# if we can fix it here (by excluding this file from the image and hence the
# manifest) then we'll be able to see a successful olpc-contents-verify
# after first boot (see #9839)
#
# this copy can probably be removed too, see
# https://bugzilla.redhat.com/show_bug.cgi?id=546215
rm -f /etc/avahi/etc/localtime
