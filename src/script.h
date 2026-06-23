#ifndef BOYLESS_SCRIPT_H
#define BOYLESS_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <Core/gb.h>
#include "symbols.h"

typedef enum {
    CMD_WAIT,        /* count = frames to advance */
    CMD_SETTLE,      /* count = frames of stability required */
    CMD_PRESS,       /* key held for `count` frames, then released */
    CMD_DOWN,        /* key pressed and left down */
    CMD_UP,          /* key released */
    CMD_SCREENSHOT,  /* number/has_number = explicit id, else auto */
    CMD_COMPARE,     /* number = reference id to assert against */
    CMD_MEMORY,      /* addr; value/has_value = assert, else print */
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    GB_key_t   key;        /* press/down/up */
    unsigned   count;      /* wait/settle: frames; press: hold frames */
    unsigned   number;     /* screenshot/compare: id */
    bool       has_number; /* screenshot: id given; compare: always true */
    uint16_t   addr;       /* memory: address */
    unsigned   value;      /* memory: expected byte (0-255) */
    bool       has_value;  /* memory: value given */
    unsigned   line;       /* 1-based source line, for diagnostics */
} command_t;

typedef struct {
    command_t *commands;
    size_t     count;
} script_t;

/* Parse a script from an open stream. `label` is used in error messages.
   `syms` (may be NULL) resolves {symbol} token references. On success fills
   `out` and returns true. On failure logs to stderr and returns false. */
bool script_parse_stream(FILE *stream, const char *label,
                         const symbols_t *syms, script_t *out);

/* Open `path` and parse it. If `path` is NULL or "-", reads from stdin. */
bool script_parse_path(const char *path, const symbols_t *syms, script_t *out);

void script_free(script_t *script);

#endif
