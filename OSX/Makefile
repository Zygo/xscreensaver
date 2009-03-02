# XScreenSaver for MacOS X, Copyright (c) 2006 by Jamie Zawinski.

XCODE_TARGET = "All Savers"

default: release
all: debug release

clean:
	-rm -rf build
#	cd ..; xcodebuild -target $(XCODE_TARGET) clean

distclean:
	-rm -f config.status config.cache config.log \
	  *.bak *.rej TAGS *~ "#"*
	-rm -rf autom4te*.cache
	-rm -rf build

distdepend:: update_plist_version

debug: distdepend
	cd ..; xcodebuild -target $(XCODE_TARGET) -configuration Debug   build

release:: distdepend
	cd ..; xcodebuild -target $(XCODE_TARGET) -configuration Release build

release:: check_versions

release:: sign

sign:
	codesign -vfs 'Jamie Zawinski' build/Release/*.saver

check_versions:
	@\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		\
  DIR=build/Release ;							\
  RESULT=0 ;								\
  for S in $$DIR/*.saver ; do						\
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
  for S in XScreenSaver.plist SaverTester.plist ; do			      \
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
  echo + cp -pr "$$SRC/"\*.saver "$$STAGE" ;				      \
         cp -pr "$$SRC/"*.saver  "$$STAGE" ;				      \
  set -x ;								      \
  cp -p bindist.rtf "$$STAGE/ READ ME.rtf" ;				      \
  cp -p bindist-DS_Store "$$STAGE/.DS_Store" ;				      \
  cp -p XScreenSaverDMG.icns "$$STAGE/.VolumeIcon.icns" ;		      \
  /Developer/Tools/SetFile -a C "$$STAGE" ;				      \
  /Developer/Tools/SetFile -a E "$$STAGE/ READ ME.rtf" ;		      \
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
