.PHONY: sanity sanity mkuser

include /usr/share/selinux/devel/Makefile

pspol.pp: tmp/pspol.mod tmp/pspol.mod.fc
	$(verbose) $(SEMOD_PKG) -o pspol.pp -m tmp/pspol.mod -f tmp/pspol.mod.fc

reload.stamp: pspol.pp
	semodule -i pspol.pp
	touch reload.stamp

sanity: pspol.pp
	-semodule -l | grep pspol || semodule -i pspol.pp

mkuser:
	semanage user -l | grep pspol_u && semanage user -a -R pspol_r pspol_u || true
	semanage user -m -R pspol_r pspol_u
	semanage user -l | grep pspol_u && semanage login -a -s pspol_u psuser || true
	semanage login -m -s pspol_u psuser
