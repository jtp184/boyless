; input.asm — CGB functional test ROM for boyless.
;
; Each frame, reads the joypad and repaints the background with a color unique
; to the button currently held, so each of the eight buttons yields a distinct
; screen. Requires CGB (uses CGB background palettes). Build with `rgbfix -C`.
;
; The background is a single tile that uses CGB palette 0, color 1. Every frame
; we recompute that color from the held button and rewrite it. CGB palette RAM
; can only be written reliably outside of mode 3, so the palette write is done
; during VBlank.

SECTION "Entry", ROM0[$100]
    nop
    jp Start
    ; $104-$14F written by `rgbfix -v`; `rgbfix -C` marks the ROM CGB-only.

SECTION "Main", ROM0[$150]
Start:
    di
.wait_vblank:
    ld a, [$FF44]
    cp 144
    jr c, .wait_vblank
    xor a
    ld [$FF40], a          ; LCD off

    ; Tile 1 = solid color index 1 (low bitplane set, high clear).
    ld hl, $8010
    ld b, 8
.tile:
    ld a, $FF
    ld [hl+], a            ; low bitplane
    xor a
    ld [hl+], a            ; high bitplane
    dec b
    jr nz, .tile

    ; Fill BG map ($9800) with tile 1. VBK=0 after boot; bank-1 attributes
    ; default to palette 0, which is what we drive below.
    ld hl, $9800
    ld bc, 32*32
.map:
    ld a, 1
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .map

    ld a, %10010001        ; LCDC on, BG on, tiles @ $8000, map @ $9800
    ld [$FF40], a

Loop:
    ; --- directions: P14=0 (bit5=1, bit4=0) => $20 ---
    ld a, $20
    ld [$FF00], a
    ld a, [$FF00]
    ld a, [$FF00]          ; settle
    cpl                    ; 1 = pressed
    and $0F
    ld d, a                ; b0 Right, b1 Left, b2 Up, b3 Down

    ; --- actions: P15=0 (bit5=0, bit4=1) => $10 ---
    ld a, $10
    ld [$FF00], a
    ld a, [$FF00]
    ld a, [$FF00]          ; settle
    cpl
    and $0F
    swap a                 ; actions into high nibble
    or d                   ; b0..3 dirs, b4 A, b5 B, b6 Select, b7 Start
    ld c, a

    ld a, $30              ; deselect joypad
    ld [$FF00], a

    ; Pick color index by lowest set bit (a single held button maps to its own
    ; slot). No button -> default slot 8.
    ld b, 0
.scan:
    ld a, c
    or a
    jr z, .none            ; nothing pressed
    rr c
    jr c, .have_index      ; this bit set -> b is the index
    inc b
    jr .scan
.none:
    ld b, 8                ; no button held -> default color slot
.have_index:
    ; hl = ColorTable + b*2
    ld hl, ColorTable
    sla b                  ; *2 (entry is 2 bytes)
    ld a, l
    add b
    ld l, a
    jr nc, .nocarry
    inc h
.nocarry:
    ; Load the two color bytes into d (low) and e (high) before VBlank.
    ld a, [hl+]
    ld d, a                ; color low byte
    ld a, [hl]
    ld e, a                ; color high byte

    ; Wait until we are well inside VBlank (LY = 148, lines 144-153) so the
    ; palette write never races the mode-3 -> mode-1 transition.
.vbl_wait:
    ld a, [$FF44]
    cp 148
    jr nz, .vbl_wait

    ; Write palette 0, color 1 (auto-increment from index $82).
    ld a, $82
    ld [$FF68], a          ; BCPS
    ld a, d
    ld [$FF69], a          ; BCPD low
    ld a, e
    ld [$FF69], a          ; BCPD high

.vbl_pass:
    ld a, [$FF44]
    cp 148
    jr z, .vbl_pass
    jp Loop

SECTION "Colors", ROM0
ColorTable:                ; BGR555, low byte then high byte
    dw $001F               ; 0 Right  red
    dw $03E0               ; 1 Left   green
    dw $7C00               ; 2 Up     blue
    dw $03FF               ; 3 Down   yellow
    dw $7C1F               ; 4 A      magenta
    dw $7FE0               ; 5 B      cyan
    dw $0015               ; 6 Select dark red
    dw $02A0               ; 7 Start  dark green
    dw $4210               ; 8 none   gray
