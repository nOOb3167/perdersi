.PHONY: all seport sereload

include /usr/share/selinux/devel/Makefile

all:

seport:
	semanage port -a -t http_port_t -p tcp 5200 || true
	semanage port -m -t http_port_t -p tcp 5201

sereload: pspol.pp
	semodule -i pspol.pp
	semanage user -a -R pspol_r pspol_u || true
	semanage user -m -R pspol_r pspol_u
	semanage login -a -s pspol_u psuser || true
	semanage login -m -s pspol_u psuser
