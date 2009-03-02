static char *LockProcs[] =
{
$%LISTLIBSX#ifdef USE_BOMB
"bomb",
"random"
#else
"random"
#endif
};

#define numprocs (sizeof (LockProcs) / sizeof (LockProcs[0]))
