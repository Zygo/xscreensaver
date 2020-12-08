;; dmsc
;; 	
;; PostPosted: Thu Dec 13, 2007 11:57 pm Post subject: Dragon curve
;; fractal 
;;
;; 
;; Hi!
;; 
;; This draws the dragon curve fractal (really a "twin dragon"):

lda #0
 sta $0

lop:
 lda $0
 sta $1
 lda #3
 sta $6
 lda #232
 sta $5

 ldx #0
lpN:
 lsr $1
 bcc nos
 lda $5
 clc
 adc tL,x
 sta $5
 lda $6
 adc tH,x
 sta $6
nos:
 inx
 lda $1
 bne lpN
 inx
 txa
 ldy #0
 sta ($5),y

 inc $0
 bne lop
 rts

tL:
 dcb 32, 31, 254, 190, 128, 132, 8, 8
tH:
 dcb 0, 0, 255, 255, 255, 255, 0, 1 
