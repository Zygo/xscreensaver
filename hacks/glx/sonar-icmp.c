/* sonar, Copyright (c) 1998-2019 Jamie Zawinski and Stephen Martin
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * This implements the "ping" sensor for sonar.
 */

#include "screenhackI.h"
#include "sonar.h"
#include "version.h"
#include "async_netdb.h"

#undef usleep /* conflicts with unistd.h on OSX */

#ifdef USE_IPHONE
  /* Note: to get this to compile for iPhone, you need to fix Xcode!
     The icmp headers exist for the simulator build environment, but
     not for the real-device build environment.  This appears to 
     just be an Apple bug, not intentional.

     xc=/Applications/Xcode.app/Contents
     for path in    /Developer/Platforms/iPhone*?/Developer/SDKs/?* \
                 $xc/Developer/Platforms/iPhone*?/Developer/SDKs/?* ; do
       for file in \
         /usr/include/netinet/ip.h \
         /usr/include/netinet/in_systm.h \
         /usr/include/netinet/ip_icmp.h \
         /usr/include/netinet/ip_var.h \
         /usr/include/netinet/udp.h
       do
         ln -s "$file" "$path$file"
       done
     done
  */
#endif

#ifndef HAVE_MOBILE
# define READ_FILES
#endif

#if defined(HAVE_ICMP) || defined(HAVE_ICMPHDR)
# include <unistd.h>
# include <sys/stat.h>
# include <limits.h>
# include <signal.h>
# include <fcntl.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/ipc.h>
# ifndef HAVE_ANDROID
#  include <sys/shm.h>
# endif
# include <sys/socket.h>
# include <netinet/in_systm.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/udp.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <errno.h>
# ifdef HAVE_GETIFADDRS
#  include <ifaddrs.h>
# endif
# ifdef HAVE_LIBCAP
#  include <sys/capability.h>
# endif
#endif /* HAVE_ICMP || HAVE_ICMPHDR */

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

#ifndef HAVE_MOBILE
# define LOAD_FILES
#endif

#ifndef HAVE_PING

sonar_sensor_data *
sonar_init_ping (Display *dpy, char **error_ret, char **desc_ret, 
                 const char *subnet, int timeout,
                 Bool resolve_p, Bool times_p, Bool debug_p)
{
  if (! (!subnet || !*subnet || !strcmp(subnet, "default")))
    fprintf (stderr, "%s: not compiled with support for pinging hosts.\n",
             progname);
  return 0;
}

#else /* HAVE_PING -- whole file */


#if defined(__DECC) || defined(_IP_VHL)
   /* This is how you do it on DEC C, and possibly some BSD systems. */
# define IP_HDRLEN(ip)   ((ip)->ip_vhl & 0x0F)
#else
   /* This is how you do it on everything else. */
# define IP_HDRLEN(ip)   ((ip)->ip_hl)
#endif

/* yes, there is only one, even when multiple savers are running in the
   same address space - since we can only open this socket before dropping
   privs.
 */
static int global_icmpsock = 0;

/* Set by a signal handler. */
static int timer_expired;



static u_short checksum(u_short *, int);
static long delta(struct timeval *, struct timeval *);


typedef struct {
  Display *dpy;                 /* Only used to get *useThreads. */

  char *version;		/* short version number of xscreensaver */
  int icmpsock;			/* socket for sending pings */
  int pid;			/* our process ID */
  int seq;			/* packet sequence number */
  int timeout;			/* packet timeout */

  int target_count;
  sonar_bogie *targets;		/* the hosts we will ping;
                                   those that pong end up on ssd->pending. */
  sonar_bogie *last_pinged;	/* pointer into 'targets' list */
  double last_ping_time;

  Bool resolve_p;
  Bool times_p;
  Bool debug_p;

} ping_data;

typedef struct {
  async_name_from_addr_t lookup_name;
  async_addr_from_name_t lookup_addr;
  async_netdb_sockaddr_storage_t address;	/* ip address */
  socklen_t addrlen;
  char *fallback;
} ping_bogie;



/* Packs an IP address quad into bigendian network order. */
static in_addr_t
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




/* Resolves the bogie's name (either a hostname or ip address string)
   to a hostent.  Returns 1 if successful, 0 if something went wrong.
 */
static int
resolve_bogie_hostname (ping_data *pd, sonar_bogie *sb, Bool resolve_p)
{
  ping_bogie *pb = (ping_bogie *) sb->closure;

  unsigned int ip[4];
  char c;

  if (4 == sscanf (sb->name, " %u.%u.%u.%u %c",
                   &ip[0], &ip[1], &ip[2], &ip[3], &c))
    {
      /* It's an IP address.
       */
      struct sockaddr_in *iaddr = (struct sockaddr_in *) &(pb->address);

      if (ip[3] == 0)
        {
          if (pd->debug_p > 1)
            fprintf (stderr, "%s:   ignoring bogus IP %s\n",
                     progname, sb->name);
          return 0;
        }

      iaddr->sin_family = AF_INET;
      iaddr->sin_addr.s_addr = pack_addr (ip[0], ip[1], ip[2], ip[3]);
      pb->addrlen = sizeof(struct sockaddr_in);

      if (resolve_p)
        {
          pb->lookup_name =
            async_name_from_addr_start (pd->dpy,
                                        (const struct sockaddr *)&pb->address,
                                        pb->addrlen);
          if (!pb->lookup_name)
            {
              fprintf (stderr, "%s:   unable to start host resolution.\n",
                       progname);
            }
        }
    }
  else
    {
      /* It's a host name. */

      /* don't waste time being confused by non-hostname tokens
         in .ssh/known_hosts */
      if (!strcmp (sb->name, "ssh-rsa") ||
          !strcmp (sb->name, "ssh-dsa") ||
          !strcmp (sb->name, "ssh-dss") ||
          !strncmp (sb->name, "ecdsa-", 6) ||
          strlen (sb->name) >= 80)
        return 0;

      /* .ssh/known_hosts sometimes contains weirdness like "[host]:port".
         Ignore it. */
      if (strchr (sb->name, '['))
        {
          if (pd->debug_p)
            fprintf (stderr, "%s:   ignoring bogus address \"%s\"\n", 
                     progname, sb->name);
          return 0;
        }

      /* If the name contains a colon, it's probably IPv6. */
      if (strchr (sb->name, ':'))
        {
          if (pd->debug_p)
            fprintf (stderr, "%s:   ignoring ipv6 address \"%s\"\n", 
                     progname, sb->name);
          return 0;
        }

      pb->lookup_addr = async_addr_from_name_start(pd->dpy, sb->name);
      if (!pb->lookup_addr)
        {
          if (pd->debug_p)
            /* Either address space exhaustion or RAM exhaustion. */
            fprintf (stderr, "%s:   unable to start host resolution.\n",
                     progname);
          return 0;
        }
    }
  return 1;
}


static void
print_address (FILE *out, int width, const void *sockaddr, socklen_t addrlen)
{
#ifdef HAVE_GETADDRINFO
  char buf[NI_MAXHOST];
#else
  char buf[50];
#endif

  const struct sockaddr *addr = (const struct sockaddr *)sockaddr;
  const char *ips = buf;

  if (!addr->sa_family)
    ips = "<no address>";
  else
    {
#ifdef HAVE_GETADDRINFO
      int gai_error = getnameinfo (sockaddr, addrlen, buf, sizeof(buf),
                                   NULL, 0, NI_NUMERICHOST);
      if (gai_error == EAI_SYSTEM)
        ips = strerror(errno);
      else if (gai_error)
        ips = gai_strerror(gai_error);
#else
      switch (addr->sa_family)
        {
        case AF_INET:
          {
            u_long ip = ((struct sockaddr_in *)sockaddr)->sin_addr.s_addr;
            unsigned int a, b, c, d;
            unpack_addr (ip, &a, &b, &c, &d);   /* ip is in network order */
            sprintf (buf, "%u.%u.%u.%u", a, b, c, d);
          }
          break;
        default:
          ips = "<unknown>";
          break;
        }
#endif
    }

  fprintf (out, "%-*s", width, ips);
}


static void
print_host (FILE *out, const sonar_bogie *sb)
{
  const ping_bogie *pb = (const ping_bogie *) sb->closure;
  const char *name = sb->name;
  if (!name || !*name) name = "<unknown>";
  print_address (out, 16, &pb->address, pb->addrlen);
  fprintf (out, " %s\n", name);
}


static Bool
is_address_ok(Bool debug_p, const sonar_bogie *b)
{
  const ping_bogie *pb = (const ping_bogie *) b->closure;
  const struct sockaddr *addr = (const struct sockaddr *)&pb->address;

  switch (addr->sa_family)
    {
    case AF_INET:
      {
        struct sockaddr_in *iaddr = (struct sockaddr_in *) addr;

        /* Don't ever use loopback (127.0.0.x) hosts */
        unsigned long ip = iaddr->sin_addr.s_addr;
        if ((ntohl (ip) & 0xFFFFFF00L) == 0x7f000000L)  /* 127.0.0.x */
          {
            if (debug_p)
              fprintf (stderr, "%s:   ignoring loopback host %s\n",
                       progname, b->name);
            return False;
          }

        /* Don't ever use broadcast (255.x.x.x) hosts */
        if ((ntohl (ip) & 0xFF000000L) == 0xFF000000L)  /* 255.x.x.x */
          {
            if (debug_p)
              fprintf (stderr, "%s:   ignoring broadcast host %s\n",
                       progname, b->name);
            return False;
          }
      }

      break;
    }

  return True;
}


/* Create a sonar_bogie from a host name or ip address string.
   Returns NULL if the name could not be resolved.
 */
static sonar_bogie *
bogie_for_host (sonar_sensor_data *ssd, const char *name, const char *fallback)
{
  ping_data *pd = (ping_data *) ssd->closure;
  sonar_bogie *b = (sonar_bogie *) calloc (1, sizeof(*b));
  ping_bogie *pb = (ping_bogie *) calloc (1, sizeof(*pb));
  Bool resolve_p = pd->resolve_p;

  b->name = strdup (name);
  b->closure = pb;

  if (! resolve_bogie_hostname (pd, b, resolve_p))
    goto FAIL;

  if (! pb->lookup_addr && ! is_address_ok (pd->debug_p, b))
    goto FAIL;

  if (pd->debug_p > 1)
    {
      fprintf (stderr, "%s:   added ", progname);
      print_host (stderr, b);
    }

  if (fallback)
    pb->fallback = strdup (fallback);
  return b;

 FAIL:
  if (b) sonar_free_bogie (ssd, b);

  if (fallback)
    return bogie_for_host (ssd, fallback, NULL);

  return 0;
}


#ifdef READ_FILES

/* Return a list of bogies read from a file.
   The file can be like /etc/hosts or .ssh/known_hosts or probably
   just about anything that has host names in it.
 */
static sonar_bogie *
read_hosts_file (sonar_sensor_data *ssd, const char *filename) 
{
  ping_data *pd = (ping_data *) ssd->closure;
  FILE *fp;
  char buf[LINE_MAX];
  char *p;
  sonar_bogie *list = 0;
  char *addr, *name;
  sonar_bogie *new;

  /* Kludge: on OSX, variables have not been expanded in the command
     line arguments, so as a special case, allow the string to begin
     with literal "$HOME/" or "~/".

     This is so that the "Known Hosts" menu item in sonar.xml works.
   */
  if (!strncmp(filename, "~/", 2) || !strncmp(filename, "$HOME/", 6)) 
    {
      char *s = strchr (filename, '/');
      strcpy (buf, getenv("HOME"));
      strcat (buf, s);
      filename = buf;
    }

  fp = fopen(filename, "r");
  if (!fp)
    {
      char buf2[1024];
      sprintf(buf2, "%s:  %s", progname, filename);
#ifdef HAVE_JWXYZ
      if (pd->debug_p)  /* on OSX don't syslog this */
#endif
        perror (buf2);
      return 0;
    }

  if (pd->debug_p)
    fprintf (stderr, "%s:  reading \"%s\"\n", progname, filename);

  while ((p = fgets(buf, LINE_MAX, fp)))
    {
      while ((*p == ' ') || (*p == '\t'))	/* skip whitespace */
        p++;
      if (*p == '#')				/* skip comments */
        continue;

      /* Get the name and address */

      if ((addr = strtok(buf, " ,;\t\n")))
        name = strtok(0, " ,;\t\n");
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
            addr = 0;
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
              if (pd->debug_p > 1)
                fprintf (stderr, "%s:  skipping bogus name \"%s\" (%s)\n",
                         progname, name, addr);
              name = 0;
            }
        }

      /* Create a new target using first the name then the address */

      if (!name)
        {
          name = addr;
          addr = NULL;
        }

      new = bogie_for_host (ssd, name, addr);

      if (new)
        {
          new->next = list;
          list = new;
        }
    }

  fclose(fp);
  return list;
}
#endif /* READ_FILES */


static sonar_bogie **
found_duplicate_host (const ping_data *pd, sonar_bogie **list,
                      sonar_bogie *bogie)
{
  if (pd->debug_p)
  {
    fprintf (stderr, "%s: deleted duplicate: ", progname);
    print_host (stderr, bogie);
  }

  return list;
}


static sonar_bogie **
find_duplicate_host (const ping_data *pd, sonar_bogie **list,
                     sonar_bogie *bogie)
{
  const ping_bogie *pb = (const ping_bogie *) bogie->closure;
  const struct sockaddr *addr1 = (const struct sockaddr *) &(pb->address);

  while(*list)
    {
      const ping_bogie *pb2 = (const ping_bogie *) (*list)->closure;

      if (!pb2->lookup_addr)
        {
          const struct sockaddr *addr2 =
            (const struct sockaddr *) &(pb2->address);

          if (addr1->sa_family == addr2->sa_family)
            {
              switch (addr1->sa_family)
                {
                case AF_INET:
                  {
                    unsigned long ip1 =
                      ((const struct sockaddr_in *)addr1)->sin_addr.s_addr;
                    const struct sockaddr_in *i2 =
                      (const struct sockaddr_in *)addr2;
                    unsigned long ip2 = i2->sin_addr.s_addr;

                    if (ip1 == ip2)
                      return found_duplicate_host (pd, list, bogie);
                  }
                  break;
#ifdef AF_INET6
                case AF_INET6:
                  {
                    if (! memcmp(
                            &((const struct sockaddr_in6 *)addr1)->sin6_addr,
                            &((const struct sockaddr_in6 *)addr2)->sin6_addr,
                            16))
                      return found_duplicate_host (pd, list, bogie);
                  }
                  break;
#endif
                default:
                  {
                    /* Fallback behavior: Just memcmp the two addresses.

                       For this to work, unused space in the sockaddr must be
                       set to zero. Which may actually be the case:
                       - async_addr_from_name_finish won't put garbage into
                         sockaddr_in.sin_zero or elsewhere unless getaddrinfo
                         does.
                       - ping_bogie is allocated with calloc(). */

                    if (pb->addrlen == pb2->addrlen &&
                        ! memcmp(addr1, addr2, pb->addrlen))
                      return found_duplicate_host (pd, list, bogie);
                  }
                  break;
                }
            }
        }

      list = &(*list)->next;
    }

  return NULL;
}


static sonar_bogie *
delete_duplicate_hosts (sonar_sensor_data *ssd, sonar_bogie *list)
{
  ping_data *pd = (ping_data *) ssd->closure;
  sonar_bogie *head = list;
  sonar_bogie *sb = head;

  while (sb)
    {
      ping_bogie *pb = (ping_bogie *) sb->closure;

      if (!pb->lookup_addr)
        {
          sonar_bogie **sb2 = find_duplicate_host (pd, &sb->next, sb);
          if (sb2)
            *sb2 = (*sb2)->next;
            /* #### sb leaked */
          else
            sb = sb->next;
        }
      else
        sb = sb->next;
    }

  return head;
}


static unsigned long
width_mask (unsigned long width)
{
  unsigned long m = 0;
  int i;
  for (i = 0; i < width; i++)
    m |= (1L << (31-i));
  return m;
}


#ifdef HAVE_GETIFADDRS
static unsigned int
mask_width (unsigned long mask)
{
  int i;
  for (i = 0; i < 32; i++)
    if (mask & (1 << i))
      break;
  return 32-i;
}
#endif


/* Generate a list of bogies consisting of all of the entries on
  the same subnet.  'base' ip is in network order; 0 means localhost.
 */
static sonar_bogie *
subnet_hosts (sonar_sensor_data *ssd, char **error_ret, char **desc_ret,
              unsigned long n_base, int subnet_width)
{
  ping_data *pd = (ping_data *) ssd->closure;
  unsigned long h_mask;   /* host order */
  unsigned long h_base;   /* host order */
  char address[BUFSIZ];
  char *p;
  int i;
  sonar_bogie *new;
  sonar_bogie *list = 0;
  char buf[1024];

  if (subnet_width < 24)
    {
      sprintf (buf,
               "Pinging %lu hosts is a bad\n"
               "idea.  Please use a subnet\n"
               "mask of 24 bits or more.",
               (unsigned long) (1L << (32 - subnet_width)) - 1);
      *error_ret = strdup(buf);
      return 0;
    }
  else if (subnet_width > 30)
    {
      sprintf (buf, 
               "An %d-bit subnet\n"
               "doesn't make sense.\n"
               "Try \"subnet/24\"\n"
               "or \"subnet/29\".\n",
               subnet_width);
      *error_ret = strdup(buf);
      return 0;
    }


  if (pd->debug_p)
    fprintf (stderr, "%s:   adding %d-bit subnet\n", progname, subnet_width);


  if (! n_base)
    {
# ifdef HAVE_GETIFADDRS

      /* To determine the local subnet, we need to know the local IP address.
         Do this by looking at the IPs of every network interface.
      */
      struct in_addr in = { 0, };
      struct ifaddrs *all = 0, *ifa;

      if (pd->debug_p)
        fprintf (stderr, "%s:   listing network interfaces\n", progname);

      getifaddrs (&all);
      for (ifa = all; ifa; ifa = ifa->ifa_next)
        {
          struct in_addr in2;
          unsigned long mask;
          if (! ifa->ifa_addr)
            continue;
          else if (ifa->ifa_addr->sa_family != AF_INET)
            {
              if (pd->debug_p)
                fprintf (stderr, "%s:     if: %4s: %s\n", progname,
                         ifa->ifa_name,
                         (
# ifdef AF_UNIX
                          ifa->ifa_addr->sa_family == AF_UNIX  ? "local" :
# endif
# ifdef AF_LINK
                          ifa->ifa_addr->sa_family == AF_LINK  ? "link"  :
# endif
# ifdef AF_INET6
                          ifa->ifa_addr->sa_family == AF_INET6 ? "ipv6"  :
# endif
                          "other"));
              continue;
            }
          in2 = ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
          mask = ntohl (((struct sockaddr_in *) ifa->ifa_netmask)
                        ->sin_addr.s_addr);
          if (pd->debug_p)
            fprintf (stderr, "%s:     if: %4s: inet = %s /%d 0x%08lx\n",
                     progname,
                     ifa->ifa_name,
                     inet_ntoa (in2),
                     mask_width (mask),
                     mask);
          if (in2.s_addr == 0x0100007f ||   /* 127.0.0.1 in network order */
              ((in2.s_addr & 0x000000ff) == 0x7f) ||  /* 127.0.0.0/24 */
              mask == 0)
            /* Assume all 127 addresses are loopback, not just 127.0.0.1. */
            continue;

          /* At least on the AT&T 3G network, pinging either of the two
             hosts on a /31 network doesn't work, so don't try.
           */
          if (mask_width (mask) == 31)
            {
              sprintf (buf,
                       "Can't ping subnet:\n"
                       "local network is\n"
                       "%.100s/%d,\n"
                       "a p2p bridge\n"
                       "on if %.100s.",
                       inet_ntoa (in2), mask_width (mask), ifa->ifa_name);
              if (*error_ret) free (*error_ret);
              *error_ret = strdup (buf);
              continue;
            }

          in = in2;
          subnet_width = mask_width (mask);

          /* Take the first non-loopback network: prefer en0 over en1. */
          if (in.s_addr && subnet_width)
            break;
        }

      if (in.s_addr)
        {
          if (*error_ret) free (*error_ret);
          *error_ret = 0;
          n_base = in.s_addr;  /* already in network order, I think? */
        }
      else if (!*error_ret)
        *error_ret = strdup ("Unable to determine\nlocal IP address\n");

      if (all) 
        freeifaddrs (all);

      if (*error_ret)
        return 0;

# else /* !HAVE_GETIFADDRS */

      /* If we can't walk the list of network interfaces to figure out
         our local IP address, try to do it by finding the local host
         name, then resolving that.
      */
      char hostname[BUFSIZ];
      struct hostent *hent = 0;

      if (gethostname(hostname, BUFSIZ)) 
        {
          *error_ret = strdup ("Unable to determine\n"
                               "local host name!");
          return 0;
        }

      /* Get our IP address and convert it to a string */

      hent = gethostbyname(hostname);
      if (! hent)
        {
          strcat (hostname, ".local");	/* Necessary on iphone */
          hent = gethostbyname(hostname);
        }

      if (! hent)
        {
          sprintf(buf, 
                  "Unable to resolve\n"
                  "local host \"%.100s\"", 
                  hostname);
          *error_ret = strdup(buf);
          return 0;
        }

      strcpy (address, inet_ntoa(*((struct in_addr *)hent->h_addr_list[0])));
      n_base = pack_addr (hent->h_addr_list[0][0],
                          hent->h_addr_list[0][1],
                          hent->h_addr_list[0][2],
                          hent->h_addr_list[0][3]);

      if (n_base == 0x0100007f)   /* 127.0.0.1 in network order */
        {
          unsigned int a, b, c, d;
          unpack_addr (n_base, &a, &b, &c, &d);
          sprintf (buf,
                   "Unable to determine\n"
                   "local subnet address:\n"
                   "\"%.100s\"\n"
                   "resolves to\n"
                   "loopback address\n"
                   "%u.%u.%u.%u.",
                   hostname, a, b, c, d);
          *error_ret = strdup(buf);
          return 0;
        }

# endif /* !HAVE_GETIFADDRS */
    }


  /* Construct targets for all addresses in this subnet */

  h_mask = width_mask (subnet_width);
  h_base = ntohl (n_base);

  if (desc_ret && !*desc_ret) {
    char buf2[255];
    unsigned int a, b, c, d;
    unsigned long bb = n_base & htonl(h_mask);
    unpack_addr (bb, &a, &b, &c, &d);
    if (subnet_width > 24)
      sprintf (buf2, "%u.%u.%u.%u/%d", a, b, c, d, subnet_width);
    else
      sprintf (buf2, "%u.%u.%u/%d", a, b, c, subnet_width);
    *desc_ret = strdup (buf2);
  }

  for (i = 255; i >= 0; i--) {
    unsigned int a, b, c, d;
    int ip = (h_base & 0xFFFFFF00L) | i;     /* host order */
      
    if ((ip & h_mask) != (h_base & h_mask))  /* skip out-of-subnet host */
      continue;
    else if (subnet_width == 31)	     /* 1-bit bridge: 2 hosts */
      ;
    else if ((ip & ~h_mask) == 0)	     /* skip network address */
      continue;
    else if ((ip & ~h_mask) == ~h_mask)	     /* skip broadcast address */
      continue;

    unpack_addr (htonl (ip), &a, &b, &c, &d);
    sprintf (address, "%u.%u.%u.%u", a, b, c, d);

    if (pd->debug_p > 1)
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

    new = bogie_for_host (ssd, address, NULL);
    if (new)
      {
        new->next = list;
        list = new;
      }
  }

  return list;
}


/* Send a ping packet.
 */
static void
send_ping (ping_data *pd, const sonar_bogie *b)
{
  ping_bogie *pb = (ping_bogie *) b->closure;
  u_char *packet;
  struct ICMP *icmph;
  const char *token = "org.jwz.xscreensaver.sonar";
  char *host_id;

  unsigned long pcktsiz = (sizeof(struct ICMP) + sizeof(struct timeval) +
                 sizeof(socklen_t) + pb->addrlen +
                 strlen(token) + 1 +
                 strlen(pd->version) + 1);

  /* Create the ICMP packet */

  if (! (packet = (u_char *) calloc(1, pcktsiz)))
    return;  /* Out of memory */

  icmph = (struct ICMP *) packet;
  ICMP_TYPE(icmph) = ICMP_ECHO;
  ICMP_CODE(icmph) = 0;
  ICMP_CHECKSUM(icmph) = 0;
  ICMP_ID(icmph) = pd->pid;
  ICMP_SEQ(icmph) = pd->seq++;
# ifdef GETTIMEOFDAY_TWO_ARGS
  gettimeofday((struct timeval *) &packet[sizeof(struct ICMP)],
               (struct timezone *) 0);
# else
  gettimeofday((struct timeval *) &packet[sizeof(struct ICMP)]);
# endif

  /* We store the sockaddr of the host we're pinging in the packet, and parse
     that out of the return packet later (see get_ping() for why).
     After that, we also include the name and version of this program,
     just to give a clue to anyone sniffing and wondering what's up.
   */
  host_id = (char *) &packet[sizeof(struct ICMP) + sizeof(struct timeval)];
  *(socklen_t *)host_id = pb->addrlen;
  host_id += sizeof(socklen_t);
  memcpy(host_id, &pb->address, pb->addrlen);
  host_id += pb->addrlen;
  sprintf (host_id, "%.20s %.20s", token, pd->version);

  ICMP_CHECKSUM(icmph) = checksum((u_short *)packet, pcktsiz);

  /* Send it */

  if (sendto(pd->icmpsock, packet, pcktsiz, 0, 
             (struct sockaddr *)&pb->address, sizeof(pb->address))
      != pcktsiz)
    {
#if 0
      char buf[BUFSIZ];
      sprintf(buf, "%s: pinging %.100s", progname, b->name);
      perror(buf);
#endif
    }

  free (packet);
}

/* signal handler */
static void
sigcatcher (int sig)
{
  timer_expired = 1;
}


/* Compute the checksum on a ping packet.
 */
static u_short
checksum (u_short *packet, int size) 
{
  register int nleft = size;
  register u_short *w = packet;
  register int sum = 0;
  u_short answer = 0;

  /* Using a 32 bit accumulator (sum), we add sequential 16 bit words
     to it, and at the end, fold back all the carry bits from the
     top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)
    {
      sum += *w++;
      nleft -= 2;
    }

  /* mop up an odd byte, if necessary */

  if (nleft == 1)
    {
      *(u_char *)(&answer) = *(u_char *)w ;
      *(1 + (u_char *)(&answer)) = 0;
      sum += answer;
    }

  /* add back carry outs from top 16 bits to low 16 bits */

  sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
  sum += (sum >> 16);                     /* add carry */
  answer = ~sum;                          /* truncate to 16 bits */

  return(answer);
}


/* Copies the sonar_bogie and the underlying ping_bogie.
 */
static sonar_bogie *
copy_ping_bogie (sonar_sensor_data *ssd, const sonar_bogie *b)
{
  sonar_bogie *b2 = sonar_copy_bogie (ssd, b);
  if (b->closure)
    {
      ping_bogie *pb  = (ping_bogie *) b->closure;
      ping_bogie *pb2 = (ping_bogie *) calloc (1, sizeof(*pb));
      pb2->address = pb->address;
      b2->closure = pb2;
    }
  return b2;
}


/* Look for all outstanding ping replies.
 */
static sonar_bogie *
get_ping (sonar_sensor_data *ssd)
{
  ping_data *pd = (ping_data *) ssd->closure;
  struct sockaddr from;
  socklen_t fromlen;
  int result;
  u_char packet[1024];
  struct timeval now;
  struct timeval *then;
  struct ip *ip;
  int iphdrlen;
  struct ICMP *icmph;
  sonar_bogie *bl = 0;
  sonar_bogie *new = 0;
  struct sigaction sa;
  struct itimerval it;
  fd_set rfds;
  struct timeval tv;

  /* Set up a signal to interrupt our wait for a packet */

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigcatcher;
  if (sigaction(SIGALRM, &sa, 0) == -1) 
    {
      char msg[1024];
      sprintf(msg, "%s: unable to trap SIGALRM", progname);
      perror(msg);
      exit(1);
    }

  /* Set up a timer to interupt us if we don't get a packet */

  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = pd->timeout;
  timer_expired = 0;
  setitimer(ITIMER_REAL, &it, 0);

  /* Wait for a result packet */

  fromlen = sizeof(from);
  while (! timer_expired)
    {
      tv.tv_usec = pd->timeout;
      tv.tv_sec = 0;
#if 0
      /* This breaks on BSD, which uses bzero() in the definition of FD_ZERO */
      FD_ZERO(&rfds);
#else
      memset (&rfds, 0, sizeof(rfds));
#endif
      FD_SET(pd->icmpsock, &rfds);
      /* only wait a little while, in case we raced with the timer expiration.
         From Valentijn Sessink <valentyn@openoffice.nl> */
      if (select(pd->icmpsock + 1, &rfds, 0, 0, &tv) >0)
        {
          result = (int)recvfrom (pd->icmpsock, packet, sizeof(packet),
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


          /* Ignore anything but ICMP Replies */
          if (ICMP_TYPE(icmph) != ICMP_ECHOREPLY) 
            continue;

          /* Ignore packets not set from us */
          if (ICMP_ID(icmph) != pd->pid)
	    continue;

          /* Find the bogie in 'targets' that corresponds to this packet
             and copy it, so that this bogie stays in the same spot (th)
             on the screen, and so that we don't have to resolve it again.

             We could find the bogie by comparing ip->ip_src.s_addr to
             pb->address, but it is possible that, in certain weird router
             or NAT situations, that the reply will come back from a 
             different address than the one we sent it to.  So instead,
             we parse the sockaddr out of the reply packet payload.
           */
          {
            const socklen_t *host_id = (socklen_t *) &packet[
              iphdrlen + sizeof(struct ICMP) + sizeof(struct timeval)];

            sonar_bogie *b;

            /* Ensure that a maliciously-crafted return packet can't
               make us overflow in memcmp. */
            if (result > 0 && (const u_char *)(host_id + 1) <= packet + result)
              {
                const u_char *host_end = (const u_char *)(host_id + 1) +
                                         *host_id;

                if ((const u_char *)(host_id + 1) <= host_end &&
                    host_end <= packet + result)
                  {
                    for (b = pd->targets; b; b = b->next)
                      {
                        ping_bogie *pb = (ping_bogie *)b->closure;
                        if (*host_id == pb->addrlen &&
                            !memcmp(&pb->address, host_id + 1, pb->addrlen) )
                          {
                            /* Check to see if the name lookup is done. */
                            if (pb->lookup_name &&
                                async_name_from_addr_is_done (pb->lookup_name))
                              {
                                char *host = NULL;

                                async_name_from_addr_finish (pb->lookup_name,
                                                             &host, NULL);

                                if (pd->debug_p > 1)
                                  fprintf (stderr, "%s:   %s => %s\n",
                                           progname, b->name,
                                           host ? host : "<unknown>");

                                if (host)
                                  {
                                    free(b->name);
                                    b->name = host;
                                  }

                                pb->lookup_name = NULL;
                              }

                            new = copy_ping_bogie (ssd, b);
                            break;
                          }
                      }
                  }
              }
          }

          if (! new)      /* not in targets? */
            {
              unsigned int a, b, c, d;
              unpack_addr (ip->ip_src.s_addr, &a, &b, &c, &d);
              fprintf (stderr, 
                       "%s: UNEXPECTED PING REPLY! "
                       "%4d bytes, icmp_seq=%-4d from %d.%d.%d.%d\n",
                       progname, result, ICMP_SEQ(icmph), a, b, c, d);
              continue;
            }

          new->next = bl;
          bl = new;

          {
            double msec = delta(then, &now) / 1000.0;

            if (pd->times_p)
              {
                if (new->desc) free (new->desc);
                new->desc = (char *) malloc (30);
                if      (msec > 99) sprintf (new->desc, "%.0f ms", msec);
                else if (msec >  9) sprintf (new->desc, "%.1f ms", msec);
                else if (msec >  1) sprintf (new->desc, "%.2f ms", msec);
                else                sprintf (new->desc, "%.3f ms", msec);
              }

            if (pd->debug_p && pd->times_p)  /* ping-like stdout log */
              {
                char *s = strdup(new->name);
                char *s2 = s;
                if (strlen(s) > 28)
                  {
                    s2 = s + strlen(s) - 28;
                    strncpy (s2, "...", 3);
                  }
                fprintf (stdout, 
                         "%3d bytes from %28s: icmp_seq=%-4d time=%s\n",
                         result, s2, ICMP_SEQ(icmph), new->desc);
                fflush (stdout);
                free(s);
              }

            /* The radius must be between 0.0 and 1.0.
               We want to display ping times on a logarithmic scale,
               with the three rings being 2.5, 70 and 2,000 milliseconds.
             */
            if (msec <= 0) msec = 0.001;
            new->r = log (msec * 10) / log (20000);

            /* Don't put anyone *too* close to the center of the screen. */
            if (new->r < 0) new->r = 0;
            if (new->r < 0.1) new->r += 0.1;
          }
        }
    }

  return bl;
}


/* difference between the two times in microseconds.
 */
static long
delta (struct timeval *then, struct timeval *now) 
{
  return (((now->tv_sec - then->tv_sec) * 1000000) + 
          (now->tv_usec - then->tv_usec));  
}


static void
ping_free_data (sonar_sensor_data *ssd, void *closure)
{
  ping_data *pd = (ping_data *) closure;
  sonar_bogie *b = pd->targets;
  while (b)
    {
      sonar_bogie *b2 = b->next;
      sonar_free_bogie (ssd, b);
      b = b2;
    }
  free (pd);
}

static void
ping_free_bogie_data (sonar_sensor_data *sd, void *closure)
{
  ping_bogie *pb = (ping_bogie *) closure;

  if (pb->lookup_name)
    async_name_from_addr_cancel (pb->lookup_name);
  if (pb->lookup_addr)
    async_addr_from_name_cancel (pb->lookup_addr);
  free (pb->fallback);

  free (closure);
}


/* Returns the current time in seconds as a double.
 */
static double
double_time (void)
{
  struct timeval now;
# ifdef GETTIMEOFDAY_TWO_ARGS
  struct timezone tzp;
  gettimeofday(&now, &tzp);
# else
  gettimeofday(&now);
# endif

  return (now.tv_sec + ((double) now.tv_usec * 0.000001));
}


static void
free_bogie_after_lookup(sonar_sensor_data *ssd, sonar_bogie **sbp,
                        sonar_bogie **sb)
{
  ping_bogie *pb = (ping_bogie *)(*sb)->closure;

  *sbp = (*sb)->next;
  pb->lookup_addr = NULL; /* Prevent double-free in sonar_free_bogie. */
  sonar_free_bogie (ssd, *sb);
  *sb = NULL;
}


/* Pings the next bogie, if it's time.
   Returns all outstanding ping replies.
 */
static sonar_bogie *
ping_scan (sonar_sensor_data *ssd)
{
  ping_data *pd = (ping_data *) ssd->closure;
  double now = double_time();
  double ping_cycle = 10;   /* re-ping a given host every 10 seconds */
  double ping_interval = ping_cycle / pd->target_count;

  if (now > pd->last_ping_time + ping_interval)   /* time to ping someone */
    {
      struct sonar_bogie **sbp;

      if (pd->last_pinged)
        {
          sbp = &pd->last_pinged->next;
          if (!*sbp)
            sbp = &pd->targets;
        }
      else
        sbp = &pd->targets;

      if (!*sbp)
        /* Aaaaand we're out of bogies. */
        pd->last_pinged = NULL;
      else
        {
          sonar_bogie *sb = *sbp;
          ping_bogie *pb = (ping_bogie *)sb->closure;
          if (pb->lookup_addr &&
              async_addr_from_name_is_done (pb->lookup_addr))
            {
              if (async_addr_from_name_finish (pb->lookup_addr, &pb->address,
                                               &pb->addrlen, NULL))
                {
                  char *fallback = pb->fallback;
                  pb->fallback = NULL;

                  if (pd->debug_p)
                    fprintf (stderr, "%s:   could not resolve host:  %s\n",
                             progname, sb->name);

                  free_bogie_after_lookup (ssd, sbp, &sb);

                  /* Insert the fallback bogie right where the old one was. */
                  if (fallback)
                    {
                      sonar_bogie *new_bogie = bogie_for_host (ssd, fallback,
                                                               NULL);
                      if (new_bogie) {
                        new_bogie->next = *sbp;

                        if (! ((ping_bogie *)new_bogie->closure)->lookup_addr &&
                            ! find_duplicate_host(pd, &pd->targets, new_bogie))
                          *sbp = new_bogie;
                        else
                          sonar_free_bogie (ssd, new_bogie);
                      }

                      free (fallback);
                    }
                }
              else
                {
                  if (pd->debug_p > 1)
                    {
                      fprintf (stderr, "%s:   %s => ", progname, sb->name);
                      print_address (stderr, 0, &pb->address, pb->addrlen);
                      putc('\n', stderr);
                    }

                  if (! is_address_ok (pd->debug_p, sb))
                    free_bogie_after_lookup (ssd, sbp, &sb);
                  else if (find_duplicate_host (pd, &pd->targets, sb))
                    /* Tricky: find_duplicate_host skips the current bogie when
                       scanning the targets list because pb->lookup_addr hasn't
                       been NULL'd yet.

                       Not that it matters much, but behavior here is to
                       keep the existing address.
                     */
                    free_bogie_after_lookup (ssd, sbp, &sb);
                }

              if (sb)
                pb->lookup_addr = NULL;
            }

          if (sb && !pb->lookup_addr)
            {
              if (!pb->addrlen) abort();
              send_ping (pd, sb);
              pd->last_pinged = sb;
            }
        }

      pd->last_ping_time = now;
    }

  return get_ping (ssd);
}


/* Returns a list of hosts to ping based on the "-ping" argument.
 */
static sonar_bogie *
parse_mode (sonar_sensor_data *ssd, char **error_ret, char **desc_ret,
            const char *ping_arg, Bool ping_works_p)
{
  ping_data *pd = (ping_data *) ssd->closure;
  char *source, *token, *end, dummy;
  sonar_bogie *hostlist = 0;
  const char *fallback = "subnet";

 AGAIN:

  if (fallback && (!ping_arg || !*ping_arg || !strcmp (ping_arg, "default")))
    source = strdup(fallback);
  else if (ping_arg)
    source = strdup(ping_arg);
  else
    return 0;

  token = source;
  end = source + strlen(source);
  while (token < end)
    {
      char *next;
      sonar_bogie *new = 0;
# ifdef READ_FILES
      struct stat st;
# endif
      unsigned int n0=0, n1=0, n2=0, n3=0, m=0;
      char d;

      for (next = token;
           *next &&
           *next != ',' && *next != ' ' && *next != '\t' && *next != '\n';
           next++)
        ;
      *next = 0;


      if (pd->debug_p)
        fprintf (stderr, "%s: parsing %s\n", progname, token);

      if (!ping_works_p)
        {
          *error_ret = strdup ("Sonar must be setuid or libcap to ping!\n"
                               "Running simulation instead.");
          return 0;
        }

      if ((4 == sscanf (token, "%u.%u.%u/%u %c",    &n0,&n1,&n2,    &m,&d)) ||
          (5 == sscanf (token, "%u.%u.%u.%u/%u %c", &n0,&n1,&n2,&n3,&m,&d)))
        {
          /* subnet: A.B.C.D/M
             subnet: A.B.C/M
           */
          unsigned long ip = pack_addr (n0, n1, n2, n3);
          new = subnet_hosts (ssd, error_ret, desc_ret, ip, m);
        }
      else if (4 == sscanf (token, "%u.%u.%u.%u %c", &n0, &n1, &n2, &n3, &d))
        {
          /* IP: A.B.C.D
           */
          new = bogie_for_host (ssd, token, NULL);
        }
      else if (!strcmp (token, "subnet"))
        {
          new = subnet_hosts (ssd, error_ret, desc_ret, 0, 24);
        }
      else if (1 == sscanf (token, "subnet/%u %c", &m, &dummy))
        {
          new = subnet_hosts (ssd, error_ret, desc_ret, 0, m);
        }
      else if (*token == '.' || *token == '/' || 
               *token == '$' || *token == '~')
        {
# ifdef READ_FILES
          new = read_hosts_file (ssd, token);
# else
          if (pd->debug_p) fprintf (stderr, "%s:  skipping file\n", progname);
# endif
        }
# ifdef READ_FILES
      else if (!stat (token, &st))
        {
          new = read_hosts_file (ssd, token);
        }
# endif /* READ_FILES */
      else
        {
          /* not an existant file - must be a host name
           */
          new = bogie_for_host (ssd, token, NULL);
        }

      if (new)
        {
          sonar_bogie *nn = new;
          while (nn->next)
            nn = nn->next;
          nn->next = hostlist;
          hostlist = new;
        }

      token = next + 1;
      while (token < end &&
             (*token == ',' || *token == ' ' ||
              *token == '\t' || *token == '\n'))
        token++;
    }

  free (source);

  /* If the arg was completely unparsable, fall back to the local subnet.
     This happens if the default is "/etc/hosts" but READ_FILES is off.
     Or if we're on a /31 network, in which case we try twice then fail.
   */
  if (!hostlist && fallback)
    {
      if (pd->debug_p)
        fprintf (stderr, "%s: no hosts parsed! Trying %s\n", 
                 progname, fallback);
      ping_arg = fallback;
      fallback = 0;
      goto AGAIN;
    }

  return hostlist;
}


static Bool
set_net_raw_capalibity(int enable_p)
{
  Bool ret_status = False;
# ifdef HAVE_LIBCAP
  cap_t cap_status;
  cap_value_t cap_value[] = { CAP_NET_RAW, };
  cap_flag_value_t cap_flag_value;
  cap_flag_value_t new_value = enable_p ? CAP_SET : CAP_CLEAR;

  cap_status = cap_get_proc();
  do {
    cap_flag_value = CAP_CLEAR;
    if (cap_get_flag (cap_status, CAP_NET_RAW, CAP_EFFECTIVE, &cap_flag_value))
      break;
    if (cap_flag_value == new_value)
      {
        ret_status = True;
        break;
      }

    cap_set_flag (cap_status, CAP_EFFECTIVE, 1, cap_value, new_value);
    if (!cap_set_proc(cap_status)) 
      ret_status = True;
  } while (0);

  if (cap_status) cap_free (cap_status);
# endif /* HAVE_LIBCAP */

  return ret_status;
}

static Bool
set_ping_capability (void)
{
  if (geteuid() == 0) return True;
  return set_net_raw_capalibity (True);
}


sonar_sensor_data *
sonar_init_ping (Display *dpy, char **error_ret, char **desc_ret,
                 const char *subnet, int timeout,
                 Bool resolve_p, Bool times_p, Bool debug_p)
{
  /* Important! Do not return from this function without disavowing privileges
     with setuid(getuid()).
   */
  sonar_sensor_data *ssd = (sonar_sensor_data *) calloc (1, sizeof(*ssd));
  ping_data *pd = (ping_data *) calloc (1, sizeof(*pd));
  sonar_bogie *b;
  char *s;

  Bool socket_initted_p = False;
  Bool socket_raw_p     = False;

  pd->dpy = dpy;

  pd->resolve_p = resolve_p;
  pd->times_p   = times_p;
  pd->debug_p   = debug_p;

  ssd->closure       = pd;
  ssd->scan_cb       = ping_scan;
  ssd->free_data_cb  = ping_free_data;
  ssd->free_bogie_cb = ping_free_bogie_data;

  /* Get short version number. */
  s = strchr (screensaver_id, ' ');
  pd->version = strdup (s+1);
  s = strchr (pd->version, ' ');
  *s = 0;


  /* Create the ICMP socket.  Do this before dropping privs.

     Raw sockets can only be opened by root (or setuid root), so we
     only try to do this when the effective uid is 0.

     We used to just always try, and notice the failure.  But apparently
     that causes "SELinux" to log spurious warnings when running with the
     "strict" policy.  So to avoid that, we just don't try unless we
     know it will work.

     On MacOS X, we can avoid the whole problem by using a
     non-privileged datagram instead of a raw socket.

     On recent Linux systems (2012-ish?) we can avoid setuid by instead
     using cap_set_flag(... CAP_NET_RAW). To make that call the executable
     needs to have "sudo setcap cap_net_raw=p sonar" done to it first.
   */
  if (global_icmpsock)
    {
      pd->icmpsock = global_icmpsock;
      socket_initted_p = True;
      if (debug_p)
        fprintf (stderr, "%s: re-using icmp socket\n", progname);

    } 
  else if ((pd->icmpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP)) >= 0)
    {
      socket_initted_p = True;
    }
  else if (set_ping_capability() &&
           (pd->icmpsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) >= 0)
    {
      socket_initted_p = True;
      socket_raw_p = True;
    }

  if (socket_initted_p)
    {
      global_icmpsock = pd->icmpsock;
      socket_initted_p = True;
      if (debug_p)
        fprintf (stderr, "%s: opened %s icmp socket\n", progname,
                 (socket_raw_p ? "raw" : "dgram"));
    } 
  else if (debug_p)
    fprintf (stderr, "%s: unable to open icmp socket\n", progname);

  /* Disavow privs */
  setuid(getuid());

  pd->pid = getpid() & 0xFFFF;
  pd->seq = 0;
  pd->timeout = timeout;

  /* Generate a list of targets */

  pd->targets = parse_mode (ssd, error_ret, desc_ret, subnet,
                            socket_initted_p);
  pd->targets = delete_duplicate_hosts (ssd, pd->targets);

  if (debug_p)
    {
      fprintf (stderr, "%s: Target list:\n", progname);
      for (b = pd->targets; b; b = b->next)
        {
          fprintf (stderr, "%s:   ", progname);
          print_host (stderr, b);
        }
    }

  /* Make sure there is something to ping */

  pd->target_count = 0;
  for (b = pd->targets; b; b = b->next)
    pd->target_count++;

  if (pd->target_count == 0)
    {
      if (! *error_ret)
        *error_ret = strdup ("No hosts to ping!\n"
                             "Simulating instead.");
      if (pd) ping_free_data (ssd, pd);
      if (ssd) free (ssd);
      return 0;
    }

  /* Distribute them evenly around the display field, clockwise.
     Even on a /24, allocated IPs tend to cluster together, so
     don't put any two hosts closer together than N degrees to
     avoid unnecessary overlap when we have plenty of space due
     to addresses that probably won't respond.  And don't spread
     them out too far apart, because that looks too symmetrical
     when there are a small number of hosts.
   */
  {
    double th = frand(M_PI);
    double sep = 360.0 / pd->target_count;
    if (sep < 23) sep = 23;
    if (sep > 43) sep = 43;
    sep /= 180/M_PI;
    for (b = pd->targets; b; b = b->next) {
      b->th = th;
      th += sep;
    }
  }

  return ssd;
}

#endif /* HAVE_PING -- whole file */
