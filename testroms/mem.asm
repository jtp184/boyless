; mem.asm — DMG test ROM for boyless `memory` assertions.
;
; Writes a known byte ($42) to WRAM `wValue` ($C000) immediately after boot,
; then idles. boyless can assert `memory C000 $42` (or `memory {wValue} $42`
; with --sym) passes and `memory C000 $99` fails.

SECTION "Vars", WRAM0[$C000]
wValue: ds 1

SECTION "Entry", ROM0[$100]
    nop
    jp Start
    ; $104-$14F (Nintendo logo + header) are written by `rgbfix -v`.

SECTION "Main", ROM0[$150]
Start:
    di
    ld a, $42
    ld [wValue], a
.idle:
    jr .idle
