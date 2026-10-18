/*
 * Integration test against the real libdbusmenu. Run as
 *   test_preload fixed     (under LD_PRELOAD=libdbusmenu-shortcut-shim.so)
 *   test_preload original  (without it; proves the test detects the shim)
 *
 * set_shortcut runs headless: it only needs gdk_keyval_name(), not a display.
 */
#include <string.h>

#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-gtk/menuitem.h>

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            g_printerr("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static char *stored_shortcut(DbusmenuMenuitem *item)
{
    GVariant *v = dbusmenu_menuitem_property_get_variant(item, "shortcut");
    return v ? g_variant_print(v, FALSE) : g_strdup("(unset)");
}

static void expect(DbusmenuMenuitem *item, const char *want, const char *what)
{
    char *got = stored_shortcut(item);
    if (strcmp(got, want) != 0) {
        g_printerr("FAIL %s: got %s, want %s\n", what, got, want);
        failures++;
    }
    g_free(got);
}

int main(int argc, char **argv)
{
    gboolean fixed = argc > 1 && strcmp(argv[1], "fixed") == 0;
    if (argc < 2 || (!fixed && strcmp(argv[1], "original") != 0)) {
        g_printerr("usage: %s fixed|original\n", argv[0]);
        return 2;
    }

    DbusmenuMenuitem *item = dbusmenu_menuitem_new();

    GVariant *v = g_variant_new_parsed("[['Control', 'comma']]");
    CHECK(dbusmenu_menuitem_property_set_variant(item, "shortcut", v));
    expect(item, fixed ? "[['Control', ',']]" : "[['Control', 'comma']]", "set_variant");

    CHECK(dbusmenu_menuitem_property_set_shortcut(item, 44, GDK_CONTROL_MASK));
    expect(item, fixed ? "[['Control', ',']]" : "[['Control', 'comma']]", "set_shortcut comma");

    /* Keys beyond comma, with modifiers: the gap in the old shim. */
    CHECK(dbusmenu_menuitem_property_set_shortcut(
        item, '/', GDK_CONTROL_MASK | GDK_SHIFT_MASK));
    expect(item, fixed ? "[['Control', 'Shift', '/']]" : "[['Control', 'Shift', 'slash']]",
           "set_shortcut slash");

    CHECK(dbusmenu_menuitem_property_set_shortcut(item, '[', GDK_MOD1_MASK));
    expect(item, fixed ? "[['Alt', '[']]" : "[['Alt', 'bracketleft']]", "set_shortcut bracketleft");

    /* Other keys and other properties must pass through untouched. */
    CHECK(dbusmenu_menuitem_property_set_shortcut(item, 'a', GDK_CONTROL_MASK));
    expect(item, "[['Control', 'a']]", "set_shortcut a");

    CHECK(dbusmenu_menuitem_property_set_variant(
        item, "label", g_variant_new_string("comma")));
    char *label = g_variant_print(dbusmenu_menuitem_property_get_variant(item, "label"), FALSE);
    CHECK(strcmp(label, "'comma'") == 0);
    g_free(label);

    g_object_unref(item);
    if (failures)
        g_printerr("test_preload (%s): %d failure(s)\n", argv[1], failures);
    else
        g_print("test_preload (%s): ok\n", argv[1]);
    return failures ? 1 : 0;
}
