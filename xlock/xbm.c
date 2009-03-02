#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xbm.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * Utilities for XBM processing
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * 25-May-95: David Bagley "snarfed" xv's xvxbm.c
 *            John Bradley <bradley@central.cis.upenn.edu>
 *            code used here by permission
 */
#include "xlock.h"

int
XbmReadFileToImage(char *filename,
		   int *width, int *height, unsigned char **bits)
{
	FILE       *file;
	int         c, c1;
	int         i, j, k = 0;
	unsigned char *pix;
	char        line[256], name[256];
	unsigned char hex[256];

	if ((file = my_fopen(filename, "r")) == NULL) {
		/*(void) fprintf(stderr, "could not read file \"%s\"\n", filename); */
		return BitmapOpenFailed;
	}
	/* read width:  skip lines until we hit a #define */
	for (;;) {
		if (!fgets(line, 256, file)) {
			/* not a xbm file */
			(void) fclose(file);
			return BitmapFileInvalid;
		}
		if (strncmp(line, "#define", (size_t) 7) == 0 &&
		    sscanf(line, "#define %s %d", name, width) == 2 &&
		    strcmp(name, "_width"))
			break;
	}

	/* read height:  skip lines until we hit another #define */
	for (;;) {
		if (!fgets(line, 256, file)) {
			(void) fclose(file);
			(void) fprintf(stderr, "EOF reached in header info.\n");
			return BitmapFileInvalid;
		}
		if (strncmp(line, "#define", (size_t) 7) == 0 &&
		    sscanf(line, "#define %s %d", name, height) == 2 &&
		    strcmp(name, "_height"))
			break;
	}
	/* scan forward until we see the first '0x' */
	c = getc(file);
	c1 = getc(file);
	while (c1 != EOF && !(c == '0' && c1 == 'x')) {
		c = c1;
		c1 = getc(file);
	}
	if (c1 == EOF) {
		(void) fclose(file);
		(void) fprintf(stderr, "No bitmap data found\n");
		return BitmapFileInvalid;
	}
	if (*width < 1 || *height < 1 || *width > 10000 || *height > 10000) {
		(void) fclose(file);
		(void) fprintf(stderr, "Not an xbm file");
		return BitmapFileInvalid;
	}
	*bits = (unsigned char *) calloc((size_t) ((*width + 7) / 8) * (*height),
					 (size_t) 8);
	if (!*bits) {
		(void) fclose(file);
		(void) fprintf(stderr, "couldn't malloc bits\n");
		return BitmapNoMemory;
	}
	/* initialize the 'hex' array for zippy ASCII-hex -> int conversion */

	for (i = 0; i < 256; i++)
		hex[i] = 255;	/* flag 'undefined' chars */
	for (i = '0'; i <= '9'; i++)
		hex[i] = i - '0';
	for (i = 'a'; i <= 'f'; i++)
		hex[i] = i + 10 - 'a';
	for (i = 'A'; i <= 'F'; i++)
		hex[i] = i + 10 - 'A';

	/* read the image data */

	for (i = 0, pix = *bits; i < *height; i++)
		for (j = 0; j < (*width + 7) / 8; j++, pix++) {
			/* get next byte from file.  we're already positioned at it */
			c = getc(file);
			c1 = getc(file);
			if (c < 0 || c1 < 0) {
				/* EOF: break out of loop */
				c = c1 = '0';
				i = *height;
				j = *width;
				(void) fclose(file);
				(void) fprintf(stderr, "The file would appear to be truncated.\n");
				return BitmapFileInvalid;
			}
			if (hex[c1] == 255) {
				if (hex[c] == 255)
					k = 0;	/* no digits after the '0x' ... */
				else
					k = hex[c];
			} else
				k = (hex[c] << 4) + hex[c1];

			/* advance to next '0x' */
			c = getc(file);
			c1 = getc(file);
			while (c1 != EOF && !(c == '0' && c1 == 'x')) {
				c = c1;
				c1 = getc(file);
			}
			*pix = k;
		}
	(void) fclose(file);
	return BitmapSuccess;
}
