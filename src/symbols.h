#ifndef BOYLESS_SYMBOLS_H
#define BOYLESS_SYMBOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Opaque table of RGBDS symbol name -> 16-bit address. */
typedef struct symbols symbols_t;

/* Parse an RGBDS .sym file ("BB:AAAA Label" lines; ';' comments; blanks).
   Only the 16-bit address and label are kept; bank is dropped. On success sets
   *out to a newly allocated table and returns true. On failure logs to stderr
   and returns false (*out untouched). Free with symbols_free. */
bool symbols_load(const char *path, symbols_t **out);

/* Look up `name` case-sensitively. On hit writes the address and returns true.
   The first matching entry wins. Returns false if syms is NULL or no match. */
bool symbols_lookup(const symbols_t *syms, const char *name, uint16_t *addr);

/* Expand one whitespace-delimited script token.
   - Token without a leading '{' and without any '}': not a reference; copies it
     to `out`, sets *was_ref = false, returns true.
   - A "{name}", "{name+N}", or "{name-N}" reference (N decimal or $hex):
     resolves to "$XXXX" in `out`, sets *was_ref = true, returns true.
   - Malformed braces / unknown symbol / out-of-range / syms == NULL: writes a
     reason to `errbuf` and returns false.
   `out_len` must be >= 8 ("$FFFF" + margin). */
bool symbols_expand_token(const symbols_t *syms, const char *token,
                          char *out, size_t out_len,
                          char *errbuf, size_t err_len, bool *was_ref);

void symbols_free(symbols_t *syms);

#endif
