; Brick Out by Blake Ramsdell <blaker@gmail.com> http://www.blakeramsdell.com

; A poor attempt at brick out with no player involved. Maybe someday I'll
; let you play it, or you can view this as an exercise for the reader to put
; in a paddle that is user-controlled.

; I guess this is Copyright (C) 2007 Blake Ramsdell, and you have a license to
; do whatever you want with it, just tell me what you did and give me a
; mention. If you want to sell it, and you make a billion dollars, then good
; for you. You might at least throw a party and invite me.

; The gist of it is pretty simple -- you have a ball, and the ball has an X
; and a Y velocity. When it hits something, it bounces off of it. If the thing
; that it hits is not a wall, then it erases it. Pretty dead-simple behavior.

; I don't like the vertical movement -- there's a shortcut in here somewhere
; to make it less computationally expensive I think. Right now it just does a
; two byte add and subtract of $20.

; The ball motion is also a bit weird looking. I don't know if this is an
; artifact of the simulation environment combined with a normal tearing
; artifact related to refresh or what.

; Blake Ramsdell, May 2007

init:
 lda #$fe
 sta $2         ; X velocity (0 = fast, ff = slow)
                ; (low bit is direction, 0 = down or right, 1 = up or left)
 lda #$ee
 sta $3         ; Y velocity

drawbox:
 lda #0         ; Use $0-$1 as a screen address for drawing the field
 sta $0
 lda #2
 sta $1

 ldx #$20       ; Loop $20 times
boxloop:
 lda #2         ; Line color (red)
 sta $1ff,x     ; Top line
 sta $5df,x     ; Bottom line
 ldy #0
 sta ($0),y     ; Left line
 ldy #$1f
 sta ($0),y     ; Right line

 cpx #$1        ; If we're just before the bottom line...
 beq noblocks   ; Don't draw any blocks there


 lda #3         ; First block for this row, Cyan in color
 ldy #$17       ; It's at X position $17
 sta ($0),y     ; Draw it

 lda #4         ; Second block for this row, Purple in color
 iny            ; It's at the next X position
 sta ($0),y     ; Draw it

 lda #5         ; Third block for this row, Green in color
 iny            ; It's at the next X position
 sta ($0),y     ; Draw it

 lda #6         ; Fourth block for this row, Blue in color
 iny            ; It's at the next X position
 sta ($0),y     ; Draw it


noblocks:
 clc            ; Get ready to increment the row, clear the carry for add
 lda $0         ; Get the low byte
 adc #$20       ; Add $20 to it for the next row
 sta $0         ; Put it back
 lda $1         ; Get the high byte
 adc #0         ; Factor in the carry
 sta $1         ; Put it back

 dex            ; Decrement the loop counter
 bne boxloop    ; Do it again unless it's zero

 ldx $2         ; Load the X velocity
 ldy $3         ; Load the Y velocity

 lda #$44       ; Pick a start point
 sta $0         ; Ball position low
 lda #$02
 sta $1         ; Ball position high

drawball:
 txa            ; Preserve X
 pha
 lda #1         ; Ball color (white)
 ldx #0         ; Clear X for indirect addressing for writing to screen
 sta ($0,x)     ; Draw the ball
 pla            ; Restore X
 tax

decloop:
 dex            ; Decrement the X velocity
 beq updatexpos ; If it's zero, time to adjust X
 dey            ; Decrement the Y velocity
 bne decloop    ; If it's not zero, loop, otherwise fall through to adjust Y

updateypos:
 txa            ; Preserve X
 pha
 jsr clearball  ; Put background over the current ball position
updateyposnoclear:
 lda $3         ; Get the Y velocity
 and #1         ; See if it's down
 bne moveup     ; If not, then it's up, otherwise fall through to down

movedown:
 clc            ; Prepare for moving to the next Y line and doing the add
 lda $0         ; Low byte of the current ball position
 adc #$20       ; Next row
 sta $0         ; Put it back
 bcc ycollision ; If no carry, go on to check for collision
 inc $1         ; Had a carry, fix the high byte of the address
 bne ycollision ; Z flag is always clear ($1 will never be zero)

moveup:
 sec            ; Prepare for moving to the previous Y line and subtracting
 lda $0         ; Low byte of the current ball position
 sbc #$20       ; Previous row
 sta $0         ; Put it back
 lda $1         ; High byte
 sbc #$0        ; Factor out the carry
 sta $1         ; Put it back

ycollision:
 ldx #0         ; Prepare for indirect read
 lda ($0,x)     ; Get the current pixel at the new ball position
 bne ycollided  ; If it's not zero (the background color) then we hit
 ldy $3         ; Otherwise, load up the current Y velocity
 pla            ; Restore the X velocity
 tax
 jmp drawball   ; Back to the top

ycollided:
 cmp #$2        ; Border color?
 beq ycollided2 ; If so, then we just bounce, don't eat a brick

                ; Erase brick
 lda #0         ; Background color (black)
 sta ($0,x)     ; Erase it

ycollided2:
 lda #1         ; Get ready to change direction
 eor $3         ; Flip the low bit on the Y velocity (change direction)
 sta $3         ; Put it back
 jmp updateyposnoclear  ; Go back to make sure we didn't hit anything else

updatexpos:
 jsr clearball  ; Put background over the current ball position
updatexposnoclear:
 lda $2         ; Get the current X velocity
 and #1         ; See if it's right by testing the low bit
 bne moveleft   ; If not, move left

moveright:
 inc $0         ; Move right
 bne xcollision ; Z flag is always clear

moveleft:
 dec $0         ; Move left

xcollision:
 ldx #0         ; Prepare for indirect read
 lda ($0,x)     ; Get the current pixel at the new ball position
 bne xcollided  ; If it's not zero (the background color) then we hit
 ldx $2         ; Otherwise, load up the current X velocity
 jmp drawball   ; Back to the top

xcollided:
 cmp #$2        ; Border color?
 beq xcollided2 ; If so, then we just bounce, don't eat a brick

                ; Erase brick
 lda #0         ; Background color (black)
 sta ($0,x)     ; Erase it

xcollided2:
 lda #1         ; Get ready to change direction
 eor $2         ; Flip the low bit on the X velocity (change direction)
 sta $2         ; Put it back
 jmp updatexposnoclear  ; Go back to make sure we didn't hit anything else

clearball:
 lda #0         ; Background color (black)
 tax            ; Clear X for indirect
 sta ($0,x)     ; Black out the ball
 rts            ; Return to caller

