; 6502 assembler Sierpinsky Triangle ver.2
; by Magnus Wedmark 2007-05-02
; This program is especially written for
; the 6502asm dot com competition and 
; uses the 32*32 pixel display used in that
; virtual platform. The sierpinsky 
; fractal is one of the simplest to
; implement. Here is a walk-through:
; 1) Specify 3 points that form a triangle
; 2) Choose one of them as a starting point
; 3) Choose one of them as targetpoint randomly
; 4) Set the new current position half-way 
;    between the current point and the target 
;    point.
; 5) Goto 3
	
	LDX #0
	LDY #0
new_rnd:
	LDA $FE       ; random 0-255
	AND #3        ; only 0-3 left
	CMP #3
	BNE good_rnd
	JMP new_rnd
good_rnd:     
; random = 0-2
	PHA
; transform X and Y values according to: 
; X=X/2+(P*8) and Y=Y/2+(P*16)
	ASL
	ASL
	ASL
	STA $F3 ; P*8
	PLA
	AND #1
	ASL
	ASL
	ASL
	ASL
	STA $F4 ; (P AND 1)*16
	TXA
	LSR
	ADC $F3
	TAX
	TYA
	LSR
	ADC $F4
	TAY
	JSR set_point	; use and restore regs
	JMP new_rnd

set_point: ; uses both X,Y,A and restores them
	PHA ; backup all reg-value (X,Y,A)
	TXA
	PHA
	TYA
	PHA 
	PHA 
	PHA ; triple Y push, two for int. use
	STX $F2  ; transfer X to Y using $F2
	LDY $F2
	LDA #0
	STA $F0
	LDA #$2
	STA $F1 ; set base vector to $200
	LDA #0
	PLA  ; transfer the pushed Y-coord to A
	AND #$07 ; the value %0000'0111
	ASL
	ASL
	ASL
	ASL
	ASL
	CLC
	ADC $F0
	STA $F0
	BCC no_carry
	INC $F1
no_carry:
	CLC
	PLA ; transfer the pushed Y-coord to A
	AND #$18
	LSR
	LSR
	LSR
	ADC $F1
	STA $F1		

	CLC
	TYA
	ADC $F0
	ADC $F1

	LDA #1 ;1 = white for trouble-shooting
   	JSR set_toning_point ; use for shading
	STA ($F0),Y  ; set pixel
	PLA  ; restore all reg-value (X,Y,A)
	TAY
	PLA
	TAX
	PLA
	RTS

; sub routine to shade the current pixel ($F0),Y
; lighter on a scale: $0, $B, $C, $F, $1 
; Black, DarkGrey, Grey, LightGrey, White
set_toning_point:
        LDA ($F0),Y
        CMP #$00
        BNE not_black
        LDA #$0B
        RTS
not_black:
        CMP #$0B
        BNE not_dgrey
        LDA #$0C
        RTS
not_dgrey:
        CMP #$0C
        BNE not_grey
        LDA #$0F
        RTS
not_grey:
        CMP #$0F
        BNE not_lgrey
        LDA #$01
        RTS
not_lgrey:
; white stays white
        RTS

