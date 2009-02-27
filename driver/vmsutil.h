/*#if defined(__DECC) && !defined(__STRING_LOADED)*/
int bcmp (char *src, char *dest, unsigned long int length);
/*#endif*/
void bcopy(char *src, char *dest, unsigned long int length);
void bzero(char *src, unsigned long int lenght);
#if !defined(__GNUC__) /* these are defined in stdlib.h */
char *index( char *string, char *c);
char *rindex( char *string, char *c);
#endif
void Fatal_Error(char *msg, ...);
long random();
int srandom(unsigned int new_seed);
int unlink(char *filename);
