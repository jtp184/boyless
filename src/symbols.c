#include "symbols.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char    *name;
    uint16_t addr;
} sym_entry_t;

struct symbols {
    sym_entry_t *entries;
    size_t       count;
    size_t       capacity;
};

/* Parse up to 4 hex digits into a 16-bit value. Every char must be hex. */
static bool parse_addr16(const char *s, uint16_t *out)
{
    if (!s || !*s) return false;
    size_t len = strlen(s);
    if (len > 4) return false;
    for (const char *p = s; *p; p++)
        if (!isxdigit((unsigned char)*p)) return false;
    char *end;
    unsigned long v = strtoul(s, &end, 16);
    if (*end != 0 || v > 0xFFFF) return false;
    *out = (uint16_t)v;
    return true;
}

static bool sym_append(symbols_t *s, const char *name, uint16_t addr)
{
    if (s->count == s->capacity) {
        size_t cap = s->capacity ? s->capacity * 2 : 32;
        sym_entry_t *grown = realloc(s->entries, cap * sizeof(*grown));
        if (!grown) return false;
        s->entries = grown;
        s->capacity = cap;
    }
    char *copy = strdup(name);
    if (!copy) return false;
    s->entries[s->count].name = copy;
    s->entries[s->count].addr = addr;
    s->count++;
    return true;
}

bool symbols_load(const char *path, symbols_t **out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open symbol file '%s'\n", path);
        return false;
    }

    symbols_t *s = calloc(1, sizeof(*s));
    if (!s) { fclose(f); return false; }

    char line[1024];
    unsigned line_no = 0;
    bool error = false;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        char *semi = strchr(line, ';');
        if (semi) *semi = 0;

        char *save = NULL;
        char *spec = strtok_r(line, " \t\r\n", &save);
        if (!spec) continue;                    /* blank or comment-only */
        char *label = strtok_r(NULL, " \t\r\n", &save);
        if (!label) {
            fprintf(stderr, "%s:%u: malformed symbol line\n", path, line_no);
            error = true; break;
        }

        /* spec is "BANK:ADDR"; keep only ADDR. */
        char *colon = strchr(spec, ':');
        const char *addr_str = colon ? colon + 1 : spec;
        uint16_t addr;
        if (!colon || !parse_addr16(addr_str, &addr)) {
            fprintf(stderr, "%s:%u: expected BANK:ADDR, got '%s'\n", path, line_no, spec);
            error = true; break;
        }
        if (!sym_append(s, label, addr)) { error = true; break; }
        /* Any tokens after the label are ignored. */
    }
    fclose(f);

    if (error) { symbols_free(s); return false; }
    *out = s;
    return true;
}

bool symbols_lookup(const symbols_t *syms, const char *name, uint16_t *addr)
{
    if (!syms || !name) return false;
    for (size_t i = 0; i < syms->count; i++) {
        if (strcmp(syms->entries[i].name, name) == 0) {  /* first wins */
            *addr = syms->entries[i].addr;
            return true;
        }
    }
    return false;
}

void symbols_free(symbols_t *syms)
{
    if (!syms) return;
    for (size_t i = 0; i < syms->count; i++) free(syms->entries[i].name);
    free(syms->entries);
    free(syms);
}
