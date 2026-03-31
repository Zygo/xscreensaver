/* grab-ximage.c --- grab the screen to an XImage for use with OpenGL.
 * xscreensaver, Copyright (c) 2001-2006 Jamie Zawinski <jwz@jwz.org>
 *
 * Modified by Richard Weeks <rtweeks21@gmail.com> Copyright (c) 2020
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef __GRAB_XIMAGE_H__
#define __GRAB_XIMAGE_H__

/* Grabs an image of the desktop (or another random image file) and
   loads the image into GL's texture memory.  Most of the work is done
   in the background; when the image has been loaded, a callback is run.

   As a side-effect, that image *may* be painted onto the given Window.

   If mipmap_p is true, then make mipmaps instead of just a single texture.

   If desired_width/height are non-zero, then (if possible) the image
   will be scaled to fit in that rectangle.  If they are 0, then the size
   of the window is used.  These parameters are so that you can hint to
   the image loader that smaller images are acceptable (if you will never
   be displaying the texture at 100% magnification, you can get away with
   smaller textures.)

   Returns the sizes of various things:

      texture_width/height: The size of the texture itself, in pixels.
                            This will often be larger than the grabbed
                            image, since OpenGL sometimes requires texture
                            dimensions to be a power of 2.

      image_width/height:   The size of the image: this will usually be the
                            same as the desired_width/height you passed in
                            (but may be the size of the Window instead.)

      geometry:             The position in the texture of the image bits.
                            When image files are loaded, they are scaled up
                            to the size of the window, but if the image does
                            not have the same aspect ratio as the window,
                            there will be black bars on the top/bottom or
                            left/right.  This geometry specification tells
                            you where the "real" image bits are.

   So, don't use texture coordinates from 0.0 to 1.0.  Instead use:

      [0.0 - iw/tw]         If you want to display a quad that is the same
      [0.0 - ih/th]         size as the window; or

      [gx/tw - (gx+gw)/tw]  If you want to display a quad that is the same
      [gy/th - (gy+gh)/th]  size as the loaded image file.

   When the callback is called, the image data will have been loaded
   into texture number `texid' (via glBindTexture.)

   If an error occurred, width/height will be 0.
 */
void load_texture_async (Screen *, Window, GLXContext,
                         int desired_width, int desired_height,
                         Bool mipmap_p,
                         GLuint texid,
                         void (*callback) (const char *filename,
                                           XRectangle *geometry,
                                           int image_width,
                                           int image_height,
                                           int texture_width,
                                           int texture_height,
                                           void *closure),
                         void *closure);

struct texture_loader_t;
typedef struct texture_loader_t texture_loader_t;

/* Incremental Texture Loading

   The process works like load_texture_async, but split into multiple calls
   to facilitate chunks of work fitted to animation idle periods.  The
   necessary calls are:

      * alloc_texture_loader - one call - allocates texture loader and starts
            background loading of image
      * step_texture_loader - multiple calls - each call processes part of the
            image into an OpenGL texture; calls back to provided function when
            texture is ready with image
      * free_texture_loader - one call - frees the texture loader resources
            and memory

   With the exception of the pointer to the loader and the time limit, all
   arguments are the same types as used by load_texture_async.
 */
texture_loader_t *alloc_texture_loader (Screen *, Window, GLXContext,
                                        int desired_width, int desired_height,
                                        Bool mipmap_p,
                                        GLuint texid);

/* Give an incremental texture loader time to work

   The given callback function will be called (with the passed closure) if
   the texture loader completes its work, and this callback will only be
   made once during the lifetime of the texture loader.

   This function will return either when all work is complete or when a
   processed "stripe" takes the elapsed time beyond allowed_seconds.
 */
void step_texture_loader (texture_loader_t *loader,
                          double allowed_seconds,
                          void (*callback) (const char *filename,
                                            XRectangle *geometry,
                                            int image_width,
                                            int image_height,
                                            int texture_width,
                                            int texture_height,
                                            void *closure),
                          void *closure);

Bool texture_loader_failed (texture_loader_t *loader);

void free_texture_loader (texture_loader_t *loader);

#endif /* __GRAB_XIMAGE_H__ */
