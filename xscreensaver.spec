Name: xscreensaver
Summary: X screen saver and locker
Vendor: Jamie Zawinski <jwz@jwz.org>
Version: 3.06
Release: 1
URL: http://www.jwz.org/xscreensaver/
Source: xscreensaver-%{version}.tar.gz
Copyright: BSD
Group: X11/Utilities
Buildroot: /var/tmp/xscreensaver-root

%description
A modular screen saver and locker for the X Window System.
Highly customizable: allows the use of any program that
can draw on the root window as a display mode.
More than 80 display modes are included in this package.

%prep
%setup -q

%build
./configure --prefix=/usr/X11R6
make

%install
mkdir -p $RPM_BUILD_ROOT/usr/X11R6/bin
mkdir -p $RPM_BUILD_ROOT/usr/X11R6/man/man1
mkdir -p $RPM_BUILD_ROOT/etc/X11/wmconfig
mkdir -p $RPM_BUILD_ROOT/etc/pam.d
make  prefix=$RPM_BUILD_ROOT/usr/X11R6 \
      AD_DIR=$RPM_BUILD_ROOT/usr/X11R6/lib/X11/app-defaults \
     PAM_DIR=$RPM_BUILD_ROOT/etc/pam.d \
     install-strip

# This line is redundant, except that it causes the "xscreensaver"
# executable to be installed unstripped (while all others are stripped.)
# You should install it this way so that jwz gets useful bug reports.
#
install -m 4755 driver/xscreensaver $RPM_BUILD_ROOT/usr/X11R6/bin

# Even if we weren't compiled with PAM support, make sure to include
# the PAM module file in the RPM anyway, just in case.
#
( cd driver; make PAM_DIR=$RPM_BUILD_ROOT/etc/pam.d install-pam )

cat > $RPM_BUILD_ROOT/etc/X11/wmconfig/xscreensaver <<EOF
xscreensaver name "xscreensaver (1min timeout)"
xscreensaver description "xscreensaver"
xscreensaver group "Amusements/Screen Savers"
xscreensaver exec "xscreensaver -timeout 1 -cycle 1 &"
EOF

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/X11R6/bin/*
/usr/X11R6/lib/X11/app-defaults/*
/usr/X11R6/man/man1/*
%config(missingok) /etc/X11/wmconfig/*
%config(missingok) /etc/pam.d/*
