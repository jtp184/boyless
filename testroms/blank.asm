; blank.asm — DMG test ROM that boots but renders a uniform (blank) screen.
; Used to prove boyless `noblank` fails on a single-colour screen.

SECTION "Entry", ROM0[$100]
    nop
    jp Start
    ; $104-$14F (Nintendo logo + header) are written by `rgbfix -v`.

SECTION "Main", ROM0[$150]
Start:
    di
    xor a
    ld [$FF40], a         ; LCDC = 0: LCD off so VRAM is safe to write

    ld hl, $8000          ; zero tile 0 (16 bytes) => every pixel is colour 0
    ld bc, 16
.cleartile:
    xor a
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .cleartile

    ld hl, $9800          ; fill the BG tilemap (32*32 = 1024) with tile 0
    ld bc, 32*32
.clearmap:
    xor a
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .clearmap

    ld a, %11100100       ; BGP identity (the screen is uniform regardless)
    ld [$FF47], a
    ld a, $91             ; LCDC: LCD on, BG on, BG tiles $8000, map $9800
    ld [$FF40], a
.idle:
    jr .idle
