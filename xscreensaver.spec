%define	name xscreensaver
%define	version 4.21

Summary:	X screen saver and locker
Name:		%{name}
Version:	%{version}
Release:	1
Epoch:		1
License:	BSD
Group:		Amusements/Graphics
URL:		http://www.jwz.org/xscreensaver/
Source0:	http://www.jwz.org/xscreensaver/xscreensaver-%{version}.tar.gz
Vendor:		Jamie Zawinski <jwz@jwz.org>
Buildroot:	%{_tmppath}/%{name}-root

%package base
Summary: A minimal installation of xscreensaver.
Group: Amusements/Graphics
BuildPrereq: bc, pam-devel, xorg-x11-devel
BuildPrereq: gtk2-devel libglade2-devel
Requires: /etc/pam.d/system-auth, htmlview, desktop-backgrounds-basic
Provides: xscreensaver
Provides: xscreensaver-base
Obsoletes: xscreensaver

%package extras
Summary: An enhanced set of screensavers.
Group: Amusements/Graphics
Requires: xscreensaver-base

%package gl-extras
Summary: An enhanced set of screensavers that require OpenGL.
Group: Amusements/Graphics
Requires: xscreensaver-base
Obsoletes: xscreensaver-gl

%description
A modular screen saver and locker for the X Window System.
More than 190 display modes are included in this package.

%description base
A modular screen saver and locker for the X Window System.
This package contains the bare minimum needed to blank and
lock your screen.  The graphical display modes are the
"xscreensaver-extras" and "xscreensaver-gl-extras" packages.

%description extras
A modular screen saver and locker for the X Window System.
This package contains a variety of graphical screen savers for
your mind-numbing, ambition-eroding, time-wasting, hypnotized
viewing pleasure.

%description gl-extras
A modular screen saver and locker for the X Window System.
This package contains a variety of OpenGL-based (3D) screen
savers for your mind-numbing, ambition-eroding, time-wasting,
hypnotized viewing pleasure.

%prep
%setup -q

if [ -x %{_datadir}/libtool/config.guess ]; then
  # use system-wide copy
  cp -p %{_datadir}/libtool/config.{sub,guess} .
fi

%build
archdir=`./config.guess`
mkdir $archdir
cd $archdir

export CFLAGS="${CFLAGS:-${RPM_OPT_FLAGS}}"

CONFIG_OPTS="--prefix=/usr --with-pam --without-shadow --without-kerberos"

# Red Hat doesn't like this:
CONFIG_OPTS="$CONFIG_OPTS --with-setuid-hacks"

# This is flaky:
# CONFIG_OPTS="$CONFIG_OPTS --with-login-manager"

ln -s ../configure .
%configure $CONFIG_OPTS
rm -f configure

make

%install
archdir=`./config.guess`
cd $archdir

rm -rf ${RPM_BUILD_ROOT}

# We have to make sure these directories exist,
# or nothing will be installed into them.
#
mkdir -p $RPM_BUILD_ROOT%{_bindir}                      \
         $RPM_BUILD_ROOT%{_datadir}/xscreensaver        \
         $RPM_BUILD_ROOT%{_libexecdir}/xscreensaver     \
         $RPM_BUILD_ROOT%{_mandir}/man1/xscreensaver    \
         $RPM_BUILD_ROOT/etc/pam.d

make install_prefix=$RPM_BUILD_ROOT install

desktop-file-install --vendor gnome --delete-original                         \
  --dir $RPM_BUILD_ROOT%{_datadir}/applications                               \
  $RPM_BUILD_ROOT%{_datadir}/applications/*.desktop

# This function prints a list of things that get installed.
# It does this by parsing the output of a dummy run of "make install".
#
list_files() {
  make -s install_prefix=${RPM_BUILD_ROOT} INSTALL=true "$@"	\
   | sed -n -e 's@.* \(/[^ ]*\)$@\1@p'				\
   | sed    -e "s@^${RPM_BUILD_ROOT}@@"				\
	    -e "s@/[a-z][a-z]*/\.\./@/@"			\
   | sed    -e 's@\(.*/man/.*\)@\1\*@'				\
   | sed    -e 's@\(.*/app-defaults/\)@%config \1@'		\
	    -e 's@\(.*/pam\.d/\)@%config(missingok) \1@'	\
   | sort
}

# Generate three lists of files for the three packages.
#
dd=%{_builddir}/%{name}-%{version}
(  cd hacks     ; list_files install ) >  $dd/extras.files
(  cd hacks/glx ; list_files install ) >  $dd/gl-extras.files
(  cd driver    ; list_files install ) >  $dd/base.files
(  cd po        ; list_files install ) >> $dd/base.files

# jwz: I get "find-lang.sh: No translations found for xscreensaver" on FC3
#%find_lang %{name}
#cat %{name}.lang >> $dd/base.files

# Make sure all files are readable by all, and writable only by owner.
#
chmod -R a+r,u+w,og-w ${RPM_BUILD_ROOT}

%clean
rm -rf ${RPM_BUILD_ROOT}

%post base
# This part runs on the end user's system, when the RPM is installed.

pids=`/sbin/pidof xscreensaver`
if [ -n "$pids" ]; then
  echo "sending SIGHUP to running xscreensaver ($pids)..." >&2
  kill -HUP $pids
fi

%files -f base.files base
%defattr(-,root,root)

%files -f extras.files extras
%defattr(-,root,root)

%files -f gl-extras.files gl-extras
%defattr(-,root,root)
