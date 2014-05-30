; 2d starfield
; Submitted by Anonymous

i:ldx #$7
g:lda $fe
  and #3
  adc #1
  sta $0,x
  lda $fe
  and #$1f
  sta $20,x
  dex
  bpl g
f:
  lda #$ff
  sta $10
  delay:
  nop
  dec $10
  bne delay

  lda #$00
  sta $80
  lda #$02
  sta $81
  ldx #$7
l:lda $20,x
  pha
  clc
  sbc $00,x
  and #$1f
  sta $20,x
  lda $20,x
  tay
  lda #1
  sta ($80),y
  pla
  tay
  lda #0
  sta ($80),y
  lda $80
  clc
  adc #$80
  bne n
  inc $81
n:sta $80
  dex
  bpl l
  jmp f

