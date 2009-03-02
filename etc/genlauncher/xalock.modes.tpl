static char *LockProcs[] =
{
$%LISTLIBSX
#ifdef USE_BOMB
"bomb",
"random"
#else
"random"
#endif
};

static int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
