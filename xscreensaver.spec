%define	name xscreensaver
%define	version 5.02

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
Summary(fr): Une installation minimale de xscreensaver.
Group: Amusements/Graphics
BuildRequires:	bc
BuildRequires:	gettext
BuildRequires:	pam-devel
BuildRequires:	gtk2-devel
BuildRequires:	desktop-file-utils
# Red Hat:
BuildRequires:	xorg-x11-devel
BuildRequires:	libglade2-devel
# Mandrake:
#BuildRequires:	libxorg-x11-devel
#BuildRequires:	libglade2.0_0-devel
Requires: SysVinit
Requires: /etc/pam.d/system-auth
Requires: htmlview
Requires: desktop-backgrounds-basic
Provides: xscreensaver
Provides: xscreensaver-base
Obsoletes: xscreensaver

%package extras
Summary: An enhanced set of screensavers.
Summary(fr): Un jeu étendu d'économiseurs d'écran.
Group: Amusements/Graphics
Requires: xscreensaver-base

%package gl-extras
Summary: An enhanced set of screensavers that require OpenGL.
Summary(fr): Un jeu étendu d'économiseurs d'écran qui nécessitent OpenGL.
Group: Amusements/Graphics
Requires: xscreensaver-base
Obsoletes: xscreensaver-gl

%description
A modular screen saver and locker for the X Window System.
More than 200 display modes are included in this package.

%description -l fr
Un économiseur d'écran modulaire pour le système X Window.
Plus de 200 modes d'affichages sont inclus dans ce paquet.

%description base
A modular screen saver and locker for the X Window System.
This package contains the bare minimum needed to blank and
lock your screen.  The graphical display modes are the
"xscreensaver-extras" and "xscreensaver-gl-extras" packages.

%description -l fr base 
Un économiseur d'écran modulaire pour le système X Window.
Ce paquet contient le minimum vital pour éteindre et verouiller
votre écran. Les modes d'affichages graphiques sont inclus
dans les paquets "xscreensaver-extras" et "xscreensaver-gl-extras".

%description extras
A modular screen saver and locker for the X Window System.
This package contains a variety of graphical screen savers for
your mind-numbing, ambition-eroding, time-wasting, hypnotized
viewing pleasure.

%description -l fr extras
Un économiseur d'écran modulaire pour le système X Window.
Ce paquet contient une pléthore d'économiseurs d'écran graphiques
pour votre plaisir des yeux.

%description gl-extras
A modular screen saver and locker for the X Window System.
This package contains a variety of OpenGL-based (3D) screen
savers for your mind-numbing, ambition-eroding, time-wasting,
hypnotized viewing pleasure.

%description -l fr gl-extras
Un économiseur d'écran modulaire pour le système X Window.
Ce paquet contient une pléthore d'économiseurs d'écran basés sur OpenGL (3D)
pour votre plaisir des yeux.

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
#( cd po        ; list_files install ) >> $dd/base.files

%find_lang %{name}
cat %{name}.lang >> $dd/base.files


# Make sure all files are readable by all, and writable only by owner.
#
chmod -R a+r,u+w,og-w ${RPM_BUILD_ROOT}

%clean
rm -rf ${RPM_BUILD_ROOT}

%post base
# This part runs on the end user's system, when the RPM is installed.

# This will cause the screen to unlock, which annoys people.  So, nevermind:
# people will just have to remember to re-launch it themselves, like they
# have to do with any other daemon they've upgraded.
#
#pids=`/sbin/pidof xscreensaver`
#if [ -n "$pids" ]; then
#  echo "sending SIGHUP to running xscreensaver ($pids)..." >&2
#  kill -HUP $pids
#fi

%files -f base.files base
%defattr(-,root,root)

%files -f extras.files extras
%defattr(-,root,root)

%files -f gl-extras.files gl-extras
%defattr(-,root,root)

%changelog
* Fri Nov  4  2005 Eric Lassauge <lassauge@users.sf.net>
- Updated french translations
