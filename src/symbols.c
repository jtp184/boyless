#include "symbols.h"
#include <ctype.h>
#include <errno.h>
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

/* Parse an offset string (after the +/- sign). Decimal, or $hex with a '$'
   prefix. Every remaining char must be a digit of the chosen base. */
static bool parse_offset(const char *s, unsigned long *out)
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
    errno = 0;
    *out = strtoul(s, &end, base);
    if (errno == ERANGE) return false;
    return *end == 0;
}

bool symbols_expand_token(const symbols_t *syms, const char *token,
                          char *out, size_t out_len,
                          char *errbuf, size_t err_len, bool *was_ref)
{
    *was_ref = false;
    if (!token) {
        snprintf(errbuf, err_len, "null token");
        return false;
    }
    *was_ref = (token[0] == '{');

    if (!*was_ref) {
        if (strchr(token, '}')) {
            snprintf(errbuf, err_len, "malformed symbol reference '%s'", token);
            return false;
        }
        snprintf(out, out_len, "%s", token);   /* plain token, copied verbatim */
        return true;
    }

    if (!syms) {
        snprintf(errbuf, err_len, "symbol reference '%s' requires a --sym file", token);
        return false;
    }

    size_t len = strlen(token);
    if (len < 3 || token[len - 1] != '}') {
        snprintf(errbuf, err_len, "malformed symbol reference '%s'", token);
        return false;
    }

    char inner[256];
    size_t inner_len = len - 2;                 /* strip the braces */
    if (inner_len >= sizeof(inner)) {
        snprintf(errbuf, err_len, "symbol reference too long '%s'", token);
        return false;
    }
    memcpy(inner, token + 1, inner_len);
    inner[inner_len] = 0;

    /* Split name from an optional signed offset at the first +/-. RGBDS labels
       never contain '+' or '-', so the first one starts the offset. */
    long offset = 0;
    char *op = inner;
    while (*op && *op != '+' && *op != '-') op++;
    if (*op) {
        char sign = *op;
        *op = 0;
        unsigned long mag;
        if (!parse_offset(op + 1, &mag)) {
            snprintf(errbuf, err_len, "bad offset in '%s'", token);
            return false;
        }
        offset = sign == '-' ? -(long)mag : (long)mag;
    }

    uint16_t base_addr;
    if (!symbols_lookup(syms, inner, &base_addr)) {
        snprintf(errbuf, err_len, "unknown symbol '%s'", inner);
        return false;
    }

    long result = (long)base_addr + offset;
    if (result < 0 || result > 0xFFFF) {
        snprintf(errbuf, err_len, "symbol '%s%+ld' resolves out of range", inner, offset);
        return false;
    }

    snprintf(out, out_len, "$%04X", (unsigned)result);
    return true;
}
