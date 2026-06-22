#include "script.h"
#include <limits.h>
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
        cmd->filename = NULL;
        cmd->count = 0;
        cmd->key = GB_KEY_A;

        const char *verb = tokens[0];

        if (strcasecmp(verb, "wait") == 0) {
            if (ntokens != 2 || !parse_uint(tokens[1], &cmd->count)) {
                fprintf(stderr, "%s:%u: 'wait' needs a frame count\n", label, line_no);
                error = true; break;
            }
            cmd->type = CMD_WAIT;
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
                fprintf(stderr, "%s:%u: 'screenshot' takes an optional filename\n", label, line_no);
                error = true; break;
            }
            cmd->type = CMD_SCREENSHOT;
            if (ntokens == 2) {
                cmd->filename = strdup(tokens[1]);
                if (!cmd->filename) {
                    fprintf(stderr, "%s:%u: out of memory allocating filename\n", label, line_no);
                    error = true; break;
                }
            }
        }
        else {
            fprintf(stderr, "%s:%u: unknown command '%s'\n", label, line_no, verb);
            error = true; break;
        }

        count++;
    }

    if (error) {
        for (size_t i = 0; i < count; i++) free(commands[i].filename);
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
    for (size_t i = 0; i < script->count; i++) free(script->commands[i].filename);
    free(script->commands);
    script->commands = NULL;
    script->count = 0;
}
