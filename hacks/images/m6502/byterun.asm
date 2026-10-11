; testing byterun compression
 
start:
  lda #<logo
  sta $0
  lda #>logo
  sta $1
  lda #$00
  sta $2
  lda #$02
  sta $3

decrunchLoop:
  lda $3
  cmp #$6
  bne moreWork 
  rts
moreWork:
  ldy #0
  lda ($0),y
  cmp #$ff
  bne notCrunched
  iny
  lda ($0),y ; repeat #
  sta $4
  iny
  lda ($0),y ; color
  ldy $4
drawLoop:
  ldx #0
  sta ($2,x)
  jsr nextPixel
  dey
  bne drawLoop
  jsr getNextByte
  jsr getNextByte
  jmp decrunchLoop
notCrunched:
  ldx #0
  sta ($2,x)
  jsr nextPixel
  jsr getNextByte
  jmp decrunchLoop

getNextByte:
  inc $0
  lda $0
  cmp #$00
  bne notHi
  inc $1
notHi:
  rts

nextPixel:
  inc $2
  ldx $2
  cpx #$00
  bne notNextLine
  inc $3
notNextLine:
  rts


logo:
 dcb $ff,43,1,$f,$f,$f,$c,$f,$f,$f,$ff,24,1,$c,$f,$c,0
 dcb $c,$f,$c,$ff,24,1,0,$f,$c,0,$c,$f,$c,$ff,24,1
 dcb $c,$f,$c,0,$c,$f,$c,$ff,24,1,0,$f,$c,0,$c,$f,$c
 dcb $ff,24,1,$c,$f,0,0,$c,$f,$c,$ff,24,1,0,$f,$c,0
 dcb $c,$f,$c,$ff,24,1,0,$f,$c,0,$c,$f,0,$ff,24,1
 dcb 0,$f,$c,0,$c,$f,0,$ff,23,1,$f,0,$f,$c,0,$c,$f,0,$f
 dcb $ff,22,1,$c,0,1,$c,0,$c,$f,0,$c,$ff,21,1
 dcb $f,0,0,1,0,0,$c,1,0,0,$ff,21,1,$c,0,$c,1,$c,0
 dcb $c,1,$c,0,$c,$ff,19,1,$f,0,0,$f,1,$c,0
 dcb $c,1,$f,0,0,$f,$ff,17,1,$f,0,0,0,1,1,$c,0
 dcb $c,1,1,0,0,0,$ff,16,1,$f,0,0,0,$f,1,1,0,0
 dcb $c,1,1,$f,0,0,0,$f,$ff,13,1
 dcb $c,0,0,0,$c,1,1,1,$c,0,$c,1,1,1,$c,0,0,0,$c
 dcb $ff,10,1,$c,0,0,0,0,$c,1,1,1,1,0,0
 dcb $c,1,1,1,1,0,0,0,0,0,$c,$ff,8,1
 dcb 0,0,0,0,$c,1,1,1,1,1,0,0
 dcb $c,1,1,1,1,1,$c,0,0,0,0,1,1,1,1,1
 dcb 1,1,1,1,0,0,$c,1,1,1,1,1,1,1,$c,0
 dcb $c,1,1,1,1,1,1,$f,$c,0,0,$ff,18,1,$f
 dcb $ff,53,1,0,$f,1,0,0,0,0,0,$f,1,$c
 dcb $c,1,1,1,$c,0,0,0,1,1,0,$f,$f,1,1,1
 dcb 1,1,1,1,$c,0,0,1,1,1,0,$f,1,1,$f,0
 dcb 0,$f,1,1,0,$f,1,$c,$c,1,0,$f,1,1,1,1
 dcb 1,1,1,1,0,$f,0,$f,1,1,0,$f,1,1,$f,$c
 dcb $c,$c,1,1,0,1,1,$f,0,1,0,$f,1,1,1,1
 dcb 1,1,1,1,0,1,$c,$f,1,1,$c,$f,1,1,0,$f
 dcb $f,0,1,1,0,$f,$f,0,$f,1,0,$f,1,1,1,1
 dcb 1,1,1,$c,0,$c,0,0,1,1,0,$f,1,1,0,$c
 dcb $c,0,$f,1,0,$f,0,$f,1,1,0,$f,1,1,1,1
 dcb 1,1,1,0,$c,$f,$f,0,$f,1,$c,$f,1,$c,$c,$f
 dcb $f,$c,$c,1,0,1,$f,$c,1,1,0,$f,1,1,1,1
 dcb 1,1,$f,0,1,1,1,$c,$c,1,0,$f,1,0,$f,1
 dcb 1,$f,0,1,0,$f,1,0,$f,1,0,$f,$ff,16,1
 dcb $f,$ff,5,1,$f,1,1,1,$f,$ff,38,1


