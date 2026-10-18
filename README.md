# dbusmenu-shortcut-shim

[![CI](https://github.com/kud3n013/dbusmenu-shortcut-shim/actions/workflows/ci.yml/badge.svg)](https://github.com/kud3n013/dbusmenu-shortcut-shim/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/kud3n013/dbusmenu-shortcut-shim)](LICENSE)
[![Release](https://img.shields.io/github/v/release/kud3n013/dbusmenu-shortcut-shim)](https://github.com/kud3n013/dbusmenu-shortcut-shim/releases/latest)

An `LD_PRELOAD` shim that fixes punctuation shortcut names in DBusMenu so KDE Plasma's Global Menu shows them.

## What and why

Chromium/Electron and Gecko export menu accelerators over DBusMenu (`com.canonical.dbusmenu`) using X11/GDK key names. `Ctrl+,` travels as `[["Control", "comma"]]`.

Plasma's Global Menu applet turns that into `"Ctrl+comma"` and calls `QKeySequence::fromString`. Qt only understands literal characters, so the result is empty and the shortcut tip (for example `Ctrl+,` next to *Settings*) is missing.

The shim rewrites the key names to literal characters before they are stored in the menu item:

| Key name | Character | Key name | Character |
|---|---|---|---|
| `comma` | `,` | `bracketleft` | `[` |
| `period` | `.` | `bracketright` | `]` |
| `semicolon` | `;` | `backslash` | `\` |
| `slash` | `/` | `grave` | `` ` `` |
| `minus` | `-` | `apostrophe` | `'` |
| `plus` | `+` | `equal` | `=` |

## How it works

It interposes three symbols:

- `dbusmenu_menuitem_property_set_variant` (libdbusmenu-glib): rewrites key names in the `"shortcut"` property when it has type `aas`. Every other property, and every `aas` value without such names, is passed through untouched.
- `dbusmenu_menuitem_property_set_shortcut` (libdbusmenu-gtk3): for the keys in the table above (their GDK keyvals equal their ASCII codes) it builds the `aas` value directly, with the modifiers Control, Shift, Alt and Super. Any other key is forwarded to the real function.
- `dlsym`: an application that loads libdbusmenu with `dlopen` and looks the functions up with `dlsym` would bypass plain symbol interposition. The shim answers lookups of those two names with its own versions and forwards everything else.

The original functions are found through `dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5")` (falling back to `GLIBC_2.34`), with `dlopen` fallbacks for `libdbusmenu-glib.so.4` and `libdbusmenu-gtk3.so.4`. The key table lives once, in `src/keymap.c`, and both code paths use it.

## Install

### Arch Linux (archrepo)

Packages and the database are signed with the repository key
`CFB7 9154 3B4B 2D28 21E9  127A C012 A947 6C17 0A37`.

1. Import and locally sign the key:
   ```bash
   curl -sSfL -o /tmp/archrepo.asc https://github.com/kud3n013/archrepo/releases/download/packages/archrepo.asc
   sudo pacman-key --add /tmp/archrepo.asc
   sudo pacman-key --lsign-key CFB791543B4B2D2821E9127AC012A9476C170A37
   ```

2. Add the repository configuration to `/etc/pacman.conf`:
   ```ini
   [archrepo]
   SigLevel = Required
   Server = https://github.com/kud3n013/archrepo/releases/download/packages
   ```

3. Synchronize and install: `sudo pacman -Syy dbusmenu-shortcut-shim`

### From source

Needs a C compiler, `make`, `pkg-config` and `glib2`.

```bash
make
sudo make install
```

`PREFIX` (default `/usr`), `LIBDIR` (default `$(PREFIX)/lib/dbusmenu-shortcut-shim`) and `DESTDIR` are honored.

## Usage

```bash
LD_PRELOAD=/usr/lib/dbusmenu-shortcut-shim/libdbusmenu-shortcut-shim.so <app>
```

In a launcher script, preload only when the library is present:

```bash
shim=/usr/lib/dbusmenu-shortcut-shim/libdbusmenu-shortcut-shim.so
if [ -f "$shim" ]; then
    export LD_PRELOAD="$shim${LD_PRELOAD:+:$LD_PRELOAD}"
fi
exec /path/to/app "$@"
```

## Scope and limitations

It was written for Electron/Chromium apps and Gecko apps (Zotero). It only helps under KDE Plasma's Global Menu, and only for apps that load libdbusmenu. Apps that do not use libdbusmenu are not affected.

## Development

```bash
make test       # unit tests, then a preload test against the real libdbusmenu
make sanitize   # ASan+UBSan on the key logic, UBSan on the preload test
```

`make test` runs `tests/test_preload` twice: with the shim preloaded it must see the fixed value, without it it must see the original value. That second run shows the test really detects the shim. Needs `libdbusmenu-glib`, `libdbusmenu-gtk3` and `gtk3` as well. The test runs headless.

AddressSanitizer is not applied to the preload run: an interposed `dlsym` makes its `__tls_get_addr` hook recurse.

## Releases

Bump `VERSION` in the `Makefile` and `CHANGELOG.md`, merge, then push a tag `vX.Y.Z`. The release workflow checks that the tag matches `VERSION`, runs the tests and publishes `dbusmenu-shortcut-shim-X.Y.Z.tar.gz` and its `.sha256` as release assets. Packagers should use these assets, because their checksum never changes, unlike GitHub's auto-generated archives.

## Security note

The shim is injected into other processes, so it is deliberately small: no constructors, no logging, no environment variables read, NULL-safe, and only the three symbols above are exported. It links against `glib-2.0` and libc only.

## License

[MIT](LICENSE)
