;; Jeremy English 01-December-2008
;; Snowflakes

;; Main loop Count
lda #7
sta $f

;; pattern number
lda #0
sta $10

;;Cells
lda #$00
sta $4
lda #$10
sta $5

;;Tmp
lda #$00
sta $6
lda #$11
sta $7

;;Init Cells Buffer
;;------------------------------------------------------------
ldy #$ff
initCells:
lda #0
sta ($4),y
sta ($6),y
dey
bne initCells

;;Set start position
ldy #115
lda #1
sta ($4),y
   
;;Setup offset
lda #15
sta $d
lda #16
sta $e

;;Start of main loop
;;------------------------------------------------------------
mainloop:

;;init indent
;;We want to indent every other line
lda #0
sta $a

lda #0
sta $9

;; Display Cells
;;------------------------------------------------------------
;; 248 is the total number of cells
ldy #248
display:

lda #0
sta $8

lda $a
beq stop16
lda #15
sta $b
lda #1
sta $8
jmp toggle
stop16:
lda #16
sta $b
toggle:
lda $a
eor #1
sta $a

;; Set the stop position
ldx $b
inner_display:
dex
txa
pha

dey
tya
pha
lda ($4),y

beq display_continue
ldx $8
ldy $9
lda #1
jsr paint
ldx $8
ldy $9
inx
jsr paint
ldx $8
ldy $9
iny
jsr paint
ldx $8
ldy $9
inx
iny
jsr paint
display_continue:
inc $8
inc $8

;;Life Cycle
;;------------------------------------------------------------
pla
tay
pha ;;Store y on the stack

tax
dey
lda ($4),y
iny
iny
clc
adc ($4),y
sta $c

txa
sec
sbc $d
tay
lda $c
clc
adc ($4),y
sta $c

txa
sec
sbc $e
tay
lda $c
clc
adc ($4),y
sta $c

txa
clc
adc $d
tay
lda $c
clc
adc ($4),y
sta $c

txa
clc
adc $e
tay
lda $c
clc
adc ($4),y
sta $c

pla
tay ;;Pull Y off of the stack

lda $c
and #1
beq dontset
sta ($6),y
dontset:
   
pla
tax ;;Pull x off of the stack   
beq exit_inner_display
jmp inner_display
exit_inner_display:
inc $9
inc $9
tya
beq display_exit
jmp display
display_exit:


;;Copy Temporary Buffer
;;------------------------------------------------------------
ldy #248
copybuf:
dey
lda ($6),y
sta ($4),y
tya
bne copybuf

dec $f
lda $f
beq reset_main
jmp mainloop

;;Reset main counter
;;------------------------------------------------------------
reset_main:
lda #7
sta $f

lda #$ff ;;Delay Count
sta $11
delay:
ldy #$a0
inner_delay:
nop
dey
bne inner_delay
dec $11
lda $11
bne delay

;; init buffer
;; and clear screen
clrscr:
lda $fe
and $f
cmp #1
beq clrscr ;We don't want a white background
ldy #$00
ldx #$0
cs_loop: 
sta $200,x
sta $300,x
sta $400,x
sta $500,x
pha
lda #0
sta ($6),y
sta ($4),y
pla
inx
dey
bne cs_loop

;; Setup new pattern
;;------------------------------------------------------------
inc $10
lda $10
and #3
sta $10
cmp #0
beq pattern1
cmp #1
beq pattern2
cmp #2
beq pattern3
cmp #3
beq pattern4

pattern1:
ldy #114
lda #1
sta ($4),y
ldy #115
lda #1
sta ($4),y
ldy #116
lda #1
sta ($4),y
jmp mainloop

pattern2:
ldy #113
lda #1
sta ($4),y
ldy #118
lda #1
sta ($4),y
jmp mainloop

pattern3:
ldy #115
lda #1
sta ($4),y
jmp mainloop

pattern4:
ldy #102
lda #1
sta ($4),y
ldy #128
lda #1
sta ($4),y
jmp mainloop

;;Paint subroutine
;;------------------------------------------------------------
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
