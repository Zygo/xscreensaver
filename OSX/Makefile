# XScreenSaver for MacOS X, Copyright (c) 2006-2012 by Jamie Zawinski.

XCODEBUILD = /usr/bin/xcodebuild
TARGETS    = -target "All Savers"
ARCH       = -arch i386 -arch x86_64
CERT       = 'Jamie Zawinski'
CERT       = 'iPhone Developer: Jamie Zawinski (Y5M82TL69N)'
THUMBDIR   = $(HOME)/www/xscreensaver/screenshots/

default: release
all: debug release

clean:
	-rm -rf build
#	cd ..; $(XCODEBUILD) -target $(TARGETS) clean

distclean:
	-rm -f config.status config.cache config.log \
	  *.bak *.rej TAGS *~ "#"*
	-rm -rf autom4te*.cache
	-rm -rf build

distdepend:: update_plist_version

debug: distdepend
	cd ..; $(XCODEBUILD) $(ARCH) $(TARGETS) -configuration Debug   build

release:: distdepend
	cd ..; $(XCODEBUILD) $(ARCH) $(TARGETS) -configuration Release build

release:: check_versions
release:: update_thumbs
release:: sign

sign:
	@for f in build/Release/*.{saver,app} ; do \
	  codesign -vfs $(CERT) $$f ; \
	 done

check_versions:
	@\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		\
  DIR=build/Release ;							\
  RESULT=0 ;								\
  for S in $$DIR/*.{saver,app} ; do					\
   for P in $$S/Contents/Info.plist ; do				\
     V2=`perl -0000 -n -e						\
	'm@<key>CFBundleVersion</key>\s*<string>(.*?)</string>@si 	\
	 && print $$1' < $$P` ;						\
     if [ "$$V2" != "$$V" ] ; then					\
       echo "Wrong version: $$S ($$V2)" ;				\
       RESULT=1 ;							\
     fi ;								\
   done ;								\
  done ;								\
  if [ "$$RESULT" = 0 ]; then echo "Versions match ($$V2)" ; fi ;	\
  exit $$RESULT

update_thumbs::
	./update-thumbnail.pl $(THUMBDIR) build/Release/*.saver


# Arrrrgh
ios_function_tables.h::
	@./build-fntable.pl build/Debug-iphoneos/XScreenSaver.app $@


echo_tarfiles:
	@echo `find . \
	  \( \( -name '.??*' -o -name build -o -name CVS -o -name '*~*' \
	     -o -name 'jwz.*' \) \
	     -prune \) \
	  -o -type f -print \
	| sed 's@^\./@@' \
	| sort`

update_plist_version:
	@								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  T=/tmp/xs.$$$$ ;							      \
  for S in XScreenSaver.plist SaverRunner.plist iSaverRunner.plist ; do	      \
    /bin/echo -n "Updating version number in $$S to \"$$V\"... " ;	      \
    KEYS="CFBundleVersion|CFBundleShortVersionString" ;			      \
    perl -0777 -pne							      \
      "s@(<key>($$KEYS)</key>\s*<string>)[^<>]+(</string>)@\$${1}$$V\$${3}@g" \
      < $$S > $$T ;							      \
    if cmp -s $$S $$T ; then						      \
      echo "unchanged." ;						      \
    else								      \
      cat $$T > $$S ;							      \
      echo "done." ;							      \
    fi ;								      \
  done ;								      \
  rm $$T


# -format UDBZ saves 4% (~1.2 MB) over UDZO.
dmg:: distdepend check_versions
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  TMPDIR="build" ;							      \
  SRC="build/Release" ;							      \
  EXTRAS=../../xdaliclock/OSX/build/Release/*.saver ;			      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  TMPDMG="$$TMPDIR/tmp.dmg" ;						      \
  VOLNAME="XScreenSaver $$V" ;						      \
  STAGE="$$TMPDIR/dmg_stage" ;						      \
  rm -f "$$DMG" ;							      \
  rm -rf "$$STAGE" ;							      \
  echo + mkdir "$$STAGE" ;						      \
         mkdir "$$STAGE" ;						      \
									      \
  retired=`perl -0 -ne							      \
     's/\\\\\\n//g; m/^RETIRED_EXES\s*=\s*(.*)$$/m && print "$$1\n"'	      \
      ../hacks/Makefile.in ;						      \
      perl -0 -ne							      \
     's/\\\\\\n//g; m/^RETIRED_GL_EXES\s*=\s*(.*)$$/m && print "$$1\n"'	      \
      ../hacks/glx/Makefile.in ;					      \
     echo xscreensaver ;						      \
     echo savertester` ;						      \
									      \
  for f in $$SRC/*.{saver,app} $$EXTRAS ; do				      \
   ok=yes ;								      \
   ff=`echo $$f | perl -e '$$_=<>; s@^.*/(.*)\..*$$@\L$$1@; print'`;	      \
   for r in $$retired ; do						      \
    if [ "$$ff" = "$$r" ]; then ok=no ; fi ;				      \
   done ;								      \
   if [ "$$ok" = yes ]; then						      \
    echo + cp -pr "$$f" "$$STAGE/" ;					      \
           cp -pr "$$f" "$$STAGE/" ;					      \
   else									      \
    echo skipping "$$f" ;						      \
   fi ;									      \
  done ;								      \
  set -x ;								      \
  cp -p bindist.rtf "$$STAGE/ READ ME.rtf" ;				      \
  cp -p bindist-DS_Store "$$STAGE/.DS_Store" ;				      \
  cp -p XScreenSaverDMG.icns "$$STAGE/.VolumeIcon.icns" ;		      \
  /Developer/Tools/SetFile -a C "$$STAGE" ;				      \
  /Developer/Tools/SetFile -a E "$$STAGE/ READ ME.rtf" ;		      \
  seticon -d ../../xdaliclock/OSX/daliclockSaver.icns $$STAGE/DaliClock.saver;\
  hdiutil makehybrid -quiet -ov -hfs -hfs-volume-name "$$VOLNAME"	      \
    -hfs-openfolder "$$STAGE" "$$STAGE" -o "$$TMPDMG" ;			      \
  rm -rf "$$STAGE" ;							      \
  hdiutil convert -quiet -ov -format UDBZ -imagekey zlib-level=9	      \
    "$$TMPDMG" -o "$$DMG" ;						      \
  rm -f "$$TMPDMG" ;							      \
  ls -ldhgF "$$DMG"

# Adding this is cute:
#    hdiutil internet-enable -yes -quiet "$$DMG"
# but means that nobody will ever see the display settings I used!
# When finder copies the .dmg to a folder, it doesn't preserve them.
