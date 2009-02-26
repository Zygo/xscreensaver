/* texfonts, Copyright (c) 2005 Jamie Zawinski <jwz@jwz.org>
 * Loads X11 fonts into textures for use with OpenGL.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __TEXTURE_FONT_H__
#define __TEXTURE_FONT_H__

typedef struct texture_font_data texture_font_data;

/* Loads the font named by the X resource "res" and returns
   a texture-font object.
*/
extern texture_font_data *load_texture_font (Display *, char *res);

/* Width of the string in pixels.
 */
extern int texture_string_width (texture_font_data *, const char *,
                                 int *line_height_ret);

/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
 */
extern void print_texture_string (texture_font_data *, const char *);

/* Releases the texture font.
 */
extern void free_texture_font (texture_font_data *);

#endif /* __TEXTURE_FONT_H__ */
