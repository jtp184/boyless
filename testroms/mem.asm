; mem.asm — DMG test ROM for boyless `memory` assertions.
;
; Writes a known byte ($42) to WRAM $C000 immediately after boot, then idles.
; boyless can then assert `memory C000 $42` passes and `memory C000 $99` fails.

SECTION "Entry", ROM0[$100]
    nop
    jp Start
    ; $104-$14F (Nintendo logo + header) are written by `rgbfix -v`.

SECTION "Main", ROM0[$150]
Start:
    di
    ld a, $42
    ld [$C000], a
.idle:
    jr .idle
