; awfully slow.. be patient

init:
  ldx #0
  stx $0
  inx
  stx $2
  stx $3
  inx
  stx $1

loop:
  lda $2
  cmp #$20
  bne notIncF2
  inc $3
  lda #0
  sta $2
notIncF2:
  inc $2

; CALCULATE START

  lda $2
  sta $f1
  lda $2
  sta $f2
  jsr multiply

  lda $f4
  sta $f8

  lda $3
  sta $f1
  lda $3
  sta $f2
  jsr multiply

; CALCULATE STOP

  lda $f4
  clc
  adc $f8

  lsr
  lsr
  lsr
  lsr
  lsr
  ldx #0
  sta ($0,x)
  inc $0
  lda $0
  cmp #$00
  bne notNextY
  inc $1
  lda $1
  cmp #6
  beq exit
notNextY:
  jmp loop
exit:
  rts

multiply:
  lda #0
  sta $f4
  sta $f5
  ldx #8
a:asl $f4
  rol $f5
  asl $f2
  bcc b
  clc
  lda $f4
  adc $f1
  sta $f4
  bcc b
  inc $f5
b:dex
  bne a
  rts 