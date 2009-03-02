%define	name		xscreensaver
%define	version		4.06
%define	release		1
%define	serial		1
%define	x11_prefix	/usr/X11R6
%define	gnome_prefix	/usr
%define	kde_prefix	/usr
%define gnome_datadir	%{gnome_prefix}/share

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

# This package really should be made to depend on
# control-center >= 1.4.0.2 -OR- control-center >= 1.5.12
# but there's no way to express that.

%description
A modular screen saver and locker for the X Window System.
Highly customizable: allows the use of any program that
can draw on the root window as a display mode.
More than 140 display modes are included in this package.
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

archdir=`./config.guess`
mkdir $archdir
cd $archdir
CFLAGS="$RPM_OPT_FLAGS" \
 ../configure --prefix=%{x11_prefix} \
              --with-setuid-hacks \
              $RPMOPTS
make

%install

archdir=`./config.guess`

# We have to make sure these directories exist,
# or nothing will be installed into them.
#
export KDEDIR=%{kde_prefix}
mkdir -p $RPM_BUILD_ROOT$KDEDIR/bin
mkdir -p $RPM_BUILD_ROOT%{gnome_datadir}
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

cd $archdir
make  install_prefix=$RPM_BUILD_ROOT \
      AD_DIR=%{x11_prefix}/lib/X11/app-defaults \
      GNOME_BINDIR=%{gnome_prefix}/bin \
      install-strip

# This function prints a list of things that get installed.
# It does this by parsing the output of a dummy run of "make install".
#
list_files() {
  make -s install_prefix=$RPM_BUILD_ROOT INSTALL=true           \
          GNOME_BINDIR=%{gnome_prefix}/bin                      \
          "$@"                                                  |
    sed -n -e 's@.* \(/[^ ]*\)$@\1@p'                           |
    sed    -e "s@^$RPM_BUILD_ROOT@@"                            \
           -e "s@/[a-z][a-z]*/\.\./@/@"                         |
    sed    -e 's@\(.*/man/.*\)@\1\*@'                           |
    sort
}

# Collect the names of the non-GL executables and scripts...
# (Including the names of all of the Gnome, KDE, and L10N-related files,
# whereever they might have gotten installed...)
# For the translation catalogs, prepend an appropriate %lang(..) tag.
#
(  cd    hacks ; list_files install ; \
   cd ../driver; list_files install-program install-scripts \
                            install-gnome install-kde ; \
 ( cd ../po;     list_files install | grep '\.' \
    | sed 's@^\(.*/\([^/]*\)/LC.*\)$@%lang(\2) \1@' ) \
) > $RPM_BUILD_DIR/xscreensaver-%{version}/exes-non-gl


# Collect the names of the GL-only executables...
#
( cd hacks/glx ; list_files install ) \
   | grep -v man1/xscreensaver-gl-helper \
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

%post
# This part runs on the end user's system, when the RPM is installed.

pids=`pidof xscreensaver`
if [ -n "$pids" ]; then
  echo "sending SIGHUP to running xscreensaver ($pids)..." >&2
  kill -HUP $pids
fi

%clean
if [ -d $RPM_BUILD_ROOT    ]; then rm -r $RPM_BUILD_ROOT    ; fi
if [ -d $RPM_BUILD_ROOT-gl ]; then rm -r $RPM_BUILD_ROOT-gl ; fi

# Files for the "xscreensaver" package:
#
%files -f exes-non-gl
%defattr(-,root,root)

%doc    README README.debugging
%dir    %{x11_prefix}/lib/xscreensaver
%config %{x11_prefix}/lib/X11/app-defaults/*
        %{x11_prefix}/man/man1/xscreensaver*
%config /etc/pam.d/*

# Files for the "xscreensaver-gl" package:
#
%{?USE_GL:%files -f exes-gl gl}
%{?USE_GL:%defattr(-,root,root)}
