#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "script.h"

static bool parse_str(const char *text, script_t *out)
{
    FILE *f = fmemopen((void *)text, strlen(text), "r");
    assert(f);
    bool ok = script_parse_stream(f, "<test>", out);
    fclose(f);
    return ok;
}

static void test_valid_script(void)
{
    script_t s = {0};
    const char *text =
        "# comment\n"
        "wait 30\n"
        "press a\n"
        "press start 10\n"
        "down left\n"
        "up left\n"
        "screenshot\n"
        "screenshot shot.bmp\n";
    assert(parse_str(text, &s));
    assert(s.count == 7);
    assert(s.commands[0].type == CMD_WAIT  && s.commands[0].count == 30);
    assert(s.commands[1].type == CMD_PRESS && s.commands[1].key == GB_KEY_A && s.commands[1].count == 2);
    assert(s.commands[2].type == CMD_PRESS && s.commands[2].key == GB_KEY_START && s.commands[2].count == 10);
    assert(s.commands[3].type == CMD_DOWN  && s.commands[3].key == GB_KEY_LEFT);
    assert(s.commands[4].type == CMD_UP    && s.commands[4].key == GB_KEY_LEFT);
    assert(s.commands[5].type == CMD_SCREENSHOT && s.commands[5].filename == NULL);
    assert(s.commands[6].type == CMD_SCREENSHOT && strcmp(s.commands[6].filename, "shot.bmp") == 0);
    script_free(&s);
}

static void test_rejections(void)
{
    script_t s = {0};
    assert(!parse_str("wait\n", &s));        /* missing count */
    assert(!parse_str("wait -1\n", &s));     /* signed */
    assert(!parse_str("wait 99999999999999999999\n", &s)); /* overflow */
    assert(!parse_str("press\n", &s));       /* missing key */
    assert(!parse_str("press x\n", &s));     /* unknown key */
    assert(!parse_str("down\n", &s));        /* missing key */
    assert(!parse_str("frobnicate\n", &s));  /* unknown command */
}

int main(void)
{
    test_valid_script();
    test_rejections();
    printf("test_script: OK\n");
    return 0;
}
