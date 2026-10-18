#include <string.h>

#include "../src/keymap.h"

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            g_printerr("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static const struct {
    const char *name;
    const char *literal;
} cases[] = {
    {"comma", ","}, {"period", "."}, {"semicolon", ";"}, {"slash", "/"},
    {"minus", "-"}, {"plus", "+"}, {"equal", "="}, {"bracketleft", "["},
    {"bracketright", "]"}, {"backslash", "\\"}, {"grave", "`"},
    {"apostrophe", "'"},
};

/* Returns the fixed value as text and releases it; *same tells if it was @in. */
static char *fixed_text(GVariant *in, gboolean *same)
{
    GVariant *out = keymap_fix_shortcut(in);
    char *s = g_variant_print(out, FALSE);
    *same = (out == in);
    if (!*same) {
        g_variant_ref_sink(out);
        g_variant_unref(out);
    }
    return s;
}

static void test_names(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(cases); i++) {
        CHECK(strcmp(keymap_literal_for_name(cases[i].name), cases[i].literal) == 0);
        CHECK(keymap_literal_for_keyval((guint)(unsigned char)cases[i].literal[0]) != NULL);
        CHECK(strcmp(keymap_literal_for_keyval((guint)(unsigned char)cases[i].literal[0]),
                     cases[i].literal) == 0);
    }
    CHECK(keymap_literal_for_name(NULL) == NULL);
    CHECK(keymap_literal_for_name("") == NULL);
    CHECK(keymap_literal_for_name("Comma") == NULL);
    CHECK(keymap_literal_for_name("a") == NULL);
    CHECK(keymap_literal_for_name("commas") == NULL);
    CHECK(keymap_literal_for_keyval(0) == NULL);
    CHECK(keymap_literal_for_keyval('a') == NULL);
    CHECK(keymap_literal_for_keyval(0x10000 + ',') == NULL);
}

static void test_fix_every_name(void)
{
    for (size_t i = 0; i < G_N_ELEMENTS(cases); i++) {
        gboolean same;
        GVariant *in = g_variant_ref_sink(
            g_variant_new_parsed("[['Control', %s]]", cases[i].name));
        char *got = fixed_text(in, &same);
        GVariant *want = g_variant_ref_sink(
            g_variant_new_parsed("[['Control', %s]]", cases[i].literal));
        char *want_v = g_variant_print(want, FALSE);
        CHECK(!same);
        CHECK(strcmp(got, want_v) == 0);
        g_free(got);
        g_free(want_v);
        g_variant_unref(want);
        g_variant_unref(in);
    }
}

static void test_unchanged(void)
{
    const char *texts[] = {
        "@aas []",
        "[@as []]",
        "[['Control', 'a']]",
        "[['Control', 'Shift', 'F5'], ['Alt', 'Left']]",
        "[['Control', ',']]",
    };
    for (size_t i = 0; i < G_N_ELEMENTS(texts); i++) {
        gboolean same;
        GVariant *in = g_variant_ref_sink(g_variant_new_parsed(texts[i]));
        char *got = fixed_text(in, &same);
        CHECK(same);
        g_free(got);
        g_variant_unref(in);
    }
}

static void test_multi(void)
{
    gboolean same;
    GVariant *in = g_variant_ref_sink(g_variant_new_parsed(
        "[['Control', 'Shift', 'Alt', 'bracketleft'], ['Super', 'a'], ['slash']]"));
    char *got = fixed_text(in, &same);
    CHECK(!same);
    CHECK(strcmp(got, "[['Control', 'Shift', 'Alt', '['], ['Super', 'a'], ['/']]") == 0);
    g_free(got);
    g_variant_unref(in);
}

static void test_non_aas_and_null(void)
{
    gboolean same;
    GVariant *in = g_variant_ref_sink(g_variant_new_string("comma"));
    char *got = fixed_text(in, &same);
    CHECK(same);
    CHECK(strcmp(got, "'comma'") == 0);
    g_free(got);
    g_variant_unref(in);

    in = g_variant_ref_sink(g_variant_new_parsed("['comma']"));
    got = fixed_text(in, &same);
    CHECK(same);
    g_free(got);
    g_variant_unref(in);

    CHECK(keymap_fix_shortcut(NULL) == NULL);
}

static void test_build(void)
{
    GVariant *v = g_variant_ref_sink(keymap_build_shortcut(',', KEYMAP_MOD_CONTROL));
    char *s = g_variant_print(v, FALSE);
    CHECK(strcmp(s, "[['Control', ',']]") == 0);
    g_free(s);
    g_variant_unref(v);

    v = g_variant_ref_sink(keymap_build_shortcut(
        '/', KEYMAP_MOD_CONTROL | KEYMAP_MOD_SHIFT | KEYMAP_MOD_ALT | KEYMAP_MOD_SUPER));
    s = g_variant_print(v, FALSE);
    CHECK(strcmp(s, "[['Control', 'Shift', 'Alt', 'Super', '/']]") == 0);
    g_free(s);
    g_variant_unref(v);

    v = g_variant_ref_sink(keymap_build_shortcut('\\', 0));
    s = g_variant_print(v, FALSE);
    CHECK(strcmp(s, "[['\\\\']]") == 0);
    g_free(s);
    g_variant_unref(v);

    for (size_t i = 0; i < G_N_ELEMENTS(cases); i++) {
        v = keymap_build_shortcut((guint)(unsigned char)cases[i].literal[0], 0);
        CHECK(v != NULL);
        g_variant_ref_sink(v);
        g_variant_unref(v);
    }
    CHECK(keymap_build_shortcut('a', KEYMAP_MOD_CONTROL) == NULL);
    CHECK(keymap_build_shortcut(0, 0) == NULL);
}

int main(void)
{
    test_names();
    test_fix_every_name();
    test_unchanged();
    test_multi();
    test_non_aas_and_null();
    test_build();
    if (failures)
        g_printerr("%d check(s) failed\n", failures);
    else
        g_print("test_keymap: all checks passed\n");
    return failures ? 1 : 0;
}
