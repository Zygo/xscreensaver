/* unicrud, Copyright © 2016-2022 Jamie Zawinski <jwz@jwz.org>
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

static const struct {
  unsigned long start;
  const char * const name;
} unicode_block_names[] = {

  /* Through Unicode 8.0, early 2016. */

  { 0x0000, "*Basic Multilingual" },
  { 0x0000, "Unassigned" },
  { 0x0021, "ASCII" },
  { 0x0080, "Unassigned" },
  { 0x00A1, "Latin1" },
  { 0x0100, "Latin Extended-A" },
  { 0x0180, "Latin Extended-B" },
  { 0x0250, "IPA Extensions" },
  { 0x02B0, "Spacing Modifier Letters" },
  { 0x0300, "Combining Diacritical Marks" },
  { 0x0370, "Greek and Coptic" },
  { 0x0400, "Cyrillic" },
  { 0x0500, "Cyrillic Supplement" },
  { 0x0530, "Armenian" },
  { 0x058B, "Unassigned" },
  { 0x0590, "Hebrew" },
  { 0x05F5, "Unassigned" },
  { 0x0600, "Arabic" },
  { 0x0700, "Syriac" },
  { 0x0750, "Arabic Supplement" },
  { 0x0780, "Thaana" },
  { 0x07C0, "N'Ko" },
  { 0x0800, "Samaritan" },
  { 0x0840, "Mandaic" },
  { 0x0860, "Unassigned" },
  { 0x08A0, "Arabic Extended-A" },
  { 0x0900, "Devanagari" },
  { 0x0980, "Bengali" },
  { 0x09FC, "Unassigned" },
  { 0x0A00, "Gurmukhi" },
  { 0x0A76, "Unassigned" },
  { 0x0A80, "Gujarati" },
  { 0x0AF2, "Unassigned" },
  { 0x0B00, "Oriya" },
  { 0x0B80, "Tamil" },
  { 0x0BFB, "Unassigned" },
  { 0x0C00, "Telugu" },
  { 0x0C80, "Kannada" },
  { 0x0CF0, "Unassigned" },
  { 0x0D00, "Malayalam" },
  { 0x0D80, "Sinhala" },
  { 0x0DF5, "Unassigned" },
  { 0x0E00, "Thai" },
  { 0x0E5C, "Unassigned" },
  { 0x0E80, "Lao" },
  { 0x0EE0, "Unassigned" },
  { 0x0F00, "Tibetan" },
  { 0x0FDB, "Unassigned" },
  { 0x1000, "Myanmar" },
  { 0x10A0, "Georgian" },
  { 0x1100, "Hangul Jamo" },
  { 0x1200, "Ethiopic" },
  { 0x1380, "Ethiopic Supplement" },
  { 0x13A0, "Cherokee" },
  { 0x1400, "Unified Canadian Aboriginal Syllabics" },
  { 0x1677, "Unassigned" },
  { 0x1680, "Ogham" },
  { 0x16A0, "Runic" },
  { 0x16F1, "Unassigned" },
  { 0x1700, "Tagalog" },
  { 0x1715, "Unassigned" },
  { 0x1720, "Hanunoo" },
  { 0x1737, "Unassigned" },
  { 0x1740, "Buhid" },
  { 0x1754, "Unassigned" },
  { 0x1760, "Tagbanwa" },
  { 0x1774, "Unassigned" },
  { 0x1780, "Khmer" },
  { 0x17FA, "Unassigned" },
  { 0x1800, "Mongolian" },
  { 0x18AB, "Unassigned" },
  { 0x18B0, "Unified Canadian Aboriginal Syllabics Extended" },
  { 0x18F6, "Unassigned" },
  { 0x1900, "Limbu" },
  { 0x1950, "Tai Le" },
  { 0x1975, "Unassigned" },
  { 0x1980, "Tai Lue" },
  { 0x19E0, "Khmer Symbols" },
  { 0x1A00, "Buginese" },
  { 0x1A20, "Tai Tham" },
  { 0x1AAE, "Unassigned" },
  { 0x1AB0, "Combining Diacritical Marks Extended" },
  { 0x1ABF, "Unassigned" },
  { 0x1B00, "Balinese" },
  { 0x1B80, "Sundanese" },
  { 0x1BC0, "Batak" },
  { 0x1C00, "Lepcha" },
  { 0x1C50, "Ol Chiki" },
  { 0x1C80, "Unassigned" },
  { 0x1CC0, "Sundanese Supplement" },
  { 0x1CC8, "Unassigned" },
  { 0x1CD0, "Vedic Extensions" },
  { 0x1CFA, "Unassigned" },
  { 0x1D00, "Phonetic Extensions" },
  { 0x1D80, "Phonetic Extensions Supplement" },
  { 0x1DC0, "Combining Diacritical Marks Supplement" },
  { 0x1E00, "Latin Extended Additional" },
  { 0x1F00, "Greek Extended" },
  { 0x2000, "General Punctuation" },
  { 0x2070, "Superscripts and Subscripts" },
  { 0x2095, "Unassigned" },
  { 0x20A0, "Currency Symbols" },
  { 0x20BE, "Unassigned" },
  { 0x20D0, "Combining Diacritical Marks for Symbols" },
  { 0x20F1, "Unassigned" },
  { 0x2100, "Letterlike Symbols" },
  { 0x2150, "Number Forms" },
  { 0x2190, "Arrows" },
  { 0x2200, "Mathematical Operators" },
  { 0x2300, "Miscellaneous Technical" },
  { 0x2400, "Control Pictures" },
  { 0x2437, "Unassigned" },
  { 0x2440, "Optical Character Recognition" },
  { 0x244B, "Unassigned" },
  { 0x2460, "Enclosed Alphanumerics" },
  { 0x2500, "Box Drawing" },
  { 0x2580, "Block Elements" },
  { 0x25A0, "Geometric Shapes" },
  { 0x2600, "Miscellaneous Symbols" },
  { 0x2700, "Dingbats" },
  { 0x27C0, "Miscellaneous Mathematical Symbols-A" },
  { 0x27F0, "Supplemental Arrows-A" },
  { 0x2800, "Braille Patterns" },
  { 0x2900, "Supplemental Arrows-B" },
  { 0x2980, "Miscellaneous Mathematical Symbols-B" },
  { 0x2A00, "Supplemental Mathematical Operators" },
  { 0x2B00, "Miscellaneous Symbols and Arrows" },
  { 0x2B56, "Unassigned" }, /* Unicode 5.1 */
  { 0x2BF0, "Unassigned" },
  { 0x2C00, "Glagolitic" },
  { 0x2C60, "Latin Extended-C" },
  { 0x2C80, "Coptic" },
  { 0x2D00, "Georgian Supplement" },
  { 0x2D26, "Unassigned" },
  { 0x2D30, "Tifinagh" },
  { 0x2D80, "Ethiopic Extended" },
  { 0x2DE0, "Cyrillic Extended-A" },
  { 0x2E00, "Supplemental Punctuation" },
  { 0x2E43, "Unassigned" },
  { 0x2E80, "CJK Radicals Supplement" },
  { 0x2EF5, "Unassigned" },
  { 0x2F00, "Kangxi Radicals" },
  { 0x2FD6, "Unassigned" },
  { 0x2FE0, "Unassigned" },
  { 0x2FF0, "Ideographic Description Characters" },
  { 0x2FFC, "Unassigned" },
  { 0x3000, "CJK Symbols and Punctuation" },
  { 0x3040, "Hiragana" },
  { 0x30A0, "Katakana" },
  { 0x3100, "Bopomofo" },
  { 0x3130, "Hangul Compatibility Jamo" },
  { 0x3190, "Kanbun" },
  { 0x31A0, "Bopomofo Extended" },
  { 0x31BB, "Unassigned" },
  { 0x31C0, "CJK Strokes" },
  { 0x31E4, "Unassigned" },
  { 0x31F0, "Katakana Phonetic Extensions" },
  { 0x3200, "Enclosed CJK Letters and Months" },
  { 0x3300, "CJK Compatibility" },
  { 0x3400, "CJK Unified Ideographs Extension A" },
  { 0x4DB6, "Unassigned" },
  { 0x4DC0, "Yijing Hexagram Symbols" },
  { 0x4E00, "CJK Unified Ideographs" },
  { 0x9FD6, "Unassigned" },
  { 0xA000, "Yi Syllables" },
  { 0xA48D, "Unassigned" },
  { 0xA490, "Yi Radicals" },
  { 0xA4C7, "Unassigned" },
  { 0xA4D0, "Lisu" },
  { 0xA500, "Vai" },
  { 0xA62C, "Unassigned" },
  { 0xA640, "Cyrillic Extended-B" },
  { 0xA6A0, "Bamum" },
  { 0xA700, "Modifier Tone Letters" },
  { 0xA720, "Latin Extended-D" },
  { 0xA800, "Syloti Nagri" },
  { 0xA82C, "Unassigned" },
  { 0xA830, "Common Indic Number Forms" },
  { 0xA83A, "Unassigned" },
  { 0xA840, "Phags-pa" },
  { 0xA878, "Unassigned" },
  { 0xA880, "Saurashtra" },
  { 0xA8DA, "Unassigned" },
  { 0xA8E0, "Devanagari Extended" },
  { 0xA8FE, "Unassigned" },
  { 0xA900, "Kayah Li" },
  { 0xA930, "Rejang" },
  { 0xA960, "Hangul Jamo Extended-A" },
  { 0xA97D, "Unassigned" },
  { 0xA980, "Javanese" },
  { 0xA9E0, "Myanmar Extended-B" },
  { 0xAA00, "Cham" },
  { 0xAA60, "Myanmar Extended-A" },
  { 0xAA80, "Tai Viet" },
  { 0xAAE0, "Meetei Mayek Extensions" },
  { 0xAB00, "Ethiopic Extended-A" },
  { 0xAB30, "Latin Extended-E" },
  { 0xAB66, "Unassigned" },
  { 0xAB70, "Cherokee Supplement" },
  { 0xABC0, "Meetei Mayek" },
  { 0xABFA, "Unassigned" },
  { 0xAC00, "Hangul Syllables" },
  { 0xD7B0, "Hangul Jamo Extended-B" },
  { 0xD7FC, "Unassigned" },
/*{ 0xD800, "High Surrogates" }, */  /* UTF-16 compatibility */
/*{ 0xDC00, "Low Surrogates" },  */  /* UTF-16 compatibility */
  { 0xE000, "Private Use Area" },
  { 0xF900, "CJK Compatibility Ideographs" },
  { 0xFAEA, "Unassigned" },
  { 0xFB00, "Alphabetic Presentation Forms" },
  { 0xFB50, "Arabic Presentation Forms-A" },
  { 0xFE00, "Variation Selectors" },
  { 0xFE10, "Vertical Forms" },
  { 0xFE1A, "Unassigned" },
  { 0xFE20, "Combining Half Marks" },
  { 0xFE30, "CJK Compatibility Forms" },
  { 0xFE50, "Small Form Variants" },
  { 0xFE6C, "Unassigned" },
  { 0xFE70, "Arabic Presentation Forms-B" },
  { 0xFF00, "Halfwidth and Fullwidth Forms" },
  { 0xFFF0, "Unassigned" },
  { 0xFFF9, "Specials" },
  { 0xFFFE, "Unassigned" },

  { 0x10000, "*Supplementary Multilingual" },
  { 0x10000, "Linear B Syllabary" },
  { 0x1007E, "Unassigned" },
  { 0x10080, "Linear B Ideograms" },
  { 0x100FB, "Unassigned" },
  { 0x10100, "Aegean Numbers" },
  { 0x10140, "Ancient Greek Numbers" },
  { 0x1018D, "Unassigned" },
  { 0x10190, "Ancient Symbols" },
  { 0x1019C, "Unassigned" },
  { 0x101D0, "Phaistos Disc" },
  { 0x10200, "Unassigned" },
  { 0x10280, "Lycian" },
  { 0x1029D, "Unassigned" },
  { 0x102A0, "Carian" },
  { 0x102D1, "Unassigned" },
  { 0x102E0, "Coptic Epact Numbers" },
  { 0x102FC, "Unassigned" },
  { 0x10300, "Old Italic" },
  { 0x10324, "Unassigned" },
  { 0x10330, "Gothic" },
  { 0x1034B, "Unassigned" },
  { 0x10350, "Old Permic" },
  { 0x10380, "Ugaritic" },
  { 0x103A0, "Old Persian" },
  { 0x103D6, "Unassigned" },
  { 0x103E0, "Unassigned" },
  { 0x10400, "Deseret" },
  { 0x10450, "Shavian" },
  { 0x10480, "Osmanya" },
  { 0x104AA, "Unassigned" },
  { 0x104B0, "Unassigned" },
  { 0x10500, "Elbasan" },
  { 0x10528, "Unassigned" },
  { 0x10530, "Caucasian Albanian" },
  { 0x10570, "Unassigned" },
  { 0x10600, "Linear A" },
  { 0x10768, "Unassigned" },
  { 0x10780, "Unassigned" },
  { 0x10800, "Cypriot Syllabary" },
  { 0x10840, "Imperial Aramaic" },
  { 0x10860, "Palmyrene" },
  { 0x10880, "Nabataean" },
  { 0x108B0, "Unassigned" },
  { 0x108E0, "Hatran" },
  { 0x10900, "Phoenician" },
  { 0x10920, "Lydian" },
  { 0x10940, "Unassigned" },
  { 0x10980, "Meroitic Hieroglyphs" },
  { 0x109A0, "Meroitic Cursive" },
  { 0x10A00, "Kharoshthi" },
  { 0x10A59, "Unassigned" },
  { 0x10A60, "Old South Arabian" },
  { 0x10A80, "Old North Arabian" },
  { 0x10AA0, "Unassigned" },
  { 0x10AC0, "Manichaean" },
  { 0x10AF7, "Unassigned" },
  { 0x10B00, "Avestan" },
  { 0x10B40, "Inscriptional Parthian" },
  { 0x10B60, "Inscriptional Pahlavi" },
  { 0x10B80, "Psalter Pahlavi" },
  { 0x10BB0, "Unassigned" },
  { 0x10C00, "Old Turkic" },
  { 0x10C49, "Unassigned" },
  { 0x10C50, "Unassigned" },
  { 0x10C80, "Old Hungarian" },
  { 0x10D00, "Unassigned" },
  { 0x10E60, "Rumi Numeral Symbols" },
  { 0x10E80, "Unassigned" },
  { 0x11000, "Brahmi" },
  { 0x11070, "Unassigned" },
  { 0x11080, "Kaithi" },
  { 0x110C2, "Unassigned" },
  { 0x110D0, "Sora Sompeng" },
  { 0x110FA, "Unassigned" },
  { 0x11100, "Chakma" },
  { 0x11144, "Unassigned" },
  { 0x11150, "Mahajani" },
  { 0x11177, "Unassigned" },
  { 0x11180, "Sharada" },
  { 0x111E0, "Sinhala Archaic Numbers" },
  { 0x111F5, "Unassigned" },
  { 0x11200, "Khojki" },
  { 0x1123E, "Unassigned" },
  { 0x11250, "Unassigned" },
  { 0x11280, "Multani" },
  { 0x112AA, "Unassigned" },
  { 0x112B0, "Khudawadi" },
  { 0x112FA, "Unassigned" },
  { 0x11300, "Grantha" },
  { 0x11375, "Unassigned" },
  { 0x11380, "Unassigned" },
  { 0x11480, "Tirhuta" },
  { 0x114DA, "Unassigned" },
  { 0x114E0, "Unassigned" },
  { 0x11580, "Siddham" },
  { 0x115DE, "Unassigned" },
  { 0x11600, "Modi" },
  { 0x1165A, "Unassigned" },
  { 0x11660, "Unassigned" },
  { 0x11680, "Takri" },
  { 0x116CA, "Unassigned" },
  { 0x116D0, "Unassigned" },
  { 0x11700, "Ahom" },
  { 0x11740, "Unassigned" },
  { 0x118A0, "Warang Citi" },
  { 0x11900, "Unassigned" },
  { 0x11AC0, "Pau Cin Hau" },
  { 0x11AF9, "Unassigned" },
  { 0x11B00, "Unassigned" },
  { 0x12000, "Cuneiform" },
  { 0x1239A, "Unassigned" },
  { 0x12400, "Cuneiform Numbers and Punctuation" },
  { 0x12475, "Unassigned" },
  { 0x12480, "Early Dynastic Cuneiform" },
  { 0x12544, "Unassigned" },
  { 0x12550, "Unassigned" },
  { 0x13000, "Egyptian Hieroglyphs" },
  { 0x13430, "Unassigned" },
  { 0x14400, "Anatolian Hieroglyphs" },
  { 0x14647, "Unassigned" },
  { 0x14680, "Unassigned" },
  { 0x16800, "Bamum Supplement" },
  { 0x16A39, "Unassigned" },
  { 0x16A40, "Mro" },
  { 0x16A70, "Unassigned" },
  { 0x16AD0, "Bassa Vah" },
  { 0x16AF6, "Unassigned" },
  { 0x16B00, "Pahawh Hmong" },
  { 0x16B90, "Unassigned" },
  { 0x16F00, "Miao" },
  { 0x16FA0, "Unassigned" },
  { 0x1B000, "Kana Supplement" },
  { 0x1B002, "Unassigned" },
  { 0x1B100, "Unassigned" },
  { 0x1BC00, "Duployan Shorthand" },
  { 0x1BCA4, "Unassigned" },
  { 0x1BCB0, "Unassigned" },
  { 0x1D000, "Byzantine Musical Symbols" },
  { 0x1D0F5, "Unassigned" },
  { 0x1D100, "Musical Symbols" },
  { 0x1D1E9, "Unassigned" },
  { 0x1D200, "Ancient Greek Musical Notation" },
  { 0x1D246, "Unassigned" },
  { 0x1D250, "Unassigned" },
  { 0x1D300, "Tai Xuan Jing Symbols" },
  { 0x1D357, "Unassigned" },
  { 0x1D360, "Counting Rod Numerals" },
  { 0x1D372, "Unassigned" },
  { 0x1D380, "Unassigned" },
  { 0x1D400, "Mathematical Alphanumeric Symbols" },
  { 0x1D800, "Sutton SignWriting" },
  { 0x1DAB0, "Unassigned" },
  { 0x1E800, "Mende Kikakui" },
  { 0X1E8D7, "Unassigned" },
  { 0x1E8E0, "Unassigned" },
  { 0x1EE00, "Arabic Mathematical Alphabetic Symbols" },
  { 0X1EEFC, "Unassigned" },
  { 0x1EF00, "Unassigned" },
  { 0x1F000, "Mahjong Tiles" },
  { 0x1F02C, "Unassigned" },
  { 0x1F030, "Domino Tiles" },
  { 0x1F094, "Unassigned" },
  { 0x1F0A0, "Playing Cards" },
  { 0x1F0F6, "Unassigned" },
  { 0x1F100, "Enclosed Alphanumeric Supplement" },
  { 0x1F19B, "Unassigned" },
  { 0x1F1E6, "Enclosed Alphanumeric Supplement" },
  { 0x1F200, "Enclosed Ideographic Supplement" },
  { 0x1F252, "Unassigned" },
  { 0x1F300, "Miscellaneous Symbols and Pictographs" },
  { 0x1F600, "Emoticons" },
  { 0x1F650, "Ornamental Dingbats" },
  { 0x1F680, "Transport and Map Symbols" },
  { 0x1F6F4, "Unassigned" },
  { 0x1F700, "Alchemical Symbols" },
  { 0x1F774, "Unassigned" },
  { 0x1F780, "Geometric Shapes Extended" },
  { 0x1F7D5, "Unassigned" },
  { 0x1F800, "Supplemental Arrows-C" },
  { 0x1F8AD, "Unassigned" },
  { 0x1F910, "Supplemental Symbols and Pictographs" },
  { 0x1F9C1, "Unassigned" },
  { 0x1FA00, "Unassigned" },

  { 0x20000, "*Supplementary Ideographic" },
  { 0x20000, "CJK Unified Ideographs Extension B" },
  { 0x2A6D7, "Unassigned" },
  { 0x2A6E0, "Unassigned" },
  { 0x2A700, "CJK Unified Ideographs Extension C" },
  { 0x2B740, "Unassigned" },
  { 0x2B740, "CJK Unified Ideographs Extension D" },
  { 0x2B820, "Unassigned" },
  { 0x2B820, "CJK Unified Ideographs Extension E" },
  { 0x2CEA2, "Unassigned" },
  { 0x2CEB0, "Unassigned" },
  { 0x2F800, "CJK Compatibility Ideographs Supplement" },
  { 0x2FA20, "Unassigned" },

  { 0x30000, "*Unassigned Plane 3" },
  { 0x40000, "*Unassigned Plane 4" },
  { 0x50000, "*Unassigned Plane 5" },
  { 0x60000, "*Unassigned Plane 6" },
  { 0x70000, "*Unassigned Plane 7" },
  { 0x80000, "*Unassigned Plane 8" },
  { 0x90000, "*Unassigned Plane 9" },
  { 0xA0000, "*Unassigned Plane 10" },
  { 0xB0000, "*Unassigned Plane 11" },
  { 0xC0000, "*Unassigned Plane 12" },
  { 0xD0000, "*Unassigned Plane 13" },

  { 0xE0000, "*Supplementary Special-Purpose" },
  { 0xE0080, "Unassigned" },
  { 0xE0020, "Tags" },
  { 0xE0080, "Unassigned" },
  { 0xE0100, "Variation Selectors Supplement" },
  { 0xE01F0, "Unassigned" },

  { 0xF0000,  "*Supplementary Private Use Area A" },
  { 0x100000, "*Supplementary Private Use Area B" },
};


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
      fprintf (stderr, "%s: internal error: too many retries\n", progname);
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
  GLfloat s;

  i = utf8_encode (bp->unichar, text, sizeof(text) - 1);
  text[i] = 0;

  *title = 0;
  sprintf (title + strlen(title), "Plane:\t%s\n", bp->charplane);
  sprintf (title + strlen(title), "Block:\t%s\n", bp->charblock);
  sprintf (title + strlen(title), "Name:\t%s\n",
           (bp->charname ? bp->charname : ""));
  sprintf (title + strlen(title), "Unicode:\t%04lX\n", bp->unichar);
  sprintf (title + strlen(title), "UTF-8:\t");
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

  glColor3f (1, 1, 0);
  if (do_titles)
    print_texture_label (mi->dpy, bp->title_font,
                         mi->xgwa.width, mi->xgwa.height,
                         1, title);
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
               "%s: --blocks must contain one or more of these,"
               " separated by commas:\n\n", progname);
      for (i = 0; i < countof(unicode_block_names); i++)
        {
          const char *n = unicode_block_names[i].name;
          if (*n == '*')
            continue;
          if (!strncmp (n, "Unassigned", 10) ||
              !strncmp (n, "Combining", 9))
            continue;
          fprintf (stderr, "\t%s\n", n);
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
                if (!strcasecmp (name, n))
                  {
                    match = True;
                    break;
                  }
              }
            if (! match)
              {
                fprintf (stderr, "%s: unknown block name: \"%s\"\n", 
                         progname, name);
                fprintf (stderr, "%s: use '--block help' for a list\n", 
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

  if (mi->fps_p) do_fps (mi);
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
