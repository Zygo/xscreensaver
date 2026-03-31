; -*- mode: c; tab-width: 4; fill-column: 128 -*-
; vi: set ts=4 tw=128:

; Lines, Copyright (c) 2018 Dave Odell <dmo2118@gmail.com>
;
; Permission to use, copy, modify, distribute, and sell this software and its
; documentation for any purpose is hereby granted without fee, provided that
; the above copyright notice appear in all copies and that both that
; copyright notice and this permission notice appear in supporting
; documentation.  No representations are made about the suitability of this
; software for any purpose.  It is provided "as is" without express or 
; implied warranty.

; Another port of 20 year old QBasic code.



main_loop:
	lda #$00
	sta $0

y_loop:
	ldx $0

	lda #1
	bit $1
	beq left_right

	; Up-down. Skip all blank columns.
	lda x_px0,x
	and #1
	bne fill
	jmp next_y ; next_y is too far to conditional-branch from here.

clear:
	ldx #1
	lda $0
	clc
	adc #$20
	tay

	sec
clear_loop:
	lda #0 ; $fe
	sta $0200,y
	sta $0240,y
	sta $0280,y
	sta $02c0,y
	sta $0300,y
	sta $0340,y
	sta $0380,y
	sta $03c0,y
	sta $0400,y
	sta $0440,y
	sta $0480,y
	sta $04c0,y
	sta $0500,y
	sta $0540,y
	sta $0580,y
	sta $05c0,y
	tya
	sbc #$20
	tay
	dex
	bpl clear_loop
	jmp next_y

left_right:
	; Repaint columns that were previously on.
	lda x_px0,x
	bit const_two
	beq next_y
	lda x_px0,x
	and #1
	beq clear
	;jmp fill

fill:
	ldx #1
	lda $0
	clc
	adc #$20
	tay

	sec
fill_loop:
	; 3 * 2 * 16 = 96 bytes
	lda y_px0,x
	sta $0200,y
	lda y_px1,x
	sta $0240,y
	lda y_px2,x
	sta $0280,y
	lda y_px3,x
	sta $02c0,y
	lda y_px4,x
	sta $0300,y
	lda y_px5,x
	sta $0340,y
	lda y_px6,x
	sta $0380,y
	lda y_px7,x
	sta $03c0,y
	lda y_px8,x
	sta $0400,y
	lda y_px9,x
	sta $0440,y
	lda y_pxa,x
	sta $0480,y
	lda y_pxb,x
	sta $04c0,y
	lda y_pxc,x
	sta $0500,y
	lda y_pxd,x
	sta $0540,y
	lda y_pxe,x
	sta $0580,y
	lda y_pxf,x
	sta $05c0,y
	tya
	sbc #$20
	tay
	dex
	bpl fill_loop
	;jmp next_y

next_y:
	inc $0
	lda #32
	cmp $0
	beq shift
	jmp y_loop

shift:
	lda $fe
	and #$3
	sta $1 ; Left, down, right, up.
	beq shift_x1

	cmp #2
	bmi shift_y1
	beq shift_x0

shift_y0:
	ldx #0
shift_y0_loop:
	lda y_px0,x
	eor y_px00,x
	sta y_px0,x
	inx
	cpx #31
	bne shift_y0_loop
	jmp main_loop

shift_y1:
	ldx #30
shift_y1_loop:
	lda y_px00,x
	eor y_px0,x
	sta y_px00,x
	dex
	bpl shift_y1_loop
	jmp main_loop

shift_x0:
	ldx #0
shift_x0_loop:
	; px[0] = ((px[0] ^ px[1]) & 1) | (px[1] << 1)
	lda x_px0,x
	eor x_px00,x
	lsr ; Save EOR bit in carry flag.
	lda x_px00,x
	rol ; Restore EOR bit.
	sta x_px0,x
	inx
	cpx #31
	bne shift_x0_loop
	jmp main_loop

shift_x1:
	ldx #30
shift_x1_loop:
	lda x_px00,x
	eor x_px0,x
	lsr
	lda x_px0,x
	rol
	sta x_px00,x
	dex
	bpl shift_x1_loop
	jmp main_loop

y_px0:
	dcb 0
y_px00:
	dcb 0
y_px1:
	dcb 0, 0
y_px2:
	dcb 0, 0
y_px3:
	dcb 0, 0
y_px4:
	dcb 0, 0
y_px5:
	dcb 0, 0
y_px6:
	dcb 0, 0
y_px7:
	dcb 0, 0
y_px8:
	dcb 1, 0
y_px9:
	dcb 0, 0
y_pxa:
	dcb 0, 0
y_pxb:
	dcb 0, 0
y_pxc:
	dcb 0, 0
y_pxd:
	dcb 0, 0
y_pxe:
	dcb 0, 0
y_pxf:
	dcb 0, 0

	; Bit 0: black row, bit 1: changed row
x_px0:
	dcb 0
x_px00:
	dcb    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	dcb 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

const_two: ; lolz
	dcb 2

;#include "screenhack.h"
;
;struct _lines
;{
;	unsigned width, height;
;	unsigned long delay;
;	GC gc;
;};
;
;static void *lines_init(Display *display, Window window)
;{
;	struct _lines *self = malloc(sizeof(*self));
;	XGCValues gcv;
;	XWindowAttributes xgwa;
;
;	if(!self)
;		abort();
;
;	XGetWindowAttributes(display, window, &xgwa);
;	self->width = xgwa.width;
;	self->height = xgwa.height;
;
;	self->delay = get_integer_resource(display, "delay", "Integer");
;
;	gcv.function = GXxor;
;	gcv.foreground = get_pixel_resource(display, xgwa.colormap, "foreground", "Foreground");
;	self->gc = XCreateGC(display, window, GCFunction | GCForeground, &gcv);
;
;	XDrawPoint(display, window, self->gc, xgwa.width >> 1, xgwa.height >> 1);
;
;	return self;
;}
;
;static unsigned long lines_draw(Display *display, Window window, void *self_raw)
;{
;	struct _lines *self = self_raw;
;	static const XPoint xy[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
;	const XPoint *p = xy + NRAND(4);
;
;	XCopyArea(display, window, window, self->gc, 0, 0, self->width, self->height, p->x, p->y);
;	return self->delay;
;}
;
;static void lines_reshape(Display *display, Window window, void *self_raw, unsigned width, unsigned height)
;{
;	struct _lines *self = self_raw;
;	self->width = width;
;	self->height = height;
;}
;
;static Bool lines_event(Display *display, Window window, void *self_raw, XEvent *event)
;{
;	return False;
;}
;
;static void lines_free(Display *display, Window window, void *self_raw)
;{
;	struct _lines *self = self_raw;
;	XFreeGC(display, self->gc);
;	free(self);
;}
;
;static const char *lines_defaults[] =
;{
;	"*fpsSolid:         true",
;	"*delay:            30000",
;	0
;};
;
;static XrmOptionDescRec lines_options [] =
;{
;	{"-delay", ".delay",   XrmoptionSepArg, 0},
;	{0, 0, 0, 0}
;};
;
;XSCREENSAVER_MODULE ("Lines", lines)
