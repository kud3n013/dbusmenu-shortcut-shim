VERSION := 1.0.0

PREFIX ?= /usr
LIBDIR ?= $(PREFIX)/lib/dbusmenu-shortcut-shim
CC ?= cc
CFLAGS ?= -O2 -g

PKG_CONFIG ?= pkg-config
LIB := libdbusmenu-shortcut-shim.so

# -isystem: keep third-party headers out of -Wpedantic/-Werror.
GLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags glib-2.0 | sed "s/-I/-isystem /g")
GLIB_LIBS := $(shell $(PKG_CONFIG) --libs glib-2.0)
TEST_PKGS := glib-2.0 dbusmenu-glib-0.4 dbusmenu-gtk3-0.4 gtk+-3.0
TEST_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(TEST_PKGS) | sed "s/-I/-isystem /g")
TEST_LIBS := $(shell $(PKG_CONFIG) --libs $(TEST_PKGS))

WARN := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
ALL_CFLAGS := -std=c11 $(WARN) $(CFLAGS)
SO_CFLAGS := -std=c11 $(WARN) $(CFLAGS) -fPIC -fvisibility=hidden $(GLIB_CFLAGS)
SO_LDFLAGS := -shared -Wl,-z,relro,-z,now -Wl,--as-needed $(LDFLAGS)

SAN_CFLAGS := -g -O1

.PHONY: all test check sanitize install clean

all: $(LIB)

$(LIB): src/shim.c src/keymap.c src/keymap.h
	$(CC) $(SO_CFLAGS) src/shim.c src/keymap.c -o $@ $(SO_LDFLAGS) $(GLIB_LIBS) -ldl

tests/test_keymap: tests/test_keymap.c src/keymap.c src/keymap.h
	$(CC) $(ALL_CFLAGS) $(GLIB_CFLAGS) tests/test_keymap.c src/keymap.c -o $@ $(LDFLAGS) $(GLIB_LIBS)

tests/test_preload: tests/test_preload.c
	$(CC) $(ALL_CFLAGS) $(TEST_CFLAGS) tests/test_preload.c -o $@ $(LDFLAGS) $(TEST_LIBS)

test: $(LIB) tests/test_keymap tests/test_preload
	./tests/test_keymap
	LD_PRELOAD=$(CURDIR)/$(LIB) ./tests/test_preload fixed
	env -u LD_PRELOAD ./tests/test_preload original

check: test

# AddressSanitizer cannot run with an interposed dlsym (its __tls_get_addr hook
# resolves itself through dlsym(RTLD_NEXT) and recurses), so the pure logic gets
# ASan+UBSan and the real-libdbusmenu preload test gets UBSan only.
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-fsanitize=address,undefined $(SAN_CFLAGS)' tests/test_keymap
	./tests/test_keymap
	$(MAKE) clean
	$(MAKE) CFLAGS='-fsanitize=undefined $(SAN_CFLAGS)' test
	$(MAKE) clean

install: $(LIB)
	install -Dm755 $(LIB) $(DESTDIR)$(LIBDIR)/$(LIB)
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/dbusmenu-shortcut-shim/LICENSE
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/dbusmenu-shortcut-shim/README.md

clean:
	rm -f $(LIB) tests/test_keymap tests/test_preload
