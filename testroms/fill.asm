; fill.asm — DMG functional test ROM for boyless.
;
; Fills the whole background with a solid dark tile, then idles. The rendered
; screen never changes, so under boyless with hang detection enabled this ROM
; must report a hang. Also proves the DMG path, frame advance, and capture.

SECTION "Entry", ROM0[$100]
    nop
    jp Start
    ; $104-$14F (Nintendo logo + header) are written by `rgbfix -v`.

SECTION "Main", ROM0[$150]
Start:
    di
.wait_vblank:
    ld a, [$FF44]          ; LY
    cp 144
    jr c, .wait_vblank
    xor a
    ld [$FF40], a          ; LCDC = 0 (LCD off)

    ; Tile 1 = solid color 3 (both bitplanes set). Tile base $8000.
    ld hl, $8010           ; $8000 + 1*16
    ld b, 16
.tile:
    ld a, $FF
    ld [hl+], a
    dec b
    jr nz, .tile

    ; Fill the 32x32 BG map at $9800 with tile index 1.
    ld hl, $9800
    ld bc, 32*32
.map:
    ld a, 1
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .map

    ld a, %11100100        ; BGP: 0->white .. 3->black
    ld [$FF47], a
    ld a, %10010001        ; LCDC: LCD on, BG on, tiles @ $8000, map @ $9800
    ld [$FF40], a

.idle:
    jr .idle
