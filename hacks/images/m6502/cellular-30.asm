; Rule 30 cellular automata
; by D.S.

 lda #1
 sta $20f

l3:
 lda #2
 sta 3
 sta 5
 sta 7
 lda #1
 sta 9
 sta 6
 lda #255
 sta 8
 lda #0
 sta 2
 lda #32
 sta 4
 ldx #30
l1:
 ldy #31

l2:
 lda ($2),y
 ora ($6),y
 eor ($8),y
 sta ($4),y
 dey
 bpl l2

 lda $2
 adc #32
 sta $2
 lda $3
 adc #0
 sta $3
 lda $4
 adc #32
 sta $4
 lda $5
 adc #0
 sta $5
 lda $6
 adc #32
 sta $6
 lda $7
 adc #0
 sta $7
 lda $8
 adc #32
 sta $8
 lda $9
 adc #0
 sta $9
 dex
 bpl l1

 ldy #31
l4:
 lda ($2),y
 sta $200,y
 dey
 bpl l4

 jmp l3
