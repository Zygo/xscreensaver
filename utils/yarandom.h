#undef random
#undef rand
#undef drand48
#undef srandom
#undef srand
#undef srand48

#define random()   ya_random()
#define srandom(i) ya_rand_init(0)

#undef P
#if __STDC__
# define P(x)x
#else
# define P(x)()
#endif

extern unsigned int ya_random P((void));
extern void ya_rand_init P((unsigned int));
