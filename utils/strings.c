/*
 *	STRINGS.C	(C adaptation of STRINGS.MAR)
 */
#include <string.h>

/*
 * bcopy()
 *
 * copies length of source to destination
 */
void bcopy (src, dest, length)
char *src, *dest;
unsigned long int length;
{
  memcpy(dest, src, length);
}

/*
 * bcmp()
 *
 * compare src againist destination for length bytes.
 *
 * return -1 if src < dest, 0 if src = dest, 1 if src > dest
 */
int bcmp(src, dest, length)
char *src, *dest;
unsigned long int length;
{
   return((int)memcmp(src, dest, length));
}

/*
 * bzero()
 *
 * zero out source for length bytes
 */
void bzero(src, length)
char *src;
unsigned long int length;
{
   memset(src, 0, length);
}

/*
 * ffs()
 *
 * finds the first bit set in the argument and returns the index
 * of that bit (starting at 1).
 */
int ffs(mask)
unsigned long int mask;
{
   register int i;

   for (i = 0; i < sizeof(mask); i++)
	if (mask & (1 << i)) return (i+1);

   return(-1);
}

/*
 * index()
 *
 * returns a pointer to the first occurrence of a character within
 * a string
 */

char *index(string, c)
char *string, c;
{
   return(strchr(string,c));
}

/*
 * rindex
 *
 * returns a pointer to the last occurrence of a character within
 * a string
 */

char *rindex(string, c)
char *string, c;
{
   return (strrchr(string,c));
}
