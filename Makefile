CC=gcc

# original version was not showing in xscreensaver little window;
# it also did not respond to resize window events (for example when run from terminal).

CFLAGS= -D_FORTIFY_SOURCE=2 -Wall -Wextra -pedantic -Os -fstack-protector-strong -fno-omit-frame-pointer
CLIBS = -lX11 -lm

UPDATE_CONFIG= update-xscreensaver-config
CONFIG= ${HOME}/.xscreensaver

DESTDIR=
PREFIX= /usr/local
BINDIR=$(DESTDIR)$(PREFIX)/bin

.PHONY: help all install-all uninstall-all clean test
.SILENT: help

all: neon lol pretty wtf

install-all: install-neon install-lol install-pretty install-wtf

install-% :: bin/lavanet-%
	install -d $(BINDIR)
	install -m 755 $< $(BINDIR)
	[ -e $(CONFIG) ] && ./$(UPDATE_CONFIG) $(CONFIG) $*

uninstall-all: uninstall-neon uninstall-lol uninstall-pretty uninstall-wtf

uninstall-% :: lavanet-%
	rm -rf $(BINDIR)/$<

test:
# $(CONFIG)

% :: lavanet-%/src/lavanet.c lavanet-%/src/vroot.h
	[ -e bin ] || mkdir bin
	$(CC) $(CFLAGS) -o bin/lavanet-$@ $< $(CLIBS)

help:
	echo "make [neon | lol | wtf | pretty]"
	echo "make clean"
	echo "make help"

clean:
	rm -rf bin

