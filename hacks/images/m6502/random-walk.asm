 ;; Jeremy English Dec 11 2007
 ;; Random walk
   lda #16
   sta $0         ; The current x pos
   sta $1         ; The current y pos
   lda $fe         ; Get random color
   sta $5         ; Store the color
   lda $fe         ; Amount of time to use this color
   sta $6

loop:
   ldx $0
   ldy $1
   lda $5
   jsr paint
   jsr walk
   dec $6
   bne loop
   ;; get a new color
   lda $fe
   sta $5
   ;; get a new duration
   lda $fe
   sta $6
   jmp loop

walk:
   lda $fe
   and #3
   cmp #0
   beq right
   cmp #1
   beq left
   cmp #2
   beq up
   jmp down

right:
   inc $0
   jmp done
left:
   dec $0
   jmp done
up:
   dec $1
   jmp done
down:
   inc $1
   jmp done
done:
   lda $0
   and #31
   sta $0
   lda $1
   and #31
   sta $1
   rts

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

   ;; Y cord MSB
yh:
       dcb $02, $02, $02, $02, $02, $02, $02, $02
       dcb $03, $03, $03, $03, $03, $03, $03, $03
       dcb $04, $04, $04, $04, $04, $04, $04, $04
       dcb $05, $05, $05, $05, $05, $05, $05, $05
       ;; Y cord LSB
yl:
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0
       dcb $00, $20, $40, $60, $80, $a0, $c0, $e0 
