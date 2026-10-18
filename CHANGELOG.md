# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-18

### Added
- Single shared implementation of the shim previously copy-pasted into the `cursor`, `obsidian`, `zotero` and `zalo-for-linux` packages.
- Key-name table in `src/keymap.c` used by both interposed entry points.
- Unit tests and a preload integration test against the real libdbusmenu.
- CI, release and Dependabot configuration.

### Changed
- `dbusmenu_menuitem_property_set_shortcut` now handles every punctuation key in the table, not only comma.
- Shortcut values without punctuation names are returned without any allocation.
