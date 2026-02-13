.DEFAULT_GOAL := all

SRC_DIR := $(abspath src)
TESTS_DIR := $(abspath tests)
MAN_DIR := $(abspath man)
TESTS_TARGET ?= all
PREFIX ?= /usr/local
DESTDIR ?=
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 755
INSTALL_DATA ?= $(INSTALL) -m 644
BINDIR := $(DESTDIR)$(PREFIX)/bin
MANDIR := $(DESTDIR)$(PREFIX)/share/man/man1
DOCDIR := $(DESTDIR)$(PREFIX)/share/doc/fscull
TESTS_INSTALL_DIR := $(DESTDIR)$(PREFIX)/share/tests/fscull

.PHONY: all test install distclean
all:
	$(MAKE) -C $(SRC_DIR)

test:
	$(MAKE) -C $(SRC_DIR) all
	PATH="$(SRC_DIR):$$PATH" MANPATH="$(MAN_DIR):$${MANPATH:-}" \
		$(MAKE) -C $(TESTS_DIR) $(TESTS_TARGET)

install:
	$(MAKE) -C $(SRC_DIR) fscull
	$(INSTALL) -d "$(BINDIR)" "$(MANDIR)" "$(DOCDIR)" "$(TESTS_INSTALL_DIR)"
	$(INSTALL_PROGRAM) "$(SRC_DIR)/fscull" "$(BINDIR)/fscull"
	$(INSTALL_DATA) "$(MAN_DIR)/fscull.man" "$(MANDIR)/fscull.man"
	$(INSTALL_DATA) README.md "$(DOCDIR)/README.md"
	$(INSTALL_DATA) COPYING "$(DOCDIR)/COPYING"
	$(INSTALL_DATA) "$(TESTS_DIR)/Makefile" "$(TESTS_INSTALL_DIR)/Makefile"
	$(INSTALL_PROGRAM) "$(TESTS_DIR)/create_testdata.sh" "$(TESTS_INSTALL_DIR)/create_testdata.sh"
	$(INSTALL_PROGRAM) "$(TESTS_DIR)/env_check.sh" "$(TESTS_INSTALL_DIR)/env_check.sh"
	$(INSTALL_PROGRAM) "$(TESTS_DIR)/remove_testdata.sh" "$(TESTS_INSTALL_DIR)/remove_testdata.sh"

distclean:
	$(MAKE) -C $(SRC_DIR) clean
	rm -rf "$(abspath vendor/.local)"

%:
	$(MAKE) -C $(SRC_DIR) $@
