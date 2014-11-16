# XScreenSaver for Android

default: all

all:
	@echo Nope. ; exit 1

clean::

distclean::

distdepend::

# Todo:
# check_versions:
# update_thumbs:


# Should probably exclude "gen"

echo_tarfiles:
	@echo `find . \
	  \( \( -name '.??*' -o -name build -o -name '*~*' \) \
	     -prune \) \
	  -o \( -type f -o -type l \) -print \
	| sed 's@^\./@@' \
	| sort`

