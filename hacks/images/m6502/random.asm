loop: lda $fe       ; A=rnd
      sta $00       ; ZP(0)=A
      lda $fe
      and #$3       ; A=A&3
      clc           ; Clear carry
      adc #$2       ; A+=2
      sta $01       ; ZP(1)=A
      lda $fe       ; A=rnd
      ldy #$0       ; Y=0
      sta ($00),y   ; ZP(0),ZP(1)=y
      jmp loop
