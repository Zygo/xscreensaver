; Sierpinski
; Submitted by Anonymous

start:
  lda #$e1
  sta $0
  lda #$01
  sta $1
  ldy #$20

write:
  ldx #$00
  eor ($0, x)
  sta ($0),y

  inc $0
  bne write
  inc $1
  ldx $1
  cpx #$06
  bne write

  rts

