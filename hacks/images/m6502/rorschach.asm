; "Rorschach test"
; Not at all what it was supposed to be,
; but it turns out pretty cool and can
; create some interesting patterns.

  lda #8
  tax
dr:
  sta $3cb,x
  sta $40b,x
  dex
  bpl dr
  sta $3f3
  sta $3eb

  lda #1
  sta $3ec

  ldx #255
mk:
  lda $fe
  sta $1200,x
  lda $fe
  sta $1300,x
  lda $fe
  sta $1400,x
  lda $fe
  sta $1500,x
  dex
  cpx #$ff
  bne mk

; smooth it

  ldy #0
re:
  lda #1
  sta $3ec,y

  ldx #255
sm:
  lda $1201,x
  adc $11ff,x
  adc $1220,x
  adc $11e0,x
  lsr
  lsr
  sta $1200,x

  lda $1301,x
  adc $12ff,x
  adc $1320,x
  adc $12e0,x
  lsr
  lsr
  sta $1300,x

  lda $1401,x
  adc $13ff,x
  adc $1420,x
  adc $13e0,x
  lsr
  lsr
  sta $1400,x

  lda $1501,x
  adc $14ff,x
  adc $1520,x
  adc $14e0,x
  lsr
  lsr
  sta $1500,x

  dex
  cpx #$ff
  bne sm
  iny
  cpy #7
  bne re

  lda #1
  sta $3f0

  ;copy it

  clc
  ldx #255
cp:
  lda $1200,x
  lsr
  lsr
  tay
  lda colors,y
  sta $200,x

  lda $1300,x
  lsr
  lsr
  tay
  lda colors,y
  sta $300,x

  lda $1400,x
  lsr
  lsr
  tay
  lda colors,y
  sta $400,x

  lda $1500,x
  lsr
  lsr
  tay
  lda colors,y
  sta $500,x

  dex
  cpx #$ff
  bne cp
  rts

colors:
  dcb 0,0,0,0,0,$9,$9,1,1,0,0,0,0,0

