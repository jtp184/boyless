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
        "settle 10\n"
        "press a\n"
        "press start 10\n"
        "down left\n"
        "up left\n"
        "screenshot\n"
        "screenshot 7\n"
        "compare 7\n"
        "memory C000\n"
        "memory $C000 $42\n"
        "memory FF40 144\n";
    assert(parse_str(text, &s));
    assert(s.count == 12);
    assert(s.commands[0].type == CMD_WAIT   && s.commands[0].count == 30);
    assert(s.commands[1].type == CMD_SETTLE && s.commands[1].count == 10);
    assert(s.commands[2].type == CMD_PRESS  && s.commands[2].key == GB_KEY_A && s.commands[2].count == 2);
    assert(s.commands[3].type == CMD_PRESS  && s.commands[3].key == GB_KEY_START && s.commands[3].count == 10);
    assert(s.commands[4].type == CMD_DOWN   && s.commands[4].key == GB_KEY_LEFT);
    assert(s.commands[5].type == CMD_UP     && s.commands[5].key == GB_KEY_LEFT);
    assert(s.commands[6].type == CMD_SCREENSHOT && s.commands[6].has_number == false);
    assert(s.commands[7].type == CMD_SCREENSHOT && s.commands[7].has_number && s.commands[7].number == 7);
    assert(s.commands[8].type == CMD_COMPARE && s.commands[8].number == 7);
    assert(s.commands[9].type  == CMD_MEMORY && s.commands[9].addr == 0xC000 && s.commands[9].has_value == false);
    assert(s.commands[10].type == CMD_MEMORY && s.commands[10].addr == 0xC000 && s.commands[10].has_value && s.commands[10].value == 0x42);
    assert(s.commands[11].type == CMD_MEMORY && s.commands[11].addr == 0xFF40 && s.commands[11].has_value && s.commands[11].value == 144);
    script_free(&s);
}

static void test_rejections(void)
{
    script_t s = {0};
    assert(!parse_str("wait\n", &s));         /* missing count */
    assert(!parse_str("wait -1\n", &s));      /* signed */
    assert(!parse_str("wait 99999999999999999999\n", &s)); /* overflow */
    assert(!parse_str("settle 0\n", &s));     /* zero stability is rejected */
    assert(!parse_str("settle\n", &s));       /* missing count */
    assert(!parse_str("press\n", &s));        /* missing key */
    assert(!parse_str("press x\n", &s));      /* unknown key */
    assert(!parse_str("down\n", &s));         /* missing key */
    assert(!parse_str("screenshot a b\n", &s)); /* too many args */
    assert(!parse_str("screenshot foo\n", &s)); /* non-numeric id */
    assert(!parse_str("compare\n", &s));      /* compare needs a number */
    assert(!parse_str("memory\n", &s));       /* memory needs an address */
    assert(!parse_str("memory G000\n", &s));  /* bad hex */
    assert(!parse_str("memory 10000\n", &s)); /* address out of 16-bit range */
    assert(!parse_str("memory 0xC000\n", &s)); /* '0x' prefix not accepted */
    assert(!parse_str("memory -1\n", &s));    /* signed address rejected */
    assert(!parse_str("memory C000 256\n", &s)); /* value out of byte range */
    assert(!parse_str("memory C000 0x42\n", &s)); /* '0x' value prefix not accepted */
    assert(!parse_str("memory C000 -1\n", &s)); /* signed value rejected */
    assert(!parse_str("frobnicate\n", &s));   /* unknown command */
}

int main(void)
{
    test_valid_script();
    test_rejections();
    printf("test_script: OK\n");
    return 0;
}
