25-Mar-98 Ronald Joe Record (rr@sco.com)

Binary Distributions
--------------------
A binary distribution of xlockmore for OpenServer is available at
http://www.sco.com/skunkware/osr5/x11/savers/xlockmore/
or
ftp://ftp.sco.com/skunkware/osr5/x11/savers/xlockmore/

A binary distribution of xlockmore for UnixWare 7 is available at
http://www.sco.com/skunkware/uw7/x11/savers/xlockmore/
or
ftp://ftp.sco.com/skunkware/uw7/x11/savers/xlockmore/

In both cases, you will also need to install the appropriate graphics
libraries package from either
http://www.sco.com/skunkware/osr5/libraries/Glib/
or
http://www.sco.com/skunkware/uw7/libraries/glib/

OpenServer Build Instructions
-----------------------------
I built Xlockmore 4.09 on SCO OpenServer 5.0.4 as follows:

- Install the Skunkware 98 graphics libraries package (Glib)
- Configure by running the ./Configure-osr5 script which front-ends the
  ./configure-osr5 modified configure script
- Copy config.h-osr5 to config.h in order to turn off shadow passwords,
  turn on SVR4 and turn on OSF1/DEC enhanced security passwords.
  A diff of these should look something like:

118c118
< #define HAVE_SHADOW 1
---
> /* #undef HAVE_SHADOW */
121,122c121,122
< #define SYSV 1
< /* #undef SVR4 */
---
> /* #undef SYSV */
> #define SVR4 1
209c209
< /* #undef OSF1_ENH_SEC */
---
> #define OSF1_ENH_SEC 1

- Type make
- As root, type "make install"
- As root, set permissions on /usr/local/bin/xlock as follows:

  # chgrp auth /usr/local/bin/xlock
  # chmod 2755 /usr/local/bin/xlock

