/* sonar.c --- Simulate a sonar screen.
 * Copyright (C) 1998-2006 by Stephen Martin and Jamie Zawinski
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
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
 * In order to use the ping sensor on most systems, this program must be
 * installed as setuid root, so that it can create an ICMP RAW socket.  Root
 * privileges are disavowed shortly after startup (just after connecting to
 * the X server and reading the resource database) so this is *believed* to
 * be a safe thing to do, but it is usually recommended that you have as few
 * setuid programs around as possible, on general principles.
 *
 * It is not necessary to make it setuid on MacOS systems, because on those
 * systems, unprivileged programs can ping by using ICMP DGRAM sockets
 * instead of ICMP RAW.
 *
 * It should be easy to extend this code to support other sorts of sensors.
 * Some ideas:
 *   - search the output of "netstat" for the list of hosts to ping;
 *   - plot the contents of /proc/interrupts;
 *   - plot the process table, by process size, cpu usage, or total time;
 *   - plot the logged on users by idle time or cpu usage.
 *
 * $Revision: 1.57 $
 *
 * Version 1.0 April 27, 1998.
 * - Initial version, by Stephen Martin <smartin@vanderfleet-martin.net>
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

#undef usleep /* conflicts with unistd.h on OSX */

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
 * This represents an object that is visible on the scope.
 */

typedef struct Bogie {
    char *name;			/* The name of the thing being displayed */
    char *desc;			/* Beneath the name (e.g., ping time) */
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

typedef struct ping_target ping_target;

typedef struct sonar_info sonar_info;
struct sonar_info {
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
    Bogie *visible;		/* List of visible objects */
    int current;		/* Current position of sweep */
    int sweepnum;               /* The current id of the sweep */
    int delay;			/* how long between each frame of the anim */

    int TTL;			/* The number of ticks that bogies are visible
                                   on the screen before they fade away. */

  ping_target *last_ptr;

  Bogie *(*sensor)(sonar_info *, void *); /* The current sensor */
  void *sensor_info;			  /* Information about the sensor */

};

static Bool debug_p = False;
static Bool resolve_p = True;
static Bool times_p = True;


/*
 * A list of targets to ping.
 */

struct ping_target {
    char *name;			/* The name of the target */
#ifdef HAVE_PING
    struct sockaddr address;	/* The address of the target */
#endif /* HAVE_PING */
    struct ping_target *next;	/* The next one in the list */
};


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

    distance *= 1000;
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

/* Packs an IP address quad into bigendian network order. */
static unsigned long
pack_addr (unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
  unsigned long i = (((a & 255) << 24) |
                     ((b & 255) << 16) |
                     ((c & 255) <<  8) |
                     ((d & 255)      ));
  return htonl (i);
}

/* Unpacks an IP address quad from bigendian network order. */
static void
unpack_addr (unsigned long addr,
             unsigned int *a, unsigned int *b,
             unsigned int *c, unsigned int *d)
{
  addr = ntohl (addr);
  *a = (addr >> 24) & 255;
  *b = (addr >> 16) & 255;
  *c = (addr >>  8) & 255;
  *d = (addr      ) & 255;
}


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

  unsigned int ip[4];
  char c;

  iaddr = (struct sockaddr_in *) &(target->address);
  iaddr->sin_family = AF_INET;

  if (4 == sscanf (target->name, " %u.%u.%u.%u %c",
                   &ip[0], &ip[1], &ip[2], &ip[3], &c))
    {
      /* It's an IP address.
       */
      if (ip[3] == 0)
        {
          if (debug_p > 1)
            fprintf (stderr, "%s:   ignoring bogus IP %s\n",
                     progname, target->name);
          return 0;
        }

      iaddr->sin_addr.s_addr = pack_addr (ip[0], ip[1], ip[2], ip[3]);
      if (resolve_p)
        hent = gethostbyaddr ((const char *) &iaddr->sin_addr.s_addr,
                              sizeof(iaddr->sin_addr.s_addr),
                              AF_INET);
      else
        hent = 0;

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


      /* don't waste time being confused by non-hostname tokens
         in .ssh/known_hosts */
      if (!strcmp (target->name, "ssh-rsa") ||
          !strcmp (target->name, "ssh-dsa") ||
          !strcmp (target->name, "ssh-dss") ||
          strlen (target->name) >= 80)
        return 0;

      hent = gethostbyname (target->name);
      if (!hent)
        {
          if (debug_p)
            fprintf (stderr, "%s: could not resolve host:  %s\n",
                     progname, target->name);
          return 0;
        }

      memcpy (&iaddr->sin_addr, hent->h_addr_list[0],
              sizeof(iaddr->sin_addr));

      if (debug_p > 1)
        {
          unsigned int a, b, c, d;
          unpack_addr (iaddr->sin_addr.s_addr, &a, &b, &c, &d);
          fprintf (stderr, "%s:   %s => %d.%d.%d.%d\n",
                   progname, target->name, a, b, c, d);
        }
    }
  return 1;
}


static void
print_host (FILE *out, unsigned long ip, const char *name)
{
  char ips[50];
  unsigned int a, b, c, d;
  unpack_addr (ip, &a, &b, &c, &d);		/* ip is in network order */
  sprintf (ips, "%u.%u.%u.%u", a, b, c, d);
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

    /* Don't ever use loopback (127.0.0.x) hosts */
    {
      struct sockaddr_in *iaddr = (struct sockaddr_in *) &(target->address);
      unsigned long ip = iaddr->sin_addr.s_addr;

      if ((ntohl (ip) & 0xFFFFFF00L) == 0x7f000000L)  /* 127.0.0.x */
        {
          if (debug_p)
            fprintf (stderr, "%s:   ignoring loopback host %s\n",
                     progname, target->name);
          goto target_init_error;
        }
    }

    /* Don't ever use broadcast (255.x.x.x) hosts */
    {
      struct sockaddr_in *iaddr = (struct sockaddr_in *) &(target->address);
      unsigned long ip = iaddr->sin_addr.s_addr;
      if ((ntohl (ip) & 0xFF000000L) == 0xFF000000L)  /* 255.x.x.x */
        {
          if (debug_p)
            fprintf (stderr, "%s:   ignoring broadcast host %s\n",
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

    /* Kludge: on OSX, variables have not been expanded in the command
       line arguments, so as a special case, allow the string to begin
       with literal "$HOME/" or "~/".

       This is so that the "Known Hosts" menu item in sonar.xml works.
     */
    if (!strncmp(fname, "~/", 2) || !strncmp(fname, "$HOME/", 6)) {
      char *s = strchr (fname, '/');
      strcpy (buf, getenv("HOME"));
      strcat (buf, s);
      fname = buf;
    }

    /* Open the file */

    if ((fp = fopen(fname, "r")) == NULL) {
	char msg[1024];
        sprintf(msg, "%s: unable to open host file %s", progname, fname);
#ifdef HAVE_COCOA
        if (debug_p)  /* on OSX don't syslog this */
#endif
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

      ping_target *rest2;
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
 * the same subnet.  'base' ip is in network order; 0 means localhost.
 *
 * Returns:
 *    A list of all of the hosts on this net.
 */

static ping_target *
subnetHostsList(unsigned long n_base, int subnet_width)
{
    unsigned long h_mask;   /* host order */
    unsigned long h_base;   /* host order */

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

    h_mask = 0;
    for (i = 0; i < subnet_width; i++)
      h_mask |= (1L << (31-i));

    /* If no base IP specified, assume localhost. */
    if (n_base == 0)
      n_base = pack_addr (hent->h_addr_list[0][0],
                          hent->h_addr_list[0][1],
                          hent->h_addr_list[0][2],
                          hent->h_addr_list[0][3]);
    h_base = ntohl (n_base);

    if (h_base == 0x7F000001L)   /* 127.0.0.1 in host order */
      {
        unsigned int a, b, c, d;
        unpack_addr (n_base, &a, &b, &c, &d);
        fprintf (stderr,
                 "%s: unable to determine local subnet address: \"%s\"\n"
                 "       resolves to loopback address %u.%u.%u.%u.\n",
                 progname, hostname, a, b, c, d);
        return NULL;
      }

    for (i = 255; i >= 0; i--) {
        unsigned int a, b, c, d;
        int ip = (h_base & 0xFFFFFF00L) | i;     /* host order */
      
        if ((ip & h_mask) != (h_base & h_mask))  /* not in mask range at all */
          continue;
        if ((ip & ~h_mask) == 0)                 /* broadcast address */
          continue;
        if ((ip & ~h_mask) == ~h_mask)           /* broadcast address */
          continue;

        unpack_addr (htonl (ip), &a, &b, &c, &d);
        sprintf (address, "%u.%u.%u.%u", a, b, c, d);

        if (debug_p > 1)
          {
            unsigned int aa, ab, ac, ad;
            unsigned int ma, mb, mc, md;
            unpack_addr (htonl (h_base & h_mask), &aa, &ab, &ac, &ad);
            unpack_addr (htonl (h_mask),          &ma, &mb, &mc, &md);
            fprintf (stderr,
                     "%s:  subnet: %s (%u.%u.%u.%u & %u.%u.%u.%u / %d)\n",
                     progname, address,
                     aa, ab, ac, ad,
                     ma, mb, mc, md,
                     subnet_width);
          }

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

static ping_target *parse_mode (sonar_info *, Bool ping_works_p);

/* yes, there is only one, even when multiple savers are running in the
   same address space - since we can only open this socket before dropping
   privs.
 */
static int global_icmpsock = 0;

static ping_info *
init_ping(sonar_info *si)
{

  Bool socket_initted_p = False;
  Bool socket_raw_p = False;

    /* Local Variables */

    ping_info *pi = NULL;		/* The new ping_info struct */
    ping_target *pt;			/* Used to count the targets */

    /* Create the ping info structure */

    if ((pi = (ping_info *) calloc(1, sizeof(ping_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
	goto ping_init_error;
    }

    /* Create the ICMP socket.  Do this before dropping privs.

       Raw sockets can only be opened by root (or setuid root), so we
       only try to do this when the effective uid is 0.

       We used to just always try, and notice the failure.  But apparently
       that causes "SELinux" to log spurious warnings when running with the
       "strict" policy.  So to avoid that, we just don't try unless we
       know it will work.

       On MacOS X, we can avoid the whole problem by using a
       non-privileged datagram instead of a raw socket.
     */
    if (global_icmpsock) {
      pi->icmpsock = global_icmpsock;
      socket_initted_p = True;
      if (debug_p)
        fprintf (stderr, "%s: re-using icmp socket\n", progname);

    } else if ((pi->icmpsock = 
                socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)) >= 0) {
      socket_initted_p = True;

    } else if (geteuid() == 0 &&
               (pi->icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) >= 0) {
        socket_initted_p = True;
        socket_raw_p = True;
    }

    if (socket_initted_p) {
      global_icmpsock = pi->icmpsock;
      socket_initted_p = True;
      if (debug_p)
        fprintf (stderr, "%s: opened %s icmp socket\n", progname,
                 (socket_raw_p ? "raw" : "dgram"));
    } else if (debug_p)
      fprintf (stderr, "%s: unable to open icmp socket\n", progname);

    /* Disavow privs */

    setuid(getuid());


    pi->pid = getpid() & 0xFFFF;
    pi->seq = 0;
    pi->timeout = get_integer_resource(si->dpy, "pingTimeout", "PingTimeout");

    /* Generate a list of targets */

    pi->targets = parse_mode (si, socket_initted_p);
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
# ifdef GETTIMEOFDAY_TWO_ARGS
    gettimeofday((struct timeval *) &packet[sizeof(struct ICMP)],
		 (struct timezone *) 0);
# else
    gettimeofday((struct timeval *) &packet[sizeof(struct ICMP)]);
# endif

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
    unsigned int fromlen;  /* Posix says socklen_t, but that's not portable */
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

# ifdef GETTIMEOFDAY_TWO_ARGS
	gettimeofday(&now, (struct timezone *) 0);
# else
	gettimeofday(&now);
# endif
	ip = (struct ip *) packet;
        iphdrlen = IP_HDRLEN(ip) << 2;
	icmph = (struct ICMP *) &packet[iphdrlen];
	then  = (struct timeval *) &packet[iphdrlen + sizeof(struct ICMP)];


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

# if 0  /* Don't need to do this -- the host names are already as
           resolved as they're going to get.  (We stored the resolved
           name in the outgoing ping packet, so that same string just
           came back to us.)
         */

        /* If the name is an IP addr, try to resolve it. */
        {
          int iip[4];
          char c;
          if (4 == sscanf(name, " %d.%d.%d.%d %c",
                          &iip[0], &iip[1], &iip[2], &iip[3], &c))
            {
              struct sockaddr_in iaddr;
              struct hostent *h;
              iaddr.sin_addr.s_addr = pack_addr (iip[0],iip[1],iip[2],iip[3]);
              if (resolve_p)
                h = gethostbyaddr ((const char *) &iaddr.sin_addr.s_addr,
                                   sizeof(iaddr.sin_addr.s_addr),
                                   AF_INET);
              else
                h = 0;

              if (h && h->h_name && *h->h_name)
                {
                  free (name);
                  name = strdup (h->h_name);
                }
            }
        }
# endif /* 0 */

	/* Create the new Bogie and add it to the list we are building */

	if ((new = newBogie(name, 0, si->current, si->TTL)) == NULL)
	    return bl;
	new->next = bl;
	bl = new;

        {
          float msec = delta(then, &now) / 1000.0;

          if (times_p)
            {
              if (new->desc) free (new->desc);
              new->desc = (char *) malloc (30);
              if      (msec > 99) sprintf (new->desc, "    %.0f ms   ", msec);
              else if (msec >  9) sprintf (new->desc, "    %.1f ms   ", msec);
              else if (msec >  1) sprintf (new->desc, "    %.2f ms   ", msec);
              else                sprintf (new->desc, "    %.3f ms   ", msec);
            }

          if (debug_p && times_p)  /* print ping-like stuff to stdout */
            {
              struct sockaddr_in *iaddr = (struct sockaddr_in *) &from;
              unsigned int a, b, c, d;
              char ipstr[20];
              char *s = strdup (new->desc);
              char *s2 = s, *s3 = s;
              while (*s2 == ' ') s2++;
              s3 = strchr (s2, ' ');
              if (s3) *s3 = 0;

              unpack_addr (iaddr->sin_addr.s_addr, &a, &b, &c, &d);
              sprintf (ipstr, "%d.%d.%d.%d", a, b, c, d);

              fprintf (stdout,
                       "%3d bytes from %28s: "
                       "icmp_seq=%-4d ttl=%d time=%s ms\n",
                       result,
                       name,
                       /*ipstr,*/
                       ICMP_SEQ(icmph), si->TTL, s2);
              free (s);
            }

          /* Don't put anyone *too* close to the center of the screen. */
          msec += 0.6;

          new->distance = msec * 10;
        }
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

    int tick = si->current * -1 + 1;
    if ((si->last_ptr == NULL) && (tick == 1))
	si->last_ptr = pi->targets;

    if (pi->numtargets <= 90) {
	int xdrant = 90 / pi->numtargets;
	if ((tick % xdrant) == 0) {
	    if (si->last_ptr != (ping_target *) 0) {
		sendping(pi, si->last_ptr);
		si->last_ptr = si->last_ptr->next;
	    }
	}

    } else if (pi->numtargets > 90) {
	if (si->last_ptr != (ping_target *) 0) {
	    sendping(pi, si->last_ptr);
	    si->last_ptr = si->last_ptr->next;
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

#define BELLRAND(x) (((random()%(x)) + (random()%(x)) + (random()%(x)))/3)

static sim_info *
init_sim(Display *dpy) 
{
    /* Local Variables */

    sim_info *si;
    int i;

    int maxdist = 20;  /* larger than this is off the (log) display */

    /* Create the simulation info structure */

    if ((si = (sim_info *) calloc(1, sizeof(sim_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
	return NULL;
    }

    /* Team A */

    si->numA = get_integer_resource(dpy, "teamACount", "TeamACount");
    if ((si->teamA = (sim_target *)calloc(si->numA, sizeof(sim_target)))
	== NULL) {
	free(si);
	fprintf(stderr, "%s: Out of Memory\n", progname);
	return NULL;
    }
    si->teamAID = get_string_resource(dpy, "teamAName", "TeamAName");
    for (i = 0; i < si->numA; i++) {
	if ((si->teamA[i].name = (char *) malloc(strlen(si->teamAID) + 4))
	    == NULL) {
	    free(si);
	    fprintf(stderr, "%s: Out of Memory\n", progname);
	    return NULL;
	}
	sprintf(si->teamA[i].name, "%s%03d", si->teamAID, i+1);
	si->teamA[i].nexttick = random() % 90;
	si->teamA[i].nextdist = BELLRAND(maxdist);
	si->teamA[i].movedonsweep = -1;
    }

    /* Team B */

    si->numB = get_integer_resource(dpy, "teamBCount", "TeamBCount");
    if ((si->teamB = (sim_target *)calloc(si->numB, sizeof(sim_target)))
	== NULL) {
	free(si);
	fprintf(stderr, "%s: Out of Memory\n", progname);
	return NULL;
    }
    si->teamBID = get_string_resource(dpy, "teamBName", "TeamBName");
    for (i = 0; i < si->numB; i++) {
	if ((si->teamB[i].name = (char *) malloc(strlen(si->teamBID) + 4))
	    == NULL) {
	    free(si);
	    fprintf(stderr, "%s: Out of Memory\n", progname);
	    return NULL;
	}
	sprintf(si->teamB[i].name, "%s%03d", si->teamBID, i+1);
	si->teamB[i].nexttick = random() % 90;
	si->teamB[i].nextdist = BELLRAND(maxdist);
	si->teamB[i].movedonsweep = -1;
    }

    /* Done */

    return si;
}

/*
 * Creates and returns a drawing mask for the scope:
 * mask out anything outside of the disc.
 */
static Pixmap
scope_mask (Display *dpy, Window win, sonar_info *si)
{
  XGCValues gcv;
  Pixmap mask = XCreatePixmap(dpy, win, si->width, si->height, 1);
  GC gc;
  gcv.foreground = 0;
  gc = XCreateGC (dpy, mask, GCForeground, &gcv);
  XFillRectangle (dpy, mask, gc, 0, 0, si->width, si->height);
  XSetForeground (dpy, gc, 1);
  XFillArc(dpy, mask, gc, si->minx, si->miny, 
           si->maxx - si->minx, si->maxy - si->miny,
           0, 360 * 64);
  XFreeGC (dpy, gc);
  return mask;
}


static void
reshape (sonar_info *si)
{
  XWindowAttributes xgwa;
  Pixmap mask;
  XGetWindowAttributes(si->dpy, si->win, &xgwa);
  si->width = xgwa.width;
  si->height = xgwa.height;
  si->centrex = si->width / 2;
  si->centrey = si->height / 2;
  si->maxx = si->centrex + MY_MIN(si->centrex, si->centrey) - 10;
  si->minx = si->centrex - MY_MIN(si->centrex, si->centrey) + 10;
  si->maxy = si->centrey + MY_MIN(si->centrex, si->centrey) - 10;
  si->miny = si->centrey - MY_MIN(si->centrex, si->centrey) + 10;
  si->radius = si->maxx - si->centrex;

  /* Install the clip mask... */
  if (! debug_p) {
    mask = scope_mask (si->dpy, si->win, si);
    XSetClipMask(si->dpy, si->text, mask);
    XSetClipMask(si->dpy, si->erase, mask);
    XFreePixmap (si->dpy, mask); /* it's been copied into the GCs */
  }
}


/*
 * Update the location of a simulated bogie.
 */

static void
updateLocation(sim_target *t) 
{

    int xdist, xtick;

    xtick = (int) (random() %  3) - 1;
    xdist = (int) (random() % 11) - 5;
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
computeStringX(sonar_info *si, const char *label, int x) 
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

static int
computeStringY(sonar_info *si, int y) 
{

    int fheight = si->font->ascent /* + si->font->descent */;
    return y + fheight;
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
DrawBogie(sonar_info *si, int draw, const char *name, const char *desc,
          int degrees, int distance, int ttl, int age) 
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

    x += 3;  /* move away from the dot */
    y += 7;
    y = computeStringY(si, y);
    XDrawString(si->dpy, si->win, gc,
                computeStringX(si, name, x), y,
                name, strlen(name));

    if (desc && *desc)
      {
        y = computeStringY(si, y);
        XDrawString(si->dpy, si->win, gc,
                    computeStringX(si, desc, x), y,
                    desc, strlen(desc));
      }
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

    /* Check for expired tagets and remove them from the visible list */

    prev = NULL;
    for (bp = si->visible; bp != NULL; bp = (bp ? bp->next : 0)) {

	/*
	 * Remove it from the visible list if it's expired or we have
	 * a new target with the same name.
	 */

	bp->age ++;

	if (((bp->tick == si->current) && (++bp->age >= bp->ttl)) ||
	    (findNode(bl, bp->name) != NULL)) {

#ifndef HAVE_COCOA    /* we repaint every frame: no need to erase */
	    DrawBogie(si, 0, bp->name, bp->desc, bp->tick,
		      bp->distance, bp->ttl, bp->age);
#endif /* HAVE_COCOA */

	    if (prev == NULL)
		si->visible = bp->next;
	    else
		prev->next = bp->next;
	    freeBogie(bp);
            bp = prev;
	} else
	    prev = bp;
    }

    /* Draw the sweep */

    {
      int start_deg = si->current * 4 * 64;
      int end_deg   = start_deg + (si->sweep_degrees * 64);
      int seg_deg = (end_deg - start_deg) / si->sweep_segs;
      if (seg_deg <= 0) seg_deg = 1;

      /* Remove the trailing wedge the sonar */
      XFillArc(si->dpy, si->win, si->erase, si->minx, si->miny, 
               si->maxx - si->minx, si->maxy - si->miny, 
               end_deg, 
               4 * 64);

      for (i = 0; i < si->sweep_segs; i++) {
        int ii = si->sweep_segs - i - 1;
	XSetForeground (si->dpy, si->hi, si->sweep_colors[ii].pixel);
	XFillArc (si->dpy, si->win, si->hi, si->minx, si->miny, 
                  si->maxx - si->minx, si->maxy - si->miny,
                  start_deg,
                  seg_deg * (ii + 1));
      }
    }

    /* Move the new targets to the visible list */

    for (bp = bl; bp != (Bogie *) 0; bp = bl) {
	bl = bl->next;
	bp->next = si->visible;
	si->visible = bp;
    }

    /* Draw the visible targets */

    for (bp = si->visible; bp != NULL; bp = bp->next) {
	if (bp->age < bp->ttl)		/* grins */
	   DrawBogie(si, 1, bp->name, bp->desc,
                     bp->tick, bp->distance, bp->ttl,bp->age);
    }

    /* Redraw the grid */

    drawGrid(si);
}


static ping_target *
parse_mode (sonar_info *si, Bool ping_works_p)
{
  char *source = get_string_resource (si->dpy, "ping", "Ping");
  char *token, *end;
#ifdef HAVE_PING
  char dummy;
#endif

  ping_target *hostlist = 0;

  if (!source) source = strdup("");

  if (!*source || !strcmp (source, "default"))
    {
      free (source);
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
           *next &&
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
      if ((4 == sscanf (token, "%u.%u.%u/%u %c",    &n0,&n1,&n2,    &m,&d)) ||
          (5 == sscanf (token, "%u.%u.%u.%u/%u %c", &n0,&n1,&n2,&n3,&m,&d)))
        {
          /* subnet: A.B.C.D/M
             subnet: A.B.C/M
           */
          unsigned long ip = pack_addr (n0, n1, n2, n3);
          new = subnetHostsList(ip, m);
        }
      else if (4 == sscanf (token, "%u.%u.%u.%u %c", &n0, &n1, &n2, &n3, &d))
        {
          /* IP: A.B.C.D
           */
          new = newHost (token);
        }
      else if (!strcmp (token, "subnet"))
        {
          new = subnetHostsList(0, 24);
        }
      else if (1 == sscanf (token, "subnet/%u %c", &m, &dummy))
        {
          new = subnetHostsList(0, m);
        }
      else if (*token == '.' || *token == '/' || 
               *token == '$' || *token == '~' ||
               !stat (token, &st))
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

          si->sensor = ping;
        }
#endif /* HAVE_PING */

      token = next + 1;
      while (token < end &&
             (*token == ',' || *token == ' ' ||
              *token == '\t' || *token == '\n'))
        token++;
    }

  free (source);
  return hostlist;
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

static void *
sonar_init (Display *dpy, Window win)
{
    /* Local Variables */

    XGCValues gcv;
    XWindowAttributes xwa;
    sonar_info *si;
    XColor start, end;
    int h1, h2;
    double s1, s2, v1, v2;

    debug_p = get_boolean_resource (dpy, "debug", "Debug");
    resolve_p = get_boolean_resource (dpy, "resolve", "Resolve");
    times_p = get_boolean_resource (dpy, "showTimes", "ShowTimes");

    /* Create the Sonar information structure */

    if ((si = (sonar_info *) calloc(1, sizeof(sonar_info))) == NULL) {
	fprintf(stderr, "%s: Out of memory\n", progname);
        exit (1);
    }

    /* Initialize the structure for the current environment */

    si->dpy = dpy;
    si->win = win;
    si->visible = NULL;

    XGetWindowAttributes(dpy, win, &xwa);
    si->cmap = xwa.colormap;

    si->current = 0;
    si->sweepnum = 0;

    /* Get the font */

    {
      char *fn = get_string_resource (dpy, "font", "Font");
      if (((si->font = XLoadQueryFont(dpy, fn)) == NULL) &&
          ((si->font = XLoadQueryFont(dpy, "fixed")) == NULL)) {
	fprintf(stderr, "%s: can't load an appropriate font\n", progname);
        exit (1);
      }
      if (fn) free (fn);
    }

    /* Get the delay between animation frames */

    si->delay = get_integer_resource (dpy, "delay", "Integer");

    if (si->delay < 0) si->delay = 0;
    si->TTL = get_integer_resource(dpy, "ttl", "TTL");

    /* Create the Graphics Contexts that will be used to draw things */

    gcv.foreground = 
	get_pixel_resource (dpy, si->cmap, "sweepColor", "SweepColor");
    si->hi = XCreateGC(dpy, win, GCForeground, &gcv);
    gcv.font = si->font->fid;
    si->text = XCreateGC(dpy, win, GCForeground|GCFont, &gcv);
    gcv.foreground = get_pixel_resource(dpy, si->cmap,
                                        "scopeColor", "ScopeColor");
    si->erase = XCreateGC (dpy, win, GCForeground, &gcv);
    gcv.foreground = get_pixel_resource(dpy, si->cmap,
                                        "gridColor", "GridColor");
    si->grid = XCreateGC (dpy, win, GCForeground, &gcv);

    reshape (si);

    /* Compute pixel values for fading text on the display */
    {
      char *s = get_string_resource(dpy, "textColor", "TextColor");
      XParseColor(dpy, si->cmap, s, &start);
      if (s) free (s);
      s = get_string_resource(dpy, "scopeColor", "ScopeColor");
      XParseColor(dpy, si->cmap, s, &end);
      if (s) free (s);
    }

    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);
    rgb_to_hsv (end.red, end.green, end.blue, &h2, &s2, &v2);

    si->text_steps = get_integer_resource(dpy, "textSteps", "TextSteps");
    if (si->text_steps < 0 || si->text_steps > 255)
      si->text_steps = 10;

    si->text_colors = (XColor *) calloc(si->text_steps, sizeof(XColor));
    make_color_ramp (dpy, si->cmap,
                     h1, s1, v1,
                     h2, s2, v2,
                     si->text_colors, &si->text_steps,
                     False, True, False);

    /* Compute the pixel values for the fading sweep */

    {
      char *s = get_string_resource(dpy, "sweepColor", "SweepColor");
      XParseColor(dpy, si->cmap, s, &start);
      if (s) free (s);
    }

    rgb_to_hsv (start.red, start.green, start.blue, &h1, &s1, &v1);

    si->sweep_degrees = get_integer_resource(dpy, "sweepDegrees", "Degrees");
    if (si->sweep_degrees <= 0) si->sweep_degrees = 20;
    if (si->sweep_degrees > 350) si->sweep_degrees = 350;

    si->sweep_segs = get_integer_resource(dpy, "sweepSegments", "SweepSegments");
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


# ifdef HAVE_PING
    si->sensor_info = (void *) init_ping(si);
# else  /* !HAVE_PING */
    parse_mode (dpy, 0);  /* just to check argument syntax */
# endif /* !HAVE_PING */

    if (!si->sensor)
      {
        si->sensor = simulator;
        si->sensor_info = (void *) init_sim(dpy);
        if (! si->sensor_info)
          exit(1);
      }

    /* Done */

    return si;
}


static unsigned long
sonar_draw (Display *dpy, Window window, void *closure)
{
  sonar_info *si = (sonar_info *) closure;
  Bogie *bl;
  struct timeval start, finish;
  long delay;

# ifdef HAVE_COCOA  /* repaint the whole window so that antialiasing works */
  XClearWindow (dpy,window);
  XFillRectangle (dpy, window, si->erase, 0, 0, si->width, si->height);
# endif

  /* Call the sensor and display the results */

# ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&start, (struct timezone *) 0);
# else
  gettimeofday(&start);
# endif
  bl = si->sensor(si, si->sensor_info);
  Sonar(si, bl);

  /* Set up and sleep for the next one */

  si->current = (si->current - 1) % 90;
  if (si->current == 0)
    si->sweepnum++;

# ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday(&finish, (struct timezone *) 0);
# else
  gettimeofday(&finish);
# endif

  delay = si->delay - delta(&start, &finish);
  if (delay < 0) delay = 0;
  return delay;
}


static void
sonar_reshape (Display *dpy, Window window, void *closure, 
                 unsigned int w, unsigned int h)
{
  sonar_info *si = (sonar_info *) closure;
  XClearWindow (si->dpy, si->win);
  reshape (si);
}

static Bool
sonar_event (Display *dpy, Window window, void *closure, XEvent *event)
{
  return False;
}

static void
sonar_free (Display *dpy, Window window, void *closure)
{
}



static const char *sonar_defaults [] = {
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
    "*resolve:	       true",
    "*showTimes:       true",
    ".debug:	       false",
    0
};

static XrmOptionDescRec sonar_options [] = {
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
    {"-no-dns",        ".resolve",     XrmoptionNoArg, "False" },
    {"-no-times",      ".showTimes",   XrmoptionNoArg, "False" },
    {"-debug",         ".debug",       XrmoptionNoArg, "True" },
    { 0, 0, 0, 0 }
};


XSCREENSAVER_MODULE ("Sonar", sonar)
