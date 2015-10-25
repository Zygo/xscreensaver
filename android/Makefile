# XScreenSaver for Android


TARGET		= project

default: apk

clean:: gradle_clean

distclean::

distdepend::

all:
	cd project ; ./gradlew assembleDebug

# TODO:
# check_versions:
# update_thumbs:

echo_tarfiles:
	@echo `find \
		Makefile \
		README \
		*.[ch] *.pl \
		project \
		project/xscreensaver/res/drawable/*.png \
		project/xscreensaver/res/values/settings.xml \
		project/xscreensaver/res/values/strings.xml \
	  \( \( -name '.??*' -o -name build -o -name gen \
                -o -name libs -o -name obj -o -name '*~*' \) \
	     -prune \) \
	  -o \( -type f -o -type l \) -print \
	| sed 's@^\./@@' \
	| sort`

apk: apk_raw_debug

apk_raw_debug:
		cd $(TARGET); ./gradlew assembleDebug

gradle_clean:
		-cd $(TARGET); ./gradlew clean

