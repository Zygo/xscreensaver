; -*- mode: c; tab-width: 4; fill-column: 128 -*-
; vi: set ts=4 tw=128:

; Texture, Copyright (c) 2017 Dave Odell <dmo2118@gmail.com>
;
; Permission to use, copy, modify, distribute, and sell this software and its
; documentation for any purpose is hereby granted without fee, provided that
; the above copyright notice appear in all copies and that both that
; copyright notice and this permission notice appear in supporting
; documentation.  No representations are made about the suitability of this
; software for any purpose.  It is provided "as is" without express or 
; implied warranty.

; A port of 20 year old QBasic code to a much more modern platform.





	lda $fe
	sta src_row

	ldy #0
top_loop:
	lda $fe
	and #$f
	sbc #$8
	adc src_row,y
	iny
	sta src_row,y
	cpy #31
	bne top_loop

lda #$00
sta $0
lda #$02
sta $1

init_loop:
	jsr tex
	clc
	lda $0
	adc #$20
	sta $0
	lda $1
	adc #0
	sta $1
	cmp #$06
	bne init_loop

lda #$e0
sta $0
lda #$05
sta $1

loop:
	;jmp skip_blit

	clc
	lda #0
	blit_loop2:
		tay
		lda $0220,y
		sta $0200,y
		lda $0221,y
		sta $0201,y
		lda $0222,y
		sta $0202,y
		lda $0223,y
		sta $0203,y
		lda $0224,y
		sta $0204,y
		lda $0225,y
		sta $0205,y
		lda $0226,y
		sta $0206,y
		lda $0227,y
		sta $0207,y

		tya
		adc #8
	bne blit_loop2

	clc
	lda #0
	blit_loop3:
		tay
		lda $0320,y
		sta $0300,y
		lda $0321,y
		sta $0301,y
		lda $0322,y
		sta $0302,y
		lda $0323,y
		sta $0303,y
		lda $0324,y
		sta $0304,y
		lda $0325,y
		sta $0305,y
		lda $0326,y
		sta $0306,y
		lda $0327,y
		sta $0307,y

		tya
		adc #8
	bne blit_loop3

	clc
	lda #0
	blit_loop4:
		tay
		lda $0420,y
		sta $0400,y
		lda $0421,y
		sta $0401,y
		lda $0422,y
		sta $0402,y
		lda $0423,y
		sta $0403,y
		lda $0424,y
		sta $0404,y
		lda $0425,y
		sta $0405,y
		lda $0426,y
		sta $0406,y
		lda $0427,y
		sta $0407,y

		tya
		adc #8
	bne blit_loop4

	lda #0
	blit_loop5:
		tay
		lda $0520,y
		sta $0500,y
		lda $0521,y
		sta $0501,y
		lda $0522,y
		sta $0502,y
		lda $0523,y
		sta $0503,y
		lda $0524,y
		sta $0504,y
		lda $0525,y
		sta $0505,y
		lda $0526,y
		sta $0506,y
		lda $0527,y
		sta $0507,y

		tya
		clc
		adc #8
		cmp #$e0
	bne blit_loop5

	skip_blit:

	jsr tex
jmp loop

tex:
	lda $fe
	and #$f
	sbc #$8
	adc src_row
	sta src_row

	ldy #0
	tax
	lda pal0,x
	sta ($0),y
	tex_loop0:
		lda $fe
		and #$f
		sbc #$8
		;clc
		adc src_row,y
		iny
		;clc
		adc src_row,y
		ror
		sta src_row,y

		tax
		lda pal0,x
		sta ($0),y

		cpy #31
	bne tex_loop0
	rts

pal0:
	dcb $00, $00, $00, $00, $00, $00, $00, $00
	dcb $00, $00, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0c, $0c, $0c, $0c, $0c, $0c, $0c, $0c
	dcb $0c, $0c, $0c, $0c, $0c, $0c, $0c, $0c
	dcb $0f, $0f, $0f, $0f, $0f, $0f, $0f, $0f
	dcb $0f, $0f, $0f, $0f, $0f, $0f, $0f, $0f
	dcb $0f, $01, $01, $01, $01, $01, $01, $01
	dcb $01, $01, $01, $01, $01, $01, $01, $01
	dcb $0f, $0f, $0f, $0f, $0f, $0f, $0f, $0f
	dcb $0f, $0f, $0f, $0f, $0f, $0f, $0f, $0f
	dcb $0f, $0c, $0c, $0c, $0c, $0c, $0c, $0c
	dcb $0c, $0c, $0c, $0c, $0c, $0c, $0c, $0c
	dcb $0c, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $00
	dcb $00, $00, $00, $00, $00, $00, $00, $00
	dcb $00, $00, $00, $00, $00, $00, $00, $00
	dcb $00, $00, $00, $00, $00, $00, $00, $00
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $05, $05
	dcb $05, $05, $05, $05, $05, $05, $05, $05
	dcb $05, $05, $05, $05, $05, $05, $05, $05
	dcb $0d, $0d, $0d, $0d, $0d, $0d, $0d, $0d
	dcb $0d, $0d, $0d, $0d, $0d, $0d, $0d, $0d
	dcb $0d, $05, $05, $05, $05, $05, $05, $05
	dcb $05, $05, $05, $05, $05, $05, $05, $05
	dcb $05, $05, $05, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $0b, $0b, $0b, $0b, $0b, $0b, $0b
	dcb $0b, $00, $00, $00, $00, $00, $00, $00
	dcb $00, $00, $00, $00, $00, $00, $00, $00

src_row:
	; (32 bytes)

; A full-resolution version of the same thing. Not perhaps the most interesting thing to look at.

;#include "screenhack.h"
;
;#include <inttypes.h>
;
;struct texture
;{
;	unsigned width, height;
;	Colormap colormap;
;	GC gc;
;	unsigned long palette[128];
;	XImage *image;
;	uint8_t *row;
;	unsigned graininess;
;	unsigned lines;
;};
;
;#define GRAIN(self) (NRAND((self)->graininess * 2 + 1) - (self)->graininess)
;
;static void _put_line(struct texture *self, Display *dpy, Window window, unsigned y)
;{
;	unsigned x;
;	for(x = 0; x != self->width; ++x)
;	{
;		unsigned c1 = self->row[x];
;		unsigned c0 = c1;
;		if(c0 & 64)
;			c0 ^= 127;
;		XPutPixel(self->image, x, 0, self->palette[(c0 & 63) | ((c1 & 0x80) >> 1)]);
;	}
;
;	XPutImage(dpy, window, self->gc, self->image, 0, 0, 0, y, self->width, 1);
;
;	*self->row += GRAIN(self);
;
;	for(x = 1; x != self->width; ++x);
;	{
;		unsigned avg = self->row[x - 1] + self->row[x];
;		self->row[x] = ((avg + ((avg & 2) >> 1)) >> 1) + GRAIN(self);
;	}
;
;
;}
;
;static void texture_reshape(Display *dpy, Window window, void *self_raw, unsigned w, unsigned h)
;{
;	struct texture *self = self_raw;
;	unsigned x, y;
;
;	if(w == self->width && h == self->height)
;		return;
;
;	self->image->bytes_per_line = 0;
;	self->image->width = w;
;	XInitImage(self->image);
;
;	free(self->row);
;	self->row = malloc(w);
;	free(self->image->data);
;	self->image->data = malloc(w * self->image->bytes_per_line);
;
;	self->width = w;
;	self->height = h;
;
;	*self->row = NRAND(0xff);
;	for(x = 1; x != self->width; ++x)
;		self->row[x] = self->row[x - 1] + GRAIN(self);
;
;	for(y = 0; y != self->height; ++y)
;		_put_line(self, dpy, window, y);
;}
;
;static void *texture_init(Display *dpy, Window window)
;{
;	static const XGCValues gcv_src = {GXcopy};
;	XGCValues gcv = gcv_src;
;	struct texture *self = malloc(sizeof(*self));
;	XWindowAttributes xwa;
;	unsigned i;
;
;	XGetWindowAttributes(dpy, window, &xwa);
;	self->width = 0;
;	self->height = 0;
;	self->colormap = xwa.colormap;
;
;	self->graininess = get_integer_resource(dpy, "graininess", "Graininess");
;	self->lines = get_integer_resource(dpy, "speed", "Speed");
;
;	for(i = 0; i != 64; ++i)
;	{
;		XColor color;
;		unsigned short a = i * (0x10000 / 64);
;
;		color.red   = a;
;		color.green = a;
;		color.blue  = a;
;		if(!XAllocColor(dpy, xwa.colormap, &color))
;			abort();
;		self->palette[i] = color.pixel;
;
;		color.red   = 0;
;		color.green = a;
;		color.blue  = 0;
;		if(!XAllocColor(dpy, xwa.colormap, &color))
;			abort();
;		self->palette[i | 64] = color.pixel;
;	}
;
;	self->gc = XCreateGC(dpy, window, GCFunction, &gcv);
;	self->image = XCreateImage(dpy, xwa.visual, xwa.depth, ZPixmap, 0, NULL, 0, 1, 32, 0);
;	self->row = NULL;
;
;	texture_reshape(dpy, window, self, xwa.width, xwa.height);
;
;	return self;
;}
;
;static unsigned long texture_draw(Display *dpy, Window window, void *self_raw)
;{
;	struct texture *self = self_raw;
;	unsigned y;
;	XCopyArea(dpy, window, window, self->gc, 0, self->lines, self->width, self->height - self->lines, 0, 0);
;	for(y = 0; y != self->lines; ++y)
;		_put_line(self, dpy, window, self->height - self->lines + y);
;	return 16667;
;}
;
;static Bool texture_event (Display *dpy, Window window, void *self_raw, XEvent *event)
;{
;	return False;
;}
;
;static void texture_free(Display *dpy, Window window, void *self_raw)
;{
;	struct texture *self = self_raw;
;
;	XFreeGC(dpy, self->gc);
;	XFreeColors(dpy, self->colormap, self->palette, 128, 0);
;	XDestroyImage(self->image);
;	free(self->row);
;	free(self);
;}
;
;static const char *texture_defaults[] =
;{
;	"*speed:      2",
;	"*graininess: 1",
;	"*fpsSolid:   True",
;	"*fpsTop:     True",
;	0
;};
;
;static XrmOptionDescRec texture_options[] =
;{
;	{"-speed",      ".speed",      XrmoptionSepArg, 0},
;	{"-graininess", ".graininess", XrmoptionSepArg, 0},
;	{0, 0, 0, 0}
;};
;
;XSCREENSAVER_MODULE("Texture", texture)
