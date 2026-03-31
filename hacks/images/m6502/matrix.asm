;;Matrix :) 
	
loop:   
   	lda $fe
	and #$1f
	tay
	tax
	lda matrix,y
	sta $1
	tay
	lda #0
	jsr paint
   inc $1
   lda $1
   and #$1f
   sta matrix,y
   tay
   lda #5 
   jsr paint
   	lda $fe
	and #$1f
	tay
	tax
	lda matrix,y
	sta $1
	tay
	lda #$d
	jsr paint
   	lda $fe
	and #$1f
	tay
	tax
	lda matrix,y
	sta $1
	tay
	lda #$5
	jsr paint
   jmp loop
	
paint:
   pha
   lda yl,y
   sta $2
   lda yh,y
   sta $3
   txa
   tay
   pla
   sta ($2),y
   rts

yh:
       dcb $02, $02, $02, $02, $02, $02, $02, $02
       dcb $03, $03, $03, $03, $03, $03, $03, $03
       dcb $04, $04, $04, $04, $04, $04, $04, $04
       dcb $05, $05, $05, $05, $05, $05, $05, $05
       
yl:
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
   
matrix:
	dcb 5,16,19,19,17,26,10,14,11,4,1,2,20,1,8,30
	dcb 17,26,19,19,31,21,11,19,3,24,4,24,13,8,8,26

