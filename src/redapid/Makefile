#
# For GCC/Clang builds on RED Brick Linux
#

## CONFIG #####################################################################

CC ?= gcc

WITH_LOGGING ?= yes

## RULES ######################################################################

RM := rm -f --
INSTALL := install

ifndef prefix
	prefix := /usr
endif

ifndef bindir
	bindir := $(prefix)/bin
endif

ifndef sysconfdir
ifeq ($(prefix),/usr)
	sysconfdir := /etc
else
	sysconfdir := $(prefix)/etc
endif
endif

ifndef localstatedir
ifeq ($(prefix),/usr)
	localstatedir := /var
else
	localstatedir := $(prefix)/var
endif
endif

ifndef datadir
	datadir := $(prefix)/share
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
	E := @
endif
endif

SOURCES := ../daemonlib/array.c \
           ../daemonlib/daemon.c \
           ../daemonlib/config.c \
           ../daemonlib/event.c \
           ../daemonlib/event_posix.c \
           ../daemonlib/io.c \
           ../daemonlib/log.c \
           ../daemonlib/log_posix.c \
           ../daemonlib/packet.c \
           ../daemonlib/pid_file.c \
           ../daemonlib/pipe_posix.c \
           ../daemonlib/queue.c \
           ../daemonlib/socket.c \
           ../daemonlib/socket_posix.c \
           ../daemonlib/threads_posix.c \
           ../daemonlib/utils.c \
           ../daemonlib/writer.c \
           api.c \
           brickd.c \
           config_options.c \
           directory.c \
           file.c \
           main.c \
           network.c \
           object_table.c \
           string.c

OBJECTS := ${SOURCES:.c=.o}
DEPENDS := ${SOURCES:.c=.p}

TARGET := redapid

CFLAGS += -O2 -Wall -Wextra -I..
#CFLAGS += -O0 -g -ggdb

CFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\"" -DLOCALSTATEDIR="\"$(localstatedir)\""
LDFLAGS += -pthread

ifeq ($(WITH_LOGGING),yes)
	CFLAGS += -DDAEMONLIB_WITH_LOGGING
endif

.PHONY: all clean

all: $(DIST) $(TARGET) Makefile

clean: Makefile
	$(E)$(RM) $(GENERATED) $(OBJECTS) $(TARGET) $(DEPENDS)

$(TARGET): $(OBJECTS) Makefile
	@echo LD $@
	$(E)$(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS) $(LIBS)

install: all
	@echo "MD $(bindir)"
	$(E)$(INSTALL) -d -m 755 $(bindir)

	@echo "MD $(sysconfdir)"
	$(E)$(INSTALL) -d -m 755 $(sysconfdir)

	@echo "MD $(sysconfdir)/init.d"
	$(E)$(INSTALL) -d -m 755 $(sysconfdir)/init.d

	@echo "MD $(localstatedir)/log"
	$(E)$(INSTALL) -d -m 755 $(localstatedir)/log

	@echo "MD $(localstatedir)/run"
	$(E)$(INSTALL) -d -m 755 $(localstatedir)/run

	@echo "MD $(datadir)"
	$(E)$(INSTALL) -d -m 755 $(datadir)

	@echo "MD $(datadir)/man"
	$(E)$(INSTALL) -d -m 755 $(datadir)/man

	@echo "MD $(datadir)/man/man8"
	$(E)$(INSTALL) -d -m 755 $(datadir)/man/man8

	@echo "MD $(datadir)/man/man5"
	$(E)$(INSTALL) -d -m 755 $(datadir)/man/man5

	@echo "CP redapid"
	$(E)$(INSTALL) -m 755 redapid $(bindir)

	@echo "CP redapid.conf"
	$(E)$(INSTALL) -m 644 ../build_data/linux/etc/redapid.conf $(sysconfdir)

	@echo "CP redapid (init.d script)"
	$(E)$(INSTALL) -m 755 ../build_data/linux/etc/init.d/redapid $(sysconfdir)/init.d

	@echo "CP redapid.8 (manpage)"
	$(E)$(INSTALL) -m 644 ../build_data/linux/usr/share/man/man8/redapid.8 $(datadir)/man/man8
	$(E)gzip -f $(datadir)/man/man8/redapid.8

	@echo "CP redapid.conf.5 (manpage)"
	$(E)$(INSTALL) -m 644 ../build_data/linux/usr/share/man/man5/redapid.conf.5 $(datadir)/man/man5
	$(E)gzip -f $(datadir)/man/man5/redapid.conf.5

%.o: %.c $(GENERATED) Makefile
	@echo CC $@
	$(E)$(CC) -M $(CFLAGS) -o $*.d $<; cp $*.d $*.p; sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.p; rm -f $*.d
	$(E)$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPENDS)
