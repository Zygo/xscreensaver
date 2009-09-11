;;; 6502 logo Jeremy English 12-January-2008

start:
ldx #0
stx $20
lda #5
sta $21
lda $fe
sta $22

loop:
dec $21
lda $21
beq randcolor
jmp pastrandcolor

randcolor:
lda #5
sta $21
ldx #33
inc $22
lda $22
and #7
tay
rl:
lda $1000,x
beq pastcolor1
lda color_row,y
sta $1000,x

pastcolor1:
lda $1040,x
beq pastcolor2
lda color_row,y
sta $1040,x

pastcolor2:
lda $1080,x
beq pastcolor3
lda color_row,y
sta $1080,x

pastcolor3:
lda $10c0,x
beq pastcolor4
lda color_row,y
sta $10c0,x

pastcolor4:
lda $1100,x
beq pastcolor5
lda color_row,y
sta $1100,x

pastcolor5:
lda $1140,x
beq pastcolor6
lda color_row,y
sta $1140,x

pastcolor6:
lda $1180,x
beq pastcolor7
lda color_row,y
sta $1180,x

pastcolor7:
lda $11C0,x
beq pastcolor8
lda color_row,y
sta $11C0,x

pastcolor8:
lda $1200,x
beq pastcolor9
lda color_row,y
sta $1200,x

pastcolor9:
inx
txa
and #$3f
bne rl

pastrandcolor:

inc $20
lda $20
and #$3f
tay
and #$1f
tax
lda sinus,x
tax

d:

lda #0
sta $2e0,x
sta $3e0,x
lda $1000,y
sta $300,x
lda $1080,y
sta $320,x
lda $1100,y
sta $340,x
lda $1180,y
sta $360,x
lda $1200,y
sta $380,x
lda $1280,y
sta $3a0,x
lda $1300,y
sta $3c0,x
lda $1380,y
sta $3c0,x
inx
iny
txa
and #$1f
bne d

jmp loop

; 32 ($20) long
sinus:
dcb 0,0,0,0,$20,$20,$20
dcb $40,$40,$60,$80,$a0,$a0,$c0,$c0,$c0
dcb $e0,$e0,$e0,$e0,$c0,$c0,$c0
dcb $a0,$a0,$80,$60,$40,$40,$20,$20,$20

color_row:
dcb $7,$8,$9,$2,$4,$6,$e,$3


*=$1000
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1
dcb 0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1
dcb 0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0
dcb 0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0
dcb 0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0,0,1,1,0,0,1,1
dcb 0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1
dcb 0,0,1,1,1,1,1,1,0,0,1,1,0,0,1,1,0,0,1,1,1,1,1,1
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1
dcb 0,0,1,1,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,1,1,0,0,1,1,0,0,0,0,0,0,1,1,0,0,1,1,0,0,1,1
dcb 0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1
dcb 0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1
dcb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
dcb 0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1
dcb 0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,1

