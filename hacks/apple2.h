/* xscreensaver, Copyright (c) 1998-2004 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Apple ][ CRT simulator, by Trevor Blackwell <tlb@tlb.org>
 * with additional work by Jamie Zawinski <jwz@jwz.org>
 */

#ifndef __XSCREENSAVER_APPLE_II__
#define __XSCREENSAVER_APPLE_II__

#include "analogtv.h"

typedef struct apple2_state {
  unsigned char hireslines[192][40];
  unsigned char textlines[24][40];
  int gr_text;
  enum {
    A2_GR_FULL=1,
    A2_GR_LORES=2,
    A2_GR_HIRES=4
  } gr_mode;
  int cursx;
  int cursy;
  int blink;

} apple2_state_t;


typedef struct apple2_sim_s apple2_sim_t;
struct apple2_sim_s {
  
  void *controller_data;

  apple2_state_t *st;

  analogtv *dec;
  analogtv_input *inp;
  analogtv_reception reception;

  const char *typing;
  char typing_buf[1024];
  double typing_rate;

  char *printing;
  char printing_buf[1024];

  char prompt;

  Display *dpy;
  Window window;
  XWindowAttributes xgwa;
  XImage *text_im;

  struct timeval basetime_tv;
  double curtime;
  double delay;

  int stepno;
  double next_actiontime;
  void (*controller)(apple2_sim_t *sim,
                     int *stepno,
                     double *next_actiontime);

};


enum {
  A2_HCOLOR_BLACK=0,
  A2_HCOLOR_GREEN=1,
  A2_HCOLOR_PURPLE=2,
  A2_HCOLOR_WHITE=3,
  A2_HCOLOR_ALTBLACK=4,
  A2_HCOLOR_RED=5,
  A2_HCOLOR_BLUE=6,
  A2_HCOLOR_ALTWHITE=7
 };

enum {
  A2CONTROLLER_DONE=-1,
  A2CONTROLLER_FREE=-2
};


extern apple2_sim_t * apple2_start (Display *, Window, int delay,
                                    void (*)(apple2_sim_t *, int *stepno,
                                             double *next_actiontime));
extern int apple2_one_frame (apple2_sim_t *);


void a2_poke(apple2_state_t *st, int addr, int val);
void a2_goto(apple2_state_t *st, int r, int c);
void a2_cls(apple2_state_t *st);
void a2_invalidate(apple2_state_t *st);

void a2_add_disk_item(apple2_state_t *st, char *name, unsigned char *data,
                      int len, char type);
void a2_scroll(apple2_state_t *st);
void a2_printc(apple2_state_t *st, char c);
void a2_printc_noscroll(apple2_state_t *st, char c);
void a2_prints(apple2_state_t *st, char *s);
void a2_goto(apple2_state_t *st, int r, int c);
void a2_cls(apple2_state_t *st);
void a2_clear_hgr(apple2_state_t *st);
void a2_clear_gr(apple2_state_t *st);
void a2_invalidate(apple2_state_t *st);
void a2_poke(apple2_state_t *st, int addr, int val);
void a2_display_image_loading(apple2_state_t *st, unsigned char *image,
                              int lineno);
void a2_init_memory_active(apple2_sim_t *sim);
void a2_hplot(apple2_state_t *st, int hcolor, int x, int y);
void a2_hline(apple2_state_t *st, int hcolor, int x1, int y1, int x2, int y2);
void a2_plot(apple2_state_t *st, int color, int x, int y);

#endif /* __XSCREENSAVER_APPLE_II__ */
