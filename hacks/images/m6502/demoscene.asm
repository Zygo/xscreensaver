
start:
  ldx #0
c:lda bottombar,x
  cmp #$ff
  beq init
  sta $4e0,x
  sta $5e0,x
  inx
  jmp c
init:
  jsr initDraw
  lda #0
  sta $10 ; scrptr
  sta $11 ; txtptr
loop:
  jsr drawMain
  jsr putfont
  jsr scrollarea
  jmp loop

scrollarea:
  ldx #0
g:lda $521,x
  sta $520,x
  lda $541,x
  sta $540,x
  lda $561,x
  sta $560,x
  lda $581,x
  sta $580,x
  lda $5a1,x
  sta $5a0,x
  inx
  cpx #31
  bne g
  rts

putfont:
  lda $10 ; scrptr
  cmp #0
  bne noNext
  inc $11
  ldx $11
  lda scrolltext,x
  tax
  lda fontSize,x
  sta $10
noNext:
  dec $10
  ldx $11
  lda scrolltext,x
  cmp #$ff
  bne notResetText
  lda #0
  sta $10
  sta $11
  rts

notResetText:
  asl
  tax
  lda fontlookup,x
  sta $2
  inx
  lda fontlookup,x
  sta $3
  lda #<fonts
  clc
  adc $2
  sta $0
  lda #>fonts
  adc $3
  sta $1
  ldy $10
  lda ($00),y
  sta $53f
  tya
  clc
  adc #6
  tay
  lda ($00),y
  sta $55f
  tya
  clc
  adc #6
  tay
  lda ($00),y
  sta $57f
  tya
  clc
  adc #6
  tay
  lda ($00),y
  sta $59f
  tya
  clc
  adc #6
  tay
  lda ($00),y
  sta $5bf
  rts

initDraw:
  lda #<picture
  sta $20
  lda #>picture
  sta $21
  lda #$00
  sta $22
  lda #$02
  sta $23
  ldx #$0
  rts
drawMain:
  ldx #0
  lda ($20,x)
  cmp #$ff
  beq done
  sta ($22,x)
  inc $20
  lda $20
  cmp #$00
  bne n1
  inc $21
n1:
  inc $22
  lda $22 
  cmp #$00
  bne done
  lda $23
  cmp #$05
  beq done
  inc $23
done:
  rts

picture:
  dcb 0,0,0,0,0,0,0,0,0,$b,$b,$c,$f,$f,$f,$f
  dcb $f,$b,0,0,0,$b,$b,$c,$c,$f,$f,$b,0,0,0,0
  dcb 0,0,0,0,0,0,0,0,0,$b,$c,$c,$f,$c,$f,$f
  dcb $b,$b,$b,$b,$b,0,$b,$b,$c,$f,$f,$c,0,0,0,0
  dcb 0,0,0,0,0,0,0,$b,0,$c,$b,$f,$c,$f,$f,$c
  dcb $c,$b,0,$b,$c,$c,$c,$f,$f,1,$f,$c,$b,0,0,0
  dcb 0,0,0,0,0,0,0,0,$b,$b,$c,$c,$c,$f,$f,$f
  dcb $c,$c,$c,$c,$c,$c,$f,$c,$f,$f,$f,$f,$b,0,0,0
  dcb 0,0,0,0,0,0,0,$b,0,0,$b,$c,$c,$f,$f,$f
  dcb $f,$c,$f,$f,$f,$f,$f,$f,$f,1,$f,$f,$c,0,0,0
  dcb 0,0,0,0,0,0,0,0,0,$b,$b,$b,$c,$f,$f,1
  dcb $f,$f,$c,$f,$f,$f,1,$f,$f,$f,$f,$f,$f,0,0,0
  dcb 0,0,0,0,0,0,0,0,0,$b,$b,$b,$b,$c,$f,1
  dcb $f,$f,$f,$f,$f,$f,$f,$f,1,$f,$f,$f,$f,$b,0,0
  dcb 0,0,0,0,0,0,0,0,$b,0,$b,$c,$b,$c,$c,1
  dcb 1,$f,1,$f,1,$f,1,$f,$f,1,$f,$f,1,$b,0,0
  dcb 0,0,0,0,0,0,0,$b,$b,$b,$c,$c,$b,$c,$f,1
  dcb 1,1,$f,$f,1,$f,$f,1,$f,$f,$f,$f,1,$c,0,0
  dcb 0,0,0,0,0,0,0,$b,$b,$c,$c,$c,$b,$c,$c,$f
  dcb 1,1,1,$f,$f,1,$f,1,$f,1,$f,$f,1,$c,0,0
  dcb 0,0,0,0,0,$b,$b,$b,$c,$c,$c,$f,$c,$c,$f,$f
  dcb 1,1,1,1,$f,$f,$f,1,$f,1,$f,$f,$f,$f,0,0
  dcb 0,0,0,0,0,0,$b,$c,$c,$c,$f,$c,$f,$c,$f,$f
  dcb 1,1,1,1,1,$f,$f,1,$f,$f,$f,$f,1,$f,$b,0
  dcb 0,0,0,0,$b,$b,$b,$c,$c,$f,$c,$f,$f,$c,$f,$f
  dcb 1,1,1,1,1,$f,$f,$f,1,$f,$f,$f,1,$c,$b,$b
  dcb 0,0,0,0,$b,$b,$c,$f,$c,$f,$f,$f,$f,$f,$c,$f
  dcb 1,1,1,1,1,$f,$f,$f,1,$f,$f,$f,$f,$f,$b,$b
  dcb 0,0,0,0,$b,$c,$c,$c,$f,$f,$f,$f,$f,$f,$f,$f
  dcb $f,1,1,1,$f,$b,$f,$f,$f,1,$f,$f,$f,$f,$b,$b
  dcb 0,0,0,0,$b,$c,$c,$f,$c,$f,$f,$f,$f,$f,$f,$f
  dcb $f,$f,$f,$c,$b,$f,$f,1,$f,$f,$f,$f,$f,$f,$c,$b
  dcb 0,0,0,0,$b,$b,$c,$c,$f,$c,$f,$f,$f,$f,$f,$f
  dcb $c,$c,$b,$c,$c,$f,$f,1,$c,$c,$f,$f,$f,$f,$c,$b
  dcb 0,0,0,0,$b,$b,$c,$c,$c,$f,$f,$f,$f,$f,$f,$f
  dcb $f,$f,$f,$f,$f,1,$f,$c,$b,$f,$c,$f,$c,$f,$c,$b
  dcb 0,0,0,0,0,$b,$c,$c,$c,$c,$f,$f,$f,$f,$f,$f
  dcb $f,$f,$f,$f,$f,$c,$b,$c,$c,$c,$f,$f,$c,$f,$c,$c
  dcb 0,0,0,0,0,$b,$b,$c,$c,$c,$c,$c,$f,$f,$f,$f
  dcb $f,$f,$f,$c,$b,$b,$c,$c,$c,$f,$c,$f,$f,$f,$c,$b
  dcb 0,0,0,0,0,$b,$b,$b,$b,$c,$c,$f,$c,$f,$f,$f
  dcb $c,$c,$b,$b,$b,$c,$b,$b,$c,$c,$f,$c,$c,$f,$c,$c
  dcb 0,0,0,0,0,0,$b,$b,$c,$b,$c,$c,$c,$c,$c,$c
  dcb $b,$b,$b,$b,$c,$b,$b,$c,$c,$f,$f,$f,$c,$c,$c,$b
  dcb 0,0,0,0,0,0,0,0,$b,$b,$b,$c,$c,$c,$c,$c
  dcb $c,$c,$b,$b,$b,$b,$c,$c,$f,$f,$f,$c,$c,$c,$c,$c
  dcb $ff


fontSize:
  dcb 5,5,5,5,5,5,5,5 ;abcdefgh
  dcb 2,5,5,5,6,6,5,5 ;ijklmnop
  dcb 6,5,5,4,5,6,6,6 ;qrstuvwx
  dcb 6,5,2,3         ;yz.[SPACE]

;
; a=0, b=1, c=2, d=3....
;

scrolltext:
  dcb 0

  dcb 14,13,11,24,27           ; "only "
  dcb 03,04,15,19,07,27        ; "depth "
  dcb 12,0,10,4,18,27          ; "makes "
  dcb 8,19,27                  ; "it "
  dcb 15,14,18,18,8,1,11,4     ; "possible"
  dcb 26,26,26                 ; "..."
  dcb 19,7,8,18,27             ; "this "
  dcb 8,18,27                  ; "is "
  dcb 19,7,4,27                ; "the "
  dcb 5,8,17,18,19,27          ; "first "
  dcb 3,4,12,14,27             ; "demo "
  dcb 12,0,3,4,27              ; "made "
  dcb 8,13,27                  ; "in "
  dcb 19,7,8,18,27             ; "this "
  dcb 4,13,21,26,26,26,26,27   ; "env.... "
  dcb 7,14,15,4,27             ; "hope "
  dcb 24,14,20,27              ; "you "
  dcb 11,8,10,4,27             ; "like "
  dcb 8,19,26,26,26,27,27      ; "it...  "
  dcb 22,22,22,26              ; "www."
  dcb 3,4,15,19,7,26           ; "depth."
  dcb 14,17,6,27,27,27,27,27   ; "org     "

  dcb $ff                      ; end of text

fontlookup:
  dcb $00,$00 ;a
  dcb $20,$00 ;b
  dcb $40,$00 ;c
  dcb $60,$00 ;d
  dcb $80,$00 ;e
  dcb $a0,$00 ;f
  dcb $c0,$00 ;g
  dcb $e0,$00 ;h
  dcb $00,$01 ;i
  dcb $20,$01 ;j
  dcb $40,$01 ;k
  dcb $60,$01 ;l
  dcb $80,$01 ;m
  dcb $a0,$01 ;n
  dcb $c0,$01 ;o
  dcb $e0,$01 ;p
  dcb $00,$02 ;q
  dcb $20,$02 ;r
  dcb $40,$02 ;s
  dcb $60,$02 ;t
  dcb $80,$02 ;u
  dcb $a0,$02 ;v
  dcb $c0,$02 ;w
  dcb $e0,$02 ;x
  dcb $00,$03 ;y
  dcb $20,$03 ;z
  dcb $40,$03 ;.
  dcb $60,$03 ;" "

fonts:
  dcb 0,1,1,0,0,0
  dcb 1,0,0,1,0,0
  dcb 1,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,0

  dcb 0,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0

  dcb 0,1,1,0,0,0
  dcb 1,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,0,0,0
  dcb 0,0

  dcb 0,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0

  dcb 1,1,1,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0,0,1,0,0
  dcb 1,1,1,1,0,0
  dcb 0,0

  dcb 1,1,1,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0

  dcb 1,1,1,0,0,0
  dcb 0,0,0,1,0,0
  dcb 1,1,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,1,1,0,0,0
  dcb 0,0

  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,0

  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 0,0

  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,0,0,0
  dcb 0,0

  dcb 1,0,0,1,0,0
  dcb 0,1,0,1,0,0
  dcb 0,0,1,1,0,0
  dcb 0,1,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,0

  dcb 0,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 1,1,1,1,0,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 1,1,0,1,1,0
  dcb 1,0,1,0,1,0
  dcb 1,0,0,0,1,0
  dcb 1,0,0,0,1,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 1,0,0,1,1,0
  dcb 1,0,1,0,1,0
  dcb 1,1,0,0,1,0
  dcb 1,0,0,0,1,0
  dcb 0,0

  dcb 0,1,1,0,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,0,0,0
  dcb 0,0

  dcb 0,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0,0,1,0,0
  dcb 0,0

  dcb 0,1,1,0,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,0,1,0,0
  dcb 1,0,1,0,0,0
  dcb 0,0

  dcb 0,1,1,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,1,1,1,0,0
  dcb 0,1,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 0,0

  dcb 1,1,1,0,0,0
  dcb 0,0,0,1,0,0
  dcb 0,1,1,0,0,0
  dcb 1,0,0,0,0,0
  dcb 0,1,1,1,0,0
  dcb 0,0

  dcb 1,1,1,0,0,0
  dcb 0,1,0,0,0,0
  dcb 0,1,0,0,0,0
  dcb 0,1,0,0,0,0
  dcb 0,1,0,0,0,0
  dcb 0,0

  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,0,0,1,0,0
  dcb 1,1,1,0,0,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 1,0,0,0,1,0
  dcb 1,0,0,0,1,0
  dcb 0,1,0,1,0,0
  dcb 0,0,1,0,0,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 1,0,0,0,1,0
  dcb 1,0,1,0,1,0
  dcb 1,1,0,1,1,0
  dcb 1,0,0,0,1,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 0,1,0,1,0,0
  dcb 0,0,1,0,0,0
  dcb 0,1,0,1,0,0
  dcb 1,0,0,0,1,0
  dcb 0,0

  dcb 1,0,0,0,1,0
  dcb 0,1,0,1,0,0
  dcb 0,0,1,0,0,0
  dcb 0,0,1,0,0,0
  dcb 0,0,1,0,0,0
  dcb 0,0

  dcb 1,1,1,1,0,0 ; z
  dcb 1,0,0,0,0,0
  dcb 0,1,1,0,0,0
  dcb 0,0,0,1,0,0
  dcb 1,1,1,1,0,0
  dcb 0,0

  dcb 0,0,0,0,0,0 ; .
  dcb 0,0,0,0,0,0
  dcb 0,0,0,0,0,0
  dcb 0,0,0,0,0,0
  dcb 1,0,0,0,0,0
  dcb 0,0

  dcb 0,0,0,0,0,0 ; " "
  dcb 0,0,0,0,0,0
  dcb 0,0,0,0,0,0
  dcb 0,0,0,0,0,0
  dcb 0,0,0,0,0,0
  dcb 0,0

bottombar:
  dcb $b,$9,$b,9,8,9,8,$a,8,$a,7,$a,7,1,7,1,1
  dcb 7,1,7,$a,7,$a,8,$a,8,9,8,9,$b,9,$b
  dcb $ff

