AUTOMAKE_OPTIONS= dist-shar

#config.h.in and acconfig.h to avoid a bug in automake that prevent them to be included in the
#dist...

EXTRA_DIST=oRTP.prj   ortp-config.h.in	TODO


SUBDIRS=src docs build

mod-dist:
	mkdir oRTP-bin
	cp src/mod.o oRTP-bin/.
	cp src/master oRTP-bin/.
	cp src/system oRTP-bin/.
	cp src/LOAD	oRTP-bin/.
	tar -cvf oRTP-bin.tar oRTP-bin/
	gzip oRTP-bin.tar
	rm -rf oRTP-bin
	
