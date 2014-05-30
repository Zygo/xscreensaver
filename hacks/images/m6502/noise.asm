; static noise

start: ldy #$ff
       ldx #$0
loop:  lda $fe
       sta $200,x
       and #$7
       sta $300,x
       and #$3
       sta $400,x
       and #$1
       sta $500,x
       inx
       dey
       bne loop
       rts
