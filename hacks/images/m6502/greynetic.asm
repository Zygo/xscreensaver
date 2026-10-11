;Port of Greynetic
;Jeremy English 2013

lda #$0
sta $0
lda #$2
sta $1

start:
lda $1
cmp #$6
bne randOffset
lda #$2
sta $1

randOffset:
;move position by some random offset
clc
lda $fe
adc $0
sta $0
lda $1
adc #$0
sta $1
cmp #$06 ;Did we go out of range
bne setRect ;Nope
lda #$02 ;Start back at the top
sta $1

setRect:
lda $fe
and #$f
tax
inx ;at least 1
stx $2 ;width
stx $3 ;working copy
lda $fe
and #$f
tax
inx ;at least 1
stx $4 ;height

lda $fe
sta $5 ;color

ldy #0
draw:
lda $5
sta ($0), y
dec $3
beq down

lda $0
clc
adc #$1
sta $0
lda $1
adc #$0
sta $1
cmp #$06 ;Did we go out of range
beq done ;yes
jmp draw

down:

;;Move back to the start of this row
ldx $2 ;The width of the rectangle
dex
lda $0
stx $0
sec
sbc $0
sta $0
lda $1
sbc #$0
sta $1

;;Move down one row
dec $4
beq done ;;Are we done drawing?

lda $2
sta $3 ;reset the width counter

lda $0
clc
adc #$20
sta $0
lda $1
adc #$0
sta $1
cmp #$06 ;Did we go out of range
beq done ;yes
jmp draw
done:
jmp start
