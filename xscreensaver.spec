%define	name	xscreensaver
%define	version	3.21
%define	release	1
%define	serial	1
%define	prefix	/usr/X11R6

# By default, builds the basic, non-GL package.
# To build both the basic and GL-add-on packages:
#   rpm --define "USE_GL yes" ...

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
Buildroot:	/var/tmp/%{name}-%{version}-root

%description
A modular screen saver and locker for the X Window System.
Highly customizable: allows the use of any program that
can draw on the root window as a display mode.
More than 100 display modes are included in this package.
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
 ./configure --prefix=%{prefix} \
             --enable-subdir=../lib/xscreensaver \
             $RPMOPTS

make

%install

# This is a directory that "make install" won't make as needed
# (since Linux uses /etc/pam.d/* and Solaris uses /etc/pam.conf).
#
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

# This is another (since "make install" doesn't try to install
# the xscreensaver.kss file unless $KDEDIR is set.)
#
if [ -z "$KDEDIR" ]; then export KDEDIR=/usr; fi
mkdir -p $RPM_BUILD_ROOT$KDEDIR/bin

make  install_prefix=$RPM_BUILD_ROOT \
      AD_DIR=%{prefix}/lib/X11/app-defaults \
      install-strip

# Make a pair of lists, of the GL and non-GL hacks.
# Do this by parsing the output of a dummy run of "make install"
# in the hacks/ and hacks/glx/ directories.
#
list_files() {
  make -s install_prefix=$RPM_BUILD_ROOT INSTALL=true install   |
    sed -n -e 's@.* /\([^ ]*\)$@/\1@p'                          |
    sed    -e "s@^$RPM_BUILD_ROOT@@"                            \
           -e "s@/bin/..@@"                                     |
    sort
}

( cd hacks ; list_files > $RPM_BUILD_DIR/xscreensaver-%{version}/hacks-non-gl )
( cd hacks/glx ; list_files > $RPM_BUILD_DIR/xscreensaver-%{version}/hacks-gl )



# This line is redundant, except that it causes the "xscreensaver"
# executable to be installed unstripped (while all others are stripped.)
# You should install it this way so that jwz gets useful bug reports.
#
install -m 4755 driver/xscreensaver $RPM_BUILD_ROOT%{prefix}/bin

# Even if we weren't compiled with PAM support, make sure to include
# the PAM module file in the RPM anyway, just in case.
#
( cd driver ;
  make install_prefix=$RPM_BUILD_ROOT PAM_DIR=/etc/pam.d install-pam )

# This is for wmconfig, a tool that generates init files for window managers.
#
mkdir -p $RPM_BUILD_ROOT/etc/X11/wmconfig
cat > $RPM_BUILD_ROOT/etc/X11/wmconfig/xscreensaver <<EOF
xscreensaver name "xscreensaver (1min timeout)"
xscreensaver description "xscreensaver"
xscreensaver group "Amusements/Screen Savers"
xscreensaver exec "xscreensaver -timeout 1 -cycle 1 &"
EOF

# This is for the GNOME desktop:
#
mkdir -p "$RPM_BUILD_ROOT/usr/share/apps/Amusements/Screen Savers"
cat > "$RPM_BUILD_ROOT/usr/share/apps/Amusements/Screen Savers/xscreensaver.desktop" <<EOF
[Desktop Entry]
Name=xscreensaver (1min timeout)
Description=xscreensaver
Exec=xscreensaver -timeout 1 -cycle 1
EOF

# Make sure all files are readable by all, and writable only by owner.
#
chmod -R a+r,u+w,og-w $RPM_BUILD_ROOT

%clean
if [ -d $RPM_BUILD_ROOT    ]; then rm -r $RPM_BUILD_ROOT    ; fi
if [ -d $RPM_BUILD_ROOT-gl ]; then rm -r $RPM_BUILD_ROOT-gl ; fi

%files -f hacks-non-gl
%defattr(-,root,root)

# Files for the "xscreensaver" package:
#
%doc                README README.debugging
                    %{prefix}/bin/*
%dir                %{prefix}/lib/xscreensaver
%config             %{prefix}/lib/X11/app-defaults/*
                    %{prefix}/man/man1/xscreensaver*
                    /etc/pam.d/*
%config(missingok)  /usr/bin/*.kss
%config(missingok)  /etc/X11/wmconfig/*
%config(missingok)  "/usr/share/apps/Amusements/Screen Savers/*"

# Files for the "xscreensaver-gl" package:
#
%{?USE_GL:%files -f hacks-gl gl}
