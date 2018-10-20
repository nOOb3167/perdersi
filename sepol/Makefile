.PHONY: xdo load

include /usr/share/selinux/devel/Makefile

pspol.pp: tmp/pspol.mod tmp/pspol.mod.fc
	$(verbose) $(SEMOD_PKG) -o pspol.pp -m tmp/pspol.mod -f tmp/pspol.mod.fc

load:
	$(MAKE) -f /usr/share/selinux/devel/Makefile $@

reload.stamp: pspol.pp
	semodule -i pspol.pp
	touch reload.stamp
