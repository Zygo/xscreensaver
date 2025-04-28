%define	name xscreensaver
%define	version 6.10

Summary:	X screen saver and locker
Name:		%{name}
Version:	%{version}
Release:	1
Epoch:		1
License:	BSD
Group:		Amusements/Graphics
URL:		https://www.jwz.org/xscreensaver/
Source0:	https://www.jwz.org/xscreensaver/xscreensaver-%{version}.tar.gz
Vendor:		Jamie Zawinski <jwz@jwz.org>
Buildroot:	%{_tmppath}/%{name}-root

BuildRequires:	perl
BuildRequires:	pkgconfig
BuildRequires:	desktop-file-utils
BuildRequires:	intltool
BuildRequires:	libX11-devel
BuildRequires:	libXext-devel
BuildRequires:	libXft-devel
BuildRequires:	libXi-devel
BuildRequires:	libXinerama-devel
BuildRequires:	libXrandr-devel
BuildRequires:	libXt-devel
BuildRequires:	libXxf86vm-devel
BuildRequires:	xorg-x11-proto-devel
BuildRequires:	mesa-libGL-devel
BuildRequires:	mesa-libGLU-devel
BuildRequires:	libgle-devel
BuildRequires:	pam-devel
BuildRequires:	systemd-devel
BuildRequires:	gtk3-devel
BuildRequires:	gdk-pixbuf2-devel
BuildRequires:	libglade2
BuildRequires:	libxml2-devel
BuildRequires:	gettext-devel
BuildRequires:	libjpeg-turbo-devel

Requires: SysVinit
Requires: pam
Requires: /etc/pam.d/system-auth
Requires: htmlview
Requires: desktop-backgrounds-basic
Requires: xdg-utils
Requires: systemd-libs

Provides: xscreensaver

Obsoletes: xscreensaver-base
Obsoletes: xscreensaver-common
Obsoletes: xscreensaver-data
Obsoletes: xscreensaver-data-extra
Obsoletes: xscreensaver-extra
Obsoletes: xscreensaver-extra-base
Obsoletes: xscreensaver-extra-gss
Obsoletes: xscreensaver-extras
Obsoletes: xscreensaver-extras-base
Obsoletes: xscreensaver-extras-gss
Obsoletes: xscreensaver-extrusion
Obsoletes: xscreensaver-gl
Obsoletes: xscreensaver-gl-base
Obsoletes: xscreensaver-gl-extra
Obsoletes: xscreensaver-gl-extra-gss
Obsoletes: xscreensaver-gl-extras
Obsoletes: xscreensaver-gl-extras-gss
Obsoletes: xscreensaver-lang
Obsoletes: xscreensaver-matrix
Obsoletes: xscreensaver-bsod
Obsoletes: xscreensaver-webcollage
Obsoletes: xscreensaver-screensaver-bsod
Obsoletes: xscreensaver-screensaver-webcollage

%description
A modular screen saver and locker for the X Window System.
More than 260 display modes are included in this package.

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

CONFIG_OPTS="--prefix=/usr"

ln -s ../configure .
%configure $CONFIG_OPTS
rm -f configure

make

%install
archdir=`./config.guess`
cd $archdir

rm -rf ${RPM_BUILD_ROOT}
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

make install_prefix=$RPM_BUILD_ROOT install

dd=%{_builddir}/%{name}-%{version}

make -s install_prefix=${RPM_BUILD_ROOT} \
	INSTALL=true \
	UPDATE_ICON_CACHE='true false' \
        install |
grep -v '^true false' |
sed -n -e 's@.* \(/[^ ]*\)$@\1@p' |
sed    -e "s@^${RPM_BUILD_ROOT}@@" \
       -e "s@/[a-z][a-z]*/\.\./@/@" |
sed    -e 's@\(.*/man/.*\)@\1\*@' |
sort | uniq |
sed    -e 's@\(.*/app-defaults/\)@%config \1@' \
       -e 's@\(.*/pam\.d/\)@%config(missingok) \1@' \
> $dd/all.files

%find_lang %{name}
cat %{name}.lang >> $dd/all.files

chmod -R a+r,u+w,og-w ${RPM_BUILD_ROOT}

%clean
rm -rf ${RPM_BUILD_ROOT}

%files -f all.files
%defattr(-,root,root)

%changelog
* Mon Nov 16 1998 jwz
- Created.
* Mon Jul 31 2023 jwz
- Splitting this into multiple packages is a support nightmare, please don't.
