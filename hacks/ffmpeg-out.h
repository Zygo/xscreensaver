/* ffmpeg-out, Copyright © 2023 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Writing a sequence of XImages to an output MP4 file.
 */

#ifndef __FFMPEG_OUT_H__
# define __FFMPEG_OUT_H__

typedef struct ffmpeg_out_state ffmpeg_out_state;
extern ffmpeg_out_state *ffmpeg_out_init (const char *outfile,
                                          const char *audiofile,
                                          int w, int h, int bpp,
                                          Bool fast_p);
extern void ffmpeg_out_add_frame (ffmpeg_out_state *, XImage *);
extern void ffmpeg_out_close (ffmpeg_out_state *);

#endif /* __FFMPEG_OUT_H__ */
