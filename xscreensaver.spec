Name: xscreensaver
Summary: X screen saver and locker
Vendor: Jamie Zawinski <jwz@jwz.org>
Version: 3.10
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
More than 90 display modes are included in this package.

%prep

%setup -q

%build

./configure --prefix=/usr/X11R6 \
     --enable-subdir=/usr/X11R6/lib/xscreensaver
make

%install

# This is the only directory that "make install" won't make as needed
# (since Linux uses /etc/pam.d/* and Solaris uses /etc/pam.conf).
#
mkdir -p $RPM_BUILD_ROOT/etc/pam.d

make  prefix=$RPM_BUILD_ROOT/usr/X11R6 \
      AD_DIR=$RPM_BUILD_ROOT/usr/X11R6/lib/X11/app-defaults \
     HACKDIR=$RPM_BUILD_ROOT/usr/X11R6/lib/xscreensaver \
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
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

%doc                README README.debugging
                    /usr/X11R6/bin/*
                    /usr/X11R6/lib/xscreensaver/*
%config             /usr/X11R6/lib/X11/app-defaults/*
                    /usr/X11R6/man/man1/*
                    /etc/pam.d/*
%config(missingok)  /etc/X11/wmconfig/*
%config(missingok)  "/usr/share/apps/Amusements/Screen Savers/*"
