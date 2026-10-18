/*
 * Pure key-name -> character logic. No dlsym, no libdbusmenu: this is the
 * single source of truth used by both interposed entry points, and the part
 * that the unit tests exercise directly.
 */
#ifndef DBUSMENU_SHORTCUT_SHIM_KEYMAP_H
#define DBUSMENU_SHORTCUT_SHIM_KEYMAP_H

#include <glib.h>

/* GdkModifierType bits, copied so this file needs no GTK headers. */
#define KEYMAP_MOD_SHIFT   0x1u
#define KEYMAP_MOD_CONTROL 0x4u
#define KEYMAP_MOD_ALT     0x8u
#define KEYMAP_MOD_SUPER   0x4000000u

/* "comma" -> ","; NULL if @name is NULL or not a punctuation key name. */
const char *keymap_literal_for_name(const char *name);

/* GDK keyval (== ASCII code) -> literal, e.g. 0x2c -> ","; NULL if not in the table. */
const char *keymap_literal_for_keyval(guint keyval);

/*
 * Rewrite punctuation key names inside an "aas" shortcut value.
 * Returns @value itself (same pointer, no allocation) when nothing needs to
 * change, including for NULL and for values that are not of type "aas".
 * Otherwise returns a new floating "aas" variant; @value is left untouched.
 */
GVariant *keymap_fix_shortcut(GVariant *value);

/*
 * Build the "aas" shortcut value for a keyval/modifier pair, e.g.
 * (0x2c, CONTROL) -> [["Control", ","]]. Returns a new floating variant, or
 * NULL if @keyval is not a table key (the caller then forwards the call).
 */
GVariant *keymap_build_shortcut(guint keyval, guint modifier);

#endif
