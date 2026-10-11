; Code 600 cellular automata - by D.S.
 lda #1
 sta $22f

l3:
 ldy #29

l2:
 lda  $220,y
 adc $221,y
 adc $222,y
 tax
 lda rule,x
 sta  $201,y
 dey
 bpl l2

 ldy #$c0
 sec
ll2:
 lda $503,y
 sta $523,y
 sta $53b,y
 lda $504,y
 sta $524,y
 sta $53a,y
 lda $505,y
 sta $525,y
 sta $539,y
 lda $506,y
 sta $526,y
 sta $538,y
 lda $507,y
 sta $527,y
 sta $537,y
 lda $508,y
 sta $528,y
 sta $536,y
 lda $509,y
 sta $529,y
 sta $535,y
 lda $50a,y
 sta $52a,y
 sta $534,y
 lda $50b,y
 sta $52b,y
 sta $533,y
 lda $50c,y
 sta $52c,y
 sta $532,y
 lda $50d,y
 sta $52d,y
 sta $531,y
 lda $50e,y
 sta $52e,y
 sta $530,y
 lda $50f,y
 sta $52f,y
 tya
 adc #$df
 tay
 bcs ll2

 ldy #$e0
 sec
ll3:
 lda $403,y
 sta $423,y
 sta $43b,y
 lda $404,y
 sta $424,y
 sta $43a,y
 lda $405,y
 sta $425,y
 sta $439,y
 lda $406,y
 sta $426,y
 sta $438,y
 lda $407,y
 sta $427,y
 sta $437,y
 lda $408,y
 sta $428,y
 sta $436,y
 lda $409,y
 sta $429,y
 sta $435,y
 lda $40a,y
 sta $42a,y
 sta $434,y
 lda $40b,y
 sta $42b,y
 sta $433,y
 lda $40c,y
 sta $42c,y
 sta $432,y
 lda $40d,y
 sta $42d,y
 sta $431,y
 lda $40e,y
 sta $42e,y
 sta $430,y
 lda $40f,y
 sta $42f,y
 tya
 adc #$df
 tay
 bcs ll3

 ldy #$e0
 sec
ll4:
 lda $303,y
 sta $323,y
 sta $33b,y
 lda $304,y
 sta $324,y
 sta $33a,y
 lda $305,y
 sta $325,y
 sta $339,y
 lda $306,y
 sta $326,y
 sta $338,y
 lda $307,y
 sta $327,y
 sta $337,y
 lda $308,y
 sta $328,y
 sta $336,y
 lda $309,y
 sta $329,y
 sta $335,y
 lda $30a,y
 sta $32a,y
 sta $334,y
 lda $30b,y
 sta $32b,y
 sta $333,y
 lda $30c,y
 sta $32c,y
 sta $332,y
 lda $30d,y
 sta $32d,y
 sta $331,y
 lda $30e,y
 sta $32e,y
 sta $330,y
 lda $30f,y
 sta $32f,y
 tya
 adc #$df
 tay
 bcs ll4


 ldy #$e0
 sec
ll1:
 lda $203,y
 sta $223,y
 sta $23b,y
 lda $204,y
 sta $224,y
 sta $23a,y
 lda $205,y
 sta $225,y
 sta $239,y
 lda $206,y
 sta $226,y
 sta $238,y
 lda $207,y
 sta $227,y
 sta $237,y
 lda $208,y
 sta $228,y
 sta $236,y
 lda $209,y
 sta $229,y
 sta $235,y
 lda $20a,y
 sta $22a,y
 sta $234,y
 lda $20b,y
 sta $22b,y
 sta $233,y
 lda $20c,y
 sta $22c,y
 sta $232,y
 lda $20d,y
 sta $22d,y
 sta $231,y
 lda $20e,y
 sta $22e,y
 sta $230,y
 lda $20f,y
 sta $22f,y
 tya
 adc #$df
 tay
 bcs ll1

 jmp l3

; Rules, uncomment only one line of the following.
rule:
 dcb 0,2,0,1,1,2,0 ; CODE 600
; dcb 0,2,1,0,2,0,0 ; CODE 177
; dcb 0,1,2,0,2,0,1; CODE 912 