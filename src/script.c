#include "script.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int parse_key(const char *s)
{
    if (strcasecmp(s, "a") == 0)      return GB_KEY_A;
    if (strcasecmp(s, "b") == 0)      return GB_KEY_B;
    if (strcasecmp(s, "start") == 0)  return GB_KEY_START;
    if (strcasecmp(s, "select") == 0) return GB_KEY_SELECT;
    if (strcasecmp(s, "up") == 0)     return GB_KEY_UP;
    if (strcasecmp(s, "down") == 0)   return GB_KEY_DOWN;
    if (strcasecmp(s, "left") == 0)   return GB_KEY_LEFT;
    if (strcasecmp(s, "right") == 0)  return GB_KEY_RIGHT;
    return -1;
}

static bool parse_uint(const char *s, unsigned *out)
{
    if (!s || !*s) return false;
    if (s[0] == '-' || s[0] == '+') return false;
    char *end;
    unsigned long v = strtoul(s, &end, 10);
    if (*end != 0) return false;
    if (v > UINT_MAX) return false;
    *out = (unsigned)v;
    return true;
}

/* Parse a 16-bit hex address. Accepts an optional '$' prefix. Every remaining
   character must be a hex digit, so signs and a '0x' prefix are rejected. */
static bool parse_hex16(const char *s, uint16_t *out)
{
    if (!s || !*s) return false;
    if (s[0] == '$') s++;
    if (!*s) return false;
    for (const char *p = s; *p; p++)
        if (!isxdigit((unsigned char)*p)) return false;
    char *end;
    unsigned long v = strtoul(s, &end, 16);
    if (*end != 0) return false;
    if (v > 0xFFFF) return false;
    *out = (uint16_t)v;
    return true;
}

/* Parse a single byte value. Decimal by default; a '$' prefix selects hex.
   Every remaining character must be a digit of the chosen base, so signs and
   a '0x' prefix are rejected. */
static bool parse_byte(const char *s, unsigned *out)
{
    if (!s || !*s) return false;
    int base = 10;
    if (s[0] == '$') { s++; base = 16; }
    if (!*s) return false;
    for (const char *p = s; *p; p++) {
        int ok = base == 16 ? isxdigit((unsigned char)*p) : isdigit((unsigned char)*p);
        if (!ok) return false;
    }
    char *end;
    unsigned long v = strtoul(s, &end, base);
    if (*end != 0) return false;
    if (v > 255) return false;
    *out = (unsigned)v;
    return true;
}

bool script_parse_stream(FILE *f, const char *label, script_t *out)
{
    out->commands = NULL;
    out->count = 0;

    size_t capacity = 16, count = 0;
    command_t *commands = malloc(capacity * sizeof(command_t));
    if (!commands) return false;

    char line[1024];
    unsigned line_no = 0;
    bool error = false;

    while (fgets(line, sizeof(line), f)) {
        line_no++;

        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        char *tokens[4] = {0};
        unsigned ntokens = 0;
        char *saveptr = NULL;
        for (char *tok = strtok_r(line, " \t\r\n", &saveptr);
             tok && ntokens < 4;
             tok = strtok_r(NULL, " \t\r\n", &saveptr)) {
            tokens[ntokens++] = tok;
        }

        if (ntokens == 0) continue;

        if (count == capacity) {
            capacity *= 2;
            command_t *grown = realloc(commands, capacity * sizeof(command_t));
            if (!grown) { error = true; break; }
            commands = grown;
        }
        command_t *cmd = &commands[count];
        cmd->line = line_no;
        cmd->key = GB_KEY_A;
        cmd->count = 0;
        cmd->number = 0;
        cmd->has_number = false;
        cmd->addr = 0;
        cmd->value = 0;
        cmd->has_value = false;

        const char *verb = tokens[0];

        if (strcasecmp(verb, "wait") == 0) {
            if (ntokens != 2 || !parse_uint(tokens[1], &cmd->count)) {
                fprintf(stderr, "%s:%u: 'wait' needs a frame count\n", label, line_no);
                error = true; break;
            }
            cmd->type = CMD_WAIT;
        }
        else if (strcasecmp(verb, "settle") == 0) {
            if (ntokens != 2 || !parse_uint(tokens[1], &cmd->count) || cmd->count == 0) {
                fprintf(stderr, "%s:%u: 'settle' needs a positive frame count\n", label, line_no);
                error = true; break;
            }
            cmd->type = CMD_SETTLE;
        }
        else if (strcasecmp(verb, "press") == 0) {
            int key = ntokens >= 2 ? parse_key(tokens[1]) : -1;
            if (ntokens < 2 || ntokens > 3 || key < 0) {
                fprintf(stderr, "%s:%u: 'press' needs a key and optional frame count\n", label, line_no);
                error = true; break;
            }
            cmd->key = (GB_key_t)key;
            cmd->count = 2;
            if (ntokens == 3 && !parse_uint(tokens[2], &cmd->count)) {
                fprintf(stderr, "%s:%u: invalid hold count '%s'\n", label, line_no, tokens[2]);
                error = true; break;
            }
            cmd->type = CMD_PRESS;
        }
        else if (strcasecmp(verb, "down") == 0 || strcasecmp(verb, "up") == 0) {
            int key = ntokens == 2 ? parse_key(tokens[1]) : -1;
            if (ntokens != 2 || key < 0) {
                fprintf(stderr, "%s:%u: '%s' needs a key\n", label, line_no, verb);
                error = true; break;
            }
            cmd->key = (GB_key_t)key;
            cmd->type = strcasecmp(verb, "down") == 0 ? CMD_DOWN : CMD_UP;
        }
        else if (strcasecmp(verb, "screenshot") == 0) {
            if (ntokens > 2) {
                fprintf(stderr, "%s:%u: 'screenshot' takes an optional id number\n", label, line_no);
                error = true; break;
            }
            cmd->type = CMD_SCREENSHOT;
            if (ntokens == 2) {
                if (!parse_uint(tokens[1], &cmd->number)) {
                    fprintf(stderr, "%s:%u: invalid screenshot id '%s'\n", label, line_no, tokens[1]);
                    error = true; break;
                }
                cmd->has_number = true;
            }
        }
        else if (strcasecmp(verb, "compare") == 0) {
            if (ntokens != 2 || !parse_uint(tokens[1], &cmd->number)) {
                fprintf(stderr, "%s:%u: 'compare' needs a reference id number\n", label, line_no);
                error = true; break;
            }
            cmd->has_number = true;
            cmd->type = CMD_COMPARE;
        }
        else if (strcasecmp(verb, "memory") == 0) {
            if (ntokens < 2 || ntokens > 3 || !parse_hex16(tokens[1], &cmd->addr)) {
                fprintf(stderr, "%s:%u: 'memory' needs a 16-bit hex address and optional value\n", label, line_no);
                error = true; break;
            }
            if (ntokens == 3) {
                if (!parse_byte(tokens[2], &cmd->value)) {
                    fprintf(stderr, "%s:%u: invalid memory value '%s'\n", label, line_no, tokens[2]);
                    error = true; break;
                }
                cmd->has_value = true;
            }
            cmd->type = CMD_MEMORY;
        }
        else {
            fprintf(stderr, "%s:%u: unknown command '%s'\n", label, line_no, verb);
            error = true; break;
        }

        count++;
    }

    if (error) {
        free(commands);
        return false;
    }

    out->commands = commands;
    out->count = count;
    return true;
}

bool script_parse_path(const char *path, script_t *out)
{
    if (!path || strcmp(path, "-") == 0) {
        return script_parse_stream(stdin, "<stdin>", out);
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open script '%s'\n", path);
        out->commands = NULL;
        out->count = 0;
        return false;
    }
    bool ok = script_parse_stream(f, path, out);
    fclose(f);
    return ok;
}

void script_free(script_t *script)
{
    if (!script || !script->commands) return;
    free(script->commands);
    script->commands = NULL;
    script->count = 0;
}
