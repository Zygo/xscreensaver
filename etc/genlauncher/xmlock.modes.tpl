typedef struct LockStruct_s
{
	char *cmdline_arg;          /* mode name */
	/* Maybe other things should be added here from xlock? */
	/* Should read in XLock as well to set defaults */
	char *desc;                 /* text description of mode */
} LockStruct;
		
static LockStruct LockProcs[] =
{
$%LISTMOTIF 
#ifdef USE_BOMB
  {"bomb", "Shows a bomb and will autologout after a time"},
  {"random", "Shows a random mode (except blank and bomb)"}
  #else
  {"random", "Shows a random mode (except blank)"}
#endif
};


static int  numprocs = sizeof (LockProcs) / sizeof (LockProcs[0]);
