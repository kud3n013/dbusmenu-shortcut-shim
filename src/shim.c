/*
 * libdbusmenu-shortcut-shim.so
 *
 * When Chromium/Electron or Gecko export menu accelerators over DBusMenu
 * (com.canonical.dbusmenu), punctuation keys travel as X11/GDK key names:
 * Ctrl+, becomes shortcut: [["Control", "comma"]].
 *
 * KDE Plasma's Global Menu applet (plasma-workspace, applets/appmenu) maps
 * "Control" -> "Ctrl", joins with "+" and calls QKeySequence::fromString(
 * "Ctrl+comma"). Qt only understands literal characters (","), so the result
 * is empty and the shortcut tip (e.g. Ctrl+, for Settings) is lost.
 *
 * This shim is meant for LD_PRELOAD. It rewrites those key names to literal
 * characters before they are stored in the menu item. It interposes:
 *   - dbusmenu_menuitem_property_set_variant (libdbusmenu-glib)
 *   - dbusmenu_menuitem_property_set_shortcut (libdbusmenu-gtk3)
 *   - dlsym, so apps that look the two symbols up dynamically (Electron
 *     dlopens libdbusmenu-glib) get the shim's versions as well.
 *
 * It is injected into other people's processes: it has no constructors, prints
 * nothing, reads no environment and exports only the three symbols above.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <glib.h>
#include <string.h>

#include "keymap.h"

#define SHIM_EXPORT __attribute__((visibility("default")))

typedef void DbusmenuMenuitem;
typedef void *(*dlsym_fn)(void *, const char *);
typedef gboolean (*set_variant_fn)(DbusmenuMenuitem *, const gchar *, GVariant *);
typedef gboolean (*set_shortcut_fn)(DbusmenuMenuitem *, guint, guint);

static dlsym_fn orig_dlsym;
static set_variant_fn real_set_variant;
static set_shortcut_fn real_set_shortcut;

/* dlsym and friends trade function pointers as void*; POSIX requires that to work. */
static dlsym_fn as_dlsym(void *p) { return __extension__(dlsym_fn) p; }
static set_variant_fn as_set_variant(void *p) { return __extension__(set_variant_fn) p; }
static set_shortcut_fn as_set_shortcut(void *p) { return __extension__(set_shortcut_fn) p; }
static void *fn_ptr(void (*fn)(void)) { return __extension__(void *) fn; }

/*
 * Resolve the real functions lazily; a constructor would run in every process
 * that loads us. Resolution is idempotent, so a racing second thread only
 * repeats the same lookups. glibc exposes dlsym under two versions depending
 * on its age, hence the dlvsym pair.
 */
static void init_orig(void)
{
    if (!orig_dlsym) {
        orig_dlsym = as_dlsym(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5"));
        if (!orig_dlsym)
            orig_dlsym = as_dlsym(dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34"));
    }
    if (!real_set_variant && orig_dlsym) {
        real_set_variant = as_set_variant(
            orig_dlsym(RTLD_NEXT, "dbusmenu_menuitem_property_set_variant"));
        if (!real_set_variant) {
            void *h = dlopen("libdbusmenu-glib.so.4", RTLD_LAZY);
            if (h)
                real_set_variant = as_set_variant(
                    orig_dlsym(h, "dbusmenu_menuitem_property_set_variant"));
        }
    }
    if (!real_set_shortcut && orig_dlsym) {
        real_set_shortcut = as_set_shortcut(
            orig_dlsym(RTLD_NEXT, "dbusmenu_menuitem_property_set_shortcut"));
        if (!real_set_shortcut) {
            void *h = dlopen("libdbusmenu-gtk3.so.4", RTLD_LAZY);
            if (h)
                real_set_shortcut = as_set_shortcut(
                    orig_dlsym(h, "dbusmenu_menuitem_property_set_shortcut"));
        }
    }
}

SHIM_EXPORT gboolean dbusmenu_menuitem_property_set_variant(
    DbusmenuMenuitem *item, const gchar *property, GVariant *value)
{
    init_orig();
    if (!real_set_variant)
        return FALSE;
    if (property && value && strcmp(property, "shortcut") == 0)
        value = keymap_fix_shortcut(value);
    return real_set_variant(item, property, value);
}

SHIM_EXPORT gboolean dbusmenu_menuitem_property_set_shortcut(
    DbusmenuMenuitem *item, guint key, guint modifier)
{
    init_orig();

    GVariant *shortcut = keymap_build_shortcut(key, modifier);
    if (shortcut) {
        if (!real_set_variant) {
            g_variant_ref_sink(shortcut);
            g_variant_unref(shortcut);
            return FALSE;
        }
        return real_set_variant(item, "shortcut", shortcut);
    }
    return real_set_shortcut ? real_set_shortcut(item, key, modifier) : FALSE;
}

SHIM_EXPORT void *dlsym(void *handle, const char *symbol)
{
    init_orig();
    const char *sym = symbol; /* callers may pass NULL despite glibc's nonnull */
    if (sym) {
        if (strcmp(sym, "dbusmenu_menuitem_property_set_shortcut") == 0)
            return fn_ptr((void (*)(void))dbusmenu_menuitem_property_set_shortcut);
        if (strcmp(sym, "dbusmenu_menuitem_property_set_variant") == 0)
            return fn_ptr((void (*)(void))dbusmenu_menuitem_property_set_variant);
    }
    return orig_dlsym ? orig_dlsym(handle, symbol) : NULL;
}
