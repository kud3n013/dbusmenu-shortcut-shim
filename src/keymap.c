#include "keymap.h"

#include <string.h>

/* One table drives both code paths. A literal's first byte is its GDK keyval. */
static const struct {
    const char *name;
    const char *literal;
} keymap_table[] = {
    {"comma",        ","},
    {"period",       "."},
    {"semicolon",    ";"},
    {"slash",        "/"},
    {"minus",        "-"},
    {"plus",         "+"},
    {"equal",        "="},
    {"bracketleft",  "["},
    {"bracketright", "]"},
    {"backslash",    "\\"},
    {"grave",        "`"},
    {"apostrophe",   "'"},
};

#define KEYMAP_LEN (sizeof keymap_table / sizeof keymap_table[0])

const char *keymap_literal_for_name(const char *name)
{
    if (!name)
        return NULL;
    for (size_t i = 0; i < KEYMAP_LEN; i++) {
        if (strcmp(name, keymap_table[i].name) == 0)
            return keymap_table[i].literal;
    }
    return NULL;
}

const char *keymap_literal_for_keyval(guint keyval)
{
    for (size_t i = 0; i < KEYMAP_LEN; i++) {
        if (keyval == (guint)(unsigned char)keymap_table[i].literal[0])
            return keymap_table[i].literal;
    }
    return NULL;
}

/* Scan first so the common "nothing to do" case never allocates. */
static gboolean needs_fix(GVariant *value)
{
    GVariantIter outer;
    GVariant *inner;
    gboolean found = FALSE;

    g_variant_iter_init(&outer, value);
    while (!found && (inner = g_variant_iter_next_value(&outer)) != NULL) {
        GVariantIter it;
        const gchar *str;

        g_variant_iter_init(&it, inner);
        while (!found && g_variant_iter_next(&it, "&s", &str))
            found = keymap_literal_for_name(str) != NULL;
        g_variant_unref(inner);
    }
    return found;
}

GVariant *keymap_fix_shortcut(GVariant *value)
{
    if (!value || !g_variant_is_of_type(value, G_VARIANT_TYPE("aas")))
        return value;
    if (!needs_fix(value))
        return value;

    GVariantBuilder outer;
    GVariantIter outer_it;
    GVariant *inner_val;

    g_variant_builder_init(&outer, G_VARIANT_TYPE("aas"));
    g_variant_iter_init(&outer_it, value);
    while ((inner_val = g_variant_iter_next_value(&outer_it)) != NULL) {
        GVariantBuilder inner;
        GVariantIter it;
        const gchar *str;

        g_variant_builder_init(&inner, G_VARIANT_TYPE("as"));
        g_variant_iter_init(&it, inner_val);
        while (g_variant_iter_next(&it, "&s", &str)) {
            const char *lit = keymap_literal_for_name(str);
            g_variant_builder_add(&inner, "s", lit ? lit : str);
        }
        g_variant_builder_add_value(&outer, g_variant_builder_end(&inner));
        g_variant_unref(inner_val);
    }
    return g_variant_builder_end(&outer);
}

GVariant *keymap_build_shortcut(guint keyval, guint modifier)
{
    const char *lit = keymap_literal_for_keyval(keyval);
    if (!lit)
        return NULL;

    GVariantBuilder inner, outer;

    g_variant_builder_init(&inner, G_VARIANT_TYPE("as"));
    if (modifier & KEYMAP_MOD_CONTROL)
        g_variant_builder_add(&inner, "s", "Control");
    if (modifier & KEYMAP_MOD_SHIFT)
        g_variant_builder_add(&inner, "s", "Shift");
    if (modifier & KEYMAP_MOD_ALT)
        g_variant_builder_add(&inner, "s", "Alt");
    if (modifier & KEYMAP_MOD_SUPER)
        g_variant_builder_add(&inner, "s", "Super");
    g_variant_builder_add(&inner, "s", lit);

    g_variant_builder_init(&outer, G_VARIANT_TYPE("aas"));
    g_variant_builder_add_value(&outer, g_variant_builder_end(&inner));
    return g_variant_builder_end(&outer);
}
