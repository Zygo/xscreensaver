/* xscreensaver, Copyright (c) 1991-2017 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/* JWXYZ Is Not Xlib.

   But it's a bunch of function definitions that bear some resemblance to
   Xlib and that do Cocoa-ish or OpenGL-ish things that bear some resemblance
   to the things that Xlib might have done.

   This code is used by both the original jwxyz.m and the new jwxyz-gl.c.
 */

#import "jwxyzI.h"
#import "jwxyz-cocoa.h"
#import "utf8wc.h"

#include <stdarg.h>

#ifdef USE_IPHONE
# import <OpenGLES/ES1/gl.h>
# import <OpenGLES/ES1/glext.h>
# define NSOpenGLContext EAGLContext
# define NSFont          UIFont

# define NSFontTraitMask      UIFontDescriptorSymbolicTraits
// The values for the flags for NSFontTraitMask and
// UIFontDescriptorSymbolicTraits match up, not that it really matters here.
# define NSBoldFontMask       UIFontDescriptorTraitBold
# define NSFixedPitchFontMask UIFontDescriptorTraitMonoSpace
# define NSItalicFontMask     UIFontDescriptorTraitItalic
#endif

#import <CoreText/CTLine.h>
#import <CoreText/CTRun.h>

#define VTBL JWXYZ_VTBL(dpy)

/* OS X/iOS-specific JWXYZ implementation. */

void
jwxyz_logv (Bool error, const char *fmt, va_list args)
{
  vfprintf (stderr, fmt, args);
  fputc ('\n', stderr);
}

/* Instead of calling abort(), throw a real exception, so that
   XScreenSaverView can catch it and display a dialog.
 */
void
jwxyz_abort (const char *fmt, ...)
{
  char s[10240];
  if (!fmt || !*fmt)
    strcpy (s, "abort");
  else
    {
      va_list args;
      va_start (args, fmt);
      vsprintf (s, fmt, args);
      va_end (args);
    }
  [[NSException exceptionWithName: NSInternalInconsistencyException
                reason: [NSString stringWithCString: s
                                  encoding:NSUTF8StringEncoding]
                userInfo: nil]
    raise];
# undef abort
  abort();  // not reached
}


const XRectangle *
jwxyz_frame (Drawable d)
{
  return &d->frame;
}


unsigned int
jwxyz_drawable_depth (Drawable d)
{
  return (d->type == WINDOW
          ? visual_depth (NULL, NULL)
          : d->pixmap.depth);
}


float
jwxyz_scale (Window main_window)
{
  float scale = 1;

# ifdef USE_IPHONE
  /* Since iOS screens are physically smaller than desktop screens, scale up
     the fonts to make them more readable.

     Note that X11 apps on iOS also have the backbuffer sized in points
     instead of pixels, resulting in an effective X11 screen size of 768x1024
     or so, even if the display has significantly higher resolution.  That is
     unrelated to this hack, which is really about DPI.
   */
  scale = main_window->window.view.hackedContentScaleFactor;
  if (scale < 1) // iPad Pro magnifies the backbuffer by 3x, which makes text
    scale = 1;   // excessively blurry in BSOD.

# else  // !USE_IPHONE

  /* Desktop retina displays also need fonts doubled. */
  scale = main_window->window.view.hackedContentScaleFactor;

# endif // !USE_IPHONE

  return scale;
}


/* Font metric terminology, as used by X11:

   "lbearing" is the distance from the logical origin to the leftmost pixel.
   If a character's ink extends to the left of the origin, it is negative.

   "rbearing" is the distance from the logical origin to the rightmost pixel.

   "descent" is the distance from the logical origin to the bottommost pixel.
   For characters with descenders, it is positive.  For superscripts, it
   is negative.

   "ascent" is the distance from the logical origin to the topmost pixel.
   It is the number of pixels above the baseline.

   "width" is the distance from the logical origin to the position where
   the logical origin of the next character should be placed.

   If "rbearing" is greater than "width", then this character overlaps the
   following character.  If smaller, then there is trailing blank space.
 */
static void
utf8_metrics (Display *dpy, NSFont *nsfont, NSString *nsstr, XCharStruct *cs)
{
  // Returns the metrics of the multi-character, single-line UTF8 string.

  Drawable d = XRootWindow (dpy, 0);

  CGContextRef cgc = d->cgc;
  NSDictionary *attr =
    [NSDictionary dictionaryWithObjectsAndKeys:
                    nsfont, NSFontAttributeName,
                  nil];
  NSAttributedString *astr = [[NSAttributedString alloc]
                               initWithString:nsstr
                                   attributes:attr];
  CTLineRef ctline = CTLineCreateWithAttributedString (
                       (__bridge CFAttributedStringRef) astr);
  CGContextSetTextPosition (cgc, 0, 0);
  CGContextSetShouldAntialias (cgc, True);  // #### Guess?

  memset (cs, 0, sizeof(*cs));

  // "CTRun represents set of consecutive glyphs sharing the same
  // attributes and direction".
  //
  // We also get multiple runs any time font subsitution happens:
  // E.g., if the current font is Verdana-Bold, a &larr; character
  // in the NSString will actually be rendered in LucidaGrande-Bold.
  //
  int count = 0;
  for (id runid in (NSArray *)CTLineGetGlyphRuns(ctline)) {
    CTRunRef run = (CTRunRef) runid;
    CFRange r = { 0, };
    CGRect bbox = CTRunGetImageBounds (run, cgc, r);
    CGFloat ascent, descent, leading;
    CGFloat advancement =
      CTRunGetTypographicBounds (run, r, &ascent, &descent, &leading);

# ifndef USE_IPHONE
    // Only necessary for when LCD smoothing is enabled, which iOS doesn't do.
    bbox.origin.x    -= 2.0/3.0;
    bbox.size.width  += 4.0/3.0;
    bbox.size.height += 1.0/2.0;
# endif

    // Create the metrics for this run:
    XCharStruct cc;
    cc.ascent   = ceil  (bbox.origin.y + bbox.size.height);
    cc.descent  = ceil (-bbox.origin.y);
    cc.lbearing = floor (bbox.origin.x);
    cc.rbearing = ceil  (bbox.origin.x + bbox.size.width);
    cc.width    = floor (advancement + 0.5);

    // Add those metrics into the cumulative metrics:
    if (count == 0)
      *cs = cc;
    else
      {
        cs->ascent   = MAX (cs->ascent,     cc.ascent);
        cs->descent  = MAX (cs->descent,    cc.descent);
        cs->lbearing = MIN (cs->lbearing,   cs->width + cc.lbearing);
        cs->rbearing = MAX (cs->rbearing,   cs->width + cc.rbearing);
        cs->width    = MAX (cs->width,      cs->width + cc.width);
      }

    // Why no y? What about vertical text?
    // XCharStruct doesn't encapsulate that but XGlyphInfo does.

    count++;
  }

  [astr release];
  CFRelease (ctline);
}


static NSArray *
font_family_members (NSString *family_name)
{
# ifndef USE_IPHONE
  return [[NSFontManager sharedFontManager]
          availableMembersOfFontFamily:family_name];
# else
  return [UIFont fontNamesForFamilyName:family_name];
# endif
}


const char *
jwxyz_default_font_family (int require)
{
  return require & JWXYZ_STYLE_MONOSPACE ? "Courier" : "Verdana";
}


static NSFontTraitMask
nsfonttraitmask_for (int font_style)
{
  NSFontTraitMask result = 0;
  if (font_style & JWXYZ_STYLE_BOLD)
    result |= NSBoldFontMask;
  if (font_style & JWXYZ_STYLE_ITALIC)
    result |= NSItalicFontMask;
  if (font_style & JWXYZ_STYLE_MONOSPACE)
    result |= NSFixedPitchFontMask;
  return result;
}


static NSFont *
try_font (NSFontTraitMask traits, NSFontTraitMask mask,
          NSString *family_name, float size)
{
  NSArray *family_members = font_family_members (family_name);
  if (!family_members.count) {
    family_members = font_family_members (
      [NSString stringWithUTF8String:jwxyz_default_font_family (
        traits & NSFixedPitchFontMask ? JWXYZ_STYLE_MONOSPACE : 0)]);
  }

# ifndef USE_IPHONE
  for (unsigned k = 0; k != family_members.count; ++k) {

    NSArray *member = [family_members objectAtIndex:k];
    NSFontTraitMask font_mask =
    [(NSNumber *)[member objectAtIndex:3] unsignedIntValue];

    if ((font_mask & mask) == traits) {

      NSString *name = [member objectAtIndex:0];
      NSFont *f = [NSFont fontWithName:name size:size];
      if (!f)
        break;

      /* Don't use this font if it (probably) doesn't include ASCII characters.
       */
      NSStringEncoding enc = [f mostCompatibleStringEncoding];
      if (! (enc == NSUTF8StringEncoding ||
             enc == NSISOLatin1StringEncoding ||
             enc == NSNonLossyASCIIStringEncoding ||
             enc == NSISOLatin2StringEncoding ||
             enc == NSUnicodeStringEncoding ||
             enc == NSWindowsCP1250StringEncoding ||
             enc == NSWindowsCP1252StringEncoding ||
             enc == NSMacOSRomanStringEncoding)) {
        // NSLog(@"skipping \"%@\": encoding = %d", name, enc);
        break;
      }
      // NSLog(@"using \"%@\": %d", name, enc);

      return f;
    }
  }
# else // USE_IPHONE

  // This trick needs iOS 3.1, see "Using SDK-Based Development".
  Class has_font_descriptor = [UIFontDescriptor class];

  for (NSString *fn in family_members) {
# define MATCH(X) \
       ([fn rangeOfString:X options:NSCaseInsensitiveSearch].location \
       != NSNotFound)

    NSFontTraitMask font_mask;
    if (has_font_descriptor) {
      // This only works on iOS 7 and later.
      font_mask = [[UIFontDescriptor
                    fontDescriptorWithFontAttributes:
                    @{UIFontDescriptorNameAttribute:fn}]
                   symbolicTraits];
    } else {
      font_mask = 0;
      if (MATCH(@"Bold"))
        font_mask |= NSBoldFontMask;
      if (MATCH(@"Italic") || MATCH(@"Oblique"))
        font_mask |= NSItalicFontMask;
      if (MATCH(@"Courier"))
        font_mask |= NSFixedPitchFontMask;
    }

    if ((font_mask & mask) == traits) {

      /* Check if it can do ASCII.  No good way to accomplish this!
         These are fonts present in iPhone Simulator as of June 2012
         that don't include ASCII.
       */
      if (MATCH(@"AppleGothic") ||	// Korean
          MATCH(@"Dingbats") ||		// Dingbats
          MATCH(@"Emoji") ||		// Emoticons
          MATCH(@"Geeza") ||		// Arabic
          MATCH(@"Hebrew") ||		// Hebrew
          MATCH(@"HiraKaku") ||		// Japanese
          MATCH(@"HiraMin") ||		// Japanese
          MATCH(@"Kailasa") ||		// Tibetan
          MATCH(@"Ornaments") ||	// Dingbats
          MATCH(@"STHeiti")		// Chinese
       )
         break;

      return [UIFont fontWithName:fn size:size];
    }
# undef MATCH
  }
# endif

  return NULL;
}


/* Returns a random font in the given size and face.
 */
static NSFont *
random_font (NSFontTraitMask traits, NSFontTraitMask mask, float size)
{

# ifndef USE_IPHONE
  // Providing Unbold or Unitalic in the mask for availableFontNamesWithTraits
  // returns an empty list, at least on a system with default fonts only.
  NSArray *families = [[NSFontManager sharedFontManager]
                       availableFontFamilies];
  if (!families) return 0;
# else
  NSArray *families = [UIFont familyNames];

  // There are many dups in the families array -- uniquify it.
  {
    NSArray *sorted_families =
    [families sortedArrayUsingSelector:@selector(compare:)];
    NSMutableArray *new_families =
    [NSMutableArray arrayWithCapacity:sorted_families.count];

    NSString *prev_family = @"";
    for (NSString *family in sorted_families) {
      if ([family compare:prev_family])
        [new_families addObject:family];
      prev_family = family;
    }

    families = new_families;
  }
# endif // USE_IPHONE

  long n = [families count];
  if (n <= 0) return 0;

  int j;
  for (j = 0; j < n; j++) {
    int i = random() % n;
    NSString *family_name = [families objectAtIndex:i];

    NSFont *result = try_font (traits, mask, family_name, size);
    if (result)
      return result;
  }

  // None of the fonts support ASCII?
  return 0;
}

void *
jwxyz_load_native_font (Window main_window, int traits_jwxyz, int mask_jwxyz,
                        const char *font_name_ptr, size_t font_name_length,
                        int font_name_type, float size,
                        char **family_name_ret,
                        int *ascent_ret, int *descent_ret)
{
  NSFont *nsfont = NULL;

  NSFontTraitMask
    traits = nsfonttraitmask_for (traits_jwxyz),
    mask = nsfonttraitmask_for (mask_jwxyz);

  NSString *font_name = font_name_type != JWXYZ_FONT_RANDOM ?
    [[NSString alloc] initWithBytes:font_name_ptr
                             length:font_name_length
                           encoding:NSUTF8StringEncoding] :
    nil;

  size *= jwxyz_scale (main_window);

  if (font_name_type == JWXYZ_FONT_RANDOM) {

    nsfont = random_font (traits, mask, size);
    [font_name release];

  } else if (font_name_type == JWXYZ_FONT_FACE) {

    nsfont = [NSFont fontWithName:font_name size:size];

  } else if (font_name_type == JWXYZ_FONT_FAMILY) {
  
    Assert (size > 0, "zero font size");

    if (!nsfont)
      nsfont   = try_font (traits, mask, font_name, size);

    // if that didn't work, turn off attibutes until it does
    // (e.g., there is no "Monaco-Bold".)
    //
    if (!nsfont && (mask & NSItalicFontMask)) {
      traits &= ~NSItalicFontMask;
      mask &= ~NSItalicFontMask;
      nsfont = try_font (traits, mask, font_name, size);
    }
    if (!nsfont && (mask & NSBoldFontMask)) {
      traits &= ~NSBoldFontMask;
      mask &= ~NSBoldFontMask;
      nsfont = try_font (traits, mask, font_name, size);
    }
    if (!nsfont && (mask & NSFixedPitchFontMask)) {
      traits &= ~NSFixedPitchFontMask;
      mask &= ~NSFixedPitchFontMask;
      nsfont = try_font (traits, mask, font_name, size);
    }
  }

  if (nsfont)
  {
    if (family_name_ret)
      *family_name_ret = strdup (nsfont.familyName.UTF8String);

    CFRetain (nsfont);   // needed for garbage collection?

    *ascent_ret  =  ceil ([nsfont ascender]);
    *descent_ret = -floor ([nsfont descender]);

    Assert([nsfont fontName], "broken NSFont in fid");
  }

  return nsfont;
}


void
jwxyz_release_native_font (Display *dpy, void *native_font)
{
  // #### DAMMIT!  I can't tell what's going wrong here, but I keep getting
  //      crashes in [NSFont ascender] <- query_font, and it seems to go away
  //      if I never release the nsfont.  So, fuck it, we'll just leak fonts.
  //      They're probably not very big...
  //
  //  [fid->nsfont release];
  //  CFRelease (fid->nsfont);
}


// Given a UTF8 string, return an NSString.  Bogus UTF8 characters are ignored.
// We have to do this because stringWithCString returns NULL if there are
// any invalid characters at all.
//
static NSString *
sanitize_utf8 (const char *in, size_t in_len, Bool *latin1_pP)
{
  size_t out_len = in_len * 4;   // length of string might increase
  char *s2 = (char *) malloc (out_len);
  char *out = s2;
  const char *in_end  = in  + in_len;
  const char *out_end = out + out_len;
  Bool latin1_p = True;

  while (in < in_end)
    {
      unsigned long uc;
      long L1 = utf8_decode ((const unsigned char *) in, in_end - in, &uc);
      long L2 = utf8_encode (uc, out, out_end - out);
      in  += L1;
      out += L2;
      if (uc > 255) latin1_p = False;
    }
  *out = 0;
  NSString *nsstr =
    [NSString stringWithCString:s2 encoding:NSUTF8StringEncoding];
  free (s2);
  if (latin1_pP) *latin1_pP = latin1_p;
  return (nsstr ? nsstr : @"");
}


NSString *
nsstring_from(const char *str, size_t len, int utf8_p)
{
  Bool latin1_p;
  NSString *nsstr = utf8_p ?
    sanitize_utf8 (str, len, &latin1_p) :
    [[[NSString alloc] initWithBytes:str
                              length:len
                            encoding:NSISOLatin1StringEncoding]
     autorelease];
  return nsstr;
}

void
jwxyz_render_text (Display *dpy, void *native_font,
                   const char *str, size_t len, Bool utf8_p, Bool antialias_p,
                   XCharStruct *cs_ret, char **pixmap_ret)
{
  utf8_metrics (dpy, (NSFont *)native_font, nsstring_from (str, len, utf8_p),
                cs_ret);

  Assert (!pixmap_ret, "TODO");
}


void
jwxyz_get_pos (Window w, XPoint *xvpos, XPoint *xp)
{
# ifdef USE_IPHONE

  xvpos->x = 0;
  xvpos->y = 0;

  if (xp) {
    xp->x = w->window.last_mouse_x;
    xp->y = w->window.last_mouse_y;
  }

# else  // !USE_IPHONE

  NSWindow *nsw = [w->window.view window];

  // get bottom left of window on screen, from bottom left

# if (MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_6)
  NSRect rr1 = [w->window.view convertRect: NSMakeRect(0,0,0,0) toView:nil];
  NSRect rr2 = [nsw convertRectToScreen: rr1];
  NSPoint wpos = NSMakePoint (rr2.origin.x - rr1.origin.x,
                              rr2.origin.y - rr1.origin.y);
# else
  // deprecated as of 10.7
  NSPoint wpos = [nsw convertBaseToScreen: NSMakePoint(0,0)];
# endif


  // get bottom left of view on window, from bottom left
  NSPoint vpos;
  vpos.x = vpos.y = 0;
  vpos = [w->window.view convertPoint:vpos toView:[nsw contentView]];

  // get bottom left of view on screen, from bottom left
  vpos.x += wpos.x;
  vpos.y += wpos.y;

  // get top left of view on screen, from bottom left
  vpos.y += w->frame.height;

  // get top left of view on screen, from top left
  NSArray *screens = [NSScreen screens];
  NSScreen *screen = (screens && [screens count] > 0
                      ? [screens objectAtIndex:0]
                      : [NSScreen mainScreen]);
  NSRect srect = [screen frame];
  vpos.y = srect.size.height - vpos.y;

  xvpos->x = vpos.x;
  xvpos->y = vpos.y;

  if (xp) {
    // get the mouse position on window, from bottom left
    NSEvent *e = [NSApp currentEvent];
    NSPoint p = [e locationInWindow];

    // get mouse position on screen, from bottom left
    p.x += wpos.x;
    p.y += wpos.y;

    // get mouse position on screen, from top left
    p.y = srect.size.height - p.y;

    xp->x = (int) p.x;
    xp->y = (int) p.y;
  }

# endif // !USE_IPHONE
}


#ifdef USE_IPHONE

void
check_framebuffer_status (void)
{
  int err = glCheckFramebufferStatusOES (GL_FRAMEBUFFER_OES);
  switch (err) {
  case GL_FRAMEBUFFER_COMPLETE_OES:
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES:
    Assert (0, "framebuffer incomplete attachment");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES:
    Assert (0, "framebuffer incomplete missing attachment");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES:
    Assert (0, "framebuffer incomplete dimensions");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_OES:
    Assert (0, "framebuffer incomplete formats");
    break;
  case GL_FRAMEBUFFER_UNSUPPORTED_OES:
    Assert (0, "framebuffer unsupported");
    break;
/*
  case GL_FRAMEBUFFER_UNDEFINED:
    Assert (0, "framebuffer undefined");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
    Assert (0, "framebuffer incomplete draw buffer");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
    Assert (0, "framebuffer incomplete read buffer");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    Assert (0, "framebuffer incomplete multisample");
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
    Assert (0, "framebuffer incomplete layer targets");
    break;
 */
  default:
    NSCAssert (0, @"framebuffer incomplete, unknown error 0x%04X", err);
    break;
  }
}


void
create_framebuffer (GLuint *gl_framebuffer, GLuint *gl_renderbuffer)
{
  glGenFramebuffersOES  (1, gl_framebuffer);
  glBindFramebufferOES  (GL_FRAMEBUFFER_OES,  *gl_framebuffer);

  glGenRenderbuffersOES (1, gl_renderbuffer);
  glBindRenderbufferOES (GL_RENDERBUFFER_OES, *gl_renderbuffer);
}

#endif // USE_IPHONE


#if defined JWXYZ_QUARTZ

/* Pushes a GC context; sets Fill and Stroke colors to the background color.
 */
static void
push_bg_gc (Display *dpy, Drawable d, GC gc, Bool fill_p)
{
  XGCValues *gcv = VTBL->gc_gcv (gc);
  push_color_gc (dpy, d, gc, gcv->background, gcv->antialias_p, fill_p);
}


void
jwxyz_quartz_copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
                        int src_x, int src_y,
                        unsigned int width, unsigned int height,
                        int dst_x, int dst_y)
{
  XRectangle src_frame = src->frame, dst_frame = dst->frame;
  XGCValues *gcv = VTBL->gc_gcv (gc);

  BOOL mask_p = src->type == PIXMAP && src->pixmap.depth == 1;


  /* If we're copying from a bitmap to a bitmap, and there's nothing funny
     going on with clipping masks or depths or anything, optimize it by
     just doing a memcpy instead of going through a CGI.
   */
  if (gcv->function == GXcopy &&
      !gcv->clip_mask &&
      jwxyz_drawable_depth (src) == jwxyz_drawable_depth (dst)) {

    Assert(!src_frame.x &&
           !src_frame.y &&
           !dst_frame.x &&
           !dst_frame.y,
           "unexpected non-zero origin");

    jwxyz_blit (CGBitmapContextGetData (src->cgc),
                CGBitmapContextGetBytesPerRow (src->cgc), src_x, src_y,
                CGBitmapContextGetData (dst->cgc),
                CGBitmapContextGetBytesPerRow (dst->cgc), dst_x, dst_y,
                width, height);

  } else {
    CGRect
    src_rect = CGRectMake(src_x, src_y, width, height),
    dst_rect = CGRectMake(dst_x, dst_y, width, height);

    src_rect.origin = map_point (src, src_rect.origin.x,
                                 src_rect.origin.y + src_rect.size.height);
    dst_rect.origin = map_point (dst, dst_rect.origin.x,
                                 dst_rect.origin.y + src_rect.size.height);

    NSObject *releaseme = 0;
    CGImageRef cgi;
    BOOL free_cgi_p = NO;

    // We must first copy the bits to an intermediary CGImage object, then
    // copy that to the destination drawable's CGContext.
    //
    // First we get a CGImage out of the pixmap CGContext -- it's the whole
    // pixmap, but it presumably shares the data pointer instead of copying
    // it.  We then cache that CGImage it inside the Pixmap object.  Note:
    // invalidate_drawable_cache() must be called to discard this any time a
    // modification is made to the pixmap, or we'll end up re-using old bits.
    //
    if (!src->cgi)
      src->cgi = CGBitmapContextCreateImage (src->cgc);
    cgi = src->cgi;

    // if doing a sub-rect, trim it down.
    if (src_rect.origin.x    != src_frame.x   ||
        src_rect.origin.y    != src_frame.y   ||
        src_rect.size.width  != src_frame.width ||
        src_rect.size.height != src_frame.height) {
      // #### I don't understand why this is needed...
      src_rect.origin.y = (src_frame.height -
                           src_rect.size.height - src_rect.origin.y);
      // This does not copy image data, so it should be fast.
      cgi = CGImageCreateWithImageInRect (cgi, src_rect);
      free_cgi_p = YES;
    }

    CGContextRef cgc = dst->cgc;

    if (mask_p) {		// src depth == 1

      push_bg_gc (dpy, dst, gc, YES);

      // fill the destination rectangle with solid background...
      CGContextFillRect (cgc, dst_rect);

      Assert (cgc, "no CGC with 1-bit XCopyArea");

      // then fill in a solid rectangle of the fg color, using the image as an
      // alpha mask.  (the image has only values of BlackPixel or WhitePixel.)
      set_color (dpy, cgc, gcv->foreground, VTBL->gc_depth (gc),
                 gcv->alpha_allowed_p, YES);
      CGContextClipToMask (cgc, dst_rect, cgi);
      CGContextFillRect (cgc, dst_rect);

      pop_gc (dst, gc);

    } else {		// src depth > 1

      push_gc (dst, gc);

      // copy the CGImage onto the destination CGContext
      //Assert(CGImageGetColorSpace(cgi) == dpy->colorspace, "bad colorspace");
      CGContextDrawImage (cgc, dst_rect, cgi);

      pop_gc (dst, gc);
    }

    if (free_cgi_p) CGImageRelease (cgi);
    
    if (releaseme) [releaseme release];
  }
  
  invalidate_drawable_cache (dst);
}

#elif defined JWXYZ_GL

/* Warning! The JWXYZ_GL code here is experimental and provisional and not at
   all ready for prime time. Please be careful.
 */

void
jwxyz_copy_area (Display *dpy, Drawable src, Drawable dst, GC gc,
                 int src_x, int src_y,
                 unsigned int width, unsigned int height,
                 int dst_x, int dst_y)
{
  /* TODO:
   - glCopyPixels if src == dst
   - Pixel buffer copying
   - APPLE_framebuffer_multisample has ResolveMultisampleFramebufferAPPLE,
     which is like a blit.
   */

  /* Strange and ugly flickering when going the glCopyTexImage2D route on
     OS X. (Early 2009 Mac mini, OS X 10.10)
   */
# ifdef USE_IPHONE

  /* TODO: This might not still work. */
  jwxyz_bind_drawable (dpy, dpy->main_window, src);
  jwxyz_gl_copy_area_read_tex_image (dpy, jwxyz_frame (src)->height,
                                     src_x, src_y, width, height, dst_x, dst_y);
  jwxyz_bind_drawable (dpy, dpy->main_window, dst);
  jwxyz_gl_copy_area_write_tex_image (dpy, gc, src_x, src_y,
                                      width, height, dst_x, dst_y);
# else // !USE_IPHONE
  jwxyz_gl_copy_area_read_pixels (dpy, src, dst, gc,
                                  src_x, src_y, width, height, dst_x, dst_y);
# endif // !USE_IPHONE
  jwxyz_assert_gl ();
}


void
jwxyz_assert_gl ()
{
  // This is like check_gl_error, except this happens for debug builds only.
#ifndef __OPTIMIZE__
  if([NSOpenGLContext currentContext])
  {
    // glFinish here drops FPS into the toilet. It might need to be on if
    // something goes wrong.
    // glFinish();
    GLenum error = glGetError();
    Assert (!error, "jwxyz_assert_gl: OpenGL error");
  }
#endif // !__OPTIMIZE__
}

void
jwxyz_assert_drawable (Window main_window, Drawable d)
{
#if !defined USE_IPHONE && !defined __OPTIMIZE__
  XScreenSaverView *view = main_window->window.view;
  NSOpenGLContext *ogl_ctx = [view oglContext];

  if (d->type == WINDOW) {
    Assert([ogl_ctx view] == view,
           "jwxyz_assert_display: ogl_ctx view not set!");
  }

  @try {
    /* Assert([d->ctx isKindOfClass:[NSOpenGLContext class]], "Not a context."); */
    Class c = [ogl_ctx class];
    Assert([c isSubclassOfClass:[NSOpenGLContext class]], "Not a context.");
    // [d->ctx makeCurrentContext];
  }
  @catch (NSException *exception) {
    perror([[exception reason] UTF8String]);
    jwxyz_abort();
  }
#endif // !USE_IPHONE && !__OPTIMIZE__
}


void
jwxyz_bind_drawable (Window main_window, Drawable d)
{
  /* Windows and Pixmaps need to use different contexts with OpenGL
     screenhacks, because an OpenGL screenhack sets state in an arbitrary
     fashion, but jwxyz-gl.c has its own ideas w.r.t. OpenGL state.

     On iOS, all pixmaps can use the same context with different FBOs. Which
     is nice.
   */

  /* OpenGL screenhacks in general won't be drawing on the Window, but they
     can and will draw on a Pixmap -- but an OpenGL call following an Xlib
     call won't be able to fix the fact that it's drawing offscreen.
   */

  /* EXT_direct_state_access might be appropriate, but it's apparently not
     available on Mac OS X.
   */

  // jwxyz_assert_display (dpy);
  jwxyz_assert_drawable (main_window, main_window);
  jwxyz_assert_gl ();
  jwxyz_assert_drawable (main_window, d);

#if defined USE_IPHONE && !defined __OPTIMIZE__
  Drawable current_drawable = main_window->window.current_drawable;
  Assert (!current_drawable
            || current_drawable->ogl_ctx == [EAGLContext currentContext],
          "bind_drawable: Wrong context.");
  if (current_drawable) {
    GLint framebuffer;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING_OES, &framebuffer);
    Assert (framebuffer == current_drawable->gl_framebuffer,
            "bind_drawable: Wrong framebuffer.");
  }
#endif

  if (main_window->window.current_drawable != d) {
    main_window->window.current_drawable = d;

    /* Doing this repeatedly is probably not OK performance-wise. Probably. */
#ifndef USE_IPHONE
    [d->ogl_ctx makeCurrentContext];
#else
    [EAGLContext setCurrentContext:d->ogl_ctx];
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, d->gl_framebuffer);
    if (d->type == PIXMAP) {
      glViewport (0, 0, d->frame.width, d->frame.height);
      jwxyz_set_matrices (d->frame.width, d->frame.height);
    }
#endif
  }

  jwxyz_assert_gl ();
}


Pixmap
XCreatePixmap (Display *dpy, Drawable d,
               unsigned int width, unsigned int height, unsigned int depth)
{
  Pixmap p = (Pixmap) calloc (1, sizeof(*p));
  p->type = PIXMAP;
  p->frame.width  = width;
  p->frame.height = height;
  p->pixmap.depth = depth;

  Assert (depth == 1 || depth == 32, "XCreatePixmap: depth must be 32");

  /* TODO: If Pixel buffers are not supported, do something about it. */
  Window w = XRootWindow (dpy, 0);

# ifndef USE_IPHONE

  p->ogl_ctx = [[NSOpenGLContext alloc]
                initWithFormat:w->window.pixfmt
                shareContext:w->ogl_ctx];
  CFRetain (p->ogl_ctx);

  [p->ogl_ctx makeCurrentContext]; // This is indeed necessary.

  p->pixmap.gl_pbuffer = [[NSOpenGLPixelBuffer alloc]
                          /* TODO: Only if there are rectangluar textures. */
                          initWithTextureTarget:GL_TEXTURE_RECTANGLE_EXT
                          /* TODO: Make sure GL_RGBA isn't better. */
                          textureInternalFormat:GL_RGB
                          textureMaxMipMapLevel:0
                          pixelsWide:width
                          pixelsHigh:height];
  CFRetain (p->pixmap.gl_pbuffer);

  [p->ogl_ctx setPixelBuffer:p->pixmap.gl_pbuffer
                 cubeMapFace:0
                 mipMapLevel:0
        currentVirtualScreen:w->window.virtual_screen];

# else // USE_IPHONE

  p->ogl_ctx = w->window.ogl_ctx_pixmap;

  [EAGLContext setCurrentContext:p->ogl_ctx];
  create_framebuffer (&p->gl_framebuffer, &p->gl_renderbuffer);

  glRenderbufferStorageOES (GL_RENDERBUFFER_OES, GL_RGBA8_OES, width, height);
  glFramebufferRenderbufferOES (GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
                                GL_RENDERBUFFER_OES, p->gl_renderbuffer);

  check_framebuffer_status ();

  glBindFramebufferOES(GL_FRAMEBUFFER_OES, p->gl_framebuffer);

# endif // USE_IPHONE

  w->window.current_drawable = p;
  glViewport (0, 0, width, height);
  jwxyz_set_matrices (width, height);

# ifndef __OPTIMIZE__
  glClearColor (frand(1), frand(1), frand(1), 0);
  glClear (GL_COLOR_BUFFER_BIT);
# endif

  return p;
}

int
XFreePixmap (Display *d, Pixmap p)
{
  Assert (p && p->type == PIXMAP, "not a pixmap");

  Window w = RootWindow (d, 0);

# ifndef USE_IPHONE
  CFRelease (p->ogl_ctx);
  [p->ogl_ctx release];

  CFRelease (p->pixmap.gl_pbuffer);
  [p->pixmap.gl_pbuffer release];
# else  // USE_IPHONE
  glDeleteRenderbuffersOES (1, &p->gl_renderbuffer);
  glDeleteFramebuffersOES (1, &p->gl_framebuffer);
# endif // USE_IPHONE

  if (w->window.current_drawable == p) {
    w->window.current_drawable = NULL;
  }

  free (p);
  return 0;
}

#endif // JWXYZ_GL
