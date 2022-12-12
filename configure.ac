# configure.in --- xscreensaver, Copyright © 1997-2022 Jamie Zawinski.
#

# Note: upgrading past 2.69_5 breaks the world, mostly the po/ shitshow.
AC_PREREQ([2.69])

# If you accidentally upgraded autoconf, re-install the 2019-08-23 version:
#
#    git clone --single-branch https://github.com/macports/macports-ports.git
#    cd macports-ports/
#    git checkout 1d802b48a7b00b6c154e971119a2a309550c79c2
#    cd devel/autoconf
#    sudo port install
#    port installed autoconf

AC_INIT
AC_CONFIG_SRCDIR([driver/subprocs.c])
AC_CONFIG_HEADERS([config.h])

echo "current directory: `pwd`"
echo "command line was: $0 $@"

if ! test -z "$ac_unrecognized_opts" ; then
  echo "" >&2
  exit 2
fi

###############################################################################
#
#       Autoheader stuff
#
###############################################################################

AH_TOP([
/* xscreensaver, Copyright © 1991-2022 Jamie Zawinski.
 * Generate this file by running 'configure' rather than editing it by hand.
 */
])

# Defines in config.h are dumped out alphabetically rather than in any kind
# of sane order, sigh.

#
# OS stuff.
#
AH_TEMPLATE([HAVE_PTHREAD],
	    [Define this if you have POSIX threads.])

AH_TEMPLATE([HAVE_SIGTIMEDWAIT],
	    [Define to 1 if you have the `sigtimedwait' function.])

AH_TEMPLATE([HAVE_FORKPTY],
	    [Define this if you have forkpty.])

AH_TEMPLATE([HAVE_GETTIMEOFDAY],
	    [Define this if you have gettimeofday.])

AH_TEMPLATE([GETTIMEOFDAY_TWO_ARGS],
	    [Define this if gettimeofday takes two arguments.])

AH_TEMPLATE([HAVE_ICMP],
	    [Define this if ping uses struct icmp.icmp_id.])

AH_TEMPLATE([HAVE_ICMPHDR],
	    [Define this if ping uses struct icmphdr.un.echo.id.])

AH_TEMPLATE([HAVE_GETIFADDRS],
	    [Define this if you have getifaddrs.])

AH_TEMPLATE([HAVE_LIBCAP],
	    [Define this if your system has libcap.])

#
# X11 server stuff.
#
AH_TEMPLATE([XPointer],
            [Stare into the void.])

AH_TEMPLATE([HAVE_XINPUT],
	    [Define this if you have the XInput2 extension.])

# This only ever existed in X11R4 and X11R5.
AH_TEMPLATE([HAVE_XIDLE_EXTENSION],
	    [Define this if you have the X11R5 XIDLE extension.])

# This only ever existed on SGI hardware.
AH_TEMPLATE([HAVE_SGI_SAVER_EXTENSION],
	    [Define this if you have the SGI SCREEN_SAVER extension.])

# This only ever existed on SGI hardware.
AH_TEMPLATE([HAVE_SGI_VC_EXTENSION],
	    [Define this if you have the SGI-VIDEO-CONTROL extension.])

AH_TEMPLATE([HAVE_MIT_SAVER_EXTENSION],
	    [Using the MIT-SCREEN-SAVER extension means that the X server
            will crash at random times, and fading and hysteresis won't work.
            Don't use this.  You'll be sorry.  See comment in xscreensaver.c.])

AH_TEMPLATE([HAVE_DPMS_EXTENSION],
	    [Define this if you have the XDPMS extension.])

AH_TEMPLATE([HAVE_XF86VMODE],
	    [Define this if you have XF86VidModeGetViewPort, for
            virtual desktops.])

AH_TEMPLATE([HAVE_XF86VMODE_GAMMA],
	    [Define this if you have XF86VidModeSetGamma, for fading.])

AH_TEMPLATE([HAVE_XF86VMODE_GAMMA_RAMP],
	    [Define this if you have XF86VidModeSetGammaRamp, for fading.])

AH_TEMPLATE([HAVE_XINERAMA],
	    [Define this if you have the Xinerama extension.])

AH_TEMPLATE([HAVE_XHPDISABLERESET],
	    [Define this if you have XHPDisableReset, which allows
	    Ctrl-Sh-Reset to be temporarily disabled.])

AH_TEMPLATE([HAVE_RANDR],
	    [Define this if you have the Resize and Rotate extension.])

AH_TEMPLATE([HAVE_RANDR_12],
	    [Define this if the RANDR library is version 1.2 or newer.])

AH_TEMPLATE([HAVE_XSHM_EXTENSION],
	    [Define this if you have the X Shared Memory Extension.])

AH_TEMPLATE([HAVE_DOUBLE_BUFFER_EXTENSION],
	    [Define this if you have the X Double Buffer Extension.])

#AH_TEMPLATE([HAVE_XCOMPOSITE_EXTENSION],
#	    [Define this if you have the X Composite Extension.])

AH_TEMPLATE([HAVE_XKB],
	    [Define this if you have the X Keyboard Extension.])

AH_TEMPLATE([HAVE_LIBSYSTEMD],
	    [Define this if you have libsystemd.])

AH_TEMPLATE([HAVE_LIBELOGIND],
	    [Define this if you have libelogind.])

AH_TEMPLATE([HAVE_PROC_INTERRUPTS],
	    [Define this if you have a Linux-like /proc/interrupts file which
	    can be examined to determine when keyboard activity has
	    occurred, and you use PS/2 input devices instead of USB.])

AH_TEMPLATE([HAVE_PROC_OOM],
	    [Define this if you have a Linux-like /proc/.../oom_score_adj file
	    which can be adjusted by root to exempt us from the out-of-memory
	    killer.])

#
# GUI stuff.
#
AH_TEMPLATE([HAVE_MOTIF],
            [Define this if you have Motif.])

AH_TEMPLATE([HAVE_XMCOMBOBOX],
	    [Define this if you have the XmComboBox Motif 2.0 widget.])

AH_TEMPLATE([HAVE_GTK],
            [Define this if you have Gtk 2.x.])

AH_TEMPLATE([HAVE_XML],
            [Define this if you have the XML library.])

AH_TEMPLATE([HAVE_GDK_PIXBUF],
	    [Define this if you have GDK-Pixbuf.])
AH_TEMPLATE([HAVE_GDK_PIXBUF_XLIB],
	    [Define this if you have GDK-Pixbuf-Xlib.])

AH_TEMPLATE([HAVE_GDK_PIXBUF_APPLY_EMBEDDED_ORIENTATION],
	    [Define this if you have the gdk_pixbuf_apply_embedded_orientation
	    function (gdk-pixbuf 2.12).])

#
# Image loading.
#
AH_TEMPLATE([HAVE_JPEGLIB],
	    [Define this if you have the JPEG Group's JPEG library.])

AH_TEMPLATE([HAVE_LIBPNG],
	    [Define this if you have the Portable Network Graphics library.])

AH_TEMPLATE([HAVE_XUTF8DRAWSTRING],
	    [Define this if you have the function Xutf8DrawString.])

AH_TEMPLATE([HAVE_XFT],
	    [Define this if you have libXft2.])

#
# Graphics libraries.
#
AH_TEMPLATE([HAVE_GL],
	    [Define this if you have OpenGL.])

AH_TEMPLATE([HAVE_GLES],
	    [Define this if the native OpenGL library is OpenGLES 1.x.])

AH_TEMPLATE([HAVE_GLES2],
	    [Define this if the native OpenGL library is OpenGLES 2.x.])

AH_TEMPLATE([HAVE_GLES3],
	    [Define this if the native OpenGL library is OpenGLES 3.x.])

AH_TEMPLATE([HAVE_JWZGLES],
            [Define this to emulate OpenGL 1.3 in terms of OpenGLES 1.x.])

AH_TEMPLATE([HAVE_EGL],
	    [Define this if OpenGL bindings are via EGL instead of GLX.])

AH_TEMPLATE([HAVE_GLSL],
	    [Define this if OpenGL supports the OpenGL Shading Language.])

AH_TEMPLATE([HAVE_GLBINDTEXTURE],
	    [Define this if the OpenGL version is 1.1 or newer.])

AH_TEMPLATE([HAVE_GLE],
	    [Define this if you have the GL Extrusion library.])

AH_TEMPLATE([HAVE_GLE3],
            [Define this if GL Extrusion is version 3.])


AH_TEMPLATE([HAVE_RECORD_ANIM],
	    [Define this to enable recording of videos.])

#
# Locking.
#
AH_TEMPLATE([NO_LOCKING],
	    [Define this to remove the option of locking the screen at all.])

AH_TEMPLATE([HAVE_PAM],
	    [Define this if you have Pluggable Authentication Modules.])

AH_TEMPLATE([PAM_SERVICE_NAME],
	    [The name of the "xscreensaver" PAM service.])

AH_TEMPLATE([HAVE_PAM_FAIL_DELAY],
	    [Define this if you have pam_fail_delay.])

AH_TEMPLATE([PAM_CHECK_ACCOUNT_TYPE],
            [Whether PAM should check the result of account modules
	    when authenticating.])

AH_TEMPLATE([PAM_STRERROR_TWO_ARGS],
	    [Define if pam_strerror requires two arguments.])

AH_TEMPLATE([HAVE_KERBEROS],
	    [Define this if you want to use Kerberos authentication to
	    lock/unlock the screen instead of PAM.])

AH_TEMPLATE([HAVE_KERBEROS5],
	    [Define this if you have Kerberos 5.])

AH_TEMPLATE([ALLOW_ROOT_PASSWD],
	    [Define this to allow root to unlock, when not using PAM.])

AH_TEMPLATE([HAVE_SHADOW_PASSWD],
	    [Define this if your system uses /etc/shadow.])

AH_TEMPLATE([HAVE_PWNAM_SHADOW_PASSWD],
	    [Define this if your system uses getpwnam_shadow.])

AH_TEMPLATE([HAVE_ENHANCED_PASSWD],
	    [Define this for DEC/SCO "Enhanced Security" with getprpwnam.])

AH_TEMPLATE([HAVE_ADJUNCT_PASSWD],
	    [Define this for Solaris getpwanam.])

AH_TEMPLATE([HAVE_HPUX_PASSWD],
	    [Define this for HPUX so-called "Secure Passwords".])

AH_TEMPLATE([HAVE_SYSLOG],
	    [Define this if you have openlog/syslog/closelog.])


# After checking to see that --srcdir is correct (which AC_INIT does)
# check for some random other files that come later in the tar file,
# to make sure everything is here.
#
for d in utils jwxyz hacks hacks/glx driver ; do
  f=$srcdir/$d/Makefile.in
  if test \! -r $f ; then
    echo ""
    echo "ERROR: The package is incomplete: $f does not exist."
    echo "       This probably means that your download was truncated."
    echo ""
    exit 1
  fi
done

###############################################################################
#
#       Check for availability of various gcc command-line options.
#
###############################################################################

AC_DEFUN([AC_CHECK_GCC_ARG],
 [if test -n "$GCC"; then
   AC_CACHE_CHECK([whether gcc accepts [$2]],
     ac_cv_gcc_accepts_[$1],
    [rm -rf conftest.$ac_ext
     touch conftest.$ac_ext
     if ( ( gcc -c [$2] conftest.$ac_ext -o/dev/null >/dev/null ) 2>&1 | \
          grep unrecognized >/dev/null ); then
       ac_cv_gcc_accepts_[$1]=no
     else
       ac_cv_gcc_accepts_[$1]=yes
       CC="$CC [$2]"
     fi])
   ac_gcc_accepts_[$1]="$ac_cv_gcc_accepts_[$1]"
  fi
])


###############################################################################
#
#       Function to figure out how to run the compiler.
#
###############################################################################

AC_DEFUN([AC_PROG_CC_ANSI],
 [AC_REQUIRE([AC_PROG_CC])

  if test -z "$GCC"; then
    # not using GCC
    AC_MSG_CHECKING(how to request ANSI compilation)
    case "$host" in
      *-hpux* )
        AC_MSG_RESULT(HPUX: adding -Ae)
        CC="$CC -Ae"
      ;;
      *-aix* )
        AC_MSG_RESULT(AIX: adding -qlanglvl=ansi -qhalt=e)
        CC="$CC -qlanglvl=ansi -qhalt=e"
      ;;
      *-dec-* )
        AC_MSG_RESULT(DEC: adding -std1 -ieee)
        CC="$CC -std1"
      ;;
      *)
        AC_MSG_RESULT(no idea)
      ;;
    esac
  else
    # using GCC
    case "$host" in
      *-solaris*)
        AC_MSG_RESULT(Solaris: adding -D__EXTENSIONS__)
        CC="$CC -D__EXTENSIONS__"
      ;;
    esac

    # Prior to 1996, XScreenSaver was written in K&R C, because ANSI C
    # compilers (function prototypes!) were not yet universally deployed.
    # And even after the conversion to ANSI C, GNU C's various non-standard
    # extensions (such as allowing C++ style "//" comments in C code, and
    # declarations after statements) were not supported by non-gcc compilers
    # for many years, so we continued to use "-std=c89" to prevent those
    # GNU-isms from sneaking in to the code.  Well, these days various
    # vendors have sloppily allowed non-ANSI syntax to slip into their
    # library header files.  GTK and OpenGL have been major offenders in this
    # way.  So... as of 2021, I guess we are forced to use "-std=gnu89", to
    # allow those extensions, and hope that the warnings generated by
    # "-pedantic" are heeded.
    #
    AC_CHECK_GCC_ARG(gnu89, -std=gnu89)
    AC_CHECK_GCC_ARG(pedantic, -pedantic)
    AC_CHECK_GCC_ARG(Wall, -Wall)
    AC_CHECK_GCC_ARG(wnested_externs,       -Wnested-externs)
    AC_CHECK_GCC_ARG(wstrict_prototypes,    -Wstrict-prototypes)
    AC_CHECK_GCC_ARG(wmissing_prototypes,   -Wmissing-prototypes)
    AC_CHECK_GCC_ARG(declaration_after,     -Wdeclaration-after-statement)
    # "string length is greater than ISO C89 compilers required to support"
    AC_CHECK_GCC_ARG(no_overlength_strings, -Wno-overlength-strings)
    # Ignore warnings about using "inline"
    AC_CHECK_GCC_ARG(no_language_ext_token, -Wno-language-extension-token)
    # Xlib headers use named variadic macros.
    AC_CHECK_GCC_ARG(no_variadic_macros,    -Wno-variadic-macros)
  fi
])


###############################################################################
#
#       Function to figure out how to create directory trees.
#
###############################################################################

AC_DEFUN([AC_PROG_INSTALL_DIRS],
 [AC_CACHE_CHECK([whether "\${INSTALL} -d" creates intermediate directories],
    ac_cv_install_d_creates_dirs,
    [ac_cv_install_d_creates_dirs=no
     rm -rf conftestdir
     if mkdir conftestdir; then
       cd conftestdir 2>/dev/null
       ${INSTALL} -d `pwd`/dir1/dir2 >/dev/null 2>&1
       if test -d dir1/dir2/. ; then
         ac_cv_install_d_creates_dirs=yes
       fi
       cd .. 2>/dev/null
       rm -rf conftestdir
     fi
    ])

  if test "$ac_cv_install_d_creates_dirs" = no ; then
    AC_CACHE_CHECK([whether "mkdir -p" creates intermediate directories],
      ac_cv_mkdir_p_creates_dirs,
      [ac_cv_mkdir_p_creates_dirs=no
       rm -rf conftestdir
       if mkdir conftestdir; then
         cd conftestdir 2>/dev/null
         mkdir -p dir1/dir2 >/dev/null 2>&1
         if test -d dir1/dir2/. ; then
           ac_cv_mkdir_p_creates_dirs=yes
         fi
         cd .. 2>/dev/null
         rm -rf conftestdir
       fi
      ])
  fi

  if test "$ac_cv_install_d_creates_dirs" = yes ; then
    INSTALL_DIRS='${INSTALL} -d'
  elif test "$ac_cv_mkdir_p_creates_dirs" = yes ; then
    INSTALL_DIRS='mkdir -p'
  else
    # any other ideas?
    INSTALL_DIRS='${INSTALL} -d'
  fi
])


###############################################################################
#
#       Function to check whether gettimeofday() exists, and how to call it.
#	This may define HAVE_GETTIMEOFDAY and GETTIMEOFDAY_TWO_ARGS.
#
###############################################################################

AC_DEFUN([AC_GETTIMEOFDAY_ARGS],
 [AC_MSG_CHECKING(how to call gettimeofday)
  AC_CACHE_VAL(ac_cv_gettimeofday_args,
   [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([#include <stdlib.h>
                    #include <sys/time.h>],
                      [struct timeval tv; struct timezone tzp;
                    gettimeofday(&tv, &tzp);])],
    [ac_gettimeofday_args=2],
    [ac_gettimeofday_args=1])
    ac_cv_gettimeofday_args=$ac_gettimeofday_args])
  ac_gettimeofday_args=$ac_cv_gettimeofday_args
  if test "$ac_gettimeofday_args" = 1 ; then
    AC_DEFINE(HAVE_GETTIMEOFDAY)
    AC_MSG_RESULT(one argument)
  elif test "$ac_gettimeofday_args" = 2 ; then
    AC_DEFINE(HAVE_GETTIMEOFDAY)
    AC_DEFINE(GETTIMEOFDAY_TWO_ARGS)
    AC_MSG_RESULT(two arguments)
  else
    AC_MSG_RESULT(unknown)
  fi
])


###############################################################################
#
#       Function to find perl5 (defines PERL and PERL_VERSION.)
#
###############################################################################

# M4 sucks!!  perl sucks too!!
changequote(X,Y)
perl_version_cmd='print $]'
changequote([,])

AC_DEFUN([AC_PROG_PERL],
 [AC_PATH_PROGS(PERL, [perl5 perl],,)
  if test -z "$PERL" ; then
    PERL_VERSION=0
  else
    AC_CACHE_CHECK([perl version], ac_cv_perl_version,
                   [ac_cv_perl_version=`$PERL -e "$perl_version_cmd"`])
    PERL_VERSION=$ac_cv_perl_version
  fi
 ])


###############################################################################
#
#       Functions to check how to do ICMP PING requests.
#
###############################################################################

AC_DEFUN([AC_CHECK_ICMP],
 [AC_CACHE_CHECK([for struct icmp], ac_cv_have_icmp,
  [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([#include <stdlib.h>
                   #include <stdio.h>
                   #include <math.h>
                   #include <unistd.h>
                   #include <limits.h>
                   #include <signal.h>
                   #include <fcntl.h>
                   #include <sys/types.h>
                   #include <sys/time.h>
                   #include <sys/ipc.h>
                   #include <sys/shm.h>
                   #include <sys/socket.h>
                   #include <netinet/in_systm.h>
                   #include <netinet/in.h>
                   #include <netinet/ip.h>
                   #include <netinet/ip_icmp.h>
                   #include <netinet/udp.h>
                   #include <arpa/inet.h>
                   #include <netdb.h>],
                  [struct icmp i;
                   struct sockaddr s;
                   struct sockaddr_in si;
                   struct ip ip;
                   i.icmp_type = ICMP_ECHO;
                   i.icmp_code = 0;
                   i.icmp_cksum = 0;
                   i.icmp_id = 0;
                   i.icmp_seq = 0;
                   si.sin_family = AF_INET;
                   #if defined(__DECC) || defined(_IP_VHL)
                   ip.ip_vhl = 0;
                   #else
                   ip.ip_hl = 0;
                   #endif
                   ])],
   [ac_cv_have_icmp=yes],
   [ac_cv_have_icmp=no])])
 if test "$ac_cv_have_icmp" = yes ; then
   AC_DEFINE(HAVE_ICMP)
 fi])

AC_DEFUN([AC_CHECK_ICMPHDR],
 [AC_CACHE_CHECK([for struct icmphdr], ac_cv_have_icmphdr,
  [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([#include <stdlib.h>
                   #include <stdio.h>
                   #include <math.h>
                   #include <unistd.h>
                   #include <limits.h>
                   #include <signal.h>
                   #include <fcntl.h>
                   #include <sys/types.h>
                   #include <sys/time.h>
                   #include <sys/ipc.h>
                   #include <sys/shm.h>
                   #include <sys/socket.h>
                   #include <netinet/in_systm.h>
                   #include <netinet/in.h>
                   #include <netinet/ip.h>
                   #include <netinet/ip_icmp.h>
                   #include <netinet/udp.h>
                   #include <arpa/inet.h>
                   #include <netdb.h>],
                  [struct icmphdr i;
                   struct sockaddr s;
                   struct sockaddr_in si;
                   struct ip ip;
                   i.type = ICMP_ECHO;
                   i.code = 0;
                   i.checksum = 0;
                   i.un.echo.id = 0;
                   i.un.echo.sequence = 0;
                   si.sin_family = AF_INET;
                   ip.ip_hl = 0;])],
    [ac_cv_have_icmphdr=yes],
    [ac_cv_have_icmphdr=no])])
 if test "$ac_cv_have_icmphdr" = yes ; then
   AC_DEFINE(HAVE_ICMPHDR)
 fi])


###############################################################################
#
#       Functions to check for various X11 crap.
#
###############################################################################

# Try and find the app-defaults directory.
# It sucks that autoconf doesn't do this already...
# (AC_PATH_X_APP_DEFAULTS_XMKMF is no longer used.)
#
AC_DEFUN([AC_PATH_X_APP_DEFAULTS_XMKMF],[
  rm -fr conftestdir
  if mkdir conftestdir; then
    cd conftestdir 2>/dev/null
    # Make sure to not put "make" in the Imakefile rules, since we grep it out.
    cat > Imakefile <<'EOF'
acfindx:
	@echo 'ac_x_app_defaults="${XAPPLOADDIR}"'
EOF
    if (xmkmf) >/dev/null 2>&1 && test -f Makefile; then
      # GNU make sometimes prints "make[1]: Entering...", which'd confuse us.
      eval `${MAKE-make} acfindx 2>/dev/null | grep -v make`
    fi
    cd .. 2>/dev/null
    rm -fr conftestdir
  fi])

AC_DEFUN([AC_PATH_X_APP_DEFAULTS_DIRECT],[
  # Look for the directory under a standard set of common directories.
  # Check X11 before X11Rn because it's often a symlink to the current release.
  for ac_dir in                                 \
                                                \
    /usr/share/X11/app-defaults                 \
                                                \
    /usr/X11/lib/app-defaults                   \
    /usr/X11R6/lib/app-defaults                 \
    /usr/X11R6/lib/X11/app-defaults             \
    /usr/X11R5/lib/app-defaults                 \
    /usr/X11R5/lib/X11/app-defaults             \
    /usr/X11R4/lib/app-defaults                 \
    /usr/X11R4/lib/X11/app-defaults             \
                                                \
    /usr/lib/X11/app-defaults                   \
    /usr/lib/X11R6/app-defaults                 \
    /usr/lib/X11R5/app-defaults                 \
    /usr/lib/X11R4/app-defaults                 \
                                                \
    /etc/X11/app-defaults                       \
                                                \
    /usr/local/X11/lib/app-defaults             \
    /usr/local/X11R6/lib/app-defaults           \
    /usr/local/X11R5/lib/app-defaults           \
    /usr/local/X11R4/lib/app-defaults           \
                                                \
    /usr/local/lib/X11/app-defaults             \
    /usr/local/lib/X11R6/app-defaults           \
    /usr/local/lib/X11R6/X11/app-defaults       \
    /usr/local/lib/X11R5/app-defaults           \
    /usr/local/lib/X11R5/X11/app-defaults       \
    /usr/local/lib/X11R4/app-defaults           \
    /usr/local/lib/X11R4/X11/app-defaults       \
                                                \
    /usr/X386/lib/X11/app-defaults              \
    /usr/x386/lib/X11/app-defaults              \
    /usr/XFree86/lib/X11/app-defaults           \
                                                \
    /usr/lib/X11/app-defaults                   \
    /usr/local/lib/X11/app-defaults             \
    /usr/unsupported/lib/X11/app-defaults       \
    /usr/athena/lib/X11/app-defaults            \
    /usr/local/x11r5/lib/X11/app-defaults       \
    /usr/lpp/Xamples/lib/X11/app-defaults       \
    /lib/usr/lib/X11/app-defaults               \
                                                \
    /usr/openwin/lib/app-defaults               \
    /usr/openwin/lib/X11/app-defaults           \
    /usr/openwin/share/lib/app-defaults         \
    /usr/openwin/share/lib/X11/app-defaults     \
                                                \
    /X11R6/lib/app-defaults                     \
    /X11R5/lib/app-defaults                     \
    /X11R4/lib/app-defaults                     \
    ; \
  do
    if test -d "$ac_dir"; then
      ac_x_app_defaults=$ac_dir
      break
    fi
  done
])

AC_DEFUN([AC_PATH_X_APP_DEFAULTS],
  [AC_REQUIRE_CPP()
    AC_CACHE_CHECK([for X app-defaults directory], ac_cv_x_app_defaults,
     [# skip this, it's always wrong these days.
      #  AC_PATH_X_APP_DEFAULTS_XMKMF
      if test x"$ac_x_app_defaults" = x; then
        true AC_PATH_X_APP_DEFAULTS_DIRECT
      fi
      if test x"$ac_x_app_defaults" = x; then
        /bin/echo -n 'fallback: '
        ac_cv_x_app_defaults="/usr/lib/X11/app-defaults"
      else
        # Record where we found app-defaults for the cache.
        ac_cv_x_app_defaults="$ac_x_app_defaults"
      fi])
    eval ac_x_app_defaults="$ac_cv_x_app_defaults"])


AC_DEFUN([AC_XPOINTER],
 [AC_CACHE_CHECK([for XPointer], ac_cv_xpointer,
                 [AC_TRY_X_COMPILE([#include <X11/Xlib.h>],
                                   [XPointer foo = (XPointer) 0;],
                                   [ac_cv_xpointer=yes],
                                   [ac_cv_xpointer=no])])
  if test "$ac_cv_xpointer" != yes; then
   AC_DEFINE(XPointer,[char*])
  fi])

AC_DEFUN([AC_XT],
 [AC_CACHE_CHECK([for Xt], ac_cv_libxt,
                 [AC_TRY_X_COMPILE([#include <X11/Xlib.h>
                                    #include <X11/Intrinsic.h>],
                                   [Widget foo = (Widget) 0;],
                                   [ac_cv_libxt=yes],
                                   [ac_cv_libxt=no])])])


# Random special-cases for X on certain pathological OSes.
# You know who you are.
#
AC_DEFUN([AC_X_RANDOM_PATHS],
 [case "$host" in
    *-hpux*)

      # The following arcana was gleaned from conversations with
      # Eric Schwartz <erics@col.hp.com>:
      #
      # On HPUX 10.x, the parts of X that HP considers "standard" live in
      # /usr/{include,lib}/X11R6/.  The parts that HP doesn't consider
      # "standard", notably, Xaw and Xmu, live in /usr/contrib/X11R6/.
      # Yet /usr/contrib/X11R6/ comes preinstalled on all HPUX systems.
      # Also, there are symlinks from /usr/include/ and /usr/lib/ into
      # /usr/{include,lib}/X11R6/, so that (if you don't use Xmu at all)
      # you don't need any -I or -L arguments.
      #
      # On HPUX 9.x, /usr/{include,lib}/X11R5/ and /usr/contrib/X11R5/
      # are the same division as 10.x.  However, there are no symlinks to
      # the X stuff from /usr/include/ and /usr/lib/, so -I and -L
      # arguments are always necessary.
      #
      # However, X11R6 was available on HPUX 9.x as a patch: if that
      # patch was installed, then all of X11R6 went in to
      # /usr/contrib/X11R6/ (there was no /usr/{include,lib}/X11R6/.)
      #
      # HPUX 8.x was the same as 9.x, but was X11R4 instead (I don't know
      # whether R5 was available as a patch; R6 undoubtedly was not.)
      #
      # So.  We try and use the highest numbered pair of
      # /usr/{include,lib}/X11R?/ and /usr/contrib/X11R?/{include,lib}/
      # that are available.  We do not mix and match different versions
      # of X.
      #
      # Question I still don't know the answer to: (do you?)
      #
      #   * On HPUX 9.x, where /usr/include/X11R5/ was standard, and
      #     /usr/contrib/X11R6/ could be installed as a patch, what was in
      #     that contrib directory?  Did it contain so-called "standard"
      #     X11R6, or did it include Xaw and Xmu as well?  If the former,
      #     where did one find Xaw and Xmu on 9.x R6 systems?  Would this
      #     be a situation where one had to reach into the R5 headers and
      #     libs to find Xmu?  That is, must both R6 and R5 directories
      #     be on the -I and -L lists in that case?
      #
      for version in X11R6 X11R5 X11R4 ; do
        # if either pair of directories exists...
        if test -d /usr/include/$version || test -d /usr/contrib/$version/include
        then
           # if contrib exists, use it...
           if test -d /usr/contrib/$version/include ; then
             X_CFLAGS="$X_CFLAGS -I/usr/contrib/$version/include"
             X_LIBS="$X_LIBS -L/usr/contrib/$version/lib"
           fi
           # if the "standard" one exists, use it.
           if test -d /usr/include/$version ; then
             X_CFLAGS="$X_CFLAGS -I/usr/include/$version"
             X_LIBS="$X_LIBS -L/usr/lib/$version"
           fi
           # since at least one of the pair exists, go no farther.
           break
        fi
      done

      # Now find Motif.  Thanks for not making xmkmf find this by
      # default, you losers.
      #
      if test -d /usr/include/Motif2.1 ; then
        X_CFLAGS="$X_CFLAGS -I/usr/include/Motif2.1"
        X_LIBS="$X_LIBS -L/usr/lib/Motif2.1"
      elif test -d /usr/include/Motif1.2 ; then
        X_CFLAGS="$X_CFLAGS -I/usr/include/Motif1.2"
        X_LIBS="$X_LIBS -L/usr/lib/Motif1.2"
      elif test -d /usr/include/Motif1.1 ; then
        X_CFLAGS="$X_CFLAGS -I/usr/include/Motif1.1"
        X_LIBS="$X_LIBS -L/usr/lib/Motif1.1"
      fi

      # Now let's check for the pseudo-standard locations for OpenGL.
      #
      if test -d /opt/graphics/OpenGL/include ; then
        # HP-UX 10.20 puts it here
        X_CFLAGS="-I/opt/graphics/OpenGL/include $X_CFLAGS"
        X_LIBS="-L/opt/graphics/OpenGL/lib $X_LIBS"
      elif test -d /opt/Mesa/lib ; then
        X_CFLAGS="-I/opt/Mesa/include $X_CFLAGS"
        X_LIBS="-L/opt/Mesa/lib $X_LIBS"
      fi


      # On HPUX, default to installing in /opt/xscreensaver/ instead of
      # in /usr/local/, unless there is already an xscreensaver in
      # /usr/local/bin/.  This can be overridden with the --prefix arg
      # to configure.  I'm not sure this is the right thing to do, but
      # Richard Lloyd says so...
      #
      if test \! -x /usr/local/bin/xscreensaver ; then
        ac_default_prefix=/opt/xscreensaver
      fi

    ;;
    *-solaris*)

      # Thanks for not making xmkmf find this by default, pinheads.
      # And thanks for moving things around again, too.  Is this
      # really the standard location now?  What happened to the
      # joke that this kind of thing went in /opt?
      # cthomp says "answer: CDE (Common Disorganized Environment)"
      #
      if test -f /usr/dt/include/Xm/Xm.h ; then
        X_CFLAGS="$X_CFLAGS -I/usr/dt/include"
        MOTIF_LIBS="$MOTIF_LIBS -L/usr/dt/lib -R/usr/dt/lib"

        # Some versions of Slowlaris Motif require -lgen.  But not all.  Why?
        AC_CHECK_LIB(gen, regcmp, [MOTIF_LIBS="$MOTIF_LIBS -lgen"])
      fi

    ;;
    *-darwin*)

      # MacPorts on macOS
      if test -d /opt/local/include ; then
        X_CFLAGS="-I/opt/local/include $X_CFLAGS"
          X_LIBS="-L/opt/local/lib $X_LIBS"
      fi

      # Fink on macOS
      if test -d /sw/include ; then
        X_CFLAGS="-I/sw/include $X_CFLAGS"
          X_LIBS="-L/sw/lib $X_LIBS"
      fi

      # Homebrew on macOS Intel
      if test -d /usr/local/include ; then
        X_CFLAGS="-I/usr/local/include $X_CFLAGS"
          X_LIBS="-L/usr/local/lib $X_LIBS"
      fi

      # Homebrew on macOS ARM
      if test -d /opt/homebrew/include ; then
        X_CFLAGS="-I/opt/homebrew/include $X_CFLAGS"
          X_LIBS="-L/opt/homebrew/lib $X_LIBS"
      fi
    ;;
  esac])

AC_DEFUN([AC_CHECK_GETIFADDRS],
 [AC_CACHE_CHECK([for getifaddrs], ac_cv_have_getifaddrs,
  [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([#include <stdlib.h>
                   #include <unistd.h>
                   #include <arpa/inet.h>
                   #include <ifaddrs.h>],
                  [struct ifaddrs *ifa;
                   getifaddrs (&ifa);
                   ifa->ifa_next = 0;
                   ifa->ifa_addr->sa_family = 0;])],
    [ac_cv_have_getifaddrs=yes],
    [ac_cv_have_getifaddrs=no])])
 if test "$ac_cv_have_getifaddrs" = yes ; then
   AC_DEFINE(HAVE_GETIFADDRS)
 fi])

AC_DEFUN([AC_TYPE_SOCKLEN_T],
 [AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
  [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([
      #include <sys/types.h>
      #include <sys/socket.h>],
     [socklen_t socklen;])],
    [ac_cv_type_socklen_t=yes],
    [ac_cv_type_socklen_t=no])])
  if test "$ac_cv_type_socklen_t" != yes; then
    AC_DEFINE(socklen_t, int,
  [Define to `int' if <sys/types.h> or <sys/socket.h> does not define.])
  fi])

###############################################################################
#
#       Some utility functions to make checking for X things easier.
#
###############################################################################

# Like AC_CHECK_HEADER, but it uses the already-computed -I directories.
#
AC_DEFUN([AC_CHECK_X_HEADER], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  CPPFLAGS=`eval eval eval eval eval eval eval eval eval echo $CPPFLAGS`
  AC_CHECK_HEADER([$1],[$2],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])

# Like AC_EGREP_HEADER, but it uses the already-computed -I directories.
#
AC_DEFUN([AC_EGREP_X_HEADER], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  CPPFLAGS=`eval eval eval eval eval eval eval eval eval echo $CPPFLAGS`
  AC_EGREP_HEADER([$1], [$2], [$3], [$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])

# Like AC_COMPILE_IFELSE but it uses the already-computed -I directories.
#
AC_DEFUN([AC_TRY_X_COMPILE], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  CPPFLAGS=`eval eval eval eval eval eval eval eval eval echo $CPPFLAGS`
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$1], [$2])],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])


# Like AC_CHECK_LIB, but it uses the already-computed -I and -L directories.
# Use this sparingly; it probably doesn't work very well on X programs.
#
AC_DEFUN([AC_CHECK_X_LIB], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  ac_save_LDFLAGS="$LDFLAGS"
#  ac_save_LIBS="$LIBS"

  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  # note: $X_CFLAGS includes $x_includes
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  if test \! -z "$libdir" ; then
    LDFLAGS="$LDFLAGS -L$libdir"
  fi
  # note: $X_LIBS includes $x_libraries
  LDFLAGS="$LDFLAGS $X_LIBS $X_EXTRA_LIBS"

  CPPFLAGS=`eval eval eval eval eval eval eval eval eval echo $CPPFLAGS`
  LDFLAGS=`eval eval eval eval eval eval eval eval eval echo $LDFLAGS`
  AC_CHECK_LIB([$1], [$2], [$3], [$4], [$5])
  CPPFLAGS="$ac_save_CPPFLAGS"
  LDFLAGS="$ac_save_LDFLAGS"
#  LIBS="$ac_save_LIBS"
  ])

# Like AC_RUN_IFELSE but it uses the already-computed -I directories.
# (But not the -L directories!)
#
AC_DEFUN([AC_TRY_X_RUN], [
  ac_save_CPPFLAGS="$CPPFLAGS"
  if test \! -z "$includedir" ; then 
    CPPFLAGS="$CPPFLAGS -I$includedir"
  fi
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  CPPFLAGS=`eval eval eval eval eval eval eval eval eval echo $CPPFLAGS`
  AC_RUN_IFELSE([AC_LANG_SOURCE([$1])],[$2],[$3],[$4])
  CPPFLAGS="$ac_save_CPPFLAGS"])



# Usage: HANDLE_X_PATH_ARG([variable_name],
#                          [--command-line-option],
#                          [descriptive string])
#
# All of the --with options take three forms:
#
#   --with-foo (or --with-foo=yes)
#   --without-foo (or --with-foo=no)
#   --with-foo=/DIR
#
# This function, HANDLE_X_PATH_ARG, deals with the /DIR case.  When it sees
# a directory (string beginning with a slash) it checks to see whether
# /DIR/include and /DIR/lib exist, and adds them to $X_CFLAGS and $X_LIBS
# as appropriate.
#
AC_DEFUN([HANDLE_X_PATH_ARG], [
   case "$[$1]" in
    yes) ;;
    no)  ;;

    /*)
     AC_MSG_CHECKING([for [$3] headers])
     d=$[$1]/include
     if test -d $d; then
       X_CFLAGS="-I$d $X_CFLAGS"
       AC_MSG_RESULT($d)
     else
       AC_MSG_RESULT(not found ($d: no such directory))
     fi

     AC_MSG_CHECKING([for [$3] libs])
     d=$[$1]/lib
     if test -d $d; then
       X_LIBS="-L$d $X_LIBS"
       AC_MSG_RESULT($d)
     else
       AC_MSG_RESULT(not found ($d: no such directory))
     fi

     # replace the directory string with "yes".
     [$1]_req="yes"
     [$1]=$[$1]_req
     ;;

    *)
     echo ""
     echo "error: argument to [$2] must be \"yes\", \"no\", or a directory."
     echo "       If it is a directory, then \`DIR/include' will be added to"
     echo "       the -I list, and \`DIR/lib' will be added to the -L list."
     exit 1
     ;;
   esac
  ])


###############################################################################
###############################################################################
#
#       End of function definitions.  Now start actually executing stuff.
#
###############################################################################
###############################################################################

# Default $PATH might not contain /sbin and thus miss /sbin/setcap.
#
export PATH="$PATH:/usr/bin:/bin:/usr/sbin:/sbin"


# WTF!  autoconf emits this *way* too late.  Do it earlier.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'

# random compiler setup
AC_CANONICAL_HOST

ac_original_cc=$CC
AC_PROG_CC
AC_PROG_CC_ANSI
AC_PROG_CPP
AC_C_CONST
AC_C_INLINE
AC_EXEEXT

# stuff for Makefiles
AC_PROG_INSTALL
AC_PROG_INSTALL_DIRS
AC_PROG_MAKE_SET

# By default, autoconf sets INSTALL_SCRIPT to '${INSTALL_PROGRAM}'.
# That's wrong: it should be set to '${INSTALL}', so that one can
# implement the "install-strip" target properly (strip executables,
# but do not try to strip scripts.)
#
INSTALL_SCRIPT='${INSTALL}'

# random libc stuff
AC_CHECK_HEADERS(unistd.h inttypes.h)
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_DEFINE_UNQUOTED([RETSIGTYPE],[void],[Return type of signal handlers])
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_GETTIMEOFDAY_ARGS
AC_SYS_LARGEFILE
AC_CHECK_FUNCS(select fcntl uname nice setpriority getcwd getwd putenv sbrk)
AC_CHECK_FUNCS(sigaction syslog realpath setrlimit)
AC_CHECK_FUNCS(setlocale sqrtf)
AC_CHECK_FUNCS(getaddrinfo)
AC_CHECK_MEMBERS([struct sockaddr.sa_len],,, [#include <sys/socket.h>])
AC_CHECK_ICMP
AC_CHECK_ICMPHDR
AC_CHECK_GETIFADDRS
AC_TYPE_SOCKLEN_T
AC_CHECK_HEADERS(crypt.h sys/select.h)
AC_PROG_PERL

if test -z "$PERL" ; then
  # don't let it be blank...
  PERL=/usr/bin/perl
fi

AC_PATH_XTRA

if test "$have_x" != yes; then
  AC_MSG_ERROR(Couldn't find X11 headers/libs.  Try `$0 --help'.)
fi

AC_X_RANDOM_PATHS
AC_XPOINTER
AC_XT


AC_MSG_CHECKING(whether this is macOS)
  ac_macosx=no
  ac_irix=no
  case "$host" in
    *-apple-darwin* )
      ac_macosx=yes
    ;;
    *-irix* )
      ac_irix=yes
    ;;
  esac
AC_MSG_RESULT($ac_macosx)


###############################################################################
#
#       pkg-config
#
###############################################################################

# Note: In the decades since I wrote this, PKG_CHECK_MODULES came into
# existence, which could probably simplify the following quite a bit.

AC_PATH_TOOL(pkg_config, pkg-config)

if test -z "$pkg_config" ; then
  AC_MSG_WARN([pkg-config not found!])
  pkg_config="false"
fi


# Utility function for running pkg-config-based tests...
#
pkgs=''
pkg_check_version() {
  if test "$ok" = yes ; then
    req="$1"
    min="$2"
    AC_MSG_CHECKING(for $req)
    if $pkg_config --exists "$req" ; then
      vers=`$pkg_config --modversion "$req"`
      if $pkg_config --exists "$req >= $min" ; then
        AC_MSG_RESULT($vers)
        pkgs="$pkgs $req"
        return 1
      else
        AC_MSG_RESULT($vers (wanted >= $min))
        ok=no
        return 0
      fi
    else
      AC_MSG_RESULT(no)
      ok=no
      return 0
    fi
  fi
}


###############################################################################
#
#       Gettext support
#
###############################################################################

# Wow, this is complete madness.  Why does po/Makefile.in.in exist??  Why does
# aclocal.m4 exist??  Why can't I upgrade it without everything breaking??
# This IT_PROG_INTLTOOL nonsense has so many self-modifying levels of insanity
# that it *must* have been written by RMS.  Or an equally demented protégé.
# Won't someone please have mercy on us all and write a normal po/Makefile.in
# that uses wildcards, and write a sane set of rules to detect gettext(3) and
# msgfmt(1) and just do the obviously straightforward thing?

IT_PROG_INTLTOOL
GETTEXT_PACKAGE=xscreensaver
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
		   [This is the name of the gettext package to use.])
AC_SUBST(GETTEXT_PACKAGE)

ALL_LINGUAS="da de es et fi fr hu it ja ko nb nl pl pt pt_BR ru sk sv vi wa zh_CN zh_TW"
AM_GNU_GETTEXT([external])
MKINSTALLDIRS="$INSTALL_DIRS"

# The library is there, but neither $INTLLIBS nor $LIBINTL are getting set.
if test -z "$INTLLIBS" ; then INTLLIBS="$LIBINTL" ; fi
if test -z "$INTLLIBS" ; then
  AC_CHECK_X_LIB(intl, gettext, [INTLLIBS="-lintl"])
fi
LIBINTL="$INTLLIBS"
AC_SUBST(INTLLIBS)
AC_SUBST(LIBINTL)
AC_SUBST(CATALOGS)


###############################################################################
#
#       The --with-x-app-defaults option
#
###############################################################################

#### H1

AC_ARG_WITH(app-defaults,[
Installation Options:

  --prefix=PREFIX         Install root of architecture-independent files.
  --exec-prefix=EPREFIX   Install root of architecture-dependent files.
  --with-app-defaults=DIR Where to install the default settings file.],
	[ac_cv_x_app_defaults="$withval"],
	[eval ac_x_app_defaults="$withval"])
AC_PATH_X_APP_DEFAULTS


###############################################################################
#
#       Handle the --with-hackdir option
#
###############################################################################

have_hackdir=yes
with_hackdir_req=unspecified
AC_ARG_WITH(hackdir,
[  --with-hackdir=DIR      Where to install the hundreds of demo executables.],
  [with_hackdir="$withval"; with_hackdir_req="$withval"],[with_hackdir=yes])

if test x"$with_hackdir" = xyes; then
  HACKDIR='${libexecdir}/xscreensaver'
elif test x"$with_hackdir" = xno; then
  HACKDIR='${bindir}'
else
  HACKDIR=$with_hackdir
fi

# canonicalize slashes.
HACKDIR=`echo "${HACKDIR}" | sed 's@/$@@;s@//*@/@g'`

# Expand HACKDIR as HACKDIR_FULL
HACKDIR_FULL=`eval eval eval eval eval eval eval eval eval echo $HACKDIR`

# This option used to be called --enable-subdir; make sure that is no longer
# used, since configure brain-damagedly ignores unknown --enable options.

obsolete_enable=
AC_ARG_ENABLE(subdir,,[obsolete_enable=yes])
if test -n "$obsolete_enable"; then
  echo "error: the --enable-subdir option has been replaced with"
  echo "       the new --with-hackdir option; see \`configure --help'"
  echo "       for more information."
  exit 1
fi


###############################################################################
#
#       Handle the --with-configdir option
#
###############################################################################

have_configdir=yes
with_configdir_req=unspecified
AC_ARG_WITH(configdir,
[  --with-configdir=DIR    Where to install the GUI configuration files.],
  [with_configdir="$withval"; with_configdir_req="$withval"],
  [with_configdir=yes])

if test x"$with_configdir" = xyes; then
  HACK_CONF_DIR='${datadir}/xscreensaver/config'
elif test x"$with_configdir" = xno; then
  echo "error: must be yes, or a pathname: --with-configdir=$with_configdir"
  exit 1
else
  # there must be a better way than this...
  if test -z "`echo $with_configdir | sed 's@^/.*@@'`" ; then
    # absolute path
    HACK_CONF_DIR=$with_configdir
  else
    # relative path
    HACK_CONF_DIR="\${exec_prefix}$with_configdir"
  fi
fi



###############################################################################
#
#       Handle the --with-fontdir option
#
###############################################################################

have_fontdir=yes
with_fontdir_req=unspecified
AC_ARG_WITH(fontdir,
[  --with-fontdir=DIR      Where to install the extra font files.],
  [with_fontdir="$withval"; with_fontdir_req="$withval"],
  [with_fontdir=yes])

if test x"$with_fontdir" = xyes; then
  FONT_DIR='${datarootdir}/fonts/xscreensaver'
  if test "$ac_macosx" = yes; then
    FONT_DIR='/Library/Fonts'
  fi
elif test x"$with_fontdir" = xno; then
  FONT_DIR=''
else
  # there must be a better way than this...
  if test -z "`echo $with_fontdir | sed 's@^/.*@@'`" ; then
    # absolute path
    FONT_DIR="$with_fontdir"
  else
    # relative path
    FONT_DIR='${datarootdir}/fonts/'"$with_fontdir"
  fi
fi



###############################################################################
#
#       Check for the DPMS server extension.
#
###############################################################################

#### H1

have_dpms=no
with_dpms_req=unspecified
AC_ARG_WITH(dpms-ext,
[
Server Extension Options:

  --with-dpms-ext         Include support for power management.],
  [with_dpms="$withval"; with_dpms_req="$withval"],[with_dpms=yes])

HANDLE_X_PATH_ARG(with_dpms, --with-dpms-ext, DPMS)

if test "$with_dpms" = yes; then

  # first check for dpms.h
  AC_CHECK_X_HEADER(X11/extensions/dpms.h, [have_dpms=yes],,
                    [#include <X11/Xlib.h>
		     #include <X11/Xmd.h>])

  # if that succeeded, then check for the DPMS code in the libraries
  if test "$have_dpms" = yes; then

    # first look in -lXext (this is where it is with XFree86 4.0)
    have_dpms=no
    AC_CHECK_X_LIB(Xext, DPMSInfo, [have_dpms=yes], [true], -lXext -lX11)

    # if that failed, look in -lXdpms (this is where it was in XFree86 3.x)
    if test "$have_dpms" = no; then
      AC_CHECK_X_LIB(Xdpms, DPMSInfo,
                    [have_dpms=yes; XDPMS_LIBS="-lXdpms"], [true],
                    -lXext -lX11)
    fi
  fi


  # if that succeeded, then we've really got it.
  if test "$have_dpms" = yes; then
    AC_DEFINE(HAVE_DPMS_EXTENSION)
  fi

elif test "$with_dpms" != no; then
  echo "error: must be yes or no: --with-dpms-ext=$with_dpms"
  exit 1
fi


###############################################################################
#
#       Check for the XF86VMODE server extension (for virtual screens.)
#
###############################################################################

have_xf86vmode=no
with_xf86vmode_req=unspecified
AC_ARG_WITH(xf86vmode-ext,
[  --with-xf86vmode-ext    Include support for virtual screens.],
  [with_xf86vmode="$withval"; with_xf86vmode_req="$withval"],
  [with_xf86vmode=yes])

HANDLE_X_PATH_ARG(with_xf86vmode, --with-xf86vmode-ext, xf86vmode)

VIDMODE_LIBS=""

if test "$with_xf86vmode" = yes; then

  # first check for xf86vmode.h
  AC_CHECK_X_HEADER(X11/extensions/xf86vmode.h, [have_xf86vmode=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for the -lXxf86vm
  if test "$have_xf86vmode" = yes; then
    have_xf86vmode=no
    AC_CHECK_X_LIB(Xxf86vm, XF86VidModeGetViewPort,
                  [have_xf86vmode=yes;
                   VIDMODE_LIBS="-lXxf86vm";
                   SAVER_LIBS="$SAVER_LIBS $VIDMODE_LIBS"],
                   [true], -lXext -lX11)
  fi

  # if that succeeded, then we've really got it.
  if test "$have_xf86vmode" = yes; then
    AC_DEFINE(HAVE_XF86VMODE)
  fi

elif test "$with_xf86vmode" != no; then
  echo "error: must be yes or no: --with-xf86vmode-ext=$with_xf86vmode"
  exit 1
fi


###############################################################################
#
#       Check for the XINERAMA server extension.
#
###############################################################################

have_xinerama=no
with_xinerama_req=unspecified
AC_ARG_WITH(xinerama-ext,
[  --with-xinerama-ext     Include support for multiple monitors.],
  [with_xinerama="$withval"; with_xinerama_req="$withval"],[with_xinerama=yes])

HANDLE_X_PATH_ARG(with_xinerama, --with-xinerama-ext, XINERAMA)

if test "$with_xinerama" = yes; then

  # first check for Xinerama.h
  AC_CHECK_X_HEADER(X11/extensions/Xinerama.h, [have_xinerama=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for the XINERAMA code in the libraries
  if test "$have_xinerama" = yes; then

    # first look in -lXext
    have_xinerama=no
    AC_CHECK_X_LIB(Xext, XineramaQueryScreens, [have_xinerama=yes], [true],
                  -lXext -lX11)

    # if that failed, look in -lXinerama (this is where it is in XFree86 4.1.)
    if test "$have_xinerama" = no; then
      AC_CHECK_X_LIB(Xinerama, XineramaQueryScreens,
                     [have_xinerama=yes;
                      XINERAMA_LIBS="$XINERAMA_LIBS -lXinerama"],
                     [true], -lXext -lX11)
    fi
  fi

  # if that succeeded, then we've really got it.
  if test "$have_xinerama" = yes; then
    AC_DEFINE(HAVE_XINERAMA)
  fi

elif test "$with_xinerama" != no; then
  echo "error: must be yes or no: --with-xinerama-ext=$with_xinerama"
  exit 1
fi


###############################################################################
#
#       Check for the RANDR (Resize and Rotate) server extension.
#
#       We need this to detect when the resolution of the desktop
#       has changed out from under us (this is a newer, different
#       mechanism than the XF86VMODE virtual viewports.)
#
###############################################################################

have_randr=no
with_randr_req=unspecified
AC_ARG_WITH(randr-ext,
[  --with-randr-ext        Include support for multiple monitors.],
  [with_randr="$withval"; with_randr_req="$withval"],[with_randr=yes])

HANDLE_X_PATH_ARG(with_randr, --with-randr-ext, RANDR)

if test "$with_randr" = yes; then

  # first check for Xrandr.h
  AC_CHECK_X_HEADER(X11/extensions/Xrandr.h, [have_randr=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for the XRR code in the libraries
  if test "$have_randr" = yes; then

    # RANDR probably needs -lXrender
    xrender_libs=
    AC_CHECK_X_LIB(Xrender, XRenderSetSubpixelOrder,
                   [xrender_libs="-lXrender"], [true], -lXext -lX11)

    # first look for RANDR in -lXext
    have_randr=no
    AC_CHECK_X_LIB(Xext, XRRGetScreenInfo,
                   [have_randr=yes; SAVER_LIBS="$SAVER_LIBS $xrender_libs"],
                   [true], $xrender_libs -lXext -lX11)

    # if that failed, look in -lXrandr
    if test "$have_randr" = no; then
      AC_CHECK_X_LIB(Xrandr, XRRGetScreenInfo,
             [have_randr=yes;
              XINERAMA_LIBS="$XINERAMA_LIBS -lXrandr $xrender_libs"],
                     [true], $xrender_libs -lXext -lX11)
    fi
  fi

  # if that succeeded, then we've really got it.
  if test "$have_randr" = yes; then
    AC_DEFINE(HAVE_RANDR)

    # Now check for version 1.2 in the same libs.
    # Try to compile, since on macOS 10.5.7, headers are older than libs!
    AC_CACHE_CHECK([for XRRGetScreenResources], ac_cv_randr_12,
      [ac_cv_randr_12=no
       AC_TRY_X_COMPILE([#include <stdlib.h>
                         #include <X11/Xlib.h>
                         #include <X11/extensions/Xrandr.h>],
                        [XRRScreenResources *res = 
                           XRRGetScreenResources (0, 0);],
                        [ac_cv_randr_12=yes],
                        [ac_cv_randr_12=no])])
    if test "$ac_cv_randr_12" = yes ; then
      AC_DEFINE(HAVE_RANDR_12)
    fi
#   AC_CHECK_X_LIB(c, XRRGetOutputInfo, [AC_DEFINE(HAVE_RANDR_12)],
#                  [true], $SAVER_LIBS)
  fi


elif test "$with_randr" != no; then
  echo "error: must be yes or no: --with-randr-ext=$with_randr"
  exit 1
fi


###############################################################################
#
#       Check for the XINPUT server extension.
#
###############################################################################

have_xinput=no
with_xinput_req=unspecified
xinput_halfassed=no
AC_ARG_WITH(xinput-ext,
[  --with-xinput-ext       The XInput2 extension is required.],
  [with_xinput="$withval"; with_xinput_req="$withval"], [with_xinput=yes])

HANDLE_X_PATH_ARG(with_xinput, --with-xinput-ext, XINPUT)

if test "$with_xinput" = yes; then

  # first check for Xinput2.h
  AC_CHECK_X_HEADER(X11/extensions/XInput2.h, [have_xinput=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for libXi
  if test "$have_xinput" = yes; then
    have_xinput=no
    xinput_halfassed=yes
    AC_CHECK_X_LIB(Xi, XISelectEvents,
                   [have_xinput=yes; xinput_halfassed=no;
                    SAVER_LIBS="$SAVER_LIBS -lXi"],
                   [true], -lXext -lX11)
  fi

  # if that succeeded, then we've really got it.
  if test "$have_xinput" = yes; then
    AC_DEFINE(HAVE_XINPUT)
  fi

elif test "$with_xinput" = no; then
  echo "error: --without-xinput-ext is not supported."
  exit 1

elif test "$with_xinput" != no; then
  echo "error: must be yes or no: --with-xinput-ext=$with_xinput"
  exit 1
fi


###############################################################################
#
#       Check for the XF86VMODE server extension (for gamma fading.)
#
###############################################################################

have_xf86gamma=no
have_xf86gamma_ramp=no
with_xf86gamma_req=unspecified
AC_ARG_WITH(xf86gamma-ext,
[  --with-xf86gamma-ext    Include support for XFree86 gamma fading.],
  [with_xf86gamma="$withval"; with_xf86gamma_req="$withval"],
  [with_xf86gamma=yes])

HANDLE_X_PATH_ARG(with_xf86gamma, --with-xf86gamma-ext, xf86gamma)

if test "$with_xf86gamma" = yes; then

  # first check for xf86vmode.h, if we haven't already
  if test "$have_xf86vmode" = yes; then
    have_xf86gamma=yes
  else
    AC_CHECK_X_HEADER(X11/extensions/xf86vmode.h, [have_xf86gamma=yes],,
                      [#include <X11/Xlib.h>])
  fi

  # if that succeeded, then check for the -lXxf86vm
  if test "$have_xf86gamma" = yes; then
    have_xf86gamma=no
    AC_CHECK_X_LIB(Xxf86vm, XF86VidModeSetGamma,
                  [have_xf86gamma=yes],
                   [true], -lXext -lX11)
  fi

  # check for the Ramp versions of the functions too.
  if test "$have_xf86gamma" = yes; then
    have_xf86gamma_ramp=no
    AC_CHECK_X_LIB(Xxf86vm, XF86VidModeSetGammaRamp,
                  [have_xf86gamma_ramp=yes],
                   [true], -lXext -lX11)
  fi

  # if those tests succeeded, then we've really got the functions.
  if test "$have_xf86gamma" = yes; then
    AC_DEFINE(HAVE_XF86VMODE_GAMMA)
  fi

  if test "$have_xf86gamma_ramp" = yes; then
    AC_DEFINE(HAVE_XF86VMODE_GAMMA_RAMP)
  fi

  # pull in the lib, if we haven't already
  if test "$have_xf86gamma" = yes -a "$have_xf86vmode" = no; then
    SAVER_LIBS="$SAVER_LIBS -lXxf86vm"
  fi

elif test "$with_xf86gamma" != no; then
  echo "error: must be yes or no: --with-xf86gamma-ext=$with_xf86vmode"
  exit 1
fi


###############################################################################
#
#       Check for the XIDLE server extension.
#
###############################################################################

have_xidle=no
with_xidle_req=unspecified
AC_ARG_WITH(xidle-ext,
[  --with-xidle-ext        Include support for the X11R5 XIDLE extension.],
  [with_xidle="$withval"; with_xidle_req="$withval"],[with_xidle=yes])

HANDLE_X_PATH_ARG(with_xidle, --with-xidle-ext, XIDLE)

if test "$with_xidle" = yes; then
  AC_CHECK_X_HEADER(X11/extensions/xidle.h,
                    [have_xidle=yes
                     AC_DEFINE(HAVE_XIDLE_EXTENSION)],,
                    [#include <X11/Xlib.h>])
elif test "$with_xidle" != no; then
  echo "error: must be yes or no: --with-xidle-ext=$with_xidle"
  exit 1
fi


###############################################################################
#
#       Check for the MIT-SCREEN-SAVER server extension.
#
###############################################################################

have_mit=no
with_mit=no
with_mit_req=unspecified
#AC_ARG_WITH(mit-ext,
#[  --with-mit-ext          Include support for the MIT-SCREEN-SAVER extension.],
#  [with_mit="$withval"; with_mit_req="$withval"],[with_mit=no])
#HANDLE_X_PATH_ARG(with_mit, --with-mit-ext, MIT-SCREEN-SAVER)

if test "$with_mit" = yes; then
  AC_CHECK_X_HEADER(X11/extensions/scrnsaver.h, [have_mit=yes],,
                    [#include <X11/Xlib.h>])

  # Now check to see if it's really in the library; XF86Free-3.3 ships
  # scrnsaver.h, but doesn't include the code in libXext.a, the idiots!
  #
  if test "$have_mit" = yes; then
    AC_CHECK_X_LIB(Xext, XScreenSaverRegister, [true], [have_mit=no], -lm)

    if test "$have_mit" = no; then
      # Fuck!  Looks like XF86Free-3.3 actually puts it in XExExt instead
      # of in Xext.  Thank you master, may I have another.
      AC_CHECK_X_LIB(XExExt, XScreenSaverRegister,
                     [have_mit=yes; SAVER_LIBS="$SAVER_LIBS -lXExExt"],
                     [true], -lX11 -lXext -lm)
    fi

    if test "$have_mit" = no; then
      # Double fuck!  Looks like some versions of XFree86 (whichever version
      # it is that comes with RedHat Linux 2.0 -- I can't find a version 
      # number) put this garbage in Xss instead of Xext.  Thank you master,
      #  may I have another.
      AC_CHECK_X_LIB(Xss, XScreenSaverRegister,
                     [have_mit=yes; SAVER_LIBS="$SAVER_LIBS -lXss"],
                     [true], -lX11 -lXext -lm)
    fi

    if test "$have_mit" = yes; then
#     AC_DEFINE(HAVE_MIT_SAVER_EXTENSION)
      echo "Using the MIT-SCREEN-SAVER extension will crash your server. Don't."
      exit 1
    fi

  fi

elif test "$with_mit" != no; then
  echo "error: must be yes or no: --with-mit-ext=$with_mit"
  exit 1
fi


###############################################################################
#
#       Check for the SGI SCREEN_SAVER server extension.
#
###############################################################################

have_sgi=no
with_sgi_req=unspecified
AC_ARG_WITH(sgi-ext,
[  --with-sgi-ext          Include support for the SGI SCREEN_SAVER extension.],
  [with_sgi="$withval"; with_sgi_req="$withval"],[with_sgi=$ac_irix])

HANDLE_X_PATH_ARG(with_sgi, --with-sgi-ext, SGI SCREEN_SAVER)

if test "$with_sgi" = yes; then
  AC_CHECK_X_HEADER(X11/extensions/XScreenSaver.h,
                    [have_sgi=yes
                     AC_DEFINE(HAVE_SGI_SAVER_EXTENSION)],,
                    [#include <X11/Xlib.h>])

elif test "$with_sgi" != no; then
  echo "error: must be yes or no: --with-sgi-ext=$with_sgi"
  exit 1
fi


###############################################################################
#
#       Check for the SGI-VIDEO-CONTROL server extension.
#
###############################################################################

have_sgivc=no
with_sgivc_req=unspecified
AC_ARG_WITH(sgivc-ext,
[  --with-sgivc-ext        Include support for the SGI-VIDEO-CONTROL extension.],
  [with_sgivc="$withval"; with_sgivc_req="$withval"],[with_sgivc=$ac_irix])

HANDLE_X_PATH_ARG(with_sgivc, --with-sgivc-ext, SGI-VIDEO-CONTROL)

if test "$with_sgivc" = yes; then

  # first check for XSGIvc.h
  AC_CHECK_X_HEADER(X11/extensions/XSGIvc.h, [have_sgivc=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for the -lXsgivc
  if test "$have_sgivc" = yes; then
    have_sgivc=no
    AC_CHECK_X_LIB(Xsgivc, XSGIvcQueryGammaMap,
                  [have_sgivc=yes; SAVER_LIBS="$SAVER_LIBS -lXsgivc"], [true],
                  -lXext -lX11)
  fi

  # if that succeeded, then we've really got it.
  if test "$have_sgivc" = yes; then
    AC_DEFINE(HAVE_SGI_VC_EXTENSION)
  fi

elif test "$with_sgivc" != no; then
  echo "error: must be yes or no: --with-sgivc-ext=$with_sgivc"
  exit 1
fi


###############################################################################
#
#       Check for the XSHM server extension.
#
###############################################################################

have_xshm=no
with_xshm_req=unspecified
AC_ARG_WITH(xshm-ext,
[  --with-xshm-ext         Include support for the Shared Memory extension.],
  [with_xshm="$withval"; with_xshm_req="$withval"],[with_xshm=yes])

HANDLE_X_PATH_ARG(with_xshm, --with-xshm-ext, XSHM)

if test "$with_xshm" = yes; then

  # first check for Xshm.h.
  AC_CHECK_X_HEADER(X11/extensions/XShm.h, [have_xshm=yes],,
                    [#include <X11/Xlib.h>])

  # if that succeeded, then check for sys/ipc.h.
  if test "$have_xshm" = yes; then
    have_xshm=no
    AC_CHECK_X_HEADER(sys/ipc.h, [have_xshm=yes])
  fi

  # if that succeeded, then check for sys/shm.h.
  if test "$have_xshm" = yes; then
    have_xshm=no
    AC_CHECK_X_HEADER(sys/shm.h, [have_xshm=yes])
  fi

  # AIX is pathological, as usual: apparently it's normal for the Xshm headers
  # to exist, but the library code to not exist.  And even better, the library
  # code is in its own library: libXextSam.a.  So, if we're on AIX, and that
  # lib doesn't exist, give up.  (This lib gets added to X_EXTRA_LIBS, and
  # that's not quite right, but close enough.)
  #
  case "$host" in
    *-aix*)
      if [ `uname -v` -eq 3 ]; then
        have_xshm=no
        AC_CHECK_X_LIB(XextSam, XShmQueryExtension,
                       [have_xshm=yes; X_EXTRA_LIBS="$X_EXTRA_LIBS -lXextSam"],
                       [true], -lX11 -lXext -lm)
      fi
    ;;
  esac

  # if that succeeded, then we've really got it.
  if test "$have_xshm" = yes; then
    AC_DEFINE(HAVE_XSHM_EXTENSION)
  fi

elif test "$with_xshm" != no; then
  echo "error: must be yes or no: --with-xshm-ext=$with_xshm"
  exit 1
fi


###############################################################################
#
#       Check for the DOUBLE-BUFFER server extension.
#
###############################################################################

have_xdbe=no
with_xdbe_req=unspecified
AC_ARG_WITH(xdbe-ext,
[  --with-xdbe-ext         Include support for the DOUBLE-BUFFER extension.],
  [with_xdbe="$withval"; with_xdbe_req="$withval"],[with_xdbe=yes])

HANDLE_X_PATH_ARG(with_xdbe, --with-xdbe-ext, DOUBLE-BUFFER)

if test "$with_xdbe" = yes; then

  AC_CHECK_X_HEADER(X11/extensions/Xdbe.h, [have_xdbe=yes],,
                    [#include <X11/Xlib.h>])
  if test "$have_xdbe" = yes; then
    AC_DEFINE(HAVE_DOUBLE_BUFFER_EXTENSION)
  fi

elif test "$with_xdbe" != no; then
  echo "error: must be yes or no: --with-xdbe-ext=$with_xshm"
  exit 1
fi


###############################################################################
#
#       Check for the COMPOSITE server extension.
#
###############################################################################

#have_xcomposite=no
#with_xcomposite_req=unspecified
#AC_ARG_WITH(xcomposite-ext,
#[  --with-xcomposite-ext   Include support for the XCOMPOSITE extension.],
#  [with_xcomposite="$withval"; with_xcomposite_req="$withval"],
#  [with_xcomposite=yes])
#
#HANDLE_X_PATH_ARG(with_xcomposite, --with-xcomposite-ext, XCOMPOSITE)
#
#if test "$with_xcomposite" = yes; then
#
#  AC_CHECK_X_HEADER(X11/extensions/Xcomposite.h, [have_xcomposite=yes],,
#                    [#include <X11/Xlib.h>])
#  if test "$have_xcomposite" = yes; then
#    AC_DEFINE(HAVE_XCOMPOSITE_EXTENSION)
#    SAVER_LIBS="-lXcomposite $SAVER_LIBS"
#  fi
#
#elif test "$with_xcomposite" != no; then
#  echo "error: must be yes or no: --with-xcomposite-ext=$with_xshm"
#  exit 1
#fi


###############################################################################
#
#       Check for the XKB server extension.
#
###############################################################################

have_xkb=no
with_xkb_req=unspecified
AC_ARG_WITH(xkb-ext,
[  --with-xkb-ext          Include support for the X Keyboard extension.],
  [with_xkb="$withval"; with_xkb_req="$withval"],
  [with_xkb=yes])

HANDLE_X_PATH_ARG(with_xkb, --with-xkb-ext, XKB)

if test "$with_xkb" = yes; then

  AC_CHECK_X_HEADER(X11/XKBlib.h, [have_xkb=yes],,
                    [#include <X11/Xlib.h>])
  if test "$have_xkb" = yes; then
    AC_DEFINE(HAVE_XKB)
  fi

elif test "$with_xkb" != no; then
  echo "error: must be yes or no: --with-xkb-ext=$with_xshm"
  exit 1
fi


###############################################################################
#
#       Check for HP XHPDisableReset and XHPEnableReset.
#
###############################################################################

AC_MSG_CHECKING([for XHPDisableReset in X11/XHPlib.h])
AC_EGREP_X_HEADER(XHPDisableReset, X11/XHPlib.h,
                  [AC_DEFINE(HAVE_XHPDISABLERESET)
                   SAVER_LIBS="-lXhp11 $SAVER_LIBS"
                   AC_MSG_RESULT(yes)],
                  [AC_MSG_RESULT(no)])


###############################################################################
#
#       Check for /proc/interrupts.
#
###############################################################################

have_proc_interrupts=no
with_proc_interrupts_req=unspecified
AC_ARG_WITH(proc-interrupts,
[  --with-proc-interrupts  Include support for consulting /proc/interrupts to
                          notice activity on PS/2 keyboards and mice.],
  [with_proc_interrupts="$withval"; with_proc_interrupts_req="$withval"],
  [with_proc_interrupts=no])

if test "$with_proc_interrupts" = yes; then

  # Note that we may be building in an environment (e.g. Debian buildd chroot)
  # without a proper /proc filesystem. If /proc/interrupts exists, then we'll
  # check that it has the bits we need, but otherwise we'll just go on faith.
  #
  have_proc_interrupts=yes

  if test -f /proc/interrupts; then
   AC_CACHE_CHECK([whether /proc/interrupts contains keyboard data],
    ac_cv_have_proc_interrupts,
    [ac_cv_have_proc_interrupts=no
     if grep 'keyboard\|i8042' /proc/interrupts >/dev/null 2>&1 ; then
       ac_cv_have_proc_interrupts=yes
     fi
    ])
   have_proc_interrupts=$ac_cv_have_proc_interrupts
  fi

  if test "$have_proc_interrupts" = yes; then
    AC_DEFINE(HAVE_PROC_INTERRUPTS)
  fi

elif test "$with_proc_interrupts" != no; then
  echo "error: must be yes or no: --with-proc-interrupts=$with_proc_interrupts"
  exit 1
fi


###############################################################################
#
#       Check for /proc/*/oom_score_adj.
#
###############################################################################

setuid_auth=no
have_proc_oom=no
with_proc_oom_req=unspecified
AC_ARG_WITH(proc-oom,
[  --with-proc-oom         Include support to duck the out-of-memory killer.],
  [with_proc_oom="$withval"; with_proc_oom_req="$withval"],
  [with_proc_oom=yes])

if test "$with_proc_oom_req" = yes; then

  # Note that we may be building in an environment (e.g. Debian buildd chroot)
  # without a proper /proc filesystem.
  #
  have_proc_oom=yes
  AC_DEFINE(HAVE_PROC_OOM)

elif test "$with_proc_oom_req" = unspecified; then

  have_proc_oom=no
  AC_CACHE_CHECK([whether /proc/$$/oom_score_adj exists],
    ac_cv_have_proc_oom,
    [ac_cv_have_proc_oom=no
     if test -f /proc/$$/oom_score_adj; then
       ac_cv_have_proc_oom=yes
     fi
    ])
  have_proc_oom=$ac_cv_have_proc_oom

  if test "$have_proc_oom" = yes; then
    AC_DEFINE(HAVE_PROC_OOM)
    # Only root can write to /proc/$$/oom_score_adj, even though it's us.
    AC_MSG_RESULT(enabling setuid to opt out of OOM-killer.)
    setuid_auth=yes
  fi

elif test "$with_proc_oom" != no; then
  echo "error: must be yes or no: --with-proc-oom=$with_proc_oom"
  exit 1
fi


###############################################################################
#
#       Check for -lsystemd
#
###############################################################################

have_systemd=no
with_systemd_req=unspecified
systemd_halfassed=no
systemd_too_old=no
AC_ARG_WITH(systemd,
[  --with-systemd          Support systemd requests to lock on suspend, and to
                          allow video players to inhibit the screen saver.],
  [with_systemd="$withval"; with_systemd_req="$withval"],
  [with_systemd=yes])

HANDLE_X_PATH_ARG(with_systemd, --with-systemd, systemd)

if test "$with_systemd" != yes -a "$with_systemd" != no ; then
  echo "error: must be yes or no: --with-systemd=$with_systemd"
  exit 1
fi

if test "$with_systemd" = yes; then

  pkgs=''
  ok="yes"
  pkg_check_version libsystemd 221
  have_systemd="$ok"

  if test "$have_systemd" = no; then
    if $pkg_config --exists libsystemd ; then
      systemd_too_old=yes
    fi
  fi

  if test "$have_systemd" = yes; then
    AC_CACHE_CHECK([for libsystemd includes], ac_cv_systemd_config_cflags,
               [ac_cv_systemd_config_cflags=`$pkg_config --cflags $pkgs`])
    AC_CACHE_CHECK([for libsystemd libs], ac_cv_systemd_config_libs,
               [ac_cv_systemd_config_libs=`$pkg_config --libs $pkgs`])
  fi

  ac_systemd_config_cflags=$ac_cv_systemd_config_cflags
  ac_systemd_config_libs=$ac_cv_systemd_config_libs

  if test "$have_systemd" = yes; then
    #
    # we appear to have libsystemd; check for headers/libs to be sure.
    #
    ac_save_systemd_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_systemd_config_cflags"

    have_systemd=no
    AC_CHECK_X_HEADER(systemd/sd-bus.h, [have_systemd=yes])

    CPPFLAGS="$ac_save_systemd_CPPFLAGS"
  fi

  if test "$have_systemd" = yes; then
    # we have the headers, now check for the libraries
    have_systemd=no
    systemd_halfassed=yes
    AC_MSG_RESULT(checking for libsystemd usability...)
    # sd_bus_track_count_name was added in some later version of systemd.
    AC_CHECK_X_LIB(c, sd_bus_track_count_name,
                   [have_systemd=yes; systemd_halfassed=no],,
                   $ac_systemd_config_libs -lX11 -lXext -lm)
  fi

  if test "$have_systemd" = no; then
    AC_MSG_RESULT(checking for libsystemd usability... no)
  fi
fi

if test "$have_systemd" = yes; then
  INCLUDES="$INCLUDES $ac_systemd_config_cflags"
  EXES_SYSTEMD='$(EXES_SYSTEMD)'
  SYSTEMD_LIBS="$ac_systemd_config_libs"
  AC_DEFINE(HAVE_LIBSYSTEMD)
else
  EXES_SYSTEMD=''
  SYSTEMD_LIBS=''
fi


###############################################################################
#
#       Check for -lelogind, a stripped down subset of systemd.
#
###############################################################################

have_elogind=no
with_elogind_req=unspecified
elogind_halfassed=no
elogind_too_old=no
AC_ARG_WITH(elogind,
[  --with-elogind          Use elogind instead of systemd.],
  [with_elogind="$withval"; with_elogind_req="$withval"],
  [with_elogind=yes])

HANDLE_X_PATH_ARG(with_elogind, --with-elogind, elogind)

if test "$with_elogind" != yes -a "$with_elogind" != no ; then
  echo "error: must be yes or no: --with-elogind=$with_elogind"
  exit 1
fi

if test "$with_elogind" = yes; then

  pkgs=''
  ok="yes"
  pkg_check_version libelogind 221
  have_elogind="$ok"

  if test "$have_elogind" = no; then
    if $pkg_config --exists libelogind ; then
      elogind_too_old=yes
    fi
  fi

  if test "$have_elogind" = yes; then
    AC_CACHE_CHECK([for libelogind includes], ac_cv_elogind_config_cflags,
               [ac_cv_elogind_config_cflags=`$pkg_config --cflags $pkgs`])
    AC_CACHE_CHECK([for libelogind libs], ac_cv_elogind_config_libs,
               [ac_cv_elogind_config_libs=`$pkg_config --libs $pkgs`])
  fi

  ac_elogind_config_cflags=$ac_cv_elogind_config_cflags
  ac_elogind_config_libs=$ac_cv_elogind_config_libs

  if test "$have_elogind" = yes; then
    #
    # we appear to have libelogind; check for headers/libs to be sure.
    #
    ac_save_elogind_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_elogind_config_cflags"

    have_elogind=no
    AC_CHECK_X_HEADER(elogind/sd-bus.h, [have_elogind=yes])

    CPPFLAGS="$ac_save_elogind_CPPFLAGS"
  fi

  if test "$have_elogind" = yes; then
    # we have the headers, now check for the libraries
    have_elogind=no
    elogind_halfassed=yes
    AC_MSG_RESULT(checking for libelogind usability...)
    AC_CHECK_X_LIB(c, sd_bus_track_count_name,
                   [have_elogind=yes; elogind_halfassed=no],,
                   $ac_elogind_config_libs -lX11 -lXext -lm)
  fi

  if test "$have_elogind" = no; then
    AC_MSG_RESULT(checking for libelogind usability... no)
  fi
fi

if test "$have_elogind" = yes; then
  INCLUDES="$INCLUDES $ac_elogind_config_cflags"
  EXES_SYSTEMD='$(EXES_SYSTEMD)'
  SYSTEMD_LIBS="$ac_elogind_config_libs"
  AC_DEFINE(HAVE_LIBELOGIND)
fi


###############################################################################
#
#       The --enable-locking option
#
#	Note: "configure --help" list all "--enable" options before any
#	"--with" options, so we document the --enable options inline with
#	the --with options where it looks better, instead of in AC_ARG_ENABLE
#	where it would make sense, sigh...
#
###############################################################################

AC_ARG_ENABLE(locking,
[],  # This is documented elsewhere because of --enable/--with option sorting.
  [enable_locking="$enableval"],
  [if test "$ac_macosx" = yes; then
    # We can't lock on macOS, so default to not compiling in support for it.
    # But allow --enable-locking to override that, so I can debug Linux locking
    # under macOS X11.
    enable_locking=no
   else
    enable_locking=yes
   fi])
if test "$enable_locking" = yes; then
  true
elif test "$enable_locking" = no; then
  AC_DEFINE(NO_LOCKING)
else
  echo "error: must be yes or no: --enable-locking=$enable_locking"
  exit 1
fi


##############################################################################
#
#       Whether to allow root password to unblank.
#
###############################################################################

AC_ARG_ENABLE(root-passwd,
[],  # This is documented elsewhere because of --enable/--with option sorting.
  [enable_root_passwd="$enableval"],[enable_root_passwd=no])
if test "$enable_root_passwd" = yes; then
  AC_DEFINE(ALLOW_ROOT_PASSWD)
  true
elif test "$enable_root_passwd" != no; then
  echo "error: must be yes or no: --enable-root-passwd=$enable_root_passwd"
  exit 1
fi


###############################################################################
#
#       Check for PAM.
#
###############################################################################

case "$host" in
  *-solaris*)
   # Solaris systems tend to have the PAM runtime misconfigured and unusable.
   with_pam_default=no
   ;;
  *-freebsd* )
   # FreeBSD's PAM implementation requires the calling process to be setuid
   # the whole time, which is unacceptable.
   with_pam_default=no
   ;;
  *)
   # Default to building PAM support on all other systems, if it exists.
   with_pam_default=yes
  ;;
esac

#### H1

have_pam=no
with_pam_req=unspecified

AC_ARG_WITH(pam,[
Screen Locking Options:

  --disable-locking       Do not allow locking of the display at all.
  --with-pam              Use Pluggable Authentication Modules.],
  [with_pam="$withval"; with_pam_req="$withval"],[with_pam=$with_pam_default])

AC_ARG_WITH([pam_service_name],
[  --with-pam-service-name Set the name of the xscreensaver PAM service.
  --enable-pam-account    Whether PAM should check the result of account
	    		  modules when authenticating.  Only do this if you
	    		  have "account" modules configured on your system.
  --enable-root-passwd	  Allow the root password to unlock, if not using PAM.],
  [pam_service_name="$withval"],[pam_service_name="xscreensaver"])

AC_ARG_ENABLE(pam-check-account-type,
[],  # This is documented elsewhere because of --enable/--with option sorting.
  [enable_pam_check_account_type="$enableval"],[enable_pam_check_account_type=no])
if test "$enable_pam_check_account_type" = yes ; then
  AC_DEFINE(PAM_CHECK_ACCOUNT_TYPE)
  true
elif test "$enable_pam_check_account_type" != no ; then
  echo "error: must be yes or no: --enable-pam-check-account-type=$enable_pam_check_account_type"
  exit 1
fi

HANDLE_X_PATH_ARG(with_pam, --with-pam, PAM)

if test "$enable_locking" = yes -a "$with_pam" = yes; then
  AC_CACHE_CHECK([for PAM], ac_cv_pam,
                 [AC_TRY_X_COMPILE([#include <security/pam_appl.h>],,
                                   [ac_cv_pam=yes],
                                   [ac_cv_pam=no])])
  if test "$ac_cv_pam" = yes ; then
    have_pam=yes
    AC_DEFINE(HAVE_PAM)
    AC_DEFINE_UNQUOTED(PAM_SERVICE_NAME,"$pam_service_name")
    
    PASSWD_LIBS="${PASSWD_LIBS} -lpam"

    # libpam typically requires dlopen and dlsym.  On FreeBSD,
    # those are in libc.  On Linux and Solaris, they're in libdl.
    AC_CHECK_LIB(dl, dlopen, [PASSWD_LIBS="${PASSWD_LIBS} -ldl"])

    # On Linux, sigtimedwait() is in libc; on Solaris, it's in librt.
    have_timedwait=no
    AC_CHECK_LIB(c, sigtimedwait,
                 [have_timedwait=yes
                  AC_DEFINE(HAVE_SIGTIMEDWAIT)])
    if test "$have_timedwait" = no ; then
      AC_CHECK_LIB(rt, sigtimedwait, [have_timedwait=yes
                                      AC_DEFINE(HAVE_SIGTIMEDWAIT)
                                      PASSWD_LIBS="${PASSWD_LIBS} -lrt"])
    fi

    AC_MSG_CHECKING(how to call pam_strerror)
    AC_CACHE_VAL(ac_cv_pam_strerror_args,
     [AC_TRY_X_COMPILE([#include <stdio.h>
                        #include <stdlib.h>
                        #include <security/pam_appl.h>],
                       [pam_handle_t *pamh = 0;
                        char *s = pam_strerror(pamh, PAM_SUCCESS);],
                       [ac_pam_strerror_args=2],
                       [AC_TRY_X_COMPILE([#include <stdio.h>
                                          #include <stdlib.h>
                                          #include <security/pam_appl.h>],
                                         [char *s =
                                           pam_strerror(PAM_SUCCESS);],
                                         [ac_pam_strerror_args=1],
                                         [ac_pam_strerror_args=0])])
      ac_cv_pam_strerror_args=$ac_pam_strerror_args])
    ac_pam_strerror_args=$ac_cv_pam_strerror_args
    if test "$ac_pam_strerror_args" = 1 ; then
      AC_MSG_RESULT(one argument)
    elif test "$ac_pam_strerror_args" = 2 ; then
      AC_DEFINE(PAM_STRERROR_TWO_ARGS)
      AC_MSG_RESULT(two arguments)
    else
      AC_MSG_RESULT(unknown)
    fi

# Check pam_fail_delay
    AC_MSG_CHECKING(pam_fail_delay in -lpam)
    AC_CACHE_VAL(ac_cv_pam_fail_delay,
     [ac_save_LDFLAGS="$LDFLAGS"
      LDFLAGS="-lpam"
      AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <security/pam_appl.h>],
                         [pam_handle_t *pamh = 0;
		   unsigned int usec = 1;
		   int status = pam_fail_delay (pamh, usec);])],
        [ac_pam_fail_delay=yes],
        [ac_pam_fail_delay=no])
      ac_cv_pam_fail_delay=$ac_pam_fail_delay,
      LDFLAGS=$ac_save_LDFLAGS])

      if test "$ac_pam_fail_delay" = yes ; then
        AC_MSG_RESULT(yes)
        AC_DEFINE(HAVE_PAM_FAIL_DELAY)
      else
        AC_MSG_RESULT(no)
      fi

  fi
fi


###############################################################################
#
#       Check for Kerberos.
#
###############################################################################

have_kerberos=no
have_kerberos5=no
with_kerberos_req=unspecified

AC_ARG_WITH(kerberos, 
[  --with-kerberos         Include support for Kerberos authentication.],
  [with_kerberos="$withval"; with_kerberos_req="$withval"],[with_kerberos=yes])

HANDLE_X_PATH_ARG(with_kerberos, --with-kerberos, Kerberos)

if test "$enable_locking" = yes -a "$with_kerberos" = yes; then
  AC_CACHE_CHECK([for Kerberos 4], ac_cv_kerberos,
                 [AC_TRY_X_COMPILE([#include <krb.h>],,
                                   [ac_cv_kerberos=yes],
                                   [ac_cv_kerberos=no])])
  AC_CACHE_CHECK([for Kerberos 5], ac_cv_kerberos5,
                 [AC_TRY_X_COMPILE([#include <kerberosIV/krb.h>],,
                                   [ac_cv_kerberos5=yes],
                                   [ac_cv_kerberos5=no])])

  if test "$ac_cv_kerberos" = yes ; then
    have_kerberos=yes
    AC_DEFINE(HAVE_KERBEROS)
  fi

  if test "$ac_cv_kerberos5" = yes ; then

    # Andrew Snare <ajs@pigpond.com> wrote:
    #
    # You were assuming that if kerberosV (krb5) was found, then kerberosIV
    # (krb4) was also available.  This turns out not to be the case with
    # mit-krb-1.2.7; apparently backwards-compatibility with KerberosIV
    # is optional.
    #
    # So, disable kerberosV support if libkrb4 can't be found.
    # This is not the best solution, but it makes the compile not fail.
    #
    AC_CHECK_X_LIB(krb4, krb_get_tf_realm,
                   [have_kerberos=yes],
                   [have_kerberos=no])
    if test "$have_kerberos" = yes ; then
      have_kerberos5=yes
      AC_DEFINE(HAVE_KERBEROS)
      AC_DEFINE(HAVE_KERBEROS5)
    else
      have_kerberos5=no
      AC_MSG_WARN([Cannot find compat lib (libkrb4) needed to use Kerberos 5])
    fi

  fi

  if test "$have_kerberos5" = yes ; then
    # from Matt Knopp <mhat@infocalypse.netlag.com>
    # (who got it from amu@mit.edu)

    PASSWD_LIBS="$PASSWD_LIBS -lkrb4 -ldes425 -lkrb5 -lk5crypto -lcom_err"

    # jwz: macOS uses -lkrb5, but not -lcrypt
    AC_CHECK_X_LIB(crypt, crypt, [PASSWD_LIBS="$PASSWD_LIBS -lcrypt"])

  elif test "$have_kerberos" = yes ; then
    # from Tim Showalter <tjs@psaux.com> for FreeBSD 4.2
    PASSWD_LIBS="$PASSWD_LIBS -lkrb -ldes -lcom_err"
  fi

  if test "$have_kerberos" = yes ; then
    AC_CHECK_FUNC(res_search,,
      AC_CHECK_LIB(resolv,res_search,PASSWD_LIBS="${PASSWD_LIBS} -lresolv",
        AC_MSG_WARN([Can't find DNS resolver libraries needed for Kerberos])
      ))
  fi
fi


###############################################################################
#
#       Check for the nine billion variants of shadow passwords...
#
###############################################################################

have_shadow=no
with_shadow_req=unspecified

AC_ARG_WITH(shadow,
[  --with-shadow           Include support for shadow password authentication.],
  [with_shadow="$withval"; with_shadow_req="$withval"],[with_shadow=yes])

HANDLE_X_PATH_ARG(with_shadow, --with-shadow, shadow password)

if test "$enable_locking" = no ; then
  with_shadow_req=no
  with_shadow=no
fi


###############################################################################
#
#       Check for Sun "adjunct" passwords.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for Sun-style shadow passwords], ac_cv_sun_adjunct,
                 [AC_TRY_X_COMPILE([#include <stdlib.h>
                                    #include <unistd.h>
                                    #include <sys/types.h>
                                    #include <sys/label.h>
                                    #include <sys/audit.h>
                                    #include <pwdadj.h>],
                      [struct passwd_adjunct *p = getpwanam("nobody");
                       const char *pw = p->pwa_passwd;],
                      [ac_cv_sun_adjunct=yes],
                      [ac_cv_sun_adjunct=no])])
  if test "$ac_cv_sun_adjunct" = yes; then
    have_shadow_adjunct=yes
    have_shadow=yes
    setuid_auth=yes
  fi
fi


###############################################################################
#
#       Check for DEC and SCO so-called "enhanced" security.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for DEC-style shadow passwords], ac_cv_enhanced_passwd,
                 [AC_TRY_X_COMPILE([#include <stdlib.h>
                                    #include <unistd.h>
                                    #include <sys/types.h>
                                    #include <pwd.h>
                                    #include <sys/security.h>
                                    #include <prot.h>],
                      [struct pr_passwd *p;
                       const char *pw;
                       set_auth_parameters(0, 0);
                       check_auth_parameters();
                       p = getprpwnam("nobody");
                       pw = p->ufld.fd_encrypt;],
                      [ac_cv_enhanced_passwd=yes],
                      [ac_cv_enhanced_passwd=no])])
  if test $ac_cv_enhanced_passwd = yes; then
    have_shadow_enhanced=yes
    have_shadow=yes
    setuid_auth=yes

    # On SCO, getprpwnam() is in -lprot (which uses nap() from -lx)
    # (I'm told it needs -lcurses too, but I don't understand why.)
    # But on DEC, it's in -lsecurity.
    #
    AC_CHECK_LIB(prot, getprpwnam, 
                 [PASSWD_LIBS="$PASSWD_LIBS -lprot -lcurses -lx"],
                 [AC_CHECK_LIB(security, getprpwnam, 
                               [PASSWD_LIBS="$PASSWD_LIBS -lsecurity"])],
                 [-lx])
  fi
fi

###############################################################################
#
#       Check for HP's entry in the "Not Invented Here" Sweepstakes.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for HP-style shadow passwords], ac_cv_hpux_passwd,
                 [AC_TRY_X_COMPILE([#include <stdlib.h>
                                    #include <unistd.h>
                                    #include <sys/types.h>
                                    #include <pwd.h>
                                    #include <hpsecurity.h>
                                    #include <prot.h>],
                      [struct s_passwd *p = getspwnam("nobody");
                       const char *pw = p->pw_passwd;],
                      [ac_cv_hpux_passwd=yes],
                      [ac_cv_hpux_passwd=no])])
  if test "$ac_cv_hpux_passwd" = yes; then
    have_shadow_hpux=yes
    have_shadow=yes
    setuid_auth=yes

    # on HPUX, bigcrypt is in -lsec
    AC_CHECK_LIB(sec, bigcrypt, [PASSWD_LIBS="$PASSWD_LIBS -lsec"])
  fi
fi


###############################################################################
#
#       Check for FreeBSD-style shadow passwords.
#
#       On FreeBSD, getpwnam() and friends work just like on non-shadow-
#       password systems -- except you only get stuff in the pw_passwd field
#       if the running program is setuid.  So, guess that we've got this
#       lossage to contend with if /etc/master.passwd exists, and default to
#       a setuid installation.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for FreeBSD-style shadow passwords], ac_cv_master_passwd,
                 [if test -f /etc/master.passwd ; then
                    ac_cv_master_passwd=yes
                  else
                    ac_cv_master_passwd=no
                  fi])
  if test "$ac_cv_master_passwd" = yes; then
    setuid_auth=yes
  fi
fi


###############################################################################
#
#       Check for OpenBSD-style shadow passwords.
#       It's getpwnam_shadow instead of getpwnam, and it only works as root.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for OpenBSD-style shadow passwords], ac_cv_pwnam_shadow,
                 [AC_TRY_X_COMPILE([#include <stdlib.h>
                                    #include <unistd.h>
                                    #include <sys/types.h>
                                    #include <pwd.h>],
                      [struct spwd *p = getspnam_shadow("nobody");
                       const char *pw = p->pw_passwd;],
                      [ac_cv_pwnam_shadow=yes],
                      [ac_cv_pwnam_shadow=no])])
  if test "$ac_cv_pwnam_shadow" = yes; then
    have_pwnam_shadow=yes

    # If we also have PAM, assume that we don't need to be setuid.
    if test $have_pam != yes; then
      setuid_auth=yes
    fi
  fi
fi


###############################################################################
#
#       Check for traditional (ha!) shadow passwords.
#
###############################################################################

if test "$with_shadow" = yes ; then
  AC_CACHE_CHECK([for generic shadow passwords], ac_cv_shadow,
                 [AC_TRY_X_COMPILE([#include <stdlib.h>
                                    #include <unistd.h>
                                    #include <sys/types.h>
                                    #include <pwd.h>
                                    #include <shadow.h>],
                      [struct spwd *p = getspnam("nobody");
                       const char *pw = p->sp_pwdp;],
                      [ac_cv_shadow=yes],
                      [ac_cv_shadow=no])])
  if test "$ac_cv_shadow" = yes; then
    have_shadow=yes

    # If we also have PAM, assume that we don't need to be setuid.
    if test $have_pam != yes; then
      setuid_auth=yes
    fi

    # On some systems (UnixWare 2.1), getspnam() is in -lgen instead of -lc.
    have_getspnam=no
    AC_CHECK_LIB(c, getspnam, [have_getspnam=yes])
    if test "$have_getspnam" = no ; then
      AC_CHECK_LIB(gen, getspnam,
                   [have_getspnam=yes; PASSWD_LIBS="$PASSWD_LIBS -lgen"])
    fi
  fi
fi


###############################################################################
#
#       Check for other libraries needed for non-shadow passwords.
#
###############################################################################

if test "$enable_locking" = yes ; then

  # On some systems (UnixWare 2.1), crypt() is in -lcrypt instead of -lc.
  have_crypt=no
  AC_CHECK_LIB(c, crypt, [have_crypt=yes])
  if test "$have_crypt" = no ; then
    AC_CHECK_LIB(crypt, crypt,
                 [have_crypt=yes; PASSWD_LIBS="$PASSWD_LIBS -lcrypt"])
  fi
fi


# Most of the above shadow mechanisms will have set setuid_auth to yes,
# if they were found.  But, on some systems, we need setuid even when
# using plain old vanilla passwords.
#
if test "$enable_locking" = yes -a "$have_pam" = no ; then
  case "$host" in
    *-hpux* | *-aix* | *-netbsd* | *-freebsd* )
      setuid_auth=yes
    ;;
  esac
fi


if test "$have_shadow_adjunct" = yes ; then
  AC_DEFINE(HAVE_ADJUNCT_PASSWD)
elif test "$have_shadow_enhanced" = yes ; then
  AC_DEFINE(HAVE_ENHANCED_PASSWD)
elif test "$have_shadow_hpux" = yes ; then
  AC_DEFINE(HAVE_HPUX_PASSWD)
elif test "$have_shadow" = yes ; then
  AC_DEFINE(HAVE_SHADOW_PASSWD)
elif test "$have_pwnam_shadow" = yes ; then
  AC_DEFINE(HAVE_PWNAM_SHADOW_PASSWD)
fi


###############################################################################
#
#       Check for -lgtk (and Gnome stuff)
#
###############################################################################

#### H1

have_gtk=no
with_gtk_req=unspecified
AC_ARG_WITH(gtk,
[
User Interface Options:

  --with-gtk              Use the Gtk toolkit for the user interface.],
  [with_gtk="$withval"; with_gtk_req="$withval"],[with_gtk=yes])

# if --with-gtk=/directory/ was specified, remember that directory so that
# we can also look for the `gtk-config' program in that directory.
case "$with_gtk" in
  /*)
    gtk_dir="$with_gtk"
    ;;
  *)
    gtk_dir=""
    ;;
esac

HANDLE_X_PATH_ARG(with_gtk, --with-gtk, Gtk)

if test "$with_gtk" != yes -a "$with_gtk" != no ; then
  echo "error: must be yes or no: --with-gtk=$with_gtk"
  exit 1
fi


gtk_halfassed=no

if test "$with_gtk" = yes; then
  have_gtk=no
  
  pkgs=''
  ok="yes"
  pkg_check_version       gtk+-3.0  3.0.0 ; ac_gtk_version_string="$vers"
  pkg_check_version    gmodule-2.0  2.0.0
  pkg_check_version     libxml-2.0  2.4.6
  pkg_check_version gdk-pixbuf-2.0  2.0.0
  have_gtk="$ok"
  gtk_pkgs="$pkgs"

  if test "$have_gtk" = no; then
    if test -n "$ac_gtk_version_string" ; then
      gtk_halfassed=yes
      gtk_halfassed_lib="$req"
    fi
  fi

  if test "$have_gtk" = yes; then
    AC_CACHE_CHECK([for Gtk includes], ac_cv_gtk_config_cflags,
                   [ac_cv_gtk_config_cflags=`$pkg_config --cflags $pkgs`])
    AC_CACHE_CHECK([for Gtk libs], ac_cv_gtk_config_libs,
                   [ac_cv_gtk_config_libs=`$pkg_config --libs $pkgs`])
  fi

  ac_gtk_config_cflags=$ac_cv_gtk_config_cflags
  ac_gtk_config_libs=$ac_cv_gtk_config_libs

  GTK_DATADIR=""
  if test "$have_gtk" = yes; then
    GTK_DATADIR=`$pkg_config --variable=prefix gtk+-3.0`
    GTK_DATADIR="$GTK_DATADIR/share"
  fi

  if test "$have_gtk" = yes; then
    INCLUDES="$INCLUDES $ac_gtk_config_cflags"
    GTK_LIBS="$GTK_LIBS $ac_gtk_config_libs"
    AC_DEFINE(HAVE_GTK)
    AC_DEFINE(HAVE_XML)
  fi

  if test "$have_gtk" = yes; then
    AC_CACHE_CHECK([for Glib resource compiler], ac_cv_glib_res,
     [ac_cv_glib_res=`$pkg_config --variable=glib_compile_resources gio-2.0`])
    GLIB_COMPILE_RESOURCES="$ac_cv_glib_res"
  fi

fi


# Check for the various Gnome help and URL loading programs.
#
WITH_BROWSER=gnome-open
if test "$have_gtk" = yes; then
  AC_CHECK_PROGS(gnome_open_program,     gnome-open)
  AC_CHECK_PROGS(gnome_url_show_program, gnome-url-show)
fi


###############################################################################
#
#       Check for -lXm.
#
###############################################################################

have_motif=no
with_motif_req=unspecified
AC_ARG_WITH(motif,
[  --with-motif            Use the Motif toolkit for the user interface.],
  [with_motif="$withval"; with_motif_req="$withval"],[with_motif=no])

HANDLE_X_PATH_ARG(with_motif, --with-motif, Motif)

if test "$with_motif" != yes -a "$with_motif" != no ; then
  echo "error: must be yes or no: --with-motif=$with_motif"
  exit 1
fi

if test "$with_motif" = yes; then
  have_motif=no
  AC_CHECK_X_HEADER(Xm/Xm.h,
                    [have_motif=yes
                     AC_DEFINE(HAVE_MOTIF)
                     MOTIF_LIBS="$MOTIF_LIBS -lXm"],,
                    [#include <stdlib.h>
                     #include <stdio.h>
                     #include <X11/Intrinsic.h>])
fi


if test "$have_motif" = yes; then
  AC_CHECK_X_HEADER(Xm/ComboBox.h, [AC_DEFINE(HAVE_XMCOMBOBOX)],,
                    [#include <stdlib.h>
                     #include <stdio.h>
                     #include <X11/Intrinsic.h>])
fi


###############################################################################
#
#       Checking whether Motif is really Lesstif.
#
###############################################################################

have_lesstif=no
if test "$have_motif" = yes ; then
  AC_CACHE_CHECK([whether Motif is really LessTif], 
                 ac_cv_have_lesstif,
                 [AC_TRY_X_COMPILE([#include <Xm/Xm.h>],
                                   [long vers = LesstifVersion;],
                                   [ac_cv_have_lesstif=yes],
                                   [ac_cv_have_lesstif=no])])
  have_lesstif=$ac_cv_have_lesstif
fi


lesstif_version=unknown
lesstif_version_string=unknown

if test "$have_lesstif" = yes ; then
  ltv=unknown
  echo unknown > conftest-lt
  AC_CACHE_CHECK([LessTif version number],
                 ac_cv_lesstif_version_string,
      [AC_TRY_X_RUN([#include <stdio.h>
                     #include <Xm/Xm.h>
                     int main() {
                       FILE *f = fopen("conftest-lt", "w");
                       if (!f) exit(1);
                       fprintf(f, "%d %d.%d\n", LesstifVersion,
                          LESSTIF_VERSION, LESSTIF_REVISION);
                       fclose(f);
                       exit(0);
                     }],
                    [ltv=`cat conftest-lt`
                     ac_cv_lesstif_version=`echo $ltv | sed 's/ .*//'`
                     ac_cv_lesstif_version_string=`echo $ltv | sed 's/.* //'`],
                    [ac_cv_lesstif_version=unknown
                     ac_cv_lesstif_version_string=unknown],
                    [ac_cv_lesstif_version=unknown
                     ac_cv_lesstif_version_string=unknown])])
  rm -f conftest-lt
  lesstif_version=$ac_cv_lesstif_version
  lesstif_version_string=$ac_cv_lesstif_version_string

fi


if test "$have_motif" = yes ; then
  mtv=unknown
  echo unknown > conftest-mt
  AC_CACHE_CHECK([Motif version number],
                 ac_cv_motif_version_string,
      [AC_TRY_X_RUN([#include <stdio.h>
                     #include <Xm/Xm.h>
                     int main() {
                       FILE *f = fopen("conftest-mt", "w");
                       if (!f) exit(1);
                       fprintf(f, "%d %d.%d\n", XmVersion,
                          XmVERSION, XmREVISION);
                       fclose(f);
                       exit(0);
                     }],
                    [mtv=`cat conftest-mt`
                     ac_cv_motif_version=`echo $mtv | sed 's/ .*//'`
                     ac_cv_motif_version_string=`echo $mtv | sed 's/.* //'`],
                    [ac_cv_motif_version=unknown
                     ac_cv_motif_version_string=unknown],
                    [ac_cv_motif_version=unknown
                     ac_cv_motif_version_string=unknown])])
  rm -f conftest-mt
  motif_version=$ac_cv_motif_version
  motif_version_string=$ac_cv_motif_version_string

fi


###############################################################################
#
#       Checking whether Motif requires -lXp.
#
#       Some versions of Motif (2.1.0, at least) require -lXp, the "X Printing
#       Extension".   Why this extension isn't in -lXext with all the others,
#       I have no idea.
#
###############################################################################

have_xp_ext=no
if test "$have_motif" = yes ; then
   have_xp_ext=no
   AC_CHECK_X_LIB(Xp, XpQueryExtension,
                  [have_xp_ext=yes; MOTIF_LIBS="$MOTIF_LIBS -lXp"],
                  [true], -lX11 -lXext -lm)
fi


###############################################################################
#
#       Checking whether Motif requires -lXintl (for _Xsetlocale.)
#
###############################################################################

have_xintl=no
if test "$have_motif" = yes ; then
  AC_CHECK_X_LIB(Xintl, _Xsetlocale, [have_xintl=yes], [have_xintl=no],
                 -lX11 -lXext -lm)
  if test "$have_xintl" = yes; then
    MOTIF_LIBS="$MOTIF_LIBS -lXintl"
  fi
fi


###############################################################################
#
#       Check for a login manager for a "New Login" button on the lock dialog.
#	Usually this will be "/usr/bin/gdmflexiserver".
#
###############################################################################

with_login_manager_req=unspecified
default_login_manager_1='gdmflexiserver -ls'
default_login_manager_2='kdmctl reserve'
default_login_manager_3='lxdm -c USER_SWITCH'
default_login_manager_4='dm-tool switch-to-greeter'

AC_ARG_WITH(login-manager,
[  --with-login-manager    Put a "New Login" button on the unlock dialog that
                          runs a login manager like gdmflexiserver or kdmctl.],
  [with_login_manager="$withval"; with_login_manager_req="$withval"],
  [with_login_manager=yes])
# no HANDLE_X_PATH_ARG for this one

if test "$enable_locking" = no ; then
  with_login_manager_req=no
  with_login_manager=no
fi

case "$with_login_manager_req" in
  no)
    with_login_manager=""
  ;;

  yes|unspecified)
    # Try various defaults, use the first one that exists.

    with_login_manager=""

    if test -z "$with_login_manager" ; then
      set dummy $default_login_manager_1 ; login_manager_tmp=$2
      unset ac_cv_path_login_manager_tmp  # don't cache
      AC_PATH_PROG(login_manager_tmp, $login_manager_tmp, [])
      if test ! -z "$login_manager_tmp" ; then
        with_login_manager="$default_login_manager_1"
      fi
    fi

    if test -z "$with_login_manager" ; then
      set dummy $default_login_manager_2 ; login_manager_tmp=$2
      unset ac_cv_path_login_manager_tmp  # don't cache
      AC_PATH_PROG(login_manager_tmp, $login_manager_tmp, [])
      if test ! -z "$login_manager_tmp" ; then
        with_login_manager="$default_login_manager_2"
      fi
    fi

    if test -z "$with_login_manager" ; then
      set dummy $default_login_manager_3 ; login_manager_tmp=$2
      unset ac_cv_path_login_manager_tmp  # don't cache
      AC_PATH_PROG(login_manager_tmp, $login_manager_tmp, [])
      if test ! -z "$login_manager_tmp" ; then
        with_login_manager="$default_login_manager_3"
      fi
    fi

    if test -z "$with_login_manager" ; then
      set dummy $default_login_manager_4 ; login_manager_tmp=$2
      unset ac_cv_path_login_manager_tmp  # don't cache
      AC_PATH_PROG(login_manager_tmp, $login_manager_tmp, [])
      if test ! -z "$login_manager_tmp" ; then
        with_login_manager="$default_login_manager_4"
      fi
    fi

  ;;

  *)
    # Just believe whatever command was specified.
    with_login_manager="$with_login_manager_req"
  ;;
esac
ac_cv_login_manager_program="$with_login_manager"

NEW_LOGIN_COMMAND_P=''
NEW_LOGIN_COMMAND="$ac_cv_login_manager_program"

AC_MSG_CHECKING(for login manager)
if test -z "$NEW_LOGIN_COMMAND" ; then
  NEW_LOGIN_COMMAND="$default_login_manager_1"
  NEW_LOGIN_COMMAND_P='! '
  AC_MSG_RESULT($NEW_LOGIN_COMMAND (disabled))
else
  AC_MSG_RESULT($NEW_LOGIN_COMMAND)
fi



###############################################################################
#
#       Check for a directory full of images to use as the default value
#	of the "imageDirectory" preference.
#
###############################################################################

have_imagedir=no
with_imagedir_req=unspecified

AC_ARG_WITH(image-directory,
[  --with-image-directory  Some demos will display random images from here.],
  [with_imagedir="$withval"; with_imagedir_req="$withval"],
  [with_imagedir=yes])
# no HANDLE_X_PATH_ARG for this one

case "$with_imagedir" in
  /*)
    # absolute path
    AC_MSG_CHECKING([for image directory $with_imagedir])
    if test -d "$with_imagedir" ; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      with_imagedir=""
    fi
  ;;
  yes)
    with_imagedir=""

    fallback_imgdir=""
    for dd in \
      "/usr/share/rpd-wallpaper/"		\
      "/usr/share/backgrounds/images/"		\
      "/usr/share/wallpapers/"			\
      "/usr/share/backgrounds/"			\
      "/usr/share/pixmaps/backgrounds/"		\
      "/usr/share/lxde/wallpapers/"		\
      "/Library/Desktop Pictures/"		\
    ; do
      if test -z "$with_imagedir"; then
        AC_MSG_CHECKING([for image directory $dd])
        if test -d "$dd"; then
          if ( ls "$dd" | grep -q ... ) >&- 2>&- ; then
            AC_MSG_RESULT(yes)
            with_imagedir="$dd"
          else
            AC_MSG_RESULT(empty)
            if test -z "$fallback_imgdir"; then
              fallback_imgdir="$dd"
            fi
          fi
        else
          AC_MSG_RESULT(no)
        fi
      fi
    done
    if test -z "$with_imagedir"; then
      with_imagedir="$fallback_imgdir"
    fi
;;
  no)
    with_imagedir=""
  ;;

  *)
    echo "error: must be an absolute path: --with-image-directory=$with_imagedir_req"
    exit 1
  ;;
esac
ac_cv_imagedir="$with_imagedir"

DEFAULT_IMAGES_P='True'
DEFAULT_IMAGE_DIRECTORY="$ac_cv_imagedir"

if test -z "$DEFAULT_IMAGE_DIRECTORY" ; then
  DEFAULT_IMAGES_P='False'
fi


###############################################################################
#
#       Pick a text file to use as the default of the "textFile" preference.
#	Any old file will do, but preferably one that will make interesting
#	shapes when displayed by "starwars" and "fontglide".
#
###############################################################################

have_textfile=no
with_textfile_req=unspecified

AC_ARG_WITH(text-file,
[  --with-text-file=FILE   Some demos will use this as a source of text.],
  [with_textfile="$withval"; with_textfile_req="$withval"],
  [with_textfile=yes])
# no HANDLE_X_PATH_ARG for this one

case "$with_textfile" in
  /*)
    # absolute path
    AC_MSG_CHECKING([for text file $with_textfile])
    if test -f "$with_textfile" ; then
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
      with_textfile=""
    fi
  ;;
  yes)
    with_textfile=""

    #### Could use some more defaults here...
    for f in \
      "/usr/X11R6/lib/X11/doc/README"		   \
      "/usr/share/doc/xserver-common/copyright"	   \
      "/usr/share/doc/xserver-xorg-core/copyright" \
      "/usr/X11R6/README"			   \
      "/usr/share/doc/libX11*/COPYING"		   \
      "/usr/X11/share/X11/doc/README*"		   \
      "/usr/share/doc/debian/debian-manifesto"     \
    ; do
      if test -z "$with_textfile"; then
        AC_MSG_CHECKING([for text file $f])
	f=`/bin/ls $f 2>&- | head -1`
        if test -f "$f" ; then
          AC_MSG_RESULT(yes)
          with_textfile="$f"
        else
          AC_MSG_RESULT(no)
        fi
      fi
    done

  ;;
  no)
    with_textfile=""
  ;;

  *)
    echo "error: must be an absolute path: --with-text-file=$with_textfile_req"
    exit 1
  ;;
esac
ac_cv_textfile="$with_textfile"

DEFAULT_TEXT_FILE="$ac_cv_textfile"


###############################################################################
#
#       Check the browser to see help URL
#
###############################################################################

have_browser=no
with_browser_req=unspecified

AC_ARG_WITH(browser,
[  --with-browser=BROWSER  The web browser used to open the "Help" URL.],
  [with_browser="$withval"; with_browser_req="$withval"],
  [with_browser=no ])
# no HANDLE_X_PATH_ARG for this one

case "$with_browser" in
  no )
  ;;
  * )
    WITH_BROWSER=$with_browser
    gnome_open_program=$with_browser
    AC_MSG_CHECKING([for browser $with_browser])
    with_browser_fullpath=`which $with_browser 2>/dev/null`
    case $with_browser_fullpath in
	 /* )
	        AC_MSG_RESULT(yes)
		 have_browser=yes
		 ;;
	* )
	       AC_MSG_RESULT(no)
# Only warning: we don't want to install all packages for the 
# dependency of the browser in building stage...
	       echo "WARNING: browser not found: --with-browser=$with_browser"
	       ;;
    esac
  ;;
esac
ac_cv_browser="$with_browser"


###############################################################################
#
#       Check for pthreads. Must come before checking for OpenGL,
#       since some versions of OpenGL require it.
#
###############################################################################

have_pthread=no
with_pthread_req=unspecified

# AX_PTHREAD is from the GNU Autoconf Archive.
# https://savannah.gnu.org/projects/autoconf-archive/
m4_include(ax_pthread.m4)

AC_ARG_WITH([pthread],
[],  # This is documented after --with-gl in --with-xft.
  [with_pthread="$withval"; with_pthread_req="$withval"],
  [with_pthread=yes])

if test "$with_pthread" = yes; then
  # AX_PTHREAD might want a different compiler.
  AX_PTHREAD(
   [if test "$CC" = "$PTHREAD_CC" -o -z "$ac_original_cc"; then
      have_pthread=yes
    else
      ac_prog_cc_no_pthread=yes
    fi
  ])

  if test "$have_pthread" = yes; then
    AC_DEFINE([HAVE_PTHREAD])
    CC="$PTHREAD_CC"
  fi
fi


###############################################################################
#
#       Handle --with-gles
#	Must come before --with-gl
#
###############################################################################

with_jwzgles_req=unspecified
have_jwzgles=no
AC_ARG_WITH(gles,
[],  # This is documented after --with-gl
  [with_jwzgles="$withval"; with_jwzgles_req="$withval"],[with_jwzgles=no])

HANDLE_X_PATH_ARG(with_jwzgles, --with-gles, JWZGLES)

if test "$with_jwzgles" = yes; then
  have_jwzgles=yes
  JWZGLES_OBJS='$(JWXYZ_BIN)/jwzgles.o'
  AC_MSG_RESULT(emulating OpenGL 1.3 in terms of OpenGLES 1.x.)
elif test "$with_jwzgles" != no; then
  echo "error: must be yes or no: --with-gles=$with_jwzgles"
  exit 1
fi


###############################################################################
#
#       Check for -lGL or -lMesaGL.
#
###############################################################################

#### H1

have_gl=no
ac_have_mesa_gl=no
with_gl_req=unspecified
gl_halfassed=no
have_glext=no
AC_ARG_WITH(gl,
[
Graphics Options:

  --with-gles             Emulate OpenGL 1.3 in terms of OpenGL ES 1.x.],
  [with_gl="$withval"; with_gl_req="$withval"],[with_gl=yes])

HANDLE_X_PATH_ARG(with_gl, --with-gl, GL)

# Some GL libraries require pthreads, so use the pthreads flags when
# testing them, then set them back after.
#
ac_save_pthreads_CFLAGS="$CFLAGS"
ac_save_pthreads_LIBS="$LIBS"
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"

if test "$with_gl" = yes; then
  AC_CHECK_X_HEADER(GL/gl.h, have_gl=yes, have_gl=no);
  if test "$have_gl" = yes; then
    AC_CHECK_X_HEADER(GL/glu.h, have_gl=yes, have_gl=no);
  fi
fi

if test "$with_gl" = yes; then
  #
  # Since the OpenGL and GLES specifications are a complete dumpster fire,
  # it's not easy to figure out how what libraries to use.
  #
  # OpenGLES library names:
  #
  #   1.x: libGLESv1_CM
  #   2.x: libGLESv2
  #   3.x: libGLESv2 on Windows, libGLESv3 on Android
  #
  # On modern Linux systems, these libraries exist:
  #
  #   libGL        - a superset of everything up to and including OpenGL 4.6
  #   libGLESv1_CM - OpenGLES 1.1, meaning it omits much of OpenGL 1.3
  #   libGLESv2    - OpenGLES 2 and 3, meaning it omits even more of OpenGL 1.3
  #
  # Since Linux systems support OpenGL 1.3, those link against only libGL.
  #
  # Unless we are using --with-gles on Linux, in which case we are attempting
  # to simulate a system that does not natively support OpenGL 1.3.  In that
  # case, we use the v1 and/or v2 libraries instead.
  #
  # Common practice on Android seems to be to link against the v1, v2 and v3
  # libraries in order, and then do runtime checks to see which set of
  # functions is actually supported by the runtime.  Just because you linked
  # against a GLES3 library doesn't mean that the hardware supports it: most
  # of these functions are just syscall-like stubs.
  #
  # See the commentary atop jwxyz/jwzgles.c for discussion of OpenGL versions.
  #
  if test "$have_gl" = yes; then
    have_gl=no

    if test "$have_jwzgles" = yes; then

      # Only check for the v1/2/3 libraries on Linux when using --with-gles
      # to emulate a system that does not natively support OpenGL 1.3.

      # Check for GLES 3.x
      AC_CHECK_X_LIB(GLESv3, glDrawElements,
                     [have_gl=yes
                      gl_lib_1="GLESv3"
                      GL_LIBS="-lGLESv3 $GL_LIBS"],
                     [], $GL_LIBS -lm)

      # Check for GLES 2.x
      AC_CHECK_X_LIB(GLESv2, glDrawElements,
                     [have_gl=yes
                      gl_lib_1="GLESv2"
                      GL_LIBS="-lGLESv2 $GL_LIBS"],
                     [], $GL_LIBS -lm)

      # Check for GLES 1.x
      AC_CHECK_X_LIB(GLESv1_CM, glDrawElements,
                     [have_gl=yes
                      gl_lib_1="GLESv1_CM"
                      GL_LIBS="-lGLESv1_CM $GL_LIBS"],
                     [], $GL_LIBS -lm)

    else   # $have_jwzgles = no

      # Check for OpenGL 1.x and/or OpenGL 4.x
      AC_CHECK_X_LIB(GL, glEnable,
                     [have_gl=yes
                      gl_lib_1="GL"
                      GL_LIBS="-lGL $GL_LIBS"],
                     [], $GL_LIBS -lm)

      # Check for MesaGL 1.x, but only if libGL doesn't exist.
      if test "$have_gl" = no; then
        AC_CHECK_X_LIB(MesaGL, glEnable,
                       [have_gl=yes
                       gl_lib_1="MesaGL"
                       GL_LIBS="-lMesaGL $GL_LIBS"],
                       [], $GL_LIBS -lm)
      fi
    fi
  fi

  # Figure out whether the GLU functions are in the main GL libraries or
  # in -lGLU.  Since Android does not have GLU, we don't link against libGLU
  # when using --with-gles on Linux.
  #
  if test "$have_gl" = yes -a "$have_jwzgles" = no; then
    AC_CHECK_X_LIB($gl_lib_1, gluBuild2DMipmaps,
                   [],
                   [have_gl=no], $GL_LIBS -lm)
    if test "$have_gl" = no; then
      AC_CHECK_X_LIB(GLU, gluBuild2DMipmaps,
                     [have_gl=yes
                      GL_LIBS="-lGLU $GL_LIBS"],
                     [have_gl=no
                      gl_halfassed=yes],
                      $GL_LIBS -lm)
    fi
  fi

  if test "$have_gl" = yes; then

    AC_DEFINE(HAVE_GL)

    # OpenGL 1.0 didn't have multiple textures.
    AC_CHECK_X_LIB($gl_lib_1, glBindTexture, [AC_DEFINE(HAVE_GLBINDTEXTURE)],
                   [true], $GL_LIBS -lX11 -lXext -lm)

    # Check for native GLES1
    AC_CHECK_X_LIB($gl_lib_1, glBindBuffer,
                   [AC_DEFINE(HAVE_GLES)],
                   [true], $GL_LIBS -lX11 -lXext -lm)

    # Check for GLSL and GLES2
    AC_CHECK_X_HEADER(GL/glext.h, have_glext=yes, have_glext=no,
                      [#include <GL/gl.h>])
    if test "$have_glext" = yes ; then
      AC_CHECK_X_LIB($gl_lib_1, glUseProgram,
                     [AC_DEFINE(HAVE_GLSL)
                      AC_DEFINE(HAVE_GLES2)],
                     [],
                     $GL_LIBS -lX11 -lXext -lm)
    fi

    # Check for GLES3
    AC_CHECK_X_LIB($gl_lib_1, glBlitFramebuffer,
                   [AC_DEFINE(HAVE_GLES3)],
                   [],
                   $GL_LIBS -lX11 -lXext -lm)

    if test "$have_jwzgles" = yes; then
      AC_DEFINE(HAVE_JWZGLES)
      AC_DEFINE(HAVE_GLES)
    fi

  fi

elif test "$with_gl" = no; then
  echo "error: --without-opengl is not supported."
  # You may be saying "but but but microcontrollers" -- bullshit.
  # It is the Twenty-First Century, and in This Modern World, every
  # extant microcontroller is more performant than the desktop
  # computers on which I developed most of the OpenGL hacks.
  exit 1

elif test "$with_gl" != no; then
  echo "error: must be yes or no: --with-gl=$with_gl"
  exit 1
fi


###############################################################################
#
#       Handle --with-glx
#
###############################################################################

have_glx=no
have_egl=no
with_egl_req=unspecified
AC_ARG_WITH(glx,
[  --with-glx              Use GLX to interface OpenGL and X11 instead of EGL.],
  [with_egl="$withval"; with_egl_req="$withval"],
  [with_glx=no])

HANDLE_X_PATH_ARG(with_glx, --with-glx, GLX)

if test "$with_glx" = yes; then
  AC_CHECK_X_HEADER(GLX/glx.h, have_glx=yes, have_glx=no)
elif test "$with_glx" != no; then
  echo "error: must be yes or no: --with-glx=$with_glx"
  exit 1
fi

if test "$have_gl" = yes; then

  # See if GLX functions are in -LGL.
  AC_CHECK_X_LIB($gl_lib_1, glXCreateContext,
                 [have_glx=yes],
                 [have_glx=no],
                 $GL_LIBS -lm)

  # See if EGL functions are in -LGL.
  egl_lib=""
  AC_CHECK_X_LIB($gl_lib_1, eglCreatePlatformWindowSurface,
                 [have_egl=yes],
                 [have_egl=no],
                 $GL_LIBS -lm)
  if test "$have_egl" = no; then
    # See if EGL functions are in -lEGL.
    AC_CHECK_X_LIB(EGL, eglCreatePlatformWindowSurface,
                   [have_egl=yes
                    egl_lib="EGL"],
                   [have_egl=no],
                   $GL_LIBS -lm)
  fi

  # If both GLX and EGL exist, use GLX only if requested.
  if test "$with_glx" = yes -a "$have_glx" = yes; then
    have_egl=no
  fi

  if test "$have_egl" = yes; then
    AC_DEFINE(HAVE_EGL)
    if test \! -z "$egl_lib"; then
      GL_LIBS="-l$egl_lib $GL_LIBS"
    fi
  fi

  # One of them is required
  if test "$have_egl" = no -a "$have_glx" = no; then
    have_gl=no
    gl_halfassed=yes
  fi

  # --with-gles means that we are emulating a system that supports GLES but
  # does not support OpenGL 1.3 (e.g., using Linux to simulate an Android-like
  # environment).  On such a system, GLE is required and GLX wouldn't exist.
  # There is no way to request a GLES 3.0 runtime context using GLX.
  #
  if test "$have_jwzgles" = yes -a "$have_egl" = no; then
    AC_MSG_WARN([Using --with-glx and --with-gles together is a bad idea.])
  fi

fi


# Restore flags after testing OpenGL libraries.
#
CFLAGS="$ac_save_pthreads_CFLAGS"
LIBS="$ac_save_pthreads_LIBS"


###############################################################################
#
#       Check for -lgle.
#
###############################################################################

have_gle=no
with_gle_req=unspecified
gle_halfassed=no
AC_ARG_WITH(gle,
[  --with-gle              Include support for the GL Extrusion library.],
  [with_gle="$withval"; with_gle_req="$withval"],[with_gle=yes])

HANDLE_X_PATH_ARG(with_gle, --with-gle, GLE)

GLE_LIBS=""

if test "$have_gl" = no ; then
 true
elif test "$with_gle" = yes; then

  AC_CHECK_X_HEADER(GL/gle.h, have_gle3=yes, have_gle3=no,
                    [#include <GL/gl.h>])
  if test "$have_gle3" = yes ; then
    have_gle=yes;
  else
    AC_CHECK_X_HEADER(GL/gutil.h, have_gle=yes, have_gle=no,
                    [#include <GL/gl.h>])
    if test "$have_gle" = yes ; then
      AC_CHECK_X_HEADER(GL/tube.h, have_gle=yes, have_gle=no,
                        [#include <GL/gl.h>])
    fi
  fi

  if test "$have_gle" = yes ; then
    have_gle=no
    gle_halfassed=yes
    AC_CHECK_X_LIB(gle, gleCreateGC, 
                   [have_gle=yes; gle_halfassed=no; GLE_LIBS="-lgle"],
                   [], $GL_LIBS -lX11 -lXext -lm)
  fi
  if test "$have_gle" = yes ; then
    have_gle=no
    gle_halfassed=yes

    # sometimes the libmatrix stuff is included in libgle.  look there first.
#
# I don't get it.  For some reason, this test passes on SGI, as if
# uview_direction_d() was in libgle -- but it's not, it's in libmatrix.
# Yet the link is succeeding.  Why???
#
#    AC_CHECK_X_LIB(gle, uview_direction_d, 
#                   [have_gle=yes; gle_halfassed=no],
#                   [], $GL_LIBS -lX11 -lXext -lm)

    # As of GLE 3 this is in libgle, and has changed name to uview_direction!
    # *sigh*
    if test "$have_gle3" = yes ; then
      AC_CHECK_X_LIB(gle, uview_direction, 
                     [have_gle=yes; gle_halfassed=no],
                    [], $GL_LIBS -lX11 -lXext -lm)
    fi
    # if it wasn't in libgle, then look in libmatrix.
    if test "$have_gle" = no ; then
      AC_CHECK_X_LIB(matrix, uview_direction_d, 
                     [have_gle=yes; gle_halfassed=no;
                      GLE_LIBS="$GLE_LIBS -lmatrix"],
                    [], $GL_LIBS -lX11 -lXext -lm)
    fi
  fi

  if test "$have_gle" = yes ; then
    AC_DEFINE(HAVE_GLE)
    if test "$have_gle3" = yes ; then
      AC_DEFINE(HAVE_GLE3)
    fi
  fi

elif test "$with_gle" != no; then
  echo "error: must be yes or no: --with-gle=$with_gle"
  exit 1

fi


###############################################################################
#
#       Check for -ljpeg
#
###############################################################################

have_jpeg=no
with_jpeg_req=unspecified
jpeg_halfassed=no
AC_ARG_WITH(jpeg,
[  --with-jpeg             Include support for the JPEG library.],
  [with_jpeg="$withval"; with_jpeg_req="$withval"],
  [with_jpeg=yes])

HANDLE_X_PATH_ARG(with_jpeg, --with-jpeg, JPEG)

if test "$with_jpeg" != yes -a "$with_jpeg" != no ; then
  echo "error: must be yes or no: --with-jpeg=$with_jpeg"
  exit 1
fi

if test "$with_jpeg" = yes; then

  have_jpeg=no
  AC_CHECK_X_HEADER(jpeglib.h, [have_jpeg=yes])

  if test "$have_jpeg" = yes; then
    # we have the header, now check for the library
    have_jpeg=no
    jpeg_halfassed=yes
    AC_CHECK_X_LIB(jpeg, jpeg_start_compress,
                   [have_jpeg=yes
                    jpeg_halfassed=no
                    JPEG_LIBS="-ljpeg"
                    AC_DEFINE(HAVE_JPEGLIB)])
  fi
fi


###############################################################################
#
#       Check for -lpng
#
###############################################################################

have_png=no
with_png_req=unspecified
png_halfassed=no
AC_ARG_WITH(png,
[  --with-png              Include support for the PNG library.],
  [with_png="$withval"; with_png_req="$withval"],
  [with_png=yes])

HANDLE_X_PATH_ARG(with_png, --with-png, PNG)

if test "$with_png" != yes -a "$with_png" != no ; then
  echo "error: must be yes or no: --with-png=$with_png"
  exit 1
fi

if test "$with_png" = yes; then

  have_png=no
  AC_CHECK_X_HEADER(png.h, [have_png=yes])

  if test "$have_png" = yes; then
    # we have the header, now check for the library
    have_png=no
    png_halfassed=yes
    AC_CHECK_X_LIB(png, png_create_read_struct,
                   [have_png=yes
                    png_halfassed=no
                    PNG_LIBS="-lpng"
                    AC_DEFINE(HAVE_LIBPNG)])
  fi
fi


###############################################################################
#
#       Check for -lgdk_pixbuf.
#       These tests are for gdk_pixbuf usage of the hacks, 
#       not xscreensaver-settings (thus we have to test again to get
#       the libraries right: don't want to pull in all of GTK
#       for the hacks.)
#
###############################################################################

have_gdk_pixbuf=no
have_gdk_pixbuf_xlib=no
with_gdk_pixbuf_req=unspecified
AC_ARG_WITH(pixbuf,
[  --with-pixbuf           Include support for the GDK-Pixbuf library, which
                          allows the display of JPEG, PNG, GIF and SVG images.],
  [with_gdk_pixbuf="$withval"; with_gdk_pixbuf_req="$withval"],
  [with_gdk_pixbuf=yes])

# if --with-pixbuf=/directory/ was specified, remember that directory so that
# we can also look for the `gdk-pixbuf-config' program in that directory.
case "$with_gdk_pixbuf" in
  /*)
    gdk_pixbuf_dir="$with_gdk_pixbuf"
    ;;
  *)
    gdk_pixbuf_dir=""
    ;;
esac

HANDLE_X_PATH_ARG(with_gdk_pixbuf, --with-pixbuf, GDK_PIXBUF)

if test "$with_gdk_pixbuf" != yes -a "$with_gdk_pixbuf" != no ; then
  echo "error: must be yes or no: --with-pixbuf=$with_gdk_pixbuf"
  exit 1
fi

if test "$with_gdk_pixbuf" = yes; then
  have_gdk_pixbuf=no

  pkgs=''
  ok="yes"
  pkg_check_version gdk-pixbuf-2.0  2.0.0
  pkg_check_version gio-2.0         2.0.0
  have_gdk_pixbuf="$ok"
  pixbuf_pkgs="$pkgs"

  if test "$have_gdk_pixbuf" = yes; then
    AC_CACHE_CHECK([for gdk-pixbuf includes], ac_cv_gdk_pixbuf_config_cflags,
               [ac_cv_gdk_pixbuf_config_cflags=`$pkg_config --cflags $pkgs`])
    AC_CACHE_CHECK([for gdk-pixbuf libs], ac_cv_gdk_pixbuf_config_libs,
               [ac_cv_gdk_pixbuf_config_libs=`$pkg_config --libs $pkgs`])
  fi

  ac_gdk_pixbuf_config_cflags=$ac_cv_gdk_pixbuf_config_cflags
  ac_gdk_pixbuf_config_libs=$ac_cv_gdk_pixbuf_config_libs


  if test "$have_gdk_pixbuf" = yes; then
    #
    # we appear to have pixbuf; check for headers/libs to be sure.
    #
    ac_save_gdk_pixbuf_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_gdk_pixbuf_config_cflags"
    have_gdk_pixbuf=no
    AC_CHECK_X_HEADER(gdk-pixbuf/gdk-pixbuf.h, [have_gdk_pixbuf=yes])
    CPPFLAGS="$ac_save_gdk_pixbuf_CPPFLAGS"
  fi

  if test "$have_gdk_pixbuf" = yes; then
    # we have the headers, now check for the libraries
    have_gdk_pixbuf=no
    AC_MSG_RESULT(checking for gdk_pixbuf usability...)
    AC_CHECK_X_LIB(c, gdk_pixbuf_new_from_file, [have_gdk_pixbuf=yes],,
                   $ac_gdk_pixbuf_config_libs -lX11 -lXext -lm)
  fi

  if test "$have_gdk_pixbuf" = yes; then
    INCLUDES="$INCLUDES $ac_gdk_pixbuf_config_cflags"
    PNG_LIBS="$ac_gdk_pixbuf_config_libs"
    AC_DEFINE(HAVE_GDK_PIXBUF)
  else
    AC_MSG_RESULT(checking for gdk_pixbuf usability... no)
  fi

  if test "$have_gdk_pixbuf" = yes; then
    AC_CHECK_X_LIB(c, gdk_pixbuf_apply_embedded_orientation,
                     [AC_DEFINE(HAVE_GDK_PIXBUF_APPLY_EMBEDDED_ORIENTATION)],,
                     $ac_gdk_pixbuf_config_libs -lX11 -lXext -lm)
  fi
fi


# Now that we have checked for gdk_pixbuf, check for gdk_pixbuf_xlib
# separately, since it has fallen out of fashion in recent years.
#
#have_gdk_pixbuf_xlib=no
#gdk_pixbuf_xlib_halfassed=no
#if test "$with_gdk_pixbuf" = yes -a "$have_gdk_pixbuf" = yes; then
#
#  have_gdk_pixbuf_xlib=no
#  pkgs="$pixbuf_pkgs"
#  ok="yes"
#  pkg_check_version gdk-pixbuf-xlib-2.0 2.0.0
#  pkg_check_version gdk-pixbuf-2.0      2.0.0
#  have_gdk_pixbuf_xlib="$ok"
#  pixbuf_xlib_pkgs="$pkgs"
#
#  if test "$have_gdk_pixbuf_xlib" = yes; then
#    AC_CACHE_CHECK([for gdk-pixbuf-xlib includes],
#                   ac_cv_gdk_pixbuf_xlib_config_cflags,
#            [ac_cv_gdk_pixbuf_xlib_config_cflags=`$pkg_config --cflags $pkgs`])
#    AC_CACHE_CHECK([for gdk-pixbuf-xlib libs],
#                   ac_cv_gdk_pixbuf_xlib_config_libs,
#               [ac_cv_gdk_pixbuf_xlib_config_libs=`$pkg_config --libs $pkgs`])
#  fi
#
#  ac_gdk_pixbuf_xlib_config_cflags=$ac_cv_gdk_pixbuf_xlib_config_cflags
#  ac_gdk_pixbuf_xlib_config_libs=$ac_cv_gdk_pixbuf_xlib_config_libs
#
#  if test "$have_gdk_pixbuf_xlib" = yes; then
#    #
#    # we appear to have pixbuf_xlib; check for headers/libs to be sure.
#    #
#    ac_save_gdk_pixbuf_xlib_CPPFLAGS="$CPPFLAGS"
#    CPPFLAGS="$CPPFLAGS $ac_gdk_pixbuf_xlib_config_cflags"
#    have_gdk_pixbuf_xlib=no
#    gdk_pixbuf_xlib_halfassed=yes
#    AC_CHECK_X_HEADER(gdk-pixbuf-xlib/gdk-pixbuf-xlib.h,
#                      [have_gdk_pixbuf_xlib=yes
#                       gdk_pixbuf_xlib_halfassed=no])
#    CPPFLAGS="$ac_save_gdk_pixbuf_xlib_CPPFLAGS"
#  fi
#
#  if test "$have_gdk_pixbuf_xlib" = yes; then
#    # we have the headers, now check for the libraries
#    have_gdk_pixbuf_xlib=no
#    gdk_pixbuf_xlib_halfassed=yes
#
#    AC_MSG_RESULT(checking for gdk_pixbuf_xlib usability...)
#    AC_CHECK_X_LIB(c, gdk_pixbuf_xlib_init,
#                   [have_gdk_pixbuf_xlib=yes
#                    gdk_pixbuf_xlib_halfassed=no],,
#                    $ac_gdk_pixbuf_xlib_config_libs -lX11 -lXext -lm)
#  fi
#
#  if test "$have_gdk_pixbuf_xlib" = yes; then
#    INCLUDES="$INCLUDES $ac_gdk_pixbuf_xlib_config_cflags"
#    PNG_LIBS="$ac_gdk_pixbuf_xlib_config_libs"
#    AC_DEFINE(HAVE_GDK_PIXBUF_XLIB)
#  else
#    AC_MSG_RESULT(checking for gdk_pixbuf_xlib usability... no)
#  fi
#fi


###############################################################################
#
#       Check for -lXft
#
###############################################################################

have_xutf8drawstring=no
AC_CHECK_X_LIB(X11, Xutf8DrawString,
               [have_xutf8drawstring=yes],
               [true], -lX11 -lXext -lm)
if test "$have_xutf8drawstring" = yes ; then
  AC_DEFINE(HAVE_XUTF8DRAWSTRING)
fi


have_xft=no
with_xft_req=unspecified
xft_halfassed=no
AC_ARG_WITH(xft,
[  --with-xft              Include support for the X Freetype library.
  --with-pthread          Enables POSIX threads, for SMP support.],
  [with_xft="$withval"; with_xft_req="$withval"],
  [with_xft=yes])

HANDLE_X_PATH_ARG(with_xft, --with-xft, Xft)

if test "$with_xft" != yes -a "$with_xft" != no ; then
  echo "error: must be yes or no: --with-xft=$with_xft"
  exit 1
fi

if test "$with_xft" = yes; then

  pkgs=''
  ok="yes"
  pkg_check_version xft 2.1.0
  have_xft="$ok"

  if test "$have_xft" = yes; then
    AC_CACHE_CHECK([for Xft includes], ac_cv_xft_config_cflags,
               [ac_cv_xft_config_cflags=`$pkg_config --cflags $pkgs`])
    AC_CACHE_CHECK([for Xft libs], ac_cv_xft_config_libs,
               [ac_cv_xft_config_libs=`$pkg_config --libs $pkgs`])
  fi

  ac_xft_config_cflags=$ac_cv_xft_config_cflags
  ac_xft_config_libs=$ac_cv_xft_config_libs

  if test "$have_xft" = yes; then
    #
    # we appear to have Xft; check for headers/libs to be sure.
    #
    ac_save_xft_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $ac_xft_config_cflags"

    have_xft=no
    AC_CHECK_X_HEADER(X11/Xft/Xft.h, [have_xft=yes])

    CPPFLAGS="$ac_save_xft_CPPFLAGS"
  fi

  if test "$have_xft" = yes; then
    # we have the headers, now check for the libraries
    have_xft=no
    xft_halfassed=yes

    AC_MSG_RESULT(checking for Xft usability...)
    AC_CHECK_X_LIB(c, XftDrawStringUtf8, [have_xft=yes],,
                   $ac_xft_config_libs -lX11 -lXext -lm)
  fi

  if test "$have_xft" = no; then
    AC_MSG_RESULT(checking for Xft usability... no)
  fi
fi

if test "$have_xft" = yes; then
  INCLUDES="$INCLUDES $ac_xft_config_cflags"
  XFT_LIBS="$ac_xft_config_libs"
  XFT_SRCS=''
  XFT_OBJS=''
  AC_DEFINE(HAVE_XFT)
else
  XFT_LIBS=''
  XFT_SRCS='$(UTILS_SRC)/xft.c'
  XFT_OBJS='$(UTILS_BIN)/xft.o'
fi


###############################################################################
#
#       Check for pty support: this allows 'phosphor' and 'apple2'
#	to run curses-based programs, or be used as terminal windows.
#
###############################################################################

PTY_LIBS=
AC_CHECK_HEADERS(pty.h util.h sys/termios.h)
AC_CHECK_X_LIB(util, forkpty,
               [PTY_LIBS="-lutil"
                ac_have_forkpty=yes
                AC_DEFINE(HAVE_FORKPTY)])

if test "$ac_have_forkpty" != yes ; then
  # we don't need (or have) -lutil on macOS 10.4.2...
  AC_CHECK_X_LIB(c, forkpty,
                 [PTY_LIBS=""
                  AC_DEFINE(HAVE_FORKPTY)])
fi


###############################################################################
#
#       Check for libcap, which allows "sonar" to work without setuid.
#       Except, nevermind, that's bad.  See comments in sonar-icmp.c.
#
###############################################################################

have_libcap=no
setcap_hacks_default=no
setcap_hacks="$setcap_hacks_default"
setcap_hacks_req=unspecified

#AC_ARG_WITH(setcap-hacks,
#[  --with-setcap-hacks     Build the "sonar" demo with libcap, which allows
#                          it to ping other hosts without being setuid.],
#  [setcap_hacks="$withval"; setcap_hacks_req="$withval"],
#  [setcap_hacks=yes])
#
#HANDLE_X_PATH_ARG(setcap_hacks, --with-setcap-hacks, setcap hacks)
#
#if test "$setcap_hacks" != yes -a "$setcap_hacks" != no ; then
#  echo "error: must be yes or no: --with-setcap-hacks=$setcap_hacks"
#  exit 1
#fi
#
#if test "$setcap_hacks" = yes; then
#
#  AC_CHECK_PROGS(setcap_program, setcap)
#  if test "$setcap_program" != ''; then
#    AC_CHECK_X_HEADER(sys/capability.h, [have_setcap=yes])
#  fi
#  if test "$have_setcap" = yes; then
#    AC_CHECK_X_LIB(cap, cap_set_flag,
#                   [have_libcap=yes
#                    PROG_SETCAP="$setcap_program"
#                    LIBCAP_LIBS="-lcap"
#                    AC_DEFINE(HAVE_LIBCAP)])
#  fi
#fi


###############################################################################
#
#       Sonar must be setuid to be able to ping other hosts.
#
###############################################################################

if test "$have_libcap" = yes; then
  setuid_hacks_default=no
else
  setuid_hacks_default=yes
fi

setuid_hacks="$setuid_hacks_default"
AC_ARG_WITH(setuid-hacks,
[  --with-setuid-hacks     Install the "sonar" demo as setuid root, which is
                          needed in order to ping other hosts.],
  [setuid_hacks="$withval"], [setuid_hacks="$setuid_hacks_default"])

HANDLE_X_PATH_ARG(setuid_hacks, --with-setuid-hacks, setuid hacks)

if test "$setuid_hacks" = yes; then
  true
elif test "$setuid_hacks" != no; then
  echo "error: must be yes or no: --with-setuid-hacks=$setuid_hacks"
  exit 1
fi


###############################################################################
#
#       Check for --with-record-animation
#
###############################################################################

record_anim_default=no
record_anim="$record_anim_default"
AC_ARG_WITH(record-animation,
[  --with-record-animation Include code for generating MP4 videos.],
  [record_anim="$withval"], [record_anim="$record_anim_default"])

HANDLE_X_PATH_ARG(record_anim, --with-record-animation, record animation)

if test "$record_anim" = yes; then
  true
elif test "$record_anim" != no; then
  echo "error: must be yes or no: --with-record-animation=$record_anim"
  exit 1
fi

if test "$record_anim" = yes; then
  if test "$have_gdk_pixbuf" != yes; then
    AC_MSG_WARN(--with-record-animation requires GDK-Pixbuf)
  else
    AC_MSG_RESULT(enabling --with-record-animation)
    AC_DEFINE(HAVE_RECORD_ANIM)
    ANIM_OBJS='$(ANIM_OBJS)'
    ANIM_LIBS='$(ANIM_LIBS)'
  fi
fi


###############################################################################
#
#       Done testing.  Now, set up the various -I and -L variables,
#       and decide which GUI program to build by default.
#
###############################################################################

DEPEND=makedepend
DEPEND_FLAGS=
DEPEND_DEFINES=


if test \! -z "$includedir" ; then 
  INCLUDES="$INCLUDES -I$includedir"
fi

if test \! -z "$libdir" ; then
  LDFLAGS="$LDFLAGS -L$libdir"
fi


PREFERRED_DEMO_PROGRAM=''
ALL_DEMO_PROGRAMS=
if test "$have_motif" = yes; then
  PREFERRED_DEMO_PROGRAM=xscreensaver-settings-Xm
  ALL_DEMO_PROGRAMS="$PREFERRED_DEMO_PROGRAM $ALL_DEMO_PROGRAMS"
fi
if test "$have_gtk" = yes; then
  PREFERRED_DEMO_PROGRAM=xscreensaver-settings-Gtk
  ALL_DEMO_PROGRAMS="$PREFERRED_DEMO_PROGRAM $ALL_DEMO_PROGRAMS"
fi


if test "$have_kerberos" = yes; then
  PASSWD_SRCS="$PASSWD_SRCS \$(KERBEROS_SRCS)"
  PASSWD_OBJS="$PASSWD_OBJS \$(KERBEROS_OBJS)"
fi
if test "$have_pam" = yes; then
  PASSWD_SRCS="$PASSWD_SRCS \$(PAM_SRCS)"
  PASSWD_OBJS="$PASSWD_OBJS \$(PAM_OBJS)"
  INSTALL_PAM="install-pam"
fi
if test "$enable_pam_check_account_type" = yes; then
  COMMENT_PAM_CHECK_ACCOUNT=""
else
  COMMENT_PAM_CHECK_ACCOUNT="#"
fi
PASSWD_SRCS="$PASSWD_SRCS \$(PWENT_SRCS)"
PASSWD_OBJS="$PASSWD_OBJS \$(PWENT_OBJS)"


if test "$enable_locking" = yes; then
  LOCK_SRCS='$(LOCK_SRCS_1) $(PASSWD_SRCS)'
  LOCK_OBJS='$(LOCK_OBJS_1) $(PASSWD_OBJS)'
else
  LOCK_SRCS='$(NOLOCK_SRCS_1)'
  LOCK_OBJS='$(NOLOCK_OBJS_1)'
fi

if test "$ac_macosx" = yes; then
  EXES_OSX='$(EXES_OSX)'
  SCRIPTS_OSX='$(SCRIPTS_OSX)'
  MEN_OSX='$(MEN_OSX)'
else
  EXES_OSX=
  SCRIPTS_OSX=
  MEN_OSX=
fi


if test "$setuid_auth" = yes; then
  SETUID_AUTH=yes
else
  SETUID_AUTH=no
fi

if test "$setuid_hacks" = yes; then
  SETUID_HACKS=yes
else
  SETUID_HACKS=no
fi

if test "$have_libcap" = yes; then
  SETCAP_HACKS=yes
else
  SETCAP_HACKS=no
fi

tab='	'
if test "$have_gl" = yes; then
  GL_EXES='$(GL_EXES)'
  SUID_EXES='$(SUID_EXES)'
  RETIRED_GL_EXES='$(RETIRED_GL_EXES)'
  GL_UTIL_EXES='$(GL_UTIL_EXES)'
  GL_MEN='$(GL_MEN)'
  GL_KLUDGE=" "
else
  GL_KLUDGE="-"
fi

if test "$have_gle" = yes; then
  GLE_EXES='$(GLE_EXES)'
  GLE_KLUDGE=" "
else
  GLE_KLUDGE="-"
fi

if test "$have_jpeg" = yes -a "$have_gdk_pixbuf" = yes; then
 JPEG_EXES='$(JPEG_EXES)'
fi


# Another substitution in the XScreenSaver.ad.in file:
#
if test "$gnome_open_program" != ''; then
  GNOME24=''
  GNOME22='!	'
  NOGNOME='!	'
elif test "$gnome_url_show_program" != ''; then
  GNOME24='!	'
  GNOME22=''
  NOGNOME='!	'
else
  GNOME24='!	'
  GNOME22='!	'
  NOGNOME=''
fi


# Set PO_DATADIR to something sensible.
#
AC_MSG_CHECKING([for locale directory])
if test -n "$GTK_DATADIR" ; then
  PO_DATADIR="$GTK_DATADIR"
elif test "$have_gtk" = yes; then
  PO_DATADIR=`$pkg_config --variable=prefix gtk+-2.0`
  PO_DATADIR="$PO_DATADIR/share"
fi

if test -z "$PO_DATADIR" ; then
  #
  # #### Total fucking kludge --
  # Map /build/prefix/usr/X11R6/share/ to /build/prefix/usr/share/
  # but of course we need to expand all the nested variables to do that...
  #
  dd=`eval eval eval eval eval eval eval eval eval eval eval echo $datadir`
  PO_DATADIR=`echo $dd | sed 's@/X11R6/@/@'`
fi

AC_MSG_RESULT($PO_DATADIR/locale)


# canonicalize slashes.
HACK_CONF_DIR=`echo "${HACK_CONF_DIR}" | sed 's@/$@@;s@//*@/@g'`
FONT_DIR=`echo "${FONT_DIR}" | sed 's@/$@@;s@//*@/@g'`

# gcc 3.0 likes to issue this warning for every file:
#
# cc1: warning: changing search order for system directory "/usr/local/include"
# cc1: warning:   as it has already been specified as a non-system directory
#
# Yay.  We can only avoid that by deleting "-I${prefix}/include" from the list.
# Which *should* be totally redundant, and thus an ok thing to delete?
#
INCLUDES=`echo "$INCLUDES" | sed 's@ -I${prefix}/include@@g;'`


###############################################################################
#
#       Perform substitutions and write Makefiles.
#
###############################################################################

AC_SUBST(INCLUDES)

AC_SUBST(PREFERRED_DEMO_PROGRAM)
AC_SUBST(ALL_DEMO_PROGRAMS)
AC_SUBST(SAVER_LIBS)
AC_SUBST(MOTIF_LIBS)
AC_SUBST(GTK_LIBS)
AC_SUBST(XML_LIBS)
AC_SUBST(PNG_LIBS)
AC_SUBST(JPEG_LIBS)
AC_SUBST(HACK_LIBS)
AC_SUBST(PTY_LIBS)
AC_SUBST(GL_LIBS)
AC_SUBST(GLE_LIBS)
AC_SUBST(XDPMS_LIBS)
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(XINERAMA_LIBS)
AC_SUBST(PASSWD_LIBS)
AC_SUBST(LIBCAP_CFLAGS)
AC_SUBST(LIBCAP_LIBS)
AC_SUBST(PROG_SETCAP)
AC_SUBST(SETUID_AUTH)
AC_SUBST(SETUID_HACKS)
AC_SUBST(SETCAP_HACKS)
AC_SUBST(INSTALL_DIRS)
AC_SUBST(INSTALL_PAM)
AC_SUBST(GLIB_COMPILE_RESOURCES)
AC_SUBST(HAVE_PAM_FAIL_DELAY)
AC_SUBST(COMMENT_PAM_CHECK_ACCOUNT)
AC_SUBST(NEW_LOGIN_COMMAND)
AC_SUBST(NEW_LOGIN_COMMAND_P)
AC_SUBST(DEFAULT_IMAGES_P)
AC_SUBST(DEFAULT_IMAGE_DIRECTORY)
AC_SUBST(DEFAULT_TEXT_FILE)
AC_SUBST(WITH_BROWSER)

AC_SUBST(EXES_OSX)
AC_SUBST(EXES_SYSTEMD)
AC_SUBST(SYSTEMD_LIBS)
AC_SUBST(SCRIPTS_OSX)
AC_SUBST(MEN_OSX)

AC_SUBST(PASSWD_SRCS)
AC_SUBST(PASSWD_OBJS)
AC_SUBST(XFT_SRCS)
AC_SUBST(XFT_OBJS)
AC_SUBST(XFT_LIBS)
AC_SUBST(LOCK_SRCS)
AC_SUBST(LOCK_OBJS)
AC_SUBST(JPEG_EXES)
AC_SUBST(GL_EXES)
AC_SUBST(RETIRED_GL_EXES)
AC_SUBST(SUID_EXES)
AC_SUBST(GL_UTIL_EXES)
AC_SUBST(GL_MEN)
AC_SUBST(GL_KLUDGE)
AC_SUBST(GLE_EXES)
AC_SUBST(GLE_KLUDGE)
AC_SUBST(JWZGLES_OBJS)
AC_SUBST(GNOME24)
AC_SUBST(GNOME22)
AC_SUBST(NOGNOME)
AC_SUBST(HACKDIR)
AC_SUBST(HACKDIR_FULL)
AC_SUBST(GTK_DATADIR)
AC_SUBST(PO_DATADIR)
AC_SUBST(HACK_CONF_DIR)
AC_SUBST(FONT_DIR)
AC_SUBST(ANIM_OBJS)
AC_SUBST(ANIM_LIBS)

APPDEFAULTS=$ac_x_app_defaults
AC_SUBST(APPDEFAULTS)

AC_SUBST(DEPEND)
AC_SUBST(DEPEND_FLAGS)
AC_SUBST(DEPEND_DEFINES)
AC_SUBST(PERL)

AC_CONFIG_FILES([Makefile
          utils/Makefile
          jwxyz/Makefile
          hacks/Makefile
          hacks/fonts/Makefile
          hacks/images/Makefile
          hacks/glx/Makefile
          po/Makefile.in
          driver/Makefile
          driver/xscreensaver.pam
          driver/XScreenSaver.ad])
AC_OUTPUT

###############################################################################
#
#       Print some warnings at the end.
#
###############################################################################

CONF_STATUS=0

warn_prefix_1="    Warning:"
warn_prefix_2="       Note:"
warn_prefix="$warn_prefix_1"

warning=no
warnsep='    #################################################################'

warnpre() {
  if test "$warning" = no ; then
    echo '' ; echo "$warnsep" ; echo ''
    warning=yes
  fi
}

warn() {
  warnpre
  if test "$warning" = long ; then echo '' ; fi
  warning=yes
  rest="$@"
  echo "$warn_prefix $rest"
}

warnL() {
  was=$warning
  warnpre
  warning=yes
  if test "$was" != no ; then echo '' ; fi
  rest="$@"
  echo "$warn_prefix $rest"
}

warn2() {
  rest="$@"
  echo "             $rest"
  warning=long
}

note() {
  warn_prefix="$warn_prefix_2"
  warn $@
  warn_prefix="$warn_prefix_1"
}

noteL() {
  warn_prefix="$warn_prefix_2"
  warnL $@
  warn_prefix="$warn_prefix_1"
}


# ac_prog_cc_no_pthread normally only happens on AIX, because according
# to AX_PTHREAD, AIX needs CC=xlc_r or CC=cc_r to do threads.
# If CC is specified, it takes precedence over --with-pthread.
if test "$ac_prog_cc_no_pthread" ; then
  warnL "You requested $ac_original_cc for the C compiler, but it doesn't"
  warn2 "support POSIX threads."
  echo ""
  warn2 "If you have multiple CPU cores, try CC=$PTHREAD_CC."
  CONF_STATUS=1
elif test "$with_pthread_req" = yes -a "$have_pthread" = no ; then
  warn 'POSIX threads were requested, but were not found.'
  CONF_STATUS=1
fi

if test "$with_sgi_req" = yes -a "$have_sgi" = no ; then
  warn 'The SGI saver extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_xidle_req" = yes -a "$have_xidle" = no ; then
  warn 'The XIdle extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_mit_req" = yes -a "$have_mit" = no ; then
  warn 'The MIT-SCREEN-SAVER extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_xshm_req" = yes -a "$have_xshm" = no ; then
  warn 'The XSHM extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_xdbe_req" = yes -a "$have_xdbe" = no ; then
  warn 'The DOUBLE-BUFFER extension was requested, but was not found.'
  CONF_STATUS=1
fi

#if test "$with_xcomposite_req" = yes -a "$have_xcomposite" = no ; then
#  warn 'The XCOMPOSITE extension was requested, but was not found.'
#  CONF_STATUS=1
#fi

if test "$with_xkb_req" = yes -a "$have_xkb" = no ; then
  warn 'The X Keyboard extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_sgivc_req" = yes -a "$have_sgivc" = no ; then
  warn 'The SGI-VIDEO-CONTROL extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_dpms_req" = yes -a "$have_dpms" = no ; then
  warn 'The DPMS extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_xinerama_req" = yes -a "$have_xinerama" = no ; then
  warn 'The Xinerama extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_xf86vmode_req" = yes -a "$have_xf86vmode" = no ; then
  warn 'The XF86VMODE extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_randr_req" = yes -a "$have_randr" = no ; then
  warn 'The RANDR extension was requested, but was not found.'
  CONF_STATUS=1
fi

if test "$with_proc_interrupts_req" = yes -a "$have_proc_interrupts" = no; then
  warn "Checking of /proc/interrupts was requested, but it's bogus."
  CONF_STATUS=1
fi

if test "$pkg_config" = false ; then
  warnL 'The "pkg-config" program was not found.  Without that,'
  warn2 "detection of the various GTK libraries won't work."
  CONF_STATUS=1
else
  pkgerr=`$pkg_config --list-all 2>&1 >/dev/null`
  if test "x$pkgerr" != "x" ; then
    warnL 'The "pkg-config" program produces errors.  This often causes'
    warn2 "detection of the various GTK libraries to malfunction."
    warn2 "The errors are:"
    echo ''
    echo "$pkgerr" | sed 's/^/             > /g'
    CONF_STATUS=1
  fi
fi

if test "$gtk_halfassed" != no ; then
  warnL "GTK was found, but $gtk_halfassed_lib was not, so GTK"
  warn2 "can't be used."
  CONF_STATUS=1
fi

motif_warn2() {
  warn2 "The Motif version of \`xscreensaver-settings' is missing many"
  warn2 'features.  It is strongly recommended that you build the GTK'
  warn2 'version instead.'
}

if test "$have_motif" = no -a "$have_gtk" = no; then

  if test "$with_motif" = yes; then
    warnL "Neither the GTK nor Motif libraries were found; the"
    warn2 "\`xscreensaver-settings' program requires one of these."
    echo ''
    motif_warn2
    CONF_STATUS=1
  else
    warnL "The GTK libraries do not seem to be available; the"
    warn2 "\`xscreensaver-settings' program requires them."
    CONF_STATUS=1
#   echo ''
#   warn2 'You can use Motif or Lesstif instead of GTK (use the'
#   warn2 "\`--with-motif' option) but that is NOT recommended."
#   motif_warn2
  fi

elif test "$with_motif_req" = yes -a "$have_motif" = no ; then
  warnL "Use of Motif was requested, but it wasn't found;"
  warn2 "Gtk will be used instead."
  CONF_STATUS=1

elif test "$with_gtk_req" = yes -a "$have_gtk" = no ; then
  warnL "Use of Gtk was requested, but it wasn't found."
  CONF_STATUS=1
fi


if test "$ac_cv_libxt" != yes ; then
  warnL 'The Xt library was not found.  It is required.'
  CONF_STATUS=1
fi

if test "$have_gtk" = yes -a "$have_gdk_pixbuf" = no ; then
  warn  "GTK is being used, but the GDK-Pixbuf library and/or"
  warn2 "headers were not found.  That can't be good.  Please"
  warn2 "install the GDK-Pixbuf development kit and re-configure."
  CONF_STATUS=1
fi

if test "$have_motif" = yes -a "$have_lesstif" = yes ; then

  preferred_lesstif=0.92

  if test "$lesstif_version" = unknown; then
    warnL "Unable to determine the LessTif version number!"
    warn2 "Make sure you are using version $preferred_lesstif or newer."
    warn2 "See <http://www.lesstif.org/>."

  elif test \! $lesstif_version -gt 82; then
    warnL "LessTif version $lesstif_version_string is being used."
    warn2 "LessTif versions 0.82 and earlier are too buggy to"
    warn2 "use with XScreenSaver; it is strongly recommended"
    warn2 "that you upgrade to at least version $preferred_lesstif!"
    warn2 "See <http://www.lesstif.org/>."
    CONF_STATUS=1
  fi
fi


if test "$have_motif" = yes -a "$have_gtk" = no ; then
  warn  'Motif is being used, and GTK is not.'
  echo  ''
  motif_warn2
fi

halfassery() {
  echo ''
  warn2 "More specifically, we found the headers but not the"
  warn2 "libraries.  Possibly a 'devel' package is needed."
}

if test "$with_gdk_pixbuf_req" = yes  -a "$have_gdk_pixbuf" = no; then
  warnL 'Use of GDK-Pixbuf was requested, but it was not found.'
  CONF_STATUS=1
fi

if test "$have_gdk_pixbuf" = no -o "$gdk_pixbuf_halfassed" = yes ; then

  if test "$with_gdk_pixbuf_req" = yes ; then
    true
  elif test "$with_gdk_pixbuf_req" = no ; then
    warnL 'The GDK-Pixbuf library is not being used.'
  else
    warnL 'The GDK-Pixbuf library was not found.'
  fi

  if test "$gdk_pixbuf_halfassed" = yes ; then halfassery ; fi
  if test "$have_png" = yes ; then
    echo ''
    warn2 'The PNG library is being used instead.'
  fi

  echo ''
  warn2 'Some of the demos will not use images as much as they could.'
  warn2 'You should consider installing GDK-Pixbuf and re-running'
  warn2 'configure.'
fi


#if test "$have_gdk_pixbuf" = yes -a "$have_gdk_pixbuf_xlib" = no ; then
#
#  warnL 'The GDK-Pixbuf-Xlib library was not found.'
#
#  if test "$gdk_pixbuf_xlib_halfassed" = yes ; then halfassery ; fi
#  if test "$have_png" = yes ; then
#    echo ''
#    warn2 'The PNG library is being used instead.'
#  fi
#
#  echo ''
#  warn2 'Some of the demos will not use images as much as they could.'
#  warn2 'You should consider installing GDK-Pixbuf-Xlib and re-running'
#  warn2 'configure.'
#  echo ''
#  warn2 "Recently, some distros have stopped distributing"
#  warn2 "GDK-Pixbuf-Xlib entirely.  So good luck with that."
#fi


if test "$have_jpeg" = no ; then
  if test "$with_jpeg_req" = yes ; then
    warnL 'Use of libjpeg was requested, but it was not found.'
    CONF_STATUS=1
  elif test "$with_jpeg_req" = no ; then
    warnL 'The JPEG library is not being used.'
  else
    warnL 'The JPEG library was not found.'
  fi

  if test "$jpeg_halfassed" = yes ; then halfassery ; fi
  if test "$have_gdk_pixbuf" = no ; then
    echo ''
    warn2 "This means that it won't be possible for the image-manipulating"
    warn2 "display modes to load files from disk; and it also means that"
    warn2 "the \`webcollage' program will be much slower."
    CONF_STATUS=1
  else
    warn2 "This means the \`webcollage' program will be much slower."
  fi
fi


if test "$have_png" = no ; then
  if test "$with_png_req" = yes ; then
    warnL 'Use of libpng was requested, but it was not found.'
    CONF_STATUS=1
  elif test "$with_png_req" = no ; then
    warnL 'The PNG library is not being used.'
  else
    warnL 'The PNG library was not found.'
  fi

  if test "$png_halfassed" = yes ; then halfassery ; fi
  warn2 "Many things aren't going to work right."
  CONF_STATUS=1
fi


if test "$have_xinput" = no ; then
  warnL 'The XInput2 library was not found.  It is required.'
  if test "$xinput_halfassed" = yes ; then halfassery ; else echo '' ; fi
  CONF_STATUS=1
fi


if test "$have_xft" = no ; then
  warnL 'The XFreeType library was not found.  It is required.'
  if test "$xft_halfassed" = yes ; then halfassery ; else echo '' ; fi
  CONF_STATUS=1
fi


if test "$have_systemd" = no ; then
  systemd_warned=no
  if test "$systemd_too_old" = yes ; then
    warnL "The systemd library is too old."
    systemd_warned=yes
    CONF_STATUS=1
  elif test "$with_systemd_req" = yes ; then
    warnL "Use of systemd was requested, but it was not found."
    systemd_warned=yes
    CONF_STATUS=1
  elif test "$ac_macosx" = yes; then
    true
  elif test "$with_systemd_req" = no ; then
    noteL 'The systemd library is not being used.'
    systemd_warned=yes
  else
    warnL "The systemd library was not found."
    systemd_warned=yes
  fi

  if test "$systemd_halfassed" = yes ; then
    halfassery
    systemd_warned=yes
    CONF_STATUS=1
  fi

  if test "$systemd_warned" = yes; then
    echo ''
    warn2 "This means that xscreensaver-systemd won't be built."
    warn2 "Without that, xscreensaver will not be able to auto-lock"
    warn2 "before the system is suspended (e.g., closing laptop lid)"
    warn2 "and might activate while video playback is in progress."
  fi
fi


if test "$have_gl" = no ; then
  if test "$with_gl_req" = yes ; then
    warnL 'The OpenGL 3D library is required, but it was not found.'
    CONF_STATUS=1
  elif test "$with_gl_req" = no ; then
    noteL 'The OpenGL 3D library is not being used.'
    CONF_STATUS=1
  else
    noteL 'The OpenGL 3D library was not found.'
    CONF_STATUS=1
  fi

  if test "$gl_halfassed" = yes ; then
    halfassery
    if test "$have_glx" = no -a "$have_egl" = no; then
      echo ''
      warn2 'Neither EGL nor GLX were found.'
    fi
  fi

  echo ''
  warn2 'Those demos which use 3D will not be built or installed.'
  warn2 'That is most of them.'
fi

if test "$have_gl" = yes -a "$have_gle" = no ; then

 # nobody cares about this; don't print the warning unless it was
 # requested and not found, or halfway-found.
 if test "$with_gle_req" = yes -o "$gle_halfassed" = yes ; then

  if test "$with_gle_req" = yes ; then
    noteL 'Use of the GLE (GL Extrusion) library was requested, but'
    warn2 'it was not found (though the OpenGL library was found, and'
    warn2 'is being used.)'
    CONF_STATUS=1
  elif test "$with_gle_req" = no ; then
    noteL 'The OpenGL Library is being used, but the GLE (GL Extrusion)'
    warn2 'library is not.'
  else
    noteL 'The OpenGL Library was found, but the GLE (GL Extrusion)'
    warn2 'library was not.'
  fi

  if test "$gle_halfassed" = yes ; then halfassery ; fi

  echo ''
  warn2 'Some of the OpenGL (3D) demos (those that depend on GLE)'
  warn2 'will not be built or installed.  You might want to consider'
  warn2 'installing GLE and re-running configure.  You can find the'
  warn2 'GLE library at <http://www.linas.org/gle/>'

 fi
fi


if test "$with_readdisplay_req" = yes -a "$have_readdisplay" = no ; then
  warn 'Use of XReadDisplay was requested, but it was not found.'
  CONF_STATUS=1
fi

if test "$with_kerberos_req" = yes -a "$have_kerberos" = no ; then
  warn 'Use of Kerberos was requested, but it was not found.'
  CONF_STATUS=1
fi

if test "$with_pam_req" = yes -a "$have_pam" = no ; then
  warn 'Use of PAM was requested, but it was not found.'
  CONF_STATUS=1
elif test "$have_pam" = no -a "$enable_locking" = yes ; then
  if test -d /etc/pam.d -o -f /etc/pam.conf ; then
    warn  "Your system seems to have PAM, but PAM is not being used."
    warn2 "That is probably not going to work out well."
    CONF_STATUS=1
  fi
fi

if test "$with_shadow_req" = yes -a "$have_shadow" = no ; then
  warn 'Use of shadow passwords was requested, but they were not found.'
  CONF_STATUS=1
fi

if test "$setcap_hacks_req" = yes -a "$have_libcap" = no ; then
  warn 'Use of libcap was requested, but it was not found.'
  CONF_STATUS=1
fi

if test "$ac_macosx" = yes -a "$enable_locking" = yes ; then
  warn  "You have specified --enable-locking on macOS."
  warn2 "THIS DOES NOT WORK!  Don't do this!"
fi

if test "$have_jwzgles" = yes ; then
  warn  "Do not use --with-gles unless you are sure that you know"
  warn2 "what you are doing!  It is not recommended on normal X11"
  warn2 "systems."

  if test "$have_egl" = no; then
    warn2 ""
    warn2 "Using --with-gles and --with-glx probably won't work."
  fi
fi


# You are in a twisty maze of namespaces and syntaxes, all alike.
# Expand all of these as the Makefile will probably expand them.
# Fuck the skull of Unix.
#
bindir=`eval eval eval eval eval eval eval echo $bindir`
HACKDIR=`eval eval eval eval eval eval eval echo $HACKDIR`
HACK_CONF_DIR=`eval eval eval eval eval eval eval echo $HACK_CONF_DIR`
FONT_DIR=`eval eval eval eval eval eval eval echo $FONT_DIR`

# canonicalize slashes.
bindir=`echo  "${bindir}"              | sed 's@/$@@;s@//*@/@g'`
HACKDIR=`echo "${HACKDIR}"             | sed 's@/$@@;s@//*@/@g'`
HACK_CONF_DIR=`echo "${HACK_CONF_DIR}" | sed 's@/$@@;s@//*@/@g'`
FONT_DIR=`echo "${FONT_DIR}"           | sed 's@/$@@;s@//*@/@g'`

if test -z "$FONT_DIR" ; then
  FONT_DIR='not installed';
else
  FONT_DIR="${FONT_DIR}/"
fi

# Sanity check the hackdir
for bad_choice in xscreensaver xscreensaver-settings xscreensaver-command ; do
  if test "${HACKDIR}" = "${bindir}/${bad_choice}" ; then
    echo ""
    AC_MSG_ERROR(["--with-hackdir=${bindir}/${bad_choice}" won't work.
                   There will be an executable installed with that name, so
                   that can't be the name of a directory as well.  Please
                   re-configure with a different directory name.])
  fi
done


# Warn if there's a previous RPM package installed.

rpm_vers=
if test -z "$RPM_PACKAGE_VERSION" ; then
  # Don't bother with this test if we are currently *building* an RPM.
  rpmnames="xscreensaver xscreensaver-base xscreensaver-extras
            xscreensaver-data xscreensaver-data-extra"

  # M4 sucks!!
  changequote(X,Y)
  pkg_vers=`(rpm -qv $rpmnames) 2>&- | \
        sed -n 's/^[-a-z]*-\([0-9][0-9]*[.].*\)/\1/p' | head -1`
  rpm_vers="$pkg_vers"
  changequote([,])
fi

if test \! -z "$rpm_vers" ; then

  # M4 sucks!!
  changequote(A,B)
  pkg_bindir=`rpm  -ql $rpmnames | sed -n 's@^\(.*/\)xscreensaver-settings$@\1@p'`
  pkg_bindir2=`rpm -ql $rpmnames | sed -n 's@^\(.*/\)xscreensaver-demo$@\1@p'`
  pkg_hackdir=`rpm -ql $rpmnames | sed -n 's@^\(.*/\)popsquares$@\1@p'`
  pkg_confdir=`rpm -ql $rpmnames | sed -n 's@^\(.*/\)popsquares\.xml$@\1@p'`
  pkg_fontdir=`rpm -ql $rpmnames | sed -n 's@^\(.*/\).*\.ttf$@\1@p' | head -1`
  pkg_addir=`rpm   -ql $rpmnames | sed -n 's@^\(.*/\)XScreenSaver$@\1@p'`

  if test -z "$pkg_bindir" ; then pkg_bindir="$pkg_bindir2" ; fi
  changequote([,])
fi


# Warn if there's a previous Debian package installed.
#
deb_vers=
if test -z "$pkg_vers" ; then
  debnames="xscreensaver xscreensaver-data xscreensaver-data-extra"
  pkg_vers=''
  for dpkg in $debnames ; do
    if test -z "$pkg_vers"; then
      pkg_vers=`dpkg -s $dpkg 2>/dev/null | sed -n 's/^Version: \(.*\)$/\1/p'`
      deb_vers="$pkg_vers"
    fi
  done
fi

if test \! -z "$deb_vers" ; then

  # M4 sucks!!
  changequote(A,B)
  pkg_bindir=`dpkg  -L $debnames 2>&- | sed -n 's@^\(.*/\)xscreensaver-settings$@\1@p'`
  pkg_bindir2=`dpkg -L $debnames 2>&- | sed -n 's@^\(.*/\)xscreensaver-demo$@\1@p'`
  pkg_hackdir=`dpkg -L $debnames 2>&- | sed -n 's@^\(.*/\)popsquares$@\1@p'`
  pkg_confdir=`dpkg -L $debnames 2>&- | sed -n 's@^\(.*/\)popsquares\.xml$@\1@p'`
  pkg_fontdir=`dpkg -L $debnames 2>&- | sed -n 's@^\(.*/\).*\.ttf$@\1@p' | head -1`
  pkg_addir=`dpkg   -L $debnames 2>&- | sed -n 's@^\(.*/\)XScreenSaver[^/]*$@\1@p'`
  changequote([,])

  if test -z "$pkg_bindir" ; then pkg_bindir="$pkg_bindir2" ; fi
fi

bindir_err=
hackdir_err=
confdir_err=
fontdir_err=
addir_err=
if ! test -z "$pkg_vers" ; then

  err=" (DIFFERS)"

  if test -z "$pkg_bindir"  ; then pkg_bindir='???'
  elif test "$pkg_bindir"  != "${bindir}/"        ; then bindir_err="$err" ; fi

  if test -z "$pkg_hackdir" ; then pkg_hackdir='???'
  elif test "$pkg_hackdir" != "${HACKDIR}/"       ; then hackdir_err="$err"; fi

  if test -z "$pkg_confdir" ; then pkg_confdir='???'
  elif test "$pkg_confdir" != "${HACK_CONF_DIR}/" ; then confdir_err="$err"; fi

  if test -z "$pkg_fontdir" ; then pkg_fontdir='none'
  elif test "$pkg_fontdir/" != "${FONT_DIR}"      ; then fontdir_err="$err"; fi

  if test -z "$pkg_addir"   ; then pkg_addir='???'
  elif test "$pkg_addir"   != "${APPDEFAULTS}/"   ; then addir_err="$err"  ; fi

  warning=no
  warnL "There is already an installed package of xscreensaver"
  warn2 "version \"$pkg_vers\", in these directories:"
  echo ""
  warn2 "User programs: $pkg_bindir$bindir_err"
  warn2 "Screen savers: $pkg_hackdir$hackdir_err"
  warn2 "Configuration: $pkg_confdir$confdir_err"
  warn2 "Extra fonts:   $pkg_fontdir$fontdir_err"
  warn2 "App Defaults:  $pkg_addir$addir_err"
fi

echo ""
echo "$warnsep"
echo ""
echo "    XScreenSaver will be installed in these directories:"
echo ""
echo "             User programs: ${bindir}/$bindir_err"
echo "             Screen savers: ${HACKDIR}/$hackdir_err"
echo "             Configuration: ${HACK_CONF_DIR}/$confdir_err"
echo "             Extra Fonts:   ${FONT_DIR}$fontdir_err"
echo "             App Defaults:  ${APPDEFAULTS}/$addir_err"
echo ""

exit $CONF_STATUS
