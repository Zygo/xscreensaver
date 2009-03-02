typedef struct LockStruct_s
{
	char *cmdline_arg;          /* mode name */
	/* Maybe other things should be added here from xlock? */
	/* Should read in XLock as well to set defaults */
	char *desc;                 /* text description of mode */
} LockStruct;

static LockStruct LockProcs[] =
{
$%LISTMOTIF#ifdef USE_BOMB
{(char *) "bomb", (char *) "Shows a bomb and will autologout after a time"},
{(char *) "random", (char *) "Shows a random mode (except blank and bomb)"}
#else
{(char *) "random", (char *) "Shows a random mode (except blank)"}
#endif
};

#define numprocs (sizeof(LockProcs) /sizeof(LockProcs[0]))
