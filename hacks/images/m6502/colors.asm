; submitted by Anonymous

 jmp $700
 *=$700
 ldx #0		
 ldy #0
 ;init screen
 lda #0
 sta $0
 sta $3
 lda #2
 sta $1
loop:
 lda colors,x
 bpl ok
 inc $0
 ldx #0
 lda colors,x
ok:
 inx
 sta ($0),y
 iny
 bne ok2
 inc $1
 lda $1
 cmp #6
 beq end
ok2:
 jmp loop
end:
 inc $3
 lda $3
 and #$3f
 tax
 ldy #0
 lda #2
 sta $1
 sty $0
 jmp loop

colors:
dcb 0,2,0,2,2,8,2,8,8,7,8,7,7,1,7,1,1,7,1,7,7,8,7,8
dcb 8,2,8,2,2,0,2,0,2,2,8,2,8,8,7,8,7,7,1,7,1,1,1,1
dcb 1,1,1,1,7,1,7,7,8,7,8,8,2,8,2,2,255


