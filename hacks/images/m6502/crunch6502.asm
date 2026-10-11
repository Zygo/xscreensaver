;; Show "6502" on the screen waving up and down.
;; Jeremy English 29-December-2007
;;
;; Each digit is stored as a pattern of vertical bits.
;; For example:
;;
;;   111111     This is the digit six. We think of the digit 
;;   111111     by it's column pattern. The column patterns 
;;   110000     are labeled at the bottom of the example. 
;;   110000     Pattern B is 1100110011. The basic algorithm 
;;   111111     is that we get the pattern, paint the first
;;   111111     bit (1 foreground, 0 background) then dec y 
;;   110011     and get the next bit.
;;   110011     
;;   111111     The pattern for each digit is:
;;   111111     6 = AABBCC
;;   ------     5 = DDBBCC
;;   AABBCC     0 = AAEEAA
;;              2 = CCBBDD

;; Addresses $0 and $1 are used by the paint subroutine.
;; Addresses $2 through $6 are used by the display pattern subroutine
;; Address $7 is used in the main loop
;; Address $8 through $1a  are used for the start positions
;; Address $1b is used by the display pattern subroutine
;; Address $1c is used as the color row offset.
;; Addresses $d0 through $ef store the font table

jmp init_font_table
start:

;; Initialize the pointers to the start position.
lda #<y_start_pos1
sta $b
lda #>y_start_pos1
sta $c
lda #<y_start_pos2
sta $d
lda #>y_start_pos2
sta $e
lda #<y_start_pos3
sta $f
lda #>y_start_pos3
sta $10
lda #<y_start_pos4
sta $11
lda #>y_start_pos4
sta $12
lda #<y_start_pos5
sta $13
lda #>y_start_pos5
sta $14
lda #<y_start_pos4
sta $15
lda #>y_start_pos4
sta $16
lda #<y_start_pos3
sta $17
lda #>y_start_pos3
sta $18
lda #<y_start_pos2
sta $19
lda #>y_start_pos2
sta $1a


lda #0        ; start position to use
sta $8

main_loop:
inc $1c       ; increment the color offset.
inc $1d       ; increment the starting x position
ldy $8        ; load the current start position index
ldx $b,y      ; get the lsb from the table
txa
sta $9        ; store the msb of the start position pointer
iny           ; move to the next position in the table
ldx $b,y      ; get the msb from the table
txa
sta $a        ; store the lsb of the start position pointer
iny           ; move the index up by one
tya
cmp #$10       ; have we looked at all 16 start positions?
bne store_idx ; if not then keep the index and store it
lda #0        ; set the index back to zero
store_idx:
sta $8        ; save the index back in memory

ldy #0
lda #$ff
sta $4        ; initialize the column to FF
display_loop:
  inc $4      ; increment the column
  ldx $d0,y   ; load the lsb from the font table
  stx $2
  iny
  ldx $d0,y   ; load the msb from the font table
  stx $3
  sty $7      ; save y in memory
  jsr dis_pat ; Jump to the display pattern subroutine.
  inc $4      ; increment the column	
  jsr dis_pat ; Each pattern gets painted twice so we have a thicker font
  ldy $7      ; get y out of memory
  iny         ; increment the index
  tya
  cmp #$20    ; Did we display all of the columns?
  bne display_loop ;if not continue
jmp main_loop
rts

init_font_table:
  ;;Setup a table in the zero page that contains the string "6502"
  lda #<pattern_a    ;start with digit 6. It's pattern is aabbcc
  sta $d0
  lda #>pattern_a
  sta $d1
  lda #<pattern_b
  sta $d2
  lda #>pattern_b
  sta $d3
  lda #<pattern_c
  sta $d4
  lda #>pattern_c
  sta $d5
  lda #<pattern_null  ;We want to space everything out with blanks
  sta $d6
  lda #>pattern_null
  sta $d7
  lda #<pattern_d   ;load memory for digit 5 ddbbcc
  sta $d8
  lda #>pattern_d
  sta $d9
  lda #<pattern_b
  sta $da
  lda #>pattern_b
  sta $db
  lda #<pattern_c
  sta $dc
  lda #>pattern_c
  sta $dd
  lda #<pattern_null
  sta $de
  lda #>pattern_null
  sta $df
  lda #<pattern_a   ;load memory for digit 0 aaeeaa
  sta $e0
  lda #>pattern_a
  sta $e1
  lda #<pattern_e
  sta $e2
  lda #>pattern_e
  sta $e3
  lda #<pattern_a
  sta $e4
  lda #>pattern_a
  sta $e5
  lda #<pattern_null
  sta $e6
  lda #>pattern_null
  sta $e7
  lda #<pattern_c   ;load memory for digit 2 ccbbdd
  sta $e8
  lda #>pattern_c
  sta $e9
  lda #<pattern_b
  sta $ea
  lda #>pattern_b
  sta $eb
  lda #<pattern_d
  sta $ec
  lda #>pattern_d
  sta $ed
  lda #<pattern_null
  sta $ee
  lda #>pattern_null
  sta $ef
  jmp start


;; Display a pattern on the screen. The pattern to use is 
;; stored at $2 and $3. The current column is stored at $4.
dis_pat:
  ldy $4             ; Load the current column into y
  lda ($9),y         ; Get the start position for y
  tay
  sty $5             ; Store the starting position in memory
  ldy #0             ; We have 12 bits that need to be painted
dis_pat_loop:
  lda ($2),y         ; get a bit from the pattern
  pha                ; save the color on the stack
  tya                ; move the index into the accumulator
  clc                ; clear the carry 
  adc $5             ; add the starting position to the index
  sty $6             ; store the index 
  tay                ; The calculated y position
  ldx $4             ; The x position is the current column
  pla                ; pop the color off of the stack
  beq go_paint       ; black just paint it
  clc                ; get rid of any carry bit
  sty $1b            ; save the y coordinate
  tya
  clc
  adc $1c            ; add the color offset
  and #$7            ; make sure the look up is in range
  tay                ; move the new index into y so we can look up the color
  lda color_row,y    ; if not black get the row color
  ldy $1b            ; restore the y coordinate
go_paint:
  jsr paint          ; paint the pixel on the screen
  ldy $6             ; get the index out of memory
  iny                ; increment the index
  tya
  cmp #12            ; Have we looked at all of the bits?
  bne dis_pat_loop   ; if not then continue looking
  rts                ; else return from the subroutine

;; Paint - Put a pixel on the screen by using the x registry for 
;;         the x position, the y registry for the y position and 
;;         the accumulator for the color.
paint:
   pha           ; Save the color
   lda yl,y      ; Get the LSB of the memory address for y
   sta $0        ; Store it first
   lda yh,y      ; Get the MSB of the memory address for y
   sta $1        ; Store it next
   txa           ; We want x in the y registry so we transfer it to A
   tay           ; and then A into y.
   pla           ; Pop the color off of the stack
   sta ($0),y    ; Store the color at the correct y + x address.
   rts           ; return from the subroutine.

;; Paint uses the following two tables to look up the 
;; correct address for a y coordinate between 
;; 0 and 31.

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

;; A zero is on the end of each pattern to clean up 
;; residue left by waving.
pattern_a:
  dcb 0,1,1,1,1,1,1,1,1,1,1,0

pattern_b:
  dcb 0,1,1,0,0,1,1,0,0,1,1,0

pattern_c:
  dcb 0,1,1,0,0,1,1,1,1,1,1,0

pattern_d:
  dcb 0,1,1,1,1,1,1,0,0,1,1,0

pattern_e:
  dcb 0,1,1,0,0,0,0,0,0,1,1,0

pattern_null:
  dcb 0,0,0,0,0,0,0,0,0,0,0,0

;; Table that store the current start position 
;; of each y column.
y_start_pos1:
  dcb 10,10,9,9,8,8,7,7,6,6,7,7,8,8,9,9,10,10,9,9,8,8,7,7
  dcb 6,6,7,7,8,8

y_start_pos2:
  dcb 9,9,8,8,8,8,8,8,7,7,8,8,8,8,8,8,9,9,8,8,8,8,8,8
  dcb 7,7,8,8,8,8

y_start_pos3:
  dcb 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  dcb 8,8,8,8,8,8

y_start_pos4:
  dcb 7,7,8,8,8,8,8,8,9,9,8,8,8,8,8,8,7,7,8,8,8,8,8,8
  dcb 9,9,8,8,8,8

y_start_pos5:
  dcb  6, 6,7,7,8,8,9,9,10,10,9,9,8,8,7,7, 6, 6,7,7,8,8,9,9
  dcb 10,10,9,9,8,8

color_row:
  dcb $7,$8,$9,$2,$4,$6,$e,$3,$d,$5
