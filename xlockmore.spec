# xlockmore.spec -- RPM spec file for xlockmore
#
# Xlock (c) 2001 David Bagley <bagleyd@tux.org>
#                Eric Lassauge <lassauge AT users.sourceforge.net>
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in
# supporting documentation.
#
# This file is provided AS IS with no warranties of any kind.  The author
# shall have no liability with respect to the infringement of copyrights,
# trade secrets or any patents by this file or any part thereof.  In no
# event will the author be liable for any lost revenue or profits or
# other special, indirect and consequential damages.
#
# mailto:bagleyd@tux.org
# http://www.tux.org/~bagleyd/xlockmore

%define	name		xlockmore
%define	stableversion	5.15
%define	release		1
%define	serial		1
# Comment quality for stable release
#%define quality		ALPHA
#%define quality		BETA
%define	x11_prefix	/usr/X11R6
%define	gnome_prefix	/usr

%define gnome_datadir	%{gnome_prefix}/share
%define gnome_appsdir	%{gnome_datadir}/gnome/apps/Utilities
%define xlock_datadir	%{x11_prefix}/lib/X11/xlock

%{?quality:%define version	%{stableversion}%{quality}}
%{!?quality:%define version	%{stableversion}}

# By default, builds everything, including GL modes

Summary: An X terminal locking program.
Summary(de):	Terminal-Sperrprogramm für X mit vielen Bildschirmschonern
Summary(fr):	Verrouillage de terminaux X
Summary(tr):	X terminal kilitleme programý
Name: %{name}
Version: %{version}
Release: %{release}
Serial: %{serial}
Copyright: BSD
Group: Amusements/Graphics
Url: http://www.tux.org/~bagleyd/xlockmore.html
Source: ftp://ftp.tux.org/pub/tux/bagleyd/xlockmore/%{name}-%{version}.tar.bz2
#Patch0: %{name}-%{version}-patch
BuildPrereq: esound-devel, audiofile-devel gltt-devel Mesa-devel freetype-devel
Requires: pam >= 0.74, esound, audiofile, /usr/games/fortune, freetype, gltt, Mesa
Buildroot: %{_tmppath}/%{name}-%{version}-root
Vendor: David Bagley <bagleyd@tux.org>
Packager: Eric Lassauge <lassauge@users.sourceforge.net>
# For a beautiful icon in gnorpm do :
# convert /usr/share/pixmaps/gnome-lockscreen.png /usr/src/redhat/SOURCES/xlock.xpm
# and uncomment the following line.
# Icon: xlock.xpm

%description
The xlockmore utility is an enhanced version of the standard xlock
program, which allows you to lock an X session so that other users
can't access it.  Xlockmore runs a provided screensaver until you type
in your password.

Install the xlockmore package if you need a locking program to secure
X sessions.

%description -l de
Eine erweiterte Version des Standardprogramms xlock, mit dem Sie eine
X-Sitzung für andere Benutzer sperren können, wenn Sie sich nicht an
Ihrem Rechner befinden. Es führt einen von vielen Bildschirmschonern
aus und wartet auf die Eingabe eines Paßworts, bevor es die Sitzung
freigibt und Sie an Ihre X-Programme läßt.

%description -l fr
Version améliorée du programme xlock standard et qui permet d'empêcher
les autres utilisateurs d'aller dans une session X pendant que vous
êtes éloigné de la machine. Il lance l'un des nombreux économiseurs
d'écran et attend que vous tapiez votre mot de passe, débloquant la
session et vous redonnant accès à vos programmes X.

%description -l tr
Standart xlock programýnýn bir miktar geliþtirilmiþ sürümü. xlockmore
ile makinanýn baþýndan ayrýlmanýz gerektiði zaman ekraný
kilitleyebilir, böylece istenmeyen misafirlerin sistemi
kurcalamalarýný önleyebilirsiniz.

%prep
%setup -q
#%patch0 -p1

%build
autoconf
# Feel free to change default options !
CFLAGS="$MY_CFLAGS" CXXFLAGS="$MY_CFLAGS" ./configure \
	--prefix=%{x11_prefix} \
	--disable-allow-root --disable-bomb --without-nas --without-editres \
	--with-esound --enable-vtlock --enable-pam --enable-unstable \
	--enable-orig-xpm-patch $MY_CONFFLAGS

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

# make xglock too, force using the same datadir
(cd xglock
make xglock datadir=%{x11_prefix}/lib/X11)

%install

rm -rf $RPM_BUILD_ROOT

# Most xlockmore executables go in the X bin directory (/usr/X11R6/bin/)
# but some stuff go in the GNOME share directory instead (/usr/share/gnome/).
# and the misc datas go into xlock share directory (/usr/X11R6/lib/X11/xlock/).
#
mkdir -p $RPM_BUILD_ROOT%{gnome_appsdir}
# This is a directory that "make install" won't make as needed
# (since Linux uses /etc/pam.d/* and Solaris uses /etc/pam.conf).
#
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

make install prefix=$RPM_BUILD_ROOT%{x11_prefix} xapploaddir=$RPM_BUILD_ROOT%{x11_prefix}/lib/X11/app-defaults/ INSTPGMFLAGS="-s"
install -m 644 etc/xlock.pamd $RPM_BUILD_ROOT/etc/pam.d/xlock

install etc/xlockmore.desktop $RPM_BUILD_ROOT%{gnome_appsdir}

# xlock shared data directory 
mkdir -p $RPM_BUILD_ROOT%{xlock_datadir}
# You can put all TrueType fonts here
mkdir -p $RPM_BUILD_ROOT%{xlock_datadir}/fonts/
mkdir -p $RPM_BUILD_ROOT%{xlock_datadir}/sounds/
cp sounds/*.au $RPM_BUILD_ROOT%{xlock_datadir}/sounds/

(cd etc
sed -e 's|/usr/X11/bin/wish|/usr/bin/wish|g' xlock.tcl > xlock.tcl.new
mv xlock.tcl.new xlock.tcl
chmod +x xlock.tcl
install -m 755 xlock.tcl $RPM_BUILD_ROOT%{x11_prefix}/bin/xlock.tcl
)

# strange install options will install 'xglockrc' in xlock share directory
(cd xglock
make install_xglock prefix=$RPM_BUILD_ROOT%{x11_prefix} datadir=$RPM_BUILD_ROOT%{x11_prefix}/lib/X11)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%doc 			README docs/*
%attr(644,root,root) 	%config %verify(not size mtime md5) /etc/pam.d/xlock
%attr(4111,root,root)	%{x11_prefix}/bin/xlock
%attr(755,root,root)	%{x11_prefix}/bin/xmlock
%attr(755,root,root)	%{x11_prefix}/bin/xglock
%attr(755,root,root)	%{x11_prefix}/bin/xlock.tcl
%config			%{x11_prefix}/man/man1/*lock.*
%config 		%{x11_prefix}/lib/X11/app-defaults/*
%{xlock_datadir}/*
%{gnome_appsdir}/%{name}.desktop

%changelog
* Thu Nov 22 2001 Eric Lassauge <lassauge AT users.sourceforge.net>
- added quality and stableversion defines for STABLE/ALPHA/BETA versions 
- modified configure flags: use MY_CFLAGS and MY_CONFFLAGS if needed
* Thu Oct 25 2001 Eric Lassauge <lassauge AT users.sourceforge.net>
- created xlock_datadir define
- removed patch commands as the patch file is now included
* Thu Oct 18 2001 David Bagley <bagleyd@tux.org>
- Took over ownership  :)
* Wed Oct 17 2001 Eric Lassauge <lassauge AT users.sourceforge.net>
- Created inspired by old Redhat version and xscreensaver spec file
