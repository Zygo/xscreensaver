/* sonar, Copyright (c) 1998-2008 Jamie Zawinski and Stephen Martin
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

#undef usleep /* conflicts with unistd.h on OSX */

#if defined(HAVE_ICMP) || defined(HAVE_ICMPHDR)
# include <unistd.h>
# include <sys/stat.h>
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

#ifndef HAVE_PING

sonar_sensor_data *
init_ping (Display *dpy, const char *subnet, int timeout, 
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
  struct sockaddr address;	/* ip address */
} ping_bogie;



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




/* If resolves the bogie's name (either a hostname or ip address string)
   to a hostent.  Returns 1 if successful, 0 if it failed to resolve.
 */
static int
resolve_bogie_hostname (ping_data *pd, sonar_bogie *sb, Bool resolve_p)
{
  ping_bogie *pb = (ping_bogie *) sb->closure;
  struct hostent *hent;
  struct sockaddr_in *iaddr;

  unsigned int ip[4];
  char c;

  iaddr = (struct sockaddr_in *) &(pb->address);
  iaddr->sin_family = AF_INET;

  if (4 == sscanf (sb->name, " %u.%u.%u.%u %c",
                   &ip[0], &ip[1], &ip[2], &ip[3], &c))
    {
      /* It's an IP address.
       */
      if (ip[3] == 0)
        {
          if (pd->debug_p > 1)
            fprintf (stderr, "%s:   ignoring bogus IP %s\n",
                     progname, sb->name);
          return 0;
        }

      iaddr->sin_addr.s_addr = pack_addr (ip[0], ip[1], ip[2], ip[3]);
      if (resolve_p)
        hent = gethostbyaddr ((const char *) &iaddr->sin_addr.s_addr,
                              sizeof(iaddr->sin_addr.s_addr),
                              AF_INET);
      else
        hent = 0;

      if (pd->debug_p > 1)
        fprintf (stderr, "%s:   %s => %s\n",
                 progname, sb->name,
                 ((hent && hent->h_name && *hent->h_name)
                  ? hent->h_name : "<unknown>"));

      if (hent && hent->h_name && *hent->h_name)
        sb->name = strdup (hent->h_name);
    }
  else
    {
      /* It's a host name. */

      /* don't waste time being confused by non-hostname tokens
         in .ssh/known_hosts */
      if (!strcmp (sb->name, "ssh-rsa") ||
          !strcmp (sb->name, "ssh-dsa") ||
          !strcmp (sb->name, "ssh-dss") ||
          strlen (sb->name) >= 80)
        return 0;

      hent = gethostbyname (sb->name);
      if (!hent)
        {
          if (pd->debug_p)
            fprintf (stderr, "%s: could not resolve host:  %s\n",
                     progname, sb->name);
          return 0;
        }

      memcpy (&iaddr->sin_addr, hent->h_addr_list[0],
              sizeof(iaddr->sin_addr));

      if (pd->debug_p > 1)
        {
          unsigned int a, b, c, d;
          unpack_addr (iaddr->sin_addr.s_addr, &a, &b, &c, &d);
          fprintf (stderr, "%s:   %s => %d.%d.%d.%d\n",
                   progname, sb->name, a, b, c, d);
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


/* Create a sonar_bogie a host name or ip address string.
   Returns NULL if the name could not be resolved.
 */
static sonar_bogie *
bogie_for_host (sonar_sensor_data *ssd, const char *name, Bool resolve_p)
{
  ping_data *pd = (ping_data *) ssd->closure;
  sonar_bogie *b = (sonar_bogie *) calloc (1, sizeof(*b));
  ping_bogie *pb = (ping_bogie *) calloc (1, sizeof(*pb));
  struct sockaddr_in *iaddr;
  unsigned long ip;

  b->name = strdup (name);
  b->closure = pb;

  if (! resolve_bogie_hostname (pd, b, resolve_p))
    goto FAIL;

  iaddr = (struct sockaddr_in *) &(pb->address);

  /* Don't ever use loopback (127.0.0.x) hosts */
  ip = iaddr->sin_addr.s_addr;
  if ((ntohl (ip) & 0xFFFFFF00L) == 0x7f000000L)  /* 127.0.0.x */
    {
      if (pd->debug_p)
        fprintf (stderr, "%s:   ignoring loopback host %s\n", 
                 progname, b->name);
      goto FAIL;
    }

  /* Don't ever use broadcast (255.x.x.x) hosts */
  if ((ntohl (ip) & 0xFF000000L) == 0xFF000000L)  /* 255.x.x.x */
    {
      if (pd->debug_p)
        fprintf (stderr, "%s:   ignoring broadcast host %s\n",
                 progname, b->name);
      goto FAIL;
    }

  if (pd->debug_p > 1)
    {
      fprintf (stderr, "%s:   added ", progname);
      print_host (stderr, ip, b->name);
    }

  return b;

 FAIL:
  if (b) free_bogie (ssd, b);
  return 0;
}


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
      char buf[1024];
      sprintf(buf, "%s: %s", progname, filename);
#ifdef HAVE_COCOA
      if (pd->debug_p)  /* on OSX don't syslog this */
#endif
        perror (buf);
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

      name = addr = 0;
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

      new = 0;
      if (name)
        new = bogie_for_host (ssd, name, pd->resolve_p);
      if (!new && addr)
        new = bogie_for_host (ssd, addr, pd->resolve_p);

      if (new)
        {
          new->next = list;
          list = new;
        }
    }

  fclose(fp);
  return list;
}


static sonar_bogie *
delete_duplicate_hosts (sonar_sensor_data *ssd, sonar_bogie *list)
{
  ping_data *pd = (ping_data *) ssd->closure;
  sonar_bogie *head = list;
  sonar_bogie *sb;

  for (sb = head; sb; sb = sb->next)
    {
      ping_bogie *pb = (ping_bogie *) sb->closure;
      struct sockaddr_in *i1 = (struct sockaddr_in *) &(pb->address);
      unsigned long ip1 = i1->sin_addr.s_addr;

      sonar_bogie *sb2;
      for (sb2 = sb; sb2; sb2 = sb2->next)
        {
          if (sb2 && sb2->next)
            {
              ping_bogie *pb2 = (ping_bogie *) sb2->next->closure;
              struct sockaddr_in *i2 = (struct sockaddr_in *) &(pb2->address);
              unsigned long ip2 = i2->sin_addr.s_addr;

              if (ip1 == ip2)
                {
                  if (pd->debug_p)
                    {
                      fprintf (stderr, "%s: deleted duplicate: ", progname);
                      print_host (stderr, ip2, sb2->next->name);
                    }
                  sb2->next = sb2->next->next;
                  /* #### sb leaked */
                }
            }
        }
    }

  return head;
}


/* Generate a list of bogies consisting of all of the entries on
  the same subnet.  'base' ip is in network order; 0 means localhost.
 */
static sonar_bogie *
subnet_hosts (sonar_sensor_data *ssd, char **error_ret,
              unsigned long n_base, int subnet_width)
{
  ping_data *pd = (ping_data *) ssd->closure;
  unsigned long h_mask;   /* host order */
  unsigned long h_base;   /* host order */

  /* Local Variables */

  char hostname[BUFSIZ];
  char address[BUFSIZ];
  struct hostent *hent;
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

  /* Get our hostname */

  if (gethostname(hostname, BUFSIZ)) 
    {
      *error_ret = strdup ("Unable to determine\n"
                           "local host name!");
      return 0;
    }

  /* Get our IP address and convert it to a string */

  if (! (hent = gethostbyname(hostname)))
    {
      sprintf(buf, 
              "Unable to resolve\n"
              "local host \"%s\"", 
              hostname);
      *error_ret = strdup(buf);
      return 0;
    }
  strcpy (address, inet_ntoa(*((struct in_addr *)hent->h_addr_list[0])));

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
      sprintf (buf,
               "Unable to determine\n"
               "local subnet address:\n"
               "\"%s\"\n"
               "resolves to\n"
               "loopback address\n"
               "%u.%u.%u.%u.",
               hostname, a, b, c, d);
      *error_ret = strdup(buf);
      return 0;
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

    new = bogie_for_host (ssd, address, pd->resolve_p);
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
  int result;
  const char *token = "org.jwz.xscreensaver.sonar";

  int pcktsiz = (sizeof(struct ICMP) + sizeof(struct timeval) + 
                 strlen(b->name) + 1 +
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

  /* We store the name of the host we're pinging in the packet, and parse
     that out of the return packet later (see get_ping() for why).
     After that, we also include the name and version of this program,
     just to give a clue to anyone sniffing and wondering what's up.
   */
  sprintf ((char *) &packet[sizeof(struct ICMP) + sizeof(struct timeval)],
           "%s%c%s %s",
           b->name, 0, token, pd->version);

  ICMP_CHECKSUM(icmph) = checksum((u_short *)packet, pcktsiz);

  /* Send it */

  if ((result = sendto(pd->icmpsock, packet, pcktsiz, 0, 
                       &pb->address, sizeof(pb->address)))
      != pcktsiz)
    {
#if 0
      char buf[BUFSIZ];
      sprintf(buf, "%s: pinging %s", progname, b->name);
      perror(buf);
#endif
    }
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
  sonar_bogie *b2 = copy_bogie (ssd, b);
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
  unsigned int fromlen;  /* Posix says socklen_t, but that's not portable */
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
          result = recvfrom (pd->icmpsock, packet, sizeof(packet),
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
             we parse the name out of the reply packet payload.
           */
          {
            const char *name = (char *) &packet[iphdrlen +
                                                sizeof(struct ICMP) +
                                                sizeof(struct timeval)];
            sonar_bogie *b;
            for (b = pd->targets; b; b = b->next)
              if (!strcmp (name, b->name))
                {
                  new = copy_ping_bogie (ssd, b);
                  break;
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
      free_bogie (ssd, b);
      b = b2;
    }
  free (pd);
}

static void
ping_free_bogie_data (sonar_sensor_data *sd, void *closure)
{
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


/* If a bogie is provided, pings it.
   Then, returns all outstanding ping replies.
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
      if (pd->last_pinged)
        pd->last_pinged = pd->last_pinged->next;
      if (! pd->last_pinged)
        pd->last_pinged = pd->targets;
      send_ping (pd, pd->last_pinged);
      pd->last_ping_time = now;
    }

  return get_ping (ssd);
}


/* Returns a list of hosts to ping based on the "-ping" argument.
 */
static sonar_bogie *
parse_mode (sonar_sensor_data *ssd, char **error_ret,
            const char *ping_arg, Bool ping_works_p)
{
  ping_data *pd = (ping_data *) ssd->closure;
  char *source, *token, *end, dummy;
  sonar_bogie *hostlist = 0;

  if (!ping_arg || !*ping_arg || !strcmp (ping_arg, "default"))
    source = strdup("subnet/28");
  else
    source = strdup(ping_arg);

  token = source;
  end = source + strlen(source);
  while (token < end)
    {
      char *next;
      sonar_bogie *new;
      struct stat st;
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
          *error_ret = strdup ("Sonar must be setuid to ping!\n"
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
          new = subnet_hosts (ssd, error_ret, ip, m);
        }
      else if (4 == sscanf (token, "%u.%u.%u.%u %c", &n0, &n1, &n2, &n3, &d))
        {
          /* IP: A.B.C.D
           */
          new = bogie_for_host (ssd, token, pd->resolve_p);
        }
      else if (!strcmp (token, "subnet"))
        {
          new = subnet_hosts (ssd, error_ret, 0, 24);
        }
      else if (1 == sscanf (token, "subnet/%u %c", &m, &dummy))
        {
          new = subnet_hosts (ssd, error_ret, 0, m);
        }
      else if (*token == '.' || *token == '/' || 
               *token == '$' || *token == '~' ||
               !stat (token, &st))
        {
          /* file name
           */
          new = read_hosts_file (ssd, token);
        }
      else
        {
          /* not an existant file - must be a host name
           */
          new = bogie_for_host (ssd, token, pd->resolve_p);
        }

      if (new)
        {
          sonar_bogie *nn = new;
          while (nn && nn->next)
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
  return hostlist;
}


sonar_sensor_data *
init_ping (Display *dpy, char **error_ret, 
           const char *subnet, int timeout,
           Bool resolve_p, Bool times_p, Bool debug_p)
{
  sonar_sensor_data *ssd = (sonar_sensor_data *) calloc (1, sizeof(*ssd));
  ping_data *pd = (ping_data *) calloc (1, sizeof(*pd));
  sonar_bogie *b;
  char *s;
  int i, div;

  Bool socket_initted_p = False;
  Bool socket_raw_p     = False;

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
  else if (geteuid() == 0 &&
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

  pd->targets = parse_mode (ssd, error_ret, subnet, socket_initted_p);
  pd->targets = delete_duplicate_hosts (ssd, pd->targets);

  if (debug_p)
    {
      fprintf (stderr, "%s: Target list:\n", progname);
      for (b = pd->targets; b; b = b->next)
        {
          ping_bogie *pb = (ping_bogie *) b->closure;
          struct sockaddr_in *iaddr = (struct sockaddr_in *) &(pb->address);
          unsigned long ip = iaddr->sin_addr.s_addr;
          fprintf (stderr, "%s:   ", progname);
          print_host (stderr, ip, b->name);
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

  /* Distribute them evenly around the display field.
   */
  div = pd->target_count;
  if (div > 90) div = 90;  /* no closer together than 4 degrees */
  for (i = 0, b = pd->targets; b; b = b->next, i++)
    b->th = M_PI * 2 * ((div - i) % div) / div;

  return ssd;
}

#endif /* HAVE_PING -- whole file */
