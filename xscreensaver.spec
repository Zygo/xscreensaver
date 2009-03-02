%define	name		xscreensaver
%define	version		3.33
%define	release		1
%define	serial		1
%define	x11_prefix	/usr/X11R6
%define	gnome_prefix	/usr
%define	kde_prefix	/usr

%define gnome_datadir	%{gnome_prefix}/share
%define gnome_ccdir	%{gnome_datadir}/control-center/Desktop
%define gnome_paneldir	%{gnome_datadir}/gnome/apps/Settings/Desktop
%define gnome_icondir	%{gnome_datadir}/pixmaps

# By default, builds the basic, non-GL package.
# To build both the basic and GL-add-on packages:
#   rpm --define "USE_GL yes" ...
# or uncomment the following line.
# %define	USE_GL		yes

Summary:	X screen saver and locker
Name:		%{name}
Version:	%{version}
Release:	%{release}
Serial:		%{serial}
Group:		Amusements/Graphics
Copyright:	BSD
URL:		http://www.jwz.org/xscreensaver
Vendor:		Jamie Zawinski <jwz@jwz.org>
Source:		%{name}-%{version}.tar.gz
Buildroot:	%{_tmppath}/%{name}-%{version}-root

%description
A modular screen saver and locker for the X Window System.
Highly customizable: allows the use of any program that
can draw on the root window as a display mode.
More than 120 display modes are included in this package.
%{?USE_GL:See also the xscreensaver-gl package, which}
%{?USE_GL:includes optional OpenGL display modes.}

%{?USE_GL:%package gl}
%{?USE_GL:Group:	Amusements/Graphics}
%{?USE_GL:Requires:	xscreensaver = %{version}}
%{?USE_GL:Summary:	A set of GL screensavers}
%{?USE_GL:%description gl}
%{?USE_GL:The xscreensaver-gl package contains even more screensavers for your}
%{?USE_GL:mind-numbing, ambition-eroding, time-wasting, hypnotized viewing}
%{?USE_GL:pleasure. These screensavers require OpenGL or Mesa support.}
%{?USE_GL: }
%{?USE_GL:Install the xscreensaver-gl package if you need more screensavers}
%{?USE_GL:for use with the X Window System and you have OpenGL or Mesa}
%{?USE_GL:installed.}

%prep
%setup -q

%build
RPMOPTS=""

# Is this really needed?  If so, why?
# %ifarch alpha
#  RPMOPTS="$RPMOPTS --without-xshm-ext"
# %endif

# On Solaris, build without PAM and with Shadow.
# On other systems, build with PAM and without Shadow.
#
%ifos solaris
 RPMOPTS="$RPMOPTS --without-pam"
%else
 RPMOPTS="$RPMOPTS --with-pam --without-shadow"
%endif

%{?USE_GL:RPMOPTS="$RPMOPTS --with-gl"}
%{!?USE_GL:RPMOPTS="$RPMOPTS --without-gl"}

CFLAGS="$RPM_OPT_FLAGS" \
 ./configure --prefix=%{x11_prefix} \
             --enable-subdir=../lib/xscreensaver \
             --with-zippy=/usr/games/fortune \
             --without-setuid-hacks \
             $RPMOPTS

make

%install

# Most xscreensaver executables go in the X bin directory (/usr/X11R6/bin/)
# but some of them (e.g., the control panel capplet) go in the GNOME bin
# directory instead (/usr/bin/).
#
mkdir -p $RPM_BUILD_ROOT%{gnome_prefix}/bin
mkdir -p $RPM_BUILD_ROOT%{gnome_ccdir}
mkdir -p $RPM_BUILD_ROOT%{gnome_paneldir}

# Likewise for KDE: the .kss file goes in the KDE bin directory (/usr/bin/).
#
export KDEDIR=%{kde_prefix}
mkdir -p $RPM_BUILD_ROOT$KDEDIR/bin

# This is a directory that "make install" won't make as needed
# (since Linux uses /etc/pam.d/* and Solaris uses /etc/pam.conf).
#
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

make  install_prefix=$RPM_BUILD_ROOT \
      AD_DIR=%{x11_prefix}/lib/X11/app-defaults \
      GNOME_BINDIR=%{gnome_prefix}/bin \
      install-strip

# Make a pair of lists, of the GL and non-GL executables.
# Do this by parsing the output of a dummy run of "make install"
# in the driver/, hacks/ and hacks/glx/ directories.
#
list_files() {
  make -s install_prefix=$RPM_BUILD_ROOT INSTALL=true           \
          GNOME_BINDIR=%{gnome_prefix}/bin                      \
          "$@"                                                  |
    sed -n -e 's@.* /\([^ ]*\)$@/\1@p'                          |
    sed    -e "s@^$RPM_BUILD_ROOT@@"                            \
           -e "s@/bin/\.\./@/@"                                 |
    sed    -e 's@\(.*/man/.*\)@\1\*@'                           |
    sort
}

( cd hacks ; list_files install ; \
  cd ../driver; list_files install-program install-scripts ) \
   > $RPM_BUILD_DIR/xscreensaver-%{version}/exes-non-gl
( cd hacks/glx ; list_files install ) \
   > $RPM_BUILD_DIR/xscreensaver-%{version}/exes-gl


# This line is redundant, except that it causes the "xscreensaver"
# executable to be installed unstripped (while all others are stripped.)
# You should install it this way so that jwz gets useful bug reports.
#
install -m 4755 driver/xscreensaver $RPM_BUILD_ROOT%{x11_prefix}/bin

# Even if we weren't compiled with PAM support, make sure to include
# the PAM module file in the RPM anyway, just in case.
#
( cd driver ;
  make install_prefix=$RPM_BUILD_ROOT PAM_DIR=/etc/pam.d install-pam )

# Make sure all files are readable by all, and writable only by owner.
#
chmod -R a+r,u+w,og-w $RPM_BUILD_ROOT


# This is a tricky part...
#
# xscreensaver installs several files that are also installed by the
# "control-center" RPM.  The versions from xscreensaver are better,
# and so should override control-center.  But, the way RPM works,
# if the xscreensaver RPM contained those files, the end user would
# have to "--force" to make the xscreensaver RPM install.  That's
# not something people are used to doing, so that's Bad.
#
# So instead, we rename the files so that they don't conflict with
# the control center.  Then we have a "%post" script that creates
# symbolic links to our files.

CCDIR=$RPM_BUILD_ROOT%{gnome_ccdir}
PADIR=$RPM_BUILD_ROOT%{gnome_paneldir}
CADIR=$RPM_BUILD_ROOT%{gnome_prefix}/bin
DESKF=screensaver-properties.desktop
CAPLT=screensaver-properties-capplet

if [ -f $CCDIR/$DESKF ]; then
  mv $CCDIR/$DESKF $CCDIR/x$DESKF
  mv $PADIR/$DESKF $PADIR/x$DESKF
  mv $CADIR/$CAPLT $CADIR/x$CAPLT
fi

%post
# This part runs on the end user's system, when the RPM is installed.
# (See comment above, at end of "%install" section.)

verbose=0

overwrite_links() {
  dir="$1"
  oname="$2"
  nname="$3"

  # only do this if the file we're making a link *to* exists
  # (i.e., was present in this rpm.)
  #
  if [ -f "$dir/$nname" ]; then

    # backup or delete the old version, if any.
    #
    existed=0
    if [ -f "$dir/$oname" ]; then
      existed=1
      if [ -f "$dir/$oname.rpmsave" ]; then
        rm -f "$dir/$oname"
        if [ $verbose -gt 1 ]; then
          echo "$dir/$oname.rpmsave already exists"  >&2
        fi
      else
        mv "$dir/$oname" "$dir/$oname.rpmsave"
        if [ $verbose -gt 1 ]; then
          echo "saved $dir/$oname as $oname.rpmsave" >&2
        fi
      fi
    fi

    # install a relative symlink to the new name.
    #
    ln -s "$nname" "$dir/$oname"
    if [ $verbose -ge 1 ]; then
      if [ $existed = 1 ] ; then
        echo "replaced $dir/$oname" >&2
      else
        echo "created $dir/$oname"  >&2
      fi
    fi
  fi
}

CCDIR=%{gnome_ccdir}
PADIR=%{gnome_paneldir}
CADIR=%{gnome_prefix}/bin
DESKF=screensaver-properties.desktop
CAPLT=screensaver-properties-capplet

overwrite_links $CCDIR $DESKF x$DESKF
overwrite_links $PADIR $DESKF x$DESKF
overwrite_links $CADIR $CAPLT x$CAPLT


%clean
if [ -d $RPM_BUILD_ROOT    ]; then rm -r $RPM_BUILD_ROOT    ; fi
if [ -d $RPM_BUILD_ROOT-gl ]; then rm -r $RPM_BUILD_ROOT-gl ; fi

%files -f exes-non-gl
%defattr(-,root,root)

# Files for the "xscreensaver" package:
#
%doc                README README.debugging
%dir                %{x11_prefix}/lib/xscreensaver
%config             %{x11_prefix}/lib/X11/app-defaults/*
                    %{x11_prefix}/man/man1/xscreensaver*
                    /etc/pam.d/*

%config(missingok)  %{kde_prefix}/bin/*.kss

%config(missingok)  %{gnome_prefix}/bin/*-capplet
%config(missingok)  %{gnome_ccdir}/*.desktop
%config(missingok)  %{gnome_paneldir}/*.desktop
%config(missingok)  %{gnome_icondir}/*

# Files for the "xscreensaver-gl" package:
#
%{?USE_GL:%files -f exes-gl gl}
