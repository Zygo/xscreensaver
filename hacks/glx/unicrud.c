/* unicrud, Copyright © 2016-2026 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#define DEFAULTS	"*delay:	20000          \n" \
			"*showFPS:      False          \n" \
                        "*titleFont: sans-serif bold 18\n" \
                        "*font:      sans-serif bold 300\n" \
			"*suppressRotationAnimation: True\n" \

# define release_unicrud 0

#include "xlockmore.h"
#include "rotator.h"
#include "gltrackball.h"
#include "texfont.h"
#include "utf8wc.h"
#include <ctype.h>

#ifdef USE_GL /* whole file */

#define DEF_SPIN        "True"
#define DEF_WANDER      "True"
#define DEF_SPEED       "1.0"
#define DEF_BLOCK       "ALL"
#define DEF_TITLES      "True"

typedef struct {
  GLXContext *glx_context;
  rotator *rot;
  trackball_state *trackball;
  Bool button_down_p;

  GLfloat color[4];
  texture_font_data *title_font, *char_font;
  unsigned long unichar;
  const char *charplane, *charblock, *charname;
  enum { IN, LINGER, OUT } state;
  int spin_direction;
  GLfloat ratio;

} unicrud_configuration;

static unicrud_configuration *bps = NULL;

static Bool do_spin;
static GLfloat speed;
static Bool do_wander;
static char *do_block = 0;
static Bool do_titles;

static XrmOptionDescRec opts[] = {
  { "-spin",   ".spin",   XrmoptionNoArg, "True" },
  { "+spin",   ".spin",   XrmoptionNoArg, "False" },
  { "-wander", ".wander", XrmoptionNoArg, "True" },
  { "+wander", ".wander", XrmoptionNoArg, "False" },
  { "-speed",  ".speed",  XrmoptionSepArg, 0 },
  { "-block",  ".block",  XrmoptionSepArg, 0 },
  { "-titles", ".titles", XrmoptionNoArg, "True"  },
  { "+titles", ".titles", XrmoptionNoArg, "False" },
};

static argtype vars[] = {
  {&do_spin,   "spin",   "Spin",   DEF_SPIN,   t_Bool},
  {&do_wander, "wander", "Wander", DEF_WANDER, t_Bool},
  {&speed,     "speed",  "Speed",  DEF_SPEED,  t_Float},
  {&do_block,  "block",  "Block",  DEF_BLOCK,  t_String},
  {&do_titles, "titles", "Titles", DEF_TITLES, t_Bool},
};

ENTRYPOINT ModeSpecOpt unicrud_opts = {countof(opts), opts, countof(vars), vars, NULL};

/* ENABLE_NLS: define this blank so that we can use it to tag static strings.
   We call gettext manually. */
# undef _
# define _(S) S


static const struct {
  unsigned long start;
  const char * const name;
} unicode_block_names[] = {

  /* Through Unicode 8.0, early 2016. */

  { 0x0000, "*" _("Basic Multilingual") },
  { 0x0000, _("Unassigned") },
  { 0x0021, _("ASCII") },
  { 0x0080, _("Unassigned") },
  { 0x00A1, _("Latin1") },
  { 0x0100, _("Latin Extended-A") },
  { 0x0180, _("Latin Extended-B") },
  { 0x0250, _("IPA Extensions") },
  { 0x02B0, _("Spacing Modifier Letters") },
  { 0x0300, _("Combining Diacritical Marks") },
  { 0x0370, _("Greek and Coptic") },
  { 0x0400, _("Cyrillic") },
  { 0x0500, _("Cyrillic Supplement") },
  { 0x0530, _("Armenian") },
  { 0x058B, _("Unassigned") },
  { 0x0590, _("Hebrew") },
  { 0x05F5, _("Unassigned") },
  { 0x0600, _("Arabic") },
  { 0x0700, _("Syriac") },
  { 0x0750, _("Arabic Supplement") },
  { 0x0780, _("Thaana") },
  { 0x07C0, _("N'Ko") },
  { 0x0800, _("Samaritan") },
  { 0x0840, _("Mandaic") },
  { 0x0860, _("Unassigned") },
  { 0x08A0, _("Arabic Extended-A") },
  { 0x0900, _("Devanagari") },
  { 0x0980, _("Bengali") },
  { 0x09FC, _("Unassigned") },
  { 0x0A00, _("Gurmukhi") },
  { 0x0A76, _("Unassigned") },
  { 0x0A80, _("Gujarati") },
  { 0x0AF2, _("Unassigned") },
  { 0x0B00, _("Oriya") },
  { 0x0B80, _("Tamil") },
  { 0x0BFB, _("Unassigned") },
  { 0x0C00, _("Telugu") },
  { 0x0C80, _("Kannada") },
  { 0x0CF0, _("Unassigned") },
  { 0x0D00, _("Malayalam") },
  { 0x0D80, _("Sinhala") },
  { 0x0DF5, _("Unassigned") },
  { 0x0E00, _("Thai") },
  { 0x0E5C, _("Unassigned") },
  { 0x0E80, _("Lao") },
  { 0x0EE0, _("Unassigned") },
  { 0x0F00, _("Tibetan") },
  { 0x0FDB, _("Unassigned") },
  { 0x1000, _("Myanmar") },
  { 0x10A0, _("Georgian") },
  { 0x1100, _("Hangul Jamo") },
  { 0x1200, _("Ethiopic") },
  { 0x1380, _("Ethiopic Supplement") },
  { 0x13A0, _("Cherokee") },
  { 0x1400, _("Unified Canadian Aboriginal Syllabics") },
  { 0x1677, _("Unassigned") },
  { 0x1680, _("Ogham") },
  { 0x16A0, _("Runic") },
  { 0x16F1, _("Unassigned") },
  { 0x1700, _("Tagalog") },
  { 0x1715, _("Unassigned") },
  { 0x1720, _("Hanunoo") },
  { 0x1737, _("Unassigned") },
  { 0x1740, _("Buhid") },
  { 0x1754, _("Unassigned") },
  { 0x1760, _("Tagbanwa") },
  { 0x1774, _("Unassigned") },
  { 0x1780, _("Khmer") },
  { 0x17FA, _("Unassigned") },
  { 0x1800, _("Mongolian") },
  { 0x18AB, _("Unassigned") },
  { 0x18B0, _("Unified Canadian Aboriginal Syllabics Extended") },
  { 0x18F6, _("Unassigned") },
  { 0x1900, _("Limbu") },
  { 0x1950, _("Tai Le") },
  { 0x1975, _("Unassigned") },
  { 0x1980, _("Tai Lue") },
  { 0x19E0, _("Khmer Symbols") },
  { 0x1A00, _("Buginese") },
  { 0x1A20, _("Tai Tham") },
  { 0x1AAE, _("Unassigned") },
  { 0x1AB0, _("Combining Diacritical Marks Extended") },
  { 0x1ABF, _("Unassigned") },
  { 0x1B00, _("Balinese") },
  { 0x1B80, _("Sundanese") },
  { 0x1BC0, _("Batak") },
  { 0x1C00, _("Lepcha") },
  { 0x1C50, _("Ol Chiki") },
  { 0x1C80, _("Unassigned") },
  { 0x1CC0, _("Sundanese Supplement") },
  { 0x1CC8, _("Unassigned") },
  { 0x1CD0, _("Vedic Extensions") },
  { 0x1CFA, _("Unassigned") },
  { 0x1D00, _("Phonetic Extensions") },
  { 0x1D80, _("Phonetic Extensions Supplement") },
  { 0x1DC0, _("Combining Diacritical Marks Supplement") },
  { 0x1E00, _("Latin Extended Additional") },
  { 0x1F00, _("Greek Extended") },
  { 0x2000, _("General Punctuation") },
  { 0x2070, _("Superscripts and Subscripts") },
  { 0x2095, _("Unassigned") },
  { 0x20A0, _("Currency Symbols") },
  { 0x20BE, _("Unassigned") },
  { 0x20D0, _("Combining Diacritical Marks for Symbols") },
  { 0x20F1, _("Unassigned") },
  { 0x2100, _("Letterlike Symbols") },
  { 0x2150, _("Number Forms") },
  { 0x2190, _("Arrows") },
  { 0x2200, _("Mathematical Operators") },
  { 0x2300, _("Miscellaneous Technical") },
  { 0x2400, _("Control Pictures") },
  { 0x2437, _("Unassigned") },
  { 0x2440, _("Optical Character Recognition") },
  { 0x244B, _("Unassigned") },
  { 0x2460, _("Enclosed Alphanumerics") },
  { 0x2500, _("Box Drawing") },
  { 0x2580, _("Block Elements") },
  { 0x25A0, _("Geometric Shapes") },
  { 0x2600, _("Miscellaneous Symbols") },
  { 0x2700, _("Dingbats") },
  { 0x27C0, _("Miscellaneous Mathematical Symbols-A") },
  { 0x27F0, _("Supplemental Arrows-A") },
  { 0x2800, _("Braille Patterns") },
  { 0x2900, _("Supplemental Arrows-B") },
  { 0x2980, _("Miscellaneous Mathematical Symbols-B") },
  { 0x2A00, _("Supplemental Mathematical Operators") },
  { 0x2B00, _("Miscellaneous Symbols and Arrows") },
  { 0x2B56, _("Unassigned") }, /* Unicode 5.1 */
  { 0x2BF0, _("Unassigned") },
  { 0x2C00, _("Glagolitic") },
  { 0x2C60, _("Latin Extended-C") },
  { 0x2C80, _("Coptic") },
  { 0x2D00, _("Georgian Supplement") },
  { 0x2D26, _("Unassigned") },
  { 0x2D30, _("Tifinagh") },
  { 0x2D80, _("Ethiopic Extended") },
  { 0x2DE0, _("Cyrillic Extended-A") },
  { 0x2E00, _("Supplemental Punctuation") },
  { 0x2E43, _("Unassigned") },
  { 0x2E80, _("CJK Radicals Supplement") },
  { 0x2EF5, _("Unassigned") },
  { 0x2F00, _("Kangxi Radicals") },
  { 0x2FD6, _("Unassigned") },
  { 0x2FE0, _("Unassigned") },
  { 0x2FF0, _("Ideographic Description Characters") },
  { 0x2FFC, _("Unassigned") },
  { 0x3000, _("CJK Symbols and Punctuation") },
  { 0x3040, _("Hiragana") },
  { 0x30A0, _("Katakana") },
  { 0x3100, _("Bopomofo") },
  { 0x3130, _("Hangul Compatibility Jamo") },
  { 0x3190, _("Kanbun") },
  { 0x31A0, _("Bopomofo Extended") },
  { 0x31BB, _("Unassigned") },
  { 0x31C0, _("CJK Strokes") },
  { 0x31E4, _("Unassigned") },
  { 0x31F0, _("Katakana Phonetic Extensions") },
  { 0x3200, _("Enclosed CJK Letters and Months") },
  { 0x3300, _("CJK Compatibility") },
  { 0x3400, _("CJK Unified Ideographs Extension A") },
  { 0x4DB6, _("Unassigned") },
  { 0x4DC0, _("Yijing Hexagram Symbols") },
  { 0x4E00, _("CJK Unified Ideographs") },
  { 0x9FD6, _("Unassigned") },
  { 0xA000, _("Yi Syllables") },
  { 0xA48D, _("Unassigned") },
  { 0xA490, _("Yi Radicals") },
  { 0xA4C7, _("Unassigned") },
  { 0xA4D0, _("Lisu") },
  { 0xA500, _("Vai") },
  { 0xA62C, _("Unassigned") },
  { 0xA640, _("Cyrillic Extended-B") },
  { 0xA6A0, _("Bamum") },
  { 0xA700, _("Modifier Tone Letters") },
  { 0xA720, _("Latin Extended-D") },
  { 0xA800, _("Syloti Nagri") },
  { 0xA82C, _("Unassigned") },
  { 0xA830, _("Common Indic Number Forms") },
  { 0xA83A, _("Unassigned") },
  { 0xA840, _("Phags-pa") },
  { 0xA878, _("Unassigned") },
  { 0xA880, _("Saurashtra") },
  { 0xA8DA, _("Unassigned") },
  { 0xA8E0, _("Devanagari Extended") },
  { 0xA8FE, _("Unassigned") },
  { 0xA900, _("Kayah Li") },
  { 0xA930, _("Rejang") },
  { 0xA960, _("Hangul Jamo Extended-A") },
  { 0xA97D, _("Unassigned") },
  { 0xA980, _("Javanese") },
  { 0xA9E0, _("Myanmar Extended-B") },
  { 0xAA00, _("Cham") },
  { 0xAA60, _("Myanmar Extended-A") },
  { 0xAA80, _("Tai Viet") },
  { 0xAAE0, _("Meetei Mayek Extensions") },
  { 0xAB00, _("Ethiopic Extended-A") },
  { 0xAB30, _("Latin Extended-E") },
  { 0xAB66, _("Unassigned") },
  { 0xAB70, _("Cherokee Supplement") },
  { 0xABC0, _("Meetei Mayek") },
  { 0xABFA, _("Unassigned") },
  { 0xAC00, _("Hangul Syllables") },
  { 0xD7B0, _("Hangul Jamo Extended-B") },
  { 0xD7FC, _("Unassigned") },
/*{ 0xD800, _("High Surrogates") }, */  /* UTF-16 compatibility */
/*{ 0xDC00, _("Low Surrogates") },  */  /* UTF-16 compatibility */
  { 0xE000, _("Private Use Area") },
  { 0xF900, _("CJK Compatibility Ideographs") },
  { 0xFAEA, _("Unassigned") },
  { 0xFB00, _("Alphabetic Presentation Forms") },
  { 0xFB50, _("Arabic Presentation Forms-A") },
  { 0xFE00, _("Variation Selectors") },
  { 0xFE10, _("Vertical Forms") },
  { 0xFE1A, _("Unassigned") },
  { 0xFE20, _("Combining Half Marks") },
  { 0xFE30, _("CJK Compatibility Forms") },
  { 0xFE50, _("Small Form Variants") },
  { 0xFE6C, _("Unassigned") },
  { 0xFE70, _("Arabic Presentation Forms-B") },
  { 0xFF00, _("Halfwidth and Fullwidth Forms") },
  { 0xFFF0, _("Unassigned") },
  { 0xFFF9, _("Specials") },
  { 0xFFFE, _("Unassigned") },

  { 0x10000, "*" _("Supplementary Multilingual") },
  { 0x10000, _("Linear B Syllabary") },
  { 0x1007E, _("Unassigned") },
  { 0x10080, _("Linear B Ideograms") },
  { 0x100FB, _("Unassigned") },
  { 0x10100, _("Aegean Numbers") },
  { 0x10140, _("Ancient Greek Numbers") },
  { 0x1018D, _("Unassigned") },
  { 0x10190, _("Ancient Symbols") },
  { 0x1019C, _("Unassigned") },
  { 0x101D0, _("Phaistos Disc") },
  { 0x10200, _("Unassigned") },
  { 0x10280, _("Lycian") },
  { 0x1029D, _("Unassigned") },
  { 0x102A0, _("Carian") },
  { 0x102D1, _("Unassigned") },
  { 0x102E0, _("Coptic Epact Numbers") },
  { 0x102FC, _("Unassigned") },
  { 0x10300, _("Old Italic") },
  { 0x10324, _("Unassigned") },
  { 0x10330, _("Gothic") },
  { 0x1034B, _("Unassigned") },
  { 0x10350, _("Old Permic") },
  { 0x10380, _("Ugaritic") },
  { 0x103A0, _("Old Persian") },
  { 0x103D6, _("Unassigned") },
  { 0x103E0, _("Unassigned") },
  { 0x10400, _("Deseret") },
  { 0x10450, _("Shavian") },
  { 0x10480, _("Osmanya") },
  { 0x104AA, _("Unassigned") },
  { 0x104B0, _("Unassigned") },
  { 0x10500, _("Elbasan") },
  { 0x10528, _("Unassigned") },
  { 0x10530, _("Caucasian Albanian") },
  { 0x10570, _("Unassigned") },
  { 0x10600, _("Linear A") },
  { 0x10768, _("Unassigned") },
  { 0x10780, _("Unassigned") },
  { 0x10800, _("Cypriot Syllabary") },
  { 0x10840, _("Imperial Aramaic") },
  { 0x10860, _("Palmyrene") },
  { 0x10880, _("Nabataean") },
  { 0x108B0, _("Unassigned") },
  { 0x108E0, _("Hatran") },
  { 0x10900, _("Phoenician") },
  { 0x10920, _("Lydian") },
  { 0x10940, _("Unassigned") },
  { 0x10980, _("Meroitic Hieroglyphs") },
  { 0x109A0, _("Meroitic Cursive") },
  { 0x10A00, _("Kharoshthi") },
  { 0x10A59, _("Unassigned") },
  { 0x10A60, _("Old South Arabian") },
  { 0x10A80, _("Old North Arabian") },
  { 0x10AA0, _("Unassigned") },
  { 0x10AC0, _("Manichaean") },
  { 0x10AF7, _("Unassigned") },
  { 0x10B00, _("Avestan") },
  { 0x10B40, _("Inscriptional Parthian") },
  { 0x10B60, _("Inscriptional Pahlavi") },
  { 0x10B80, _("Psalter Pahlavi") },
  { 0x10BB0, _("Unassigned") },
  { 0x10C00, _("Old Turkic") },
  { 0x10C49, _("Unassigned") },
  { 0x10C50, _("Unassigned") },
  { 0x10C80, _("Old Hungarian") },
  { 0x10D00, _("Unassigned") },
  { 0x10E60, _("Rumi Numeral Symbols") },
  { 0x10E80, _("Unassigned") },
  { 0x11000, _("Brahmi") },
  { 0x11070, _("Unassigned") },
  { 0x11080, _("Kaithi") },
  { 0x110C2, _("Unassigned") },
  { 0x110D0, _("Sora Sompeng") },
  { 0x110FA, _("Unassigned") },
  { 0x11100, _("Chakma") },
  { 0x11144, _("Unassigned") },
  { 0x11150, _("Mahajani") },
  { 0x11177, _("Unassigned") },
  { 0x11180, _("Sharada") },
  { 0x111E0, _("Sinhala Archaic Numbers") },
  { 0x111F5, _("Unassigned") },
  { 0x11200, _("Khojki") },
  { 0x1123E, _("Unassigned") },
  { 0x11250, _("Unassigned") },
  { 0x11280, _("Multani") },
  { 0x112AA, _("Unassigned") },
  { 0x112B0, _("Khudawadi") },
  { 0x112FA, _("Unassigned") },
  { 0x11300, _("Grantha") },
  { 0x11375, _("Unassigned") },
  { 0x11380, _("Unassigned") },
  { 0x11480, _("Tirhuta") },
  { 0x114DA, _("Unassigned") },
  { 0x114E0, _("Unassigned") },
  { 0x11580, _("Siddham") },
  { 0x115DE, _("Unassigned") },
  { 0x11600, _("Modi") },
  { 0x1165A, _("Unassigned") },
  { 0x11660, _("Unassigned") },
  { 0x11680, _("Takri") },
  { 0x116CA, _("Unassigned") },
  { 0x116D0, _("Unassigned") },
  { 0x11700, _("Ahom") },
  { 0x11740, _("Unassigned") },
  { 0x118A0, _("Warang Citi") },
  { 0x11900, _("Unassigned") },
  { 0x11AC0, _("Pau Cin Hau") },
  { 0x11AF9, _("Unassigned") },
  { 0x11B00, _("Unassigned") },
  { 0x12000, _("Cuneiform") },
  { 0x1239A, _("Unassigned") },
  { 0x12400, _("Cuneiform Numbers and Punctuation") },
  { 0x12475, _("Unassigned") },
  { 0x12480, _("Early Dynastic Cuneiform") },
  { 0x12544, _("Unassigned") },
  { 0x12550, _("Unassigned") },
  { 0x13000, _("Egyptian Hieroglyphs") },
  { 0x13430, _("Unassigned") },
  { 0x14400, _("Anatolian Hieroglyphs") },
  { 0x14647, _("Unassigned") },
  { 0x14680, _("Unassigned") },
  { 0x16800, _("Bamum Supplement") },
  { 0x16A39, _("Unassigned") },
  { 0x16A40, _("Mro") },
  { 0x16A70, _("Unassigned") },
  { 0x16AD0, _("Bassa Vah") },
  { 0x16AF6, _("Unassigned") },
  { 0x16B00, _("Pahawh Hmong") },
  { 0x16B90, _("Unassigned") },
  { 0x16F00, _("Miao") },
  { 0x16FA0, _("Unassigned") },
  { 0x1B000, _("Kana Supplement") },
  { 0x1B002, _("Unassigned") },
  { 0x1B100, _("Unassigned") },
  { 0x1BC00, _("Duployan Shorthand") },
  { 0x1BCA4, _("Unassigned") },
  { 0x1BCB0, _("Unassigned") },
  { 0x1D000, _("Byzantine Musical Symbols") },
  { 0x1D0F5, _("Unassigned") },
  { 0x1D100, _("Musical Symbols") },
  { 0x1D1E9, _("Unassigned") },
  { 0x1D200, _("Ancient Greek Musical Notation") },
  { 0x1D246, _("Unassigned") },
  { 0x1D250, _("Unassigned") },
  { 0x1D300, _("Tai Xuan Jing Symbols") },
  { 0x1D357, _("Unassigned") },
  { 0x1D360, _("Counting Rod Numerals") },
  { 0x1D372, _("Unassigned") },
  { 0x1D380, _("Unassigned") },
  { 0x1D400, _("Mathematical Alphanumeric Symbols") },
  { 0x1D800, _("Sutton SignWriting") },
  { 0x1DAB0, _("Unassigned") },
  { 0x1E800, _("Mende Kikakui") },
  { 0X1E8D7, _("Unassigned") },
  { 0x1E8E0, _("Unassigned") },
  { 0x1EE00, _("Arabic Mathematical Alphabetic Symbols") },
  { 0X1EEFC, _("Unassigned") },
  { 0x1EF00, _("Unassigned") },
  { 0x1F000, _("Mahjong Tiles") },
  { 0x1F02C, _("Unassigned") },
  { 0x1F030, _("Domino Tiles") },
  { 0x1F094, _("Unassigned") },
  { 0x1F0A0, _("Playing Cards") },
  { 0x1F0F6, _("Unassigned") },
  { 0x1F100, _("Enclosed Alphanumeric Supplement") },
  { 0x1F19B, _("Unassigned") },
  { 0x1F1E6, _("Enclosed Alphanumeric Supplement") },
  { 0x1F200, _("Enclosed Ideographic Supplement") },
  { 0x1F252, _("Unassigned") },
  { 0x1F300, _("Miscellaneous Symbols and Pictographs") },
  { 0x1F600, _("Emoticons") },
  { 0x1F650, _("Ornamental Dingbats") },
  { 0x1F680, _("Transport and Map Symbols") },
  { 0x1F6F4, _("Unassigned") },
  { 0x1F700, _("Alchemical Symbols") },
  { 0x1F774, _("Unassigned") },
  { 0x1F780, _("Geometric Shapes Extended") },
  { 0x1F7D5, _("Unassigned") },
  { 0x1F800, _("Supplemental Arrows-C") },
  { 0x1F8AD, _("Unassigned") },
  { 0x1F910, _("Supplemental Symbols and Pictographs") },
  { 0x1F9C1, _("Unassigned") },
  { 0x1FA00, _("Unassigned") },

  { 0x20000, "*" _("Supplementary Ideographic") },
  { 0x20000, _("CJK Unified Ideographs Extension B") },
  { 0x2A6D7, _("Unassigned") },
  { 0x2A6E0, _("Unassigned") },
  { 0x2A700, _("CJK Unified Ideographs Extension C") },
  { 0x2B740, _("Unassigned") },
  { 0x2B740, _("CJK Unified Ideographs Extension D") },
  { 0x2B820, _("Unassigned") },
  { 0x2B820, _("CJK Unified Ideographs Extension E") },
  { 0x2CEA2, _("Unassigned") },
  { 0x2CEB0, _("Unassigned") },
  { 0x2F800, _("CJK Compatibility Ideographs Supplement") },
  { 0x2FA20, _("Unassigned") },

  { 0x30000, "*" _("Unassigned Plane 3") },
  { 0x40000, "*" _("Unassigned Plane 4") },
  { 0x50000, "*" _("Unassigned Plane 5") },
  { 0x60000, "*" _("Unassigned Plane 6") },
  { 0x70000, "*" _("Unassigned Plane 7") },
  { 0x80000, "*" _("Unassigned Plane 8") },
  { 0x90000, "*" _("Unassigned Plane 9") },
  { 0xA0000, "*" _("Unassigned Plane 10") },
  { 0xB0000, "*" _("Unassigned Plane 11") },
  { 0xC0000, "*" _("Unassigned Plane 12") },
  { 0xD0000, "*" _("Unassigned Plane 13") },

  { 0xE0000, "*" _("Supplementary Special-Purpose") },
  { 0xE0080, _("Unassigned") },
  { 0xE0020, _("Tags") },
  { 0xE0080, _("Unassigned") },
  { 0xE0100, _("Variation Selectors Supplement") },
  { 0xE01F0, _("Unassigned") },

  { 0xF0000, _( "*Supplementary Private Use Area A") },
  { 0x100000, "*" _("Supplementary Private Use Area B") },
};

#ifdef ENABLE_NLS	/* Now put it back */
# undef _
# define _(S) gettext(S)
#endif


static char *
strip (char *s)
{
  unsigned long L;
  while (*s == ' ' || *s == '\t' || *s == '\n')
    s++;
  L = strlen (s);
  while (s[L-1] == ' ' || s[L-1] == '\t' || s[L-1] == '\n')
    s[--L] = 0;
  return s;
}


/* matches ("AA, BB, CC", "CC")  => True
   matches ("AA, BB, CC", "CCx") => False
 */
static Bool
matches (const char *pattern, const char *string)
{
  char *token = strdup (pattern ? pattern : "");
  char *otoken = token;
  char *name;
  Bool match = False;
  while ((name = strtok (token, ","))) {
    token = 0;
    name = strip (name);
    if (*name && !strcasecmp (name, string))
      {
        match = True;
        break;
      }
  }

  free (otoken);
  return match;
}


#ifndef HAVE_JWXYZ
static void
capitalize (char *s)
{
  int brk = 1;
  for (; *s; s++)
    {
      if (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        brk = 1;
      else if (brk)
        brk = 0;
      else if (*s >= 'A' && *s <= 'Z')
        *s += 'a' - 'A';
    }
}
#endif /* HAVE_JWXYZ */


static const char *
unicrud_charname (texture_font_data *font, unsigned long unichar)
{
# ifdef HAVE_JWXYZ
  /* macOS, iOS and Android have APIs for this. */
  return texfont_unicode_character_name (font, unichar);

# else
  /* X11 has so many to choose from!
     Options include:

     1: perl -e 'use charnames (); print charnames::viacode(0x1234);'
     2: grep "^01234\t" /opt/local/lib/perl5/\*\/unicore/Name.pl
     3: python3 -c 'import unicodedata; print (unicodedata.name("\u1234"))'
     4: Hardcode all the names into this hack directly.
        https://www.unicode.org/Public/UCD/latest/ucd/NamesList.txt is 1.6 MB.
        Stripped down to just names, it would be around 943 KB and 30K lines.
        We could embed that here and just binary-search the data.
   */
  FILE *pipe;
  char cmd[255], *s;
  static char ret[255];
  int L;
  sprintf (cmd,
           "perl -e 'use charnames (); print charnames::viacode (%lu);' 2>&-",
           unichar);
  pipe = popen (cmd, "r");
  s = fgets (ret, sizeof(ret)-1, pipe);
  pclose (pipe);
  if (!s || !*s) return 0;
  L = strlen(s);
  while (L > 0 && (s[L-1] == '\r' || s[L-1] == '\n'))
    s[--L] = 0;
  if (L <= 0) return 0;
  capitalize (s);
  return s;
# endif /* X11 */
}


static void
pick_unichar (ModeInfo *mi)
{
  unicrud_configuration *bp = &bps[MI_SCREEN(mi)];
  int i;
  unsigned long min = 0;
  unsigned long max = 0x2F800;
  unsigned long last = 0;
  int retries = 0;
  time_t start_time = time ((time_t *) 0);

 AGAIN:
  bp->unichar = min + (random() % (max - min));

  if (++retries > 0xF0000 / 2)
    {
      if (do_block && *do_block && !!strcasecmp (do_block, "all"))
        fprintf (stderr, "%s: no characters found in blocks \"%s\"\n",
                 progname, do_block);
      else
        fprintf (stderr, "%s: internal error: no characters found\n",
                 progname);
      exit (1);
    }

  /* bp->unichar = 0x1F4A9; */

  last = 0;
  bp->charplane = "Unassigned";
  bp->charblock = "Unassigned";
  for (i = 0; i < countof(unicode_block_names); i++)
    {
      if (unicode_block_names[i].start < last)
        {
          fprintf (stderr, "%s: progname: internal error: misordered: 0x%lX\n",
                   progname, unicode_block_names[i].start);
          exit (1);
        }
      last = unicode_block_names[i].start;
      if (bp->unichar >= unicode_block_names[i].start)
        {
          if (unicode_block_names[i].name[0] == '*')
            {
              bp->charplane = unicode_block_names[i].name + 1;
              bp->charblock = "Unassigned";
            }
          else
            bp->charblock = unicode_block_names[i].name;
        }
      else
        break;
    }

  if (!strncmp (bp->charblock, "Unassigned", 10) ||
      !strncmp (bp->charblock, "Combining", 9))
    goto AGAIN;

  if (*do_block && !matches (do_block, bp->charblock))
    goto AGAIN;

  /* Skip blank characters */
  {
    XCharStruct e;
    char text[10];
    i = utf8_encode (bp->unichar, text, sizeof(text) - 1);
    text[i] = 0;
    texture_string_metrics (bp->char_font, text, &e, 0, 0);

    if (e.width < 2 ||
        e.ascent + e.descent < 2 ||
        blank_character_p (bp->char_font, text))
      {
        time_t now = time ((time_t *) 0);
        if (now < start_time + 5) 	/* Might be a *very* bad font... */
          goto AGAIN;
      }
  }

  bp->charname = unicrud_charname (bp->char_font, bp->unichar);

  bp->color[0] = 0.5 + frand(0.5);
  bp->color[1] = 0.5 + frand(0.5);
  bp->color[2] = 0.5 + frand(0.5);
  bp->color[3] = 1;
}


static void
draw_unichar (ModeInfo *mi)
{
  unicrud_configuration *bp = &bps[MI_SCREEN(mi)];

  char text[10];
  char title[400];
  XCharStruct e;
  int w, h, i, j;
  GLfloat color[4] = {1, 1, 0, 1};
  GLfloat s;

  i = utf8_encode (bp->unichar, text, sizeof(text) - 1);
  text[i] = 0;

  *title = 0;
  sprintf (title + strlen(title), _("Plane:\t%s\n"), gettext (bp->charplane));
  sprintf (title + strlen(title), _("Block:\t%s\n"), gettext (bp->charblock));
  sprintf (title + strlen(title), _("Name:\t%s\n"),
           (bp->charname ? gettext (bp->charname) : ""));
  sprintf (title + strlen(title), _("Unicode:\t%04lX\n"), bp->unichar);
  sprintf (title + strlen(title), _("UTF-8:\t"));
  for (j = 0; j < i; j++)
    sprintf (title + strlen(title), "%02X ", ((unsigned char *)text)[j]);

  texture_string_metrics (bp->char_font, text, &e, 0, 0);
  w = e.width;
  h = e.ascent;

  s = 9;
  glScalef (s, s, s);
  
  s = 1.0 / (h > w ? h : w);	/* Scale to unit */
  glScalef (s, s, s);

  glTranslatef (-w/2, -h/2, 0);
  glColor4fv (bp->color);
  print_texture_string (bp->char_font, text);

  if (do_titles)
    print_texture_label (mi->dpy, bp->title_font,
                         mi->xgwa.width, mi->xgwa.height,
                         1, title, color);
}


/* Window management, etc
 */
ENTRYPOINT void
reshape_unicrud (ModeInfo *mi, int width, int height)
{
  GLfloat h = (GLfloat) height / (GLfloat) width;
  int y = 0;

  if (width > height * 5) {   /* tiny window: show middle */
    height = width * 9/16;
    y = -height/2;
    h = height / (GLfloat) width;
  }

  glViewport (0, y, (GLint) width, (GLint) height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective (30.0, 1/h, 1.0, 100.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt( 0.0, 0.0, 30.0,
             0.0, 0.0, 0.0,
             0.0, 1.0, 0.0);

  {
    GLfloat s = (MI_WIDTH(mi) < MI_HEIGHT(mi)
                 ? (MI_WIDTH(mi) / (GLfloat) MI_HEIGHT(mi))
                 : 1);
    glScalef (s, s, s);
  }

  glClear(GL_COLOR_BUFFER_BIT);
}


ENTRYPOINT Bool
unicrud_handle_event (ModeInfo *mi, XEvent *event)
{
  unicrud_configuration *bp = &bps[MI_SCREEN(mi)];

  if (gltrackball_event_handler (event, bp->trackball,
                                 MI_WIDTH (mi), MI_HEIGHT (mi),
                                 &bp->button_down_p))
    return True;
  else if (event->xany.type == KeyPress)
    {
      KeySym keysym;
      char c = 0;
      XLookupString (&event->xkey, &c, 1, &keysym, 0);
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
          if (bp->state == LINGER) bp->ratio = 1;
          return True;
        }
    }

  return False;
}


ENTRYPOINT void 
init_unicrud (ModeInfo *mi)
{
  unicrud_configuration *bp;

  MI_INIT (mi, bps);

  bp = &bps[MI_SCREEN(mi)];

  bp->glx_context = init_GL(mi);

  reshape_unicrud (mi, MI_WIDTH(mi), MI_HEIGHT(mi));

  {
    double spin_speed   = 0.05;
    double wander_speed = 0.01;
    double spin_accel   = 1.0;

    bp->rot = make_rotator (do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            do_spin ? spin_speed : 0,
                            spin_accel,
                            do_wander ? wander_speed : 0,
                            False);
    bp->trackball = gltrackball_init (True);
  }

  bp->title_font = load_texture_font (mi->dpy, "titleFont");
  bp->char_font  = load_texture_font (mi->dpy, "font");
  bp->state = IN;
  bp->ratio = 0;
  bp->spin_direction = (random() & 1) ? 1 : -1;



  if (matches ("all", do_block))
    do_block = strdup("");

  {
    char *s;
    for (s = do_block; *s; s++)
      if (*s == '_') *s = ' ';
  }

  if (matches ("help", do_block))
    {
      int i;
      fprintf (stderr,
               _("%s: --blocks must contain one or more of these,"
                 " separated by commas:\n\n"),
               progname);
      for (i = 0; i < countof(unicode_block_names); i++)
        {
          const char *n = unicode_block_names[i].name;
          if (*n == '*')
            continue;
          if (!strncmp (n, "Unassigned", 10) ||
              !strncmp (n, "Combining", 9))
            continue;
          fprintf (stderr, "\t%s\n", gettext (n));
        }
      fprintf (stderr, "\n");
      exit (1);
    }


  /* Make sure all elements in --block are valid.
   */
  if (*do_block)
    {
      char *token = strdup (do_block ? do_block : "");
      char *otoken = token;
      char *name;
      while ((name = strtok (token, ","))) {
        token = 0;
        name = strip (name);
        if (*name)
          {
            Bool match = False;
            int i;
            for (i = 0; i < countof(unicode_block_names); i++)
              {
                const char *n = unicode_block_names[i].name;
                if (*n == '*')
                  continue;
                if (!strncmp (n, "Unassigned", 10) ||
                    !strncmp (n, "Combining", 9))
                  continue;
                if (!strcasecmp (name, n) ||
                    !strcasecmp (name, gettext (n)))
                  {
                    match = True;
                    break;
                  }
              }
            if (! match)
              {
                fprintf (stderr, _("%s: unknown block name: \"%s\"\n"), 
                         progname, name);
                fprintf (stderr, _("%s: use '--block help' for a list\n"), 
                         progname);
                exit (1);
              }
          }
      }
      free (otoken);
    }

  pick_unichar (mi);
}


ENTRYPOINT void
draw_unicrud (ModeInfo *mi)
{
  unicrud_configuration *bp = &bps[MI_SCREEN(mi)];
  Display *dpy = MI_DISPLAY(mi);
  Window window = MI_WINDOW(mi);
  static const float fps_color[4] = {1, 1, 0, 1};

  if (!bp->glx_context)
    return;

  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);

  glShadeModel (GL_FLAT);
  glEnable (GL_NORMALIZE);
  glDisable (GL_CULL_FACE);
  glDisable (GL_LIGHTING);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (! bp->button_down_p)
    switch (bp->state) {
    case IN:     bp->ratio += speed * 0.05;  break;
    case OUT:    bp->ratio += speed * 0.05;  break;
    case LINGER: bp->ratio += speed * 0.005; break;
    default:     abort();
    }

  if (bp->ratio > 1.0)
    {
      bp->ratio = 0;
      switch (bp->state) {
      case IN:
        bp->state = LINGER;
        break;
      case LINGER:
        bp->state = OUT;
        bp->spin_direction = (random() & 1) ? 1 : -1;
        break;
      case OUT:
        bp->state = IN;
        pick_unichar(mi);
        break;
      default:     abort();
      }
    }

  glPushMatrix ();

  {
    double x, y, z;
    get_position (bp->rot, &x, &y, &z, !bp->button_down_p);
    glTranslatef((x - 0.5) * 6,
                 (y - 0.5) * 6,
                 (z - 0.5) * 6);

    gltrackball_rotate (bp->trackball);

    get_rotation (bp->rot, &x, &y, &z, !bp->button_down_p);
    x = y = 0;
    glRotatef (x * 360, 1.0, 0.0, 0.0);
    glRotatef (y * 360, 0.0, 1.0, 0.0);
    glRotatef (z * 360, 0.0, 0.0, 1.0);
  }

# define SINOID(N) (sin(M_PI - (N) / 2 * M_PI))
  {
    GLfloat s;
    switch (bp->state) {
    case IN:  s = SINOID (bp->ratio);   break;
    case OUT: s = SINOID (1-bp->ratio); break;
    default:  s = 1; break;
    }
    glScalef (s, s, s);
    glRotatef (360 * s * bp->spin_direction * (bp->state == IN ? -1 : 1),
               0, 0, 1);
  }

  draw_unichar (mi);

  glPopMatrix ();

  if (mi->fps_p) do_fps_color (mi, fps_color);
  glFinish();

  glXSwapBuffers(dpy, window);
}


ENTRYPOINT void
free_unicrud (ModeInfo *mi)
{
  unicrud_configuration *bp = &bps[MI_SCREEN(mi)];
  if (!bp->glx_context) return;
  glXMakeCurrent(MI_DISPLAY(mi), MI_WINDOW(mi), *bp->glx_context);
  if (bp->trackball) gltrackball_free (bp->trackball);
  if (bp->rot) free_rotator (bp->rot);
  if (bp->title_font) free_texture_font (bp->title_font);
  if (bp->char_font) free_texture_font (bp->char_font);
}

XSCREENSAVER_MODULE_2 ("Unicrud", unicrud, unicrud)

#endif /* USE_GL */
