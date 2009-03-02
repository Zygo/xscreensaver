/* sonar.c --- Simulate a sonar screen.
 *
 * This is an implementation of a general purpose reporting tool in the
 * format of a Sonar display. It is designed such that a sensor is read
 * on every movement of a sweep arm and the results of that sensor are
 * displayed on the screen. The location of the display points (targets) on the
 * screen are determined by the current localtion of the sweep and a distance
 * value associated with the target. 
 *
 * Currently the only two sensors that are implemented are the simulator
 * (the default) and the ping sensor. The simulator randomly creates a set
 * of bogies that move around on the scope while the ping sensor can be
 * used to display hosts on your network.
 *
 * The ping code is only compiled in if you define HAVE_ICMP or HAVE_ICMPHDR,
 * because, unfortunately, different systems have different ways of creating
 * these sorts of packets.
 *
 * Also: creating an ICMP socket is a privileged operation, so the program
 * needs to be installed SUID root if you want to use the ping mode.  If you
 * check the code you will see that this privilige is given up immediately
 * after the socket is created.
 *
 * It should be easy to extend this code to support other sorts of sensors.
 * Some ideas:
 *   - search the output of "netstat" for the list of hosts to ping;
 *   - plot the contents of /proc/interrupts;
 *   - plot the process table, by process size, cpu usage, or total time;
 *   - plot the logged on users by idle time or cpu usage.
 *
 * Copyright (C) 1998, 2001
 *  by Stephen Martin (smartin@vanderfleet-martin.net).
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * $Revision: 1.22 $
 *
 * Version 1.0 April 27, 1998.
 * - Initial version
 * - Submitted to RedHat Screensaver Contest
 * 
 * Version 1.1 November 3, 1998.
 * - Added simulation mode.
 * - Added enhancements by Thomas Bahls <thommy@cs.tu-berlin.de>
 * - Fixed huge memory leak.
 * - Submitted to xscreensavers
 * 
 * Version 1.2
 * - All ping code is now ifdef-ed by the compile time symbol HAVE_PING;
 *   use -DHAVE_PING to include it when you compile.
 * - Sweep now uses gradients.
 * - Fixed portability problems with icmphdr on some systems.
 * - removed lowColor option/resource.
 * - changed copyright notice so that it could be included in the xscreensavers
 *   collection.
 *
 * Version 1.3 November 16, 1998.
 * - All ping code is now ifdef-ed by the compile time symbol PING use -DPING
 *   to include it when you compile.
 * - Sweep now uses gradients.
 * - Fixed portability problems with icmphdr on some systems.
 * - removed lowcolour option/resource.
 * - changed copyright notice so that it could be included in the xscreensavers
 *   collection.
 *
 * Version 1.4 November 18, 1998.
 * - More ping portability fixes.
 *
 * Version 1.5 November 19, 1998.
 * - Synced up with jwz's changes.
 * - Now need to define HAVE_PING to compile in the ping stuff.
 */

/* These are computed by configure now:
   #define HAVE_ICMP
   #define HAVE_ICMPHDR
 */


/* Include Files */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/stat.h>

#include "screenhack.h"
#include "colors.h"
#include "hsv.h"

#if defined(HAVE_ICMP) || defined(HAVE_ICMPHDR)
# include <unistd.h>
# include <limits.h>
# include <signal.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include <sys/socket.h>
# include <netinet/in_systm.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/udp.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif /* HAVE_ICMP || HAVE_ICMPHDR */


/* Defines */

#undef MY_MIN
#define MY_MIN(a,b) ((a)<(b)?(a - 50):(b - 10))

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

/* Frigging icmp */

#if defined(HAVE_ICMP)
# define HAVE_PING
# define ICMP             icmp
# define ICMP_TYPE(p)     (p)->icmp_type
# define ICMP_CODE(p)     (p)->icmp_code
# define ICMP_CHECKSUM(p) (p)->icmp_cksum
# define ICMP_ID(p)       (p)->icmp_id
# define ICMP_SEQ(p)      (p)->icmp_seq
#elif defined(HAVE_ICMPHDR)
# define HAVE_PING
# define ICMP             icmphdr
# define ICMP_TYPE(p)     (p)->type
# define ICMP_CODE(p)     (p)->code
# define ICMP_CHECKSUM(p) (p)->checksum
# define ICMP_ID(p)       (p)->un.echo.id
# define ICMP_SEQ(p)      (p)->un.echo.sequence
#else
# undef HAVE_PING
#endif


#ifdef HAVE_PING
# if defined(__DECC) || defined(_IP_VHL)
   /* This is how you do it on DEC C, and possibly some BSD systems. */
#  define IP_HDRLEN(ip)   ((ip)->ip_vhl & 0x0F)
# else
   /* This is how you do it on everything else. */
#  define IP_HDRLEN(ip)   ((ip)->ip_hl)
# endif
#endif /* HAVE_PING */


/* Forward References */

#ifdef HAVE_PING
static u_short checksum(u_short *, int);
#endif
static long delta(struct timeval *, struct timeval *);


/* Data Structures */

/*
 * The Bogie.
 *
 * This represents an object that is visable on the scope.
 */

typedef struct Bogie {
    char *name;			/* The name of the thing being displayed */
    int distance;		/* The distance to this thing (0 - 100) */
    int tick;			/* The tick that it was found on */
    int ttl;			/* The time to live */
    int age;                    /* How long it's been around */
    struct Bogie *next;		/* The next one in the list */
} Bogie;

/*
 * Sonar Information.
 *
 * This contains all of the runtime information about the sonar scope.
 */

typedef struct {
    Display *dpy;		/* The X display */
    Window win;			/* The window */
    GC hi, 			/* The leading edge of the sweep */
	lo, 			/* The trailing part of the sweep */
	erase,			/* Used to erase things */
	grid,                   /* Used to draw the grid */
	text;			/* Used to draw text */
    Colormap cmap;		/* The colormap */
    XFontStruct *font;          /* The font to use for the labels */
    int text_steps;		/* How many steps to fade text. */
    XColor *text_colors;	/* Pixel values used to fade text */
    int sweep_degrees;		/* How much of the circle the sweep uses */
    int sweep_segs;		/* How many gradients in the sweep. */
    XColor *sweep_colors;        /* The sweep pixel values */
    int width, height;		/* Window dimensions */
    int minx, miny, maxx, maxy, /* Bounds of the scope */
	centrex, centrey, radius; /* Parts of the scope circle */
    Bogie *visable;		/* List of visable objects */
    int current;		/* Current position of sweep */
    int sweepnum;               /* The current id of the sweep */
    int delay;			/* how long between each frame of the anim */

    int TTL;			/* The number of ticks that bogies are visible
                                   on the screen before they fade away. */
} sonar_info;

static Bool debug_p = False;


/* 
 * Variables to support the differnt Sonar modes.
 */

Bogie *(*sensor)(sonar_info *, void *);	/* The current sensor */
void *sensor_info;			/* Information about the sensor */

/*
 * A list of targets to ping.
 */

typedef struct ping_target {
    char *name;			/* The name of the target */
#ifdef HAVE_PING
    struct sockaddr address;	/* The address of the target */
#endif /* HAVE_PING */
    struct ping_target *next;	/* The next one in the list */
} ping_target;


#ifdef HAVE_PING
/*
 * Ping Information.
 *
 * This contains the information for the ping sensor.
 */

typedef struct {
    int icmpsock;		/* Socket for sending pings */
    int pid;			/* Our process ID */
    int seq;			/* Packet sequence number */
    int timeout;		/* Timeout value for pings */
    ping_target *targets;	/* List of targets to ping */
    int numtargets;             /* The number of targets to ping */
} ping_info;

/* Flag to indicate that the timer has expired on us */

static int timer_expired;

#endif /* HAVE_PING */

/*
 * A list of targets for the simulator
 */

typedef struct sim_target {
    char *name;			/* The name of the target */
    int nexttick;		/* The next tick that this will be seen */
    int nextdist;		/* The distance on that tick */
    int movedonsweep;		/* The number of the sweep this last moved */
} sim_target;

/*
 * Simulator Information.
 *
 * This contains the information for the simulator mode.
 */

typedef struct {
    sim_target *teamA;		/* The bogies for the A team */
    int numA;			/* The number of bogies in team A */
    char *teamAID;		/* The identifier for bogies in team A */
    sim_target *teamB;		/* The bogies for the B team */
    int numB;			/* The number of bogies in team B */
    char *teamBID;		/* The identifier for bogies in team B */
} sim_info;

/* Name of the Screensaver hack */

char *progclass="sonar";

/* Application Defaults */

char *defaults [] = {
    ".background:      #000000",
    ".sweepColor:      #00FF00",
    "*delay:	       100000",
    "*scopeColor:      #003300",
    "*gridColor:       #00AA00",
    "*textColor:       #FFFF00",
    "*ttl:             90",
    "*mode:            default",
    "*font:            fixed",
    "*sweepDegrees:    30",

    "*textSteps:       80",	/* npixels */
    "*sweepSegments:   80",	/* npixels */

    "*pingTimeout:     3000",

    "*teamAName:       F18",
    "*teamBName:       MIG",
    "*teamACount:      4",
    "*teamBCount:      4",

    "*ping:	       default",
    ".debug:	       false",
    0
};

/* Options passed to this program */

XrmOptionDescRec options [] = {
    {"-background",   ".background",   XrmoptionSepArg, 0 },
    {"-sweep-color",  ".sweepColor",   XrmoptionSepArg, 0 },
    {"-scope-color",  ".scopeColor",   XrmoptionSepArg, 0 },
    {"-grid-color",   ".gridColor",    XrmoptionSepArg, 0 },
    {"-text-color",   ".textColor",    XrmoptionSepArg, 0 },
    {"-ttl",          ".ttl",          XrmoptionSepArg, 0 },
    {"-font",         ".font",         XrmoptionSepArg, 0 },
#ifdef HAVE_PING
    {"-ping-timeout", ".pingTimeout",  XrmoptionSepArg, 0 },
#endif /* HAVE_PING */
    {"-team-a-name",   ".teamAName",   XrmoptionSepArg, 0 },
    {"-team-b-name",   ".teamBName",   XrmoptionSepArg, 0 },
    {"-team-a-count",  ".teamACount",  XrmoptionSepArg, 0 },
    {"-team-b-count",  ".teamBCount",  XrmoptionSepArg, 0 },

    {"-ping",          ".ping",        XrmoptionSepArg, 0 },
    {"-debug",         ".debug",       XrmoptionNoArg, "True" },
    { 0, 0, 0, 0 }
};

/*
 * Create a new Bogie and set some initial values.
 *
 * Args:
 *    name     - The name of the bogie.
 *    distance - The distance value.
 *    tick     - The tick value.
 *    ttl      - The time to live value.
 *
 * Returns:
 *    The newly allocated bogie or null if a memory problem occured.
 */

static Bogie *
newBogie(char *name, int distance, int tick, int ttl) 
{

    /* Local Variables */

    Bogie *new;

    /* Allocate a bogie and initialize it */

    if ((new = (Bogie *) calloc(1, sizeof(Bogie))) == NULL) {
	fprintf(stderr, "%s: Out of Memory\n", progname);
	return NULL;
    }
    new->name = name;
    new->distance = distance;
    new->tick = tick;
    new->ttl = ttl;
    new->age = 0;
    new->next = (Bogie *) 0;
    return new;
}

/*
 * Free a Bogie.
 *
 * Args:
 *    b - The bogie to free.
 */


static void
freeBogie(Bogie *b) 
{
    if (b->name != (char *) 0)
	free(b->name);
    free(b);
}

/*
 * Find a bogie by name in a list.
 *
 * This does a simple linear search of the list for a given name.
 *
 * Args:
 *    bl   - The Bogie list to search.
 *    name - The name to look for.
 *
 * Returns:
 *    The requested Bogie or null if it wasn't found.
 */

static Bogie *
findNode(Bogie *bl, char *name) 
{

    /* Local Variables */

    Bogie *p;

    /* Abort if the list is empty or no name is given */

    if ((name == NULL) || (bl == NULL))
	return NULL;

    /* Search the list for the desired name */

    p = bl;
    while (p != NULL) {
	if (strcmp(p->name, name) == 0)
	    return p;
	p = p->next;
    }

    /* Not found */

    return NULL;
}

#ifdef HAVE_PING

/*
 * Lookup the address for a ping target;
 *
 * Args:
 *    target - The ping_target fill in the address for.
 *
 * Returns:
 *    1 if the host was successfully resolved, 0 otherwise.
 */

static int
lookupHost(ping_target *target) 
{
  struct hostent *hent;
  struct sockaddr_in *iaddr;

  int iip[4];
  char c;

  iaddr = (struct sockaddr_in *) &(target->address);
  iaddr->sin_family = AF_INET;

  if (4 == sscanf(target->name, "%d.%d.%d.%d%c",
                  &iip[0], &iip[1], &iip[2], &iip[3], &c))
    {
      /* It's an IP address.
       */
      unsigned char ip[4];

      ip[0] = iip[0];
      ip[1] = iip[1];
      ip[2] = iip[2];
      ip[3] = iip[3];

      if (ip[3] == 0)
        {
          if (debug_p > 1)
            fprintf (stderr, "%s:   ignoring bogus IP %s\n",
                     progname, target->name);
          return 0;
        }

      iaddr->sin_addr.s_addr = ((ip[3] << 24) |
                                (ip[2] << 16) |
                                (ip[1] <<  8) |
                                (ip[0]));
      hent = gethostbyaddr (ip, 4, AF_INET);

      if (debug_p > 1)
        fprintf (stderr, "%s:   %s => %s\n",
                 progname, target->name,
                 ((hent && hent->h_name && *hent->h_name)
                  ? hent->h_name : "<unknown>"));

      if (hent && hent->h_name && *hent->h_name)
        target->name = strdup (hent->h_name);
    }
  else
    {
      /* It's a host name.
       */
      hent = gethostbyname (target->name);
      if (!hent)
        {
          fprintf (stderr, "%s: could not resolve host:  %s\n",
                   progname, target->name);
          return 0;
        }

      memcpy (&iaddr->sin_addr, hent->h_addr_list[0],
              sizeof(iaddr->sin_addr));

      if (debug_p > 1)
        fprintf (stderr, "%s:   %s => %d.%d.%d.%d\n",
                 progname, target->name,
                 iaddr->sin_addr.s_addr       & 255,
                 iaddr->sin_addr.s_addr >>  8 & 255,
                 iaddr->sin_addr.s_addr >> 16 & 255,
                 iaddr->sin_addr.s_addr >> 24 & 255);
    }
  return 1;
}


static void
print_host (FILE *out, unsigned long ip, const char *name)
{
  char ips[50];
  sprintf (ips, "%lu.%lu.%lu.%lu",
           (ip)       & 255,
           (ip >>  8) & 255,
           (ip >> 16) & 255,
           (ip >> 24) & 255);
  if (!name || !*name) name = "<unknown>";
  fprintf (out, "%-16s %s\n", ips, name);
}


/*
 * Create a target for a host.
 *
 * Args:
 *    name - The name of the host.
 *
 * Returns:
 *    A newly allocated target or null if the host could not be resolved.
 */

static ping_target *
newHost(char *name) 
{

    /* Local Variables */

    ping_target *target = NULL;

    /* Create the target */

    if ((target = calloc(1, sizeof(ping_target))) == NULL) {
	fprintf(stderr, "%s: Out of Memory\n", progname);
	goto target_init_error;
    }
    if ((target->name = strdup(name)) == NULL) {
	fprintf(stderr, "%s: Out of Memory\n", progname);
	goto target_init_error;
    }

    /* Lookup the host */

    if (! lookupHost(target))
	goto target_init_error;

    /* Don't ever use loopback (127.0.0) hosts */
    {
      struct sockaddr_in *iaddr = (struct sockaddr_in *) &(target->address);
      unsigned long ip = iaddr->sin_addr.s_addr;
      if ((ip         & 255) == 127 &&
          ((ip >>  8) & 255) == 0 &&
          ((ip >> 16) & 255) == 0)
        {
          if (debug_p)
            fprintf (stderr, "%s:   ignoring loopback host %s\n",
                     progname, target->name);
          goto target_init_error;
        }
    }

    /* Done */

    if (debug_p)
      {
        struct sockaddr_in *iaddr = (struct sockaddr_in *) &(target->address);
        unsigned long ip = iaddr->sin_addr.s_addr;
        fprintf (stderr, "%s:   added ", progname);
        print_host (stderr, ip, target->name);
      }

    return target;

    /* Handle errors here */

target_init_error:
    if (target != NULL)
	free(target);
    return NULL;
}

/*
 * Generate a list of ping targets from the entries in a file.
 *
 * Args:
 *    fname - The name of the file. This file is expected to be in the same
 *            format as /etc/hosts.
 *
 * Returns:
 *    A list of targets to ping or null if an error occured.
 */

static ping_target *
readPingHostsFile(char *fname) 
{
    /* Local Variables */

    FILE *fp;
    char buf[LINE_MAX];
    char *p;
    ping_target *list = NULL;
    char *addr, *name;
    ping_target *new;

    /* Make sure we in fact have a file to process */

    if ((fname == NULL) || (fname[0] == '\0')) {
	fprintf(stderr, "%s: invalid ping host file name\n", progname);
	return NULL;
    }

    /* Open the file */

    if ((fp = fopen(fname, "r")) == NULL) {
	char msg[1024];
	sprintf(msg, "%s: unable to open host file %s", progname, fname);
	perror(msg);
	return NULL;
    }

    if (debug_p)
      fprintf (stderr, "%s:  reading file %s\n", progname, fname);

    /* Read the file line by line */

    while ((p = fgets(buf, LINE_MAX, fp)) != NULL) {

	/*
	 * Parse the line skipping those that start with '#'.
	 * The rest of the lines in the file should be in the same
	 * format as a /etc/hosts file. We are only concerned with
	 * the first two field, the IP address and the name
	 */

	while ((*p == ' ') || (*p == '\t'))
	    p++;
	if (*p == '#')
	    continue;

	/* Get the name and address */

	name = addr = NULL;
	if ((addr = strtok(buf, " ,;\t\n")) != NULL)
	    name = strtok(NULL, " ,;\t\n");
	else
	    continue;

        /* Check to see if the addr looks like an addr.  If not, assume
           the addr is a name and there is no addr.  This way, we can
           handle files whose lines have "xx.xx.xx.xx hostname" as their
           first two tokens, and also files that have a hostname as their
           first token (like .ssh/known_hosts and .rhosts.)
         */
        {
          int i; char c;
          if (4 != sscanf(addr, "%d.%d.%d.%d%c", &i, &i, &i, &i, &c))
            {
              name = addr;
              addr = NULL;
            }
        }

        /* If the name is all digits, it's not a name. */
        if (name)
          {
            const char *s;
            for (s = name; *s; s++)
              if (*s < '0' || *s > '9')
                break;
            if (! *s)
              {
                if (debug_p > 1)
                  fprintf (stderr, "%s:  skipping bogus name \"%s\" (%s)\n",
                           progname, name, addr);
                name = NULL;
              }
          }

	/* Create a new target using first the name then the address */

	new = NULL;
	if (name != NULL)
	    new = newHost(name);
	if (new == NULL && addr != NULL)
	    new = newHost(addr);

	/* Add it to the list if we got one */

	if (new != NULL) {
	    new->next = list;
	    list = new;
	}
    }

    /* Close the file and return the list */

    fclose(fp);
    return list;
}


static ping_target *
delete_duplicate_hosts (ping_target *list)
{
  ping_target *head = list;
  ping_target *rest;

  for (rest = head; rest; rest = rest->next)
    {
      struct sockaddr_in *i1 = (struct sockaddr_in *) &(rest->address);
      unsigned long ip1 = i1->sin_addr.s_addr;

      static ping_target *rest2;
      for (rest2 = rest; rest2; rest2 = rest2->next)
        {
          if (rest2 && rest2->next)
            {
              struct sockaddr_in *i2 = (struct sockaddr_in *)
                &(rest2->next->address);
              unsigned long ip2 = i2->sin_addr.s_addr;

              if (ip1 == ip2)
                {
                  if (debug_p)
                    {
                      fprintf (stderr, "%s: deleted duplicate: ", progname);
                      print_host (stderr, ip2, rest2->next->name);
                    }
                  rest2->next = rest2->next->next;
                }
            }
        }
    }

  return head;
}




/*
 * Generate a list ping targets consisting of all of the entries on
 * the same subnet.
 *
 * Returns:
 *    A list of all of the hosts on this net.
 */

static ping_target *
subnetHostsList(int base, int subnet_width) 
{
    unsigned long mask;

    /* Local Variables */

    char hostname[BUFSIZ];
    char address[BUFSIZ];
    struct hostent *hent;
    char *p;
    int i;
    ping_target *new;
    ping_target *list = NULL;

    if (subnet_width < 24)
      {
        fprintf (stderr,
   "%s: pinging %lu hosts is a bad idea; please use a subnet mask of 24 bits\n"
                 "       or more (255 hosts max.)\n",
                 progname, (unsigned long) (1L << (32 - subnet_width)) - 1);
        exit (1);
      }
    else if (subnet_width > 30)
      {
        fprintf (stderr, "%s: a subnet of %d bits doesn't make sense:"
                 " try \"subnet/24\" or \"subnet/29\".\n",
                 progname, subnet_width);
        exit (1);
      }


    if (debug_p)
      fprintf (stderr, "%s:   adding %d-bit subnet\n", progname, subnet_width);

    /* Get our hostname */

    if (gethostname(hostname, BUFSIZ)) {
	fprintf(stderr, "%s: unable to get local hostname\n", progname);
	return NULL;
    }

    /* Get our IP address and convert it to a string */

    if ((hent = gethostbyname(hostname)) == NULL) {
	fprintf(stderr, "%s: unable to lookup our IP address\n", progname);
	return NULL;
    }
    strcpy(address, inet_ntoa(*((struct in_addr *)hent->h_addr_list[0])));

    /* Construct targets for all addresses in this subnet */

    mask = 0;
    for (i = 0; i < subnet_width; i++)
      mask |= (1L << (31-i));

    /* If no base IP specified, assume localhost. */
    if (base == 0)
      base = ((((unsigned char) hent->h_addr_list[0][0]) << 24) |
              (((unsigned char) hent->h_addr_list[0][1]) << 16) |
              (((unsigned char) hent->h_addr_list[0][2]) <<  8) |
              (((unsigned char) hent->h_addr_list[0][3])));

    if (base == ((127 << 24) | 1))
      {
        fprintf (stderr,
                 "%s: unable to determine local subnet address: \"%s\"\n"
                 "       resolves to loopback address %d.%d.%d.%d.\n",
                 progname, hostname,
                 (base >> 24) & 255, (base >> 16) & 255,
                 (base >>  8) & 255, (base      ) & 255);
        return NULL;
      }

    for (i = 255; i >= 0; i--) {
        int ip = (base & 0xFFFFFF00) | i;
      
        if ((ip & mask) != (base & mask))   /* not in the mask range at all */
          continue;
        if ((ip & ~mask) == 0)              /* broadcast address */
          continue;
        if ((ip & ~mask) == ~mask)          /* broadcast address */
          continue;

        sprintf (address, "%d.%d.%d.%d", 
                 (ip>>24)&255, (ip>>16)&255, (ip>>8)&255, (ip)&255);

        if (debug_p > 1)
          fprintf(stderr, "%s:  subnet: %s (%d.%d.%d.%d & %d.%d.%d.%d / %d)\n",
                  progname,
                  address,
                  (int) (base>>24)&255,
                  (int) (base>>16)&255,
                  (int) (base>> 8)&255,
                  (int) (base&mask&255),
                  (int) (mask>>24)&255,
                  (int) (mask>>16)&255,
                  (int) (mask>> 8)&255,
                  (int) (mask&255),
                  (int) subnet_width);

        p = address + strlen(address) + 1;
	sprintf(p, "%d", i);

	new = newHost(address);
	if (new != NULL) {
	    new->next = list;
	    list = new;
	}
    }

    /* Done */

    return list;
}

/*
 * Initialize the ping sensor.
 *
 * Returns:
 *    A newly allocated ping_info structure or null if an error occured.
 */

static ping_target *parse_mode (Bool ping_works_p);

static ping_info *
init_ping(void) 
{

  Bool socket_initted_p = False;

    /* Local Variables */

    ping_info *pi = NULL;		/* The new ping_info struct */
    ping_target *pt;			/* Used to count the targets */

    /* Create the ping info structure */

    if ((pi = (ping_info *) calloc(1, sizeof(ping_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
	goto ping_init_error;
    }

    /* Create the ICMP socket */

    if ((pi->icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) >= 0) {
      socket_initted_p = True;
    }

    /* Disavow privs */

    setuid(getuid());


    pi->pid = getpid() & 0xFFFF;
    pi->seq = 0;
    pi->timeout = get_integer_resource("pingTimeout", "PingTimeout");

    /* Generate a list of targets */

    pi->targets = parse_mode (socket_initted_p);
    pi->targets = delete_duplicate_hosts (pi->targets);


    if (debug_p)
      {
        ping_target *t;
        fprintf (stderr, "%s: Target list:\n", progname);
        for (t = pi->targets; t; t = t->next)
          {
            struct sockaddr_in *iaddr = (struct sockaddr_in *) &(t->address);
            unsigned long ip = iaddr->sin_addr.s_addr;
            fprintf (stderr, "%s:   ", progname);
            print_host (stderr, ip, t->name);
          }
      }

    /* Make sure there is something to ping */

    if (pi->targets == NULL) {
      goto ping_init_error;
    }

    /* Count the targets */

    pt = pi->targets;
    pi->numtargets = 0;
    while (pt != NULL) {
	pi->numtargets++;
	pt = pt->next;
    }

    /* Done */

    return pi;

    /* Handle initialization errors here */

ping_init_error:
    if (pi != NULL)
	free(pi);
    return NULL;
}


/*
 * Ping a host.
 *
 * Args:
 *    pi   - The ping information strcuture.
 *    host - The name or IP address of the host to ping (in ascii).
 */

static void
sendping(ping_info *pi, ping_target *pt) 
{

    /* Local Variables */

    u_char *packet;
    struct ICMP *icmph;
    int result;

    /*
     * Note, we will send the character name of the host that we are
     * pinging in the packet so that we don't have to keep track of the
     * name or do an address lookup when it comes back.
     */

    int pcktsiz = sizeof(struct ICMP) + sizeof(struct timeval) +
	strlen(pt->name) + 1;

    /* Create the ICMP packet */

    if ((packet = (u_char *) malloc(pcktsiz)) == (void *) 0)
	return;  /* Out of memory */
    icmph = (struct ICMP *) packet;
    ICMP_TYPE(icmph) = ICMP_ECHO;
    ICMP_CODE(icmph) = 0;
    ICMP_CHECKSUM(icmph) = 0;
    ICMP_ID(icmph) = pi->pid;
    ICMP_SEQ(icmph) = pi->seq++;
    gettimeofday((struct timeval *) &packet[sizeof(struct ICMP)],
		 (struct timezone *) 0);
    strcpy((char *) &packet[sizeof(struct ICMP) + sizeof(struct timeval)],
	   pt->name);
    ICMP_CHECKSUM(icmph) = checksum((u_short *)packet, pcktsiz);

    /* Send it */

    if ((result = sendto(pi->icmpsock, packet, pcktsiz, 0, 
			 &pt->address, sizeof(pt->address))) !=  pcktsiz) {
#if 0
        char errbuf[BUFSIZ];
        sprintf(errbuf, "%s: error sending ping to %s", progname, pt->name);
	perror(errbuf);
#endif
    }
}

/*
 * Catch a signal and do nothing.
 *
 * Args:
 *    sig - The signal that was caught.
 */

static void
sigcatcher(int sig)
{
    timer_expired = 1;
}

/*
 * Compute the checksum on a ping packet.
 *
 * Args:
 *    packet - A pointer to the packet to compute the checksum for.
 *    size   - The size of the packet.
 *
 * Returns:
 *    The computed checksum
 *    
 */

static u_short
checksum(u_short *packet, int size) 
{

    /* Local Variables */

    register int nleft = size;
    register u_short *w = packet;
    register int sum = 0;
    u_short answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */

    while (nleft > 1)  {
	sum += *w++;
	nleft -= 2;
    }

    /* mop up an odd byte, if necessary */

    if (nleft == 1) {
	*(u_char *)(&answer) = *(u_char *)w ;
        *(1 + (u_char *)(&answer)) = 0;
	sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */

    sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
    sum += (sum >> 16);                     /* add carry */
    answer = ~sum;                          /* truncate to 16 bits */

    /* Done */

    return(answer);
}

/*
 * Look for ping replies.
 *
 * Retrieve all outstanding ping replies.
 *
 * Args:
 *    si - Information about the sonar.
 *    pi - Ping information.
 *    ttl - The time each bogie is to live on the screen
 *
 * Returns:
 *    A Bogie list of all the machines that replied.
 */

static Bogie *
getping(sonar_info *si, ping_info *pi) 
{

    /* Local Variables */

    struct sockaddr from;
    int fromlen;
    int result;
    u_char packet[1024];
    struct timeval now;
    struct timeval *then;
    struct ip *ip;
    int iphdrlen;
    struct ICMP *icmph;
    Bogie *bl = NULL;
    Bogie *new;
    char *name;
    struct sigaction sa;
    struct itimerval it;
    fd_set rfds;
    struct timeval tv;

    /* Set up a signal to interupt our wait for a packet */

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigcatcher;
    if (sigaction(SIGALRM, &sa, 0) == -1) {
	char msg[1024];
	sprintf(msg, "%s: unable to trap SIGALRM", progname);
	perror(msg);
	exit(1);
    }

    /* Set up a timer to interupt us if we don't get a packet */

    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = pi->timeout;
    timer_expired = 0;
    setitimer(ITIMER_REAL, &it, NULL);

    /* Wait for a result packet */

    fromlen = sizeof(from);
    while (! timer_expired) {
      tv.tv_usec=pi->timeout;
      tv.tv_sec=0;
#if 0
      /* This breaks on BSD, which uses bzero() in the definition of FD_ZERO */
      FD_ZERO(&rfds);
#else
      memset (&rfds, 0, sizeof(rfds));
#endif
      FD_SET(pi->icmpsock,&rfds);
      /* only wait a little while, in case we raced with the timer expiration.
         From Valentijn Sessink <valentyn@openoffice.nl> */
      if (select(pi->icmpsock+1, &rfds, NULL, NULL, &tv) >0) {
        result = recvfrom(pi->icmpsock, packet, sizeof(packet),
                      0, &from, &fromlen);

	/* Check the packet */

	gettimeofday(&now, (struct timezone *) 0);
	ip = (struct ip *) packet;
        iphdrlen = IP_HDRLEN(ip) << 2;
	icmph = (struct ICMP *) &packet[iphdrlen];

	/* Was the packet a reply?? */

	if (ICMP_TYPE(icmph) != ICMP_ECHOREPLY) {
	    /* Ignore anything but ICMP Replies */
	    continue; /* Nope */
	}

	/* Was it for us? */

	if (ICMP_ID(icmph) != pi->pid) {
	    /* Ignore packets not set from us */
	    continue; /* Nope */
	}

	/* Copy the name of the bogie */

	if ((name =
	     strdup((char *) &packet[iphdrlen + 
				    + sizeof(struct ICMP)
				    + sizeof(struct timeval)])) == NULL) {
	    fprintf(stderr, "%s: Out of memory\n", progname);
	    return bl;
	}

        /* If the name is an IP addr, try to resolve it. */
        {
          int iip[4];
          char c;
          if (4 == sscanf(name, " %d.%d.%d.%d %c",
                          &iip[0], &iip[1], &iip[2], &iip[3], &c))
            {
              unsigned char ip[4];
              struct hostent *h;
              ip[0] = iip[0]; ip[1] = iip[1]; ip[2] = iip[2]; ip[3] = iip[3];
              h = gethostbyaddr ((char *) ip, 4, AF_INET);
              if (h && h->h_name && *h->h_name)
                {
                  free (name);
                  name = strdup (h->h_name);
                }
            }
        }

	/* Create the new Bogie and add it to the list we are building */

	if ((new = newBogie(name, 0, si->current, si->TTL)) == NULL)
	    return bl;
	new->next = bl;
	bl = new;

	/* Compute the round trip time */

	then =  (struct timeval *) &packet[iphdrlen +
					  sizeof(struct ICMP)];
	new->distance = delta(then, &now) / 100;
	if (new->distance == 0)
		new->distance = 2; /* HACK */
      }
    }

    /* Done */

    return bl;
}

/*
 * Ping hosts.
 *
 * Args:
 *    si - Sonar Information.
 *    pi - Ping Information.
 *
 * Returns:
 *    A list of hosts that replied to pings or null if there were none.
 */

static Bogie *
ping(sonar_info *si, void *vpi) 
{

    /*
     * This tries to distribute the targets evely around the field of the
     * sonar.
     */

    ping_info *pi = (ping_info *) vpi;
    static ping_target *ptr = NULL;

    int tick = si->current * -1 + 1;
    if ((ptr == NULL) && (tick == 1))
	ptr = pi->targets;

    if (pi->numtargets <= 90) {
	int xdrant = 90 / pi->numtargets;
	if ((tick % xdrant) == 0) {
	    if (ptr != (ping_target *) 0) {
		sendping(pi, ptr);
		ptr = ptr->next;
	    }
	}

    } else if (pi->numtargets > 90) {
	if (ptr != (ping_target *) 0) {
	    sendping(pi, ptr);
	    ptr = ptr->next;
	}
    }

    /* Get the results */

    return getping(si, pi);
}

#endif /* HAVE_PING */

/*
 * Calculate the difference between two timevals in microseconds.
 *
 * Args:
 *    then - The older timeval.
 *    now  - The newer timeval.
 *
 * Returns:
 *   The difference between the two in microseconds.
 */

static long
delta(struct timeval *then, struct timeval *now) 
{
    return (((now->tv_sec - then->tv_sec) * 1000000) + 
	       (now->tv_usec - then->tv_usec));  
}

/*
 * Initialize the simulation mode.
 */

static sim_info *
init_sim(void) 
{

    /* Local Variables */

    sim_info *si;
    int i;

    /* Create the simulation info structure */

    if ((si = (sim_info *) calloc(1, sizeof(sim_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
	return NULL;
    }

    /* Team A */

    si->numA = get_integer_resource("teamACount", "TeamACount");
    if ((si->teamA = (sim_target *)calloc(si->numA, sizeof(sim_target)))
	== NULL) {
	free(si);
	fprintf(stderr, "%s: Out of Memory\n", progname);
	return NULL;
    }
    si->teamAID = get_string_resource("teamAName", "TeamAName");
    for (i = 0; i < si->numA; i++) {
	if ((si->teamA[i].name = (char *) malloc(strlen(si->teamAID) + 4))
	    == NULL) {
	    free(si);
	    fprintf(stderr, "%s: Out of Memory\n", progname);
	    return NULL;
	}
	sprintf(si->teamA[i].name, "%s%03d", si->teamAID, i+1);
	si->teamA[i].nexttick = (int) (90.0 * random() / RAND_MAX);
	si->teamA[i].nextdist = (int) (100.0 * random() / RAND_MAX);
	si->teamA[i].movedonsweep = -1;
    }

    /* Team B */

    si->numB = get_integer_resource("teamBCount", "TeamBCount");
    if ((si->teamB = (sim_target *)calloc(si->numB, sizeof(sim_target)))
	== NULL) {
	free(si);
	fprintf(stderr, "%s: Out of Memory\n", progname);
	return NULL;
    }
    si->teamBID = get_string_resource("teamBName", "TeamBName");
    for (i = 0; i < si->numB; i++) {
	if ((si->teamB[i].name = (char *) malloc(strlen(si->teamBID) + 4))
	    == NULL) {
	    free(si);
	    fprintf(stderr, "%s: Out of Memory\n", progname);
	    return NULL;
	}
	sprintf(si->teamB[i].name, "%s%03d", si->teamBID, i+1);
	si->teamB[i].nexttick = (int) (90.0 * random() / RAND_MAX);
	si->teamB[i].nextdist = (int) (100.0 * random() / RAND_MAX);
	si->teamB[i].movedonsweep = -1;
    }

    /* Done */

    return si;
}

/*
 * Initialize the Sonar.
 *
 * Args:
 *    dpy - The X display.
 *    win - The X window;
 *
 * Returns:
 *   A sonar_info strcuture or null if memory allocation problems occur.
 */

static sonar_info *
init_sonar(Display *dpy, Window win) 
{

    /* Local Variables */

    XGCValues gcv;
    XWindowAttributes xwa;
    sonar_info *si;
    XColor start, end;
    int h1, h2;
    double s1, s2, v1, v2;

    /* Create the Sonar information structure */

    if ((si = (sonar_info *) calloc(1, sizeof(sonar_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
	return NULL;
    }

    /* Initialize the structure for the current environment */

    si->dpy = dpy;
    si->win = win;
    si->visable = NULL;
    XGetWindowAttributes(dpy, win, &xwa);
    si->cmap = xwa.colormap;
    si->width = xwa.width;
    si->height = xwa.height;
    si->centrex = si->width / 2;
    si->centrey = si->height / 2;
    si->maxx = si->centrex + MY_MIN(si->centrex, si->centrey) - 10;
    si->minx = si->centrex - MY_MIN(si->centrex, si->centrey) + 10;
    si->maxy = si->centrey + MY_MIN(si->centrex, si->centrey) - 10;
    si->miny = si->centrey - MY_MIN(si->centrex, si->centrey) + 10;
    si->radius = si->maxx - si->centrex;
    si->current = 0;
    si->sweepnum = 0;

    /* Get the font */

    if (((si->font = XLoadQueryFont(dpy, get_string_resource ("font", "Font")))
	 == NULL) &&
	((si->font = XLoadQueryFont(dpy, "fixed")) == NULL)) {
	fprintf(stderr, "%s: can't load an appropriate font\n", progname);
	return NULL;
    }

    /* Get the delay between animation frames */

    si->delay = get_integer_resource ("delay", "Integer");

    if (si->delay < 0) si->delay = 0;
    si->TTL = get_integer_resource("ttl", "TTL");

    /* Create the Graphics Contexts that will be used to draw things */

    gcv.foreground = 
	get_pixel_resource ("sweepColor", "SweepColor", dpy, si->cmap);
    si->hi = XCreateGC(dpy, win, GCForeground, &gcv);
    gcv.font = si->font->fid;
    si->text = XCreateGC(dpy, win, GCForeground|GCFont, &gcv);
    gcv.foreground = get_pixel_resource("scopeColor", "ScopeColor",
					dpy, si->cmap);
    si->erase = XCreateGC (dpy, win, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource("gridColor", "GridColor",
					dpy, si->cmap);
    si->grid = XCreateGC (dpy, win, GCForeground, &gcv);

    /* Compute pixel values for fading text on the display */

    XParseColor(dpy, si->cmap, 
		get_string_resource("textColor", "TextColor"), &start);
    XParseColor(dpy, si->cmap, 
		get_string_resource("scopeColor", "ScopeColor"), &end);

    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);
    rgb_to_hsv (end.red, end.green, end.blue, &h2, &s2, &v2);

    si->text_steps = get_integer_resource("textSteps", "TextSteps");
    if (si->text_steps < 0 || si->text_steps > 255)
      si->text_steps = 10;

    si->text_colors = (XColor *) calloc(si->text_steps, sizeof(XColor));
    make_color_ramp (dpy, si->cmap,
                     h1, s1, v1,
                     h2, s2, v2,
                     si->text_colors, &si->text_steps,
                     False, True, False);

    /* Compute the pixel values for the fading sweep */

    XParseColor(dpy, si->cmap, 
                get_string_resource("sweepColor", "SweepColor"), &start);

    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);

    si->sweep_degrees = get_integer_resource("sweepDegrees", "Degrees");
    if (si->sweep_degrees <= 0) si->sweep_degrees = 20;
    if (si->sweep_degrees > 350) si->sweep_degrees = 350;

    si->sweep_segs = get_integer_resource("sweepSegments", "SweepSegments");
    if (si->sweep_segs < 1 || si->sweep_segs > 255)
      si->sweep_segs = 255;

    si->sweep_colors = (XColor *) calloc(si->sweep_segs, sizeof(XColor));
    make_color_ramp (dpy, si->cmap,
                     h1, s1, v1,
                     h2, s2, v2,
                     si->sweep_colors, &si->sweep_segs,
                     False, True, False);

    if (si->sweep_segs <= 0)
      si->sweep_segs = 1;

    /* Done */

    return si;
}

/*
 * Update the location of a simulated bogie.
 */

static void
updateLocation(sim_target *t) 
{

    int xdist, xtick;

    xtick = (int) (3.0 * random() / RAND_MAX) - 1;
    xdist = (int) (11.0 * random() / RAND_MAX) - 5;
    if (((t->nexttick + xtick) < 90) && ((t->nexttick + xtick) >= 0))
	t->nexttick += xtick;
    else
	t->nexttick -= xtick;
    if (((t->nextdist + xdist) < 100) && ((t->nextdist+xdist) >= 0))
	t->nextdist += xdist;
    else
	t->nextdist -= xdist;
}

/*
 * The simulator. This uses information in the sim_info to simulate a bunch
 * of bogies flying around on the screen.
 */

/*
 * TODO: It would be cool to have the two teams chase each other around and
 *       shoot it out.
 */

static Bogie *
simulator(sonar_info *si, void *vinfo) 
{

    /* Local Variables */

    int i;
    Bogie *list = NULL;
    Bogie *new;
    sim_target *t;
    sim_info *info = (sim_info *) vinfo;

    /* Check team A */

    for (i = 0; i < info->numA; i++) {
	t = &info->teamA[i];
	if ((t->movedonsweep != si->sweepnum) &&
	    (t->nexttick == (si->current * -1))) {
	    new = newBogie(strdup(t->name), t->nextdist, si->current, si->TTL);
	    if (list != NULL)
		new->next = list;
	    list = new;
	    updateLocation(t);
	    t->movedonsweep = si->sweepnum;
	}
    }

    /* Team B */

    for (i = 0; i < info->numB; i++) {
	t = &info->teamB[i];
	if ((t->movedonsweep != si->sweepnum) &&
	    (t->nexttick == (si->current * -1))) {
	    new = newBogie(strdup(t->name), t->nextdist, si->current, si->TTL);
	    if (list != NULL)
		new->next = list;
	    list = new;
	    updateLocation(t);
	    t->movedonsweep = si->sweepnum;
	}
    }

    /* Done */

    return list;
}

/*
 * Compute the X coordinate of the label.
 *
 * Args:
 *    si - The sonar info block.
 *    label - The label that will be drawn.
 *    x - The x coordinate of the bogie.
 *
 * Returns:
 *    The x coordinate of the start of the label.
 */

static int
computeStringX(sonar_info *si, char *label, int x) 
{

    int width = XTextWidth(si->font, label, strlen(label));
    return x - (width / 2);
}

/*
 * Compute the Y coordinate of the label.
 *
 * Args:
 *    si - The sonar information.
 *    y - The y coordinate of the bogie.
 *
 * Returns:
 *    The y coordinate of the start of the label.
 */

/* TODO: Add smarts to keep label in sonar screen */

static int
computeStringY(sonar_info *si, int y) 
{

    int fheight = si->font->ascent + si->font->descent;
    return y + 5 + fheight;
}

/*
 * Draw a Bogie on the radar screen.
 *
 * Args:
 *    si       - Sonar Information.
 *    draw     - A flag to indicate if the bogie should be drawn or erased.
 *    name     - The name of the bogie.
 *    degrees  - The number of degrees that it should apprear at.
 *    distance - The distance the object is from the centre.
 *    ttl      - The time this bogie has to live.
 *    age      - The time this bogie has been around.
 */

static void
DrawBogie(sonar_info *si, int draw, char *name, int degrees, 
	  int distance, int ttl, int age) 
{

    /* Local Variables */

    int x, y;
    GC gc;
    int ox = si->centrex;
    int oy = si->centrey;
    int index, delta;

    /* Compute the coordinates of the object */

    if (distance != 0)
      distance = (log((double) distance) / 10.0) * si->radius;
    x = ox + ((double) distance * cos(4.0 * ((double) degrees)/57.29578));
    y = oy - ((double) distance * sin(4.0 * ((double) degrees)/57.29578));

    /* Set up the graphics context */

    if (draw) {

	/* Here we attempt to compute the distance into the total life of
	 * object that we currently are. This distance is used against
	 * the total lifetime to compute a fraction which is the index of
	 * the color to draw the bogie.
	 */

	if (si->current <= degrees)
	    delta = (si->current - degrees) * -1;
	else
	    delta = 90 + (degrees - si->current);
	delta += (age * 90);
	index = (si->text_steps - 1) * ((float) delta / (90.0 * (float) ttl));
	gc = si->text;
	XSetForeground(si->dpy, gc, si->text_colors[index].pixel);

    } else
	gc = si->erase;

  /* Draw (or erase) the Bogie */

    XFillArc(si->dpy, si->win, gc, x, y, 5, 5, 0, 360 * 64);
    XDrawString(si->dpy, si->win, gc,
		computeStringX(si, name, x),
		computeStringY(si, y), name, strlen(name));
}


/*
 * Draw the sonar grid.
 *
 * Args:
 *    si - Sonar information block.
 */

static void
drawGrid(sonar_info *si) 
{

    /* Local Variables */

    int i;
    int width = si->maxx - si->minx;
    int height = si->maxy - si->miny;
  
    /* Draw the circles */

    XDrawArc(si->dpy, si->win, si->grid, si->minx - 10, si->miny - 10, 
	     width + 20, height + 20,  0, (360 * 64));

    XDrawArc(si->dpy, si->win, si->grid, si->minx, si->miny, 
	     width, height,  0, (360 * 64));

    XDrawArc(si->dpy, si->win, si->grid, 
	     (int) (si->minx + (.166 * width)), 
	     (int) (si->miny + (.166 * height)), 
	     (unsigned int) (.666 * width), (unsigned int)(.666 * height),
	     0, (360 * 64));

    XDrawArc(si->dpy, si->win, si->grid, 
	     (int) (si->minx + (.333 * width)),
	     (int) (si->miny + (.333 * height)), 
	     (unsigned int) (.333 * width), (unsigned int) (.333 * height),
	     0, (360 * 64));

    /* Draw the radial lines */

    for (i = 0; i < 360; i += 10)
	if (i % 30 == 0)
	    XDrawLine(si->dpy, si->win, si->grid, si->centrex, si->centrey,
		      (int) (si->centrex +
		      (si->radius + 10) * (cos((double) i / 57.29578))),
		      (int) (si->centrey -
		      (si->radius + 10)*(sin((double) i / 57.29578))));
	else
	    XDrawLine(si->dpy, si->win, si->grid, 
		      (int) (si->centrex + si->radius *
			     (cos((double) i / 57.29578))),
		      (int) (si->centrey - si->radius *
			     (sin((double) i / 57.29578))),
		      (int) (si->centrex +
		      (si->radius + 10) * (cos((double) i / 57.29578))),
		      (int) (si->centrey - 
		      (si->radius + 10) * (sin((double) i / 57.29578))));
}

/*
 * Update the Sonar scope.
 *
 * Args:
 *    si - The Sonar information.
 *    bl - A list  of bogies to add to the scope.
 */

static void
Sonar(sonar_info *si, Bogie *bl) 
{

    /* Local Variables */

    Bogie *bp, *prev;
    int i;

    /* Check for expired tagets and remove them from the visable list */

    prev = NULL;
    for (bp = si->visable; bp != NULL; bp = (bp ? bp->next : 0)) {

	/*
	 * Remove it from the visable list if it's expired or we have
	 * a new target with the same name.
	 */

	bp->age ++;

	if (((bp->tick == si->current) && (++bp->age >= bp->ttl)) ||
	    (findNode(bl, bp->name) != NULL)) {
	    DrawBogie(si, 0, bp->name, bp->tick,
		      bp->distance, bp->ttl, bp->age);
	    if (prev == NULL)
		si->visable = bp->next;
	    else
		prev->next = bp->next;
	    freeBogie(bp);
            bp = prev;
	} else
	    prev = bp;
    }

    /* Draw the sweep */

    {
      int seg_deg = (si->sweep_degrees * 64) / si->sweep_segs;
      int start_deg = si->current * 4 * 64;
      if (seg_deg <= 0) seg_deg = 1;
      for (i = 0; i < si->sweep_segs; i++) {
	XSetForeground(si->dpy, si->hi, si->sweep_colors[i].pixel);
	XFillArc(si->dpy, si->win, si->hi, si->minx, si->miny, 
		 si->maxx - si->minx, si->maxy - si->miny,
                 start_deg + (i * seg_deg),
                 seg_deg);
      }

      /* Remove the trailing wedge the sonar */
      XFillArc(si->dpy, si->win, si->erase, si->minx, si->miny, 
               si->maxx - si->minx, si->maxy - si->miny, 
               start_deg + (i * seg_deg),
               (4 * 64));
    }

    /* Move the new targets to the visable list */

    for (bp = bl; bp != (Bogie *) 0; bp = bl) {
	bl = bl->next;
	bp->next = si->visable;
	si->visable = bp;
    }

    /* Draw the visable targets */

    for (bp = si->visable; bp != NULL; bp = bp->next) {
	if (bp->age < bp->ttl)		/* grins */
	   DrawBogie(si, 1, bp->name, bp->tick, bp->distance, bp->ttl,bp->age);
    }

    /* Redraw the grid */

    drawGrid(si);
}


static ping_target *
parse_mode (Bool ping_works_p)
{
  char *source = get_string_resource ("ping", "Ping");
  char *token, *end;
  char dummy;

  ping_target *hostlist = 0;

  if (!source) source = strdup("");

  if (!*source || !strcmp (source, "default"))
    {
# ifdef HAVE_PING
      if (ping_works_p)		/* if root or setuid, ping will work. */
        source = strdup("subnet/29,/etc/hosts");
      else
# endif
        source = strdup("simulation");
    }

  token = source;
  end = source + strlen(source);
  while (token < end)
    {
      char *next;
# ifdef HAVE_PING
      ping_target *new;
      struct stat st;
      unsigned int n0=0, n1=0, n2=0, n3=0, m=0;
      char d;
# endif /* HAVE_PING */

      for (next = token;
           *next != ',' && *next != ' ' && *next != '\t' && *next != '\n';
           next++)
        ;
      *next = 0;


      if (debug_p)
        fprintf (stderr, "%s: parsing %s\n", progname, token);

      if (!strcmp (token, "simulation"))
        return 0;

      if (!ping_works_p)
        {
          fprintf(stderr,
           "%s: this program must be setuid to root for `ping mode' to work.\n"
             "       Running in `simulation mode' instead.\n",
                  progname);
          return 0;
        }

#ifdef HAVE_PING
      if ((4 == sscanf (token, "%d.%d.%d/%d %c",    &n0,&n1,&n2,    &m,&d)) ||
          (5 == sscanf (token, "%d.%d.%d.%d/%d %c", &n0,&n1,&n2,&n3,&m,&d)))
        {
          /* subnet: A.B.C.D/M
             subnet: A.B.C/M
           */
          unsigned long ip = (n0 << 24) | (n1 << 16) | (n2 << 8) | n3;
          new = subnetHostsList(ip, m);
        }
      else if (4 == sscanf (token, "%d.%d.%d.%d %c", &n0, &n1, &n2, &n3, &d))
        {
          /* IP: A.B.C.D
           */
          new = newHost (token);
        }
      else if (!strcmp (token, "subnet"))
        {
          new = subnetHostsList(0, 24);
        }
      else if (1 == sscanf (token, "subnet/%d %c", &m, &dummy))
        {
          new = subnetHostsList(0, m);
        }
      else if (*token == '.' || *token == '/' || !stat (token, &st))
        {
          /* file name
           */
          new = readPingHostsFile (token);
        }
      else
        {
          /* not an existant file - must be a host name
           */
          new = newHost (token);
        }

      if (new)
        {
          ping_target *nn = new;
          while (nn && nn->next)
            nn = nn->next;
          nn->next = hostlist;
          hostlist = new;

          sensor = ping;
        }
#endif /* HAVE_PING */

      token = next + 1;
      while (token < end &&
             (*token == ',' || *token == ' ' ||
              *token == '\t' || *token == '\n'))
        token++;
    }

  return hostlist;
}



/*
 * Main screen saver hack.
 *
 * Args:
 *    dpy - The X display.
 *    win - The X window.
 */

void 
screenhack(Display *dpy, Window win) 
{

    /* Local Variables */

    sonar_info *si;
    struct timeval start, finish;
    Bogie *bl;
    long sleeptime;

    debug_p = get_boolean_resource ("debug", "Debug");

    sensor = 0;
# ifdef HAVE_PING
    sensor_info = (void *) init_ping();
# else  /* !HAVE_PING */
    sensor_info = 0;
    parse_mode (0);  /* just to check argument syntax */
# endif /* !HAVE_PING */

    if (sensor == 0)
      {
        sensor = simulator;
        if ((sensor_info = (void *) init_sim()) == NULL)
          exit(1);
      }

    if ((si = init_sonar(dpy, win)) == (sonar_info *) 0)
	exit(1);


    /* Sonar loop */

    while (1) {

	/* Call the sensor and display the results */

	gettimeofday(&start, (struct timezone *) 0);
	bl = sensor(si, sensor_info);
	Sonar(si, bl);

        /* Set up and sleep for the next one */

	si->current = (si->current - 1) % 90;
	if (si->current == 0)
	  si->sweepnum++;
	XSync (dpy, False);
	gettimeofday(&finish, (struct timezone *) 0);
	sleeptime = si->delay - delta(&start, &finish);
        screenhack_handle_events (dpy);
	if (sleeptime > 0L)
	    usleep(sleeptime);

    }
}
