/* texfonts, Copyright (c) 2005-2014 Jamie Zawinski <jwz@jwz.org>
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

/* Bounding box of the multi-line string, in pixels.
 */
extern int texture_string_width (texture_font_data *, const char *,
                                 int *height_ret);

/* Draws the string in the scene at the origin.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().
 */
extern void print_texture_string (texture_font_data *, const char *);

/* Draws the string on the window at the given pixel position.
   Newlines and tab stops are honored.
   Any numbers inside [] will be rendered as a subscript.
   Assumes the font has been loaded as with load_texture_font().

   Position is 0 for center, 1 for top left, 2 for bottom left.
 */
void print_texture_label (Display *, texture_font_data *,
                          int window_width, int window_height,
                          int position, const char *string);

/* Releases the texture font.
 */
extern void free_texture_font (texture_font_data *);

#endif /* __TEXTURE_FONT_H__ */
