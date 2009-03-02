#ifndef _CHANGE_LOCALE_H
#define _CHANGE_LOCALE_H

/* The xscreensaver hacks deal badly with these locales, since the
   existing code by and large can't handle multi-byte characters.
   So, if the current locale is set to one beginning with any of
   these prefixes, we set the locale to "C" instead.
 */

static const char * const change_locale[] = { 
  "ja_JP",
  "ko",
  "zh",
  NULL
};

#endif
