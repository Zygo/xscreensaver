# XScreenSaver for MacOS X, Copyright © 2006-2023 Jamie Zawinski.

XCODE_APP  = /Applications/Xcode.app
TARGETS    = All Savers
ARCH       = -destination platform=macOS
CERT	   = 'Developer ID Installer: Jamie Zawinski (4627ATJELP)'
CERT2	   = 'Developer ID Application: Jamie Zawinski (4627ATJELP)'
PKGID	   = org.jwz.xscreensaver
THUMBDIR   = build/screenshots
XCODEBUILD = $(XCODE_APP)/Contents/Developer/usr/bin/xcodebuild
SETFILE    = $(XCODE_APP)/Contents/Developer/Tools/SetFile
SETICON    = ./seticon.pl

# Using $(MAKE) directly means the shell executes things even with "make -n"
MAKE2 = $(MAKE)


default: release
all: debug release

clean:
	-rm -rf build
#	$(XCODEBUILD) -target "$(TARGETS)" clean

distclean:
	-rm -f config.status config.cache config.log \
	  *.bak *.rej TAGS *~ "#"*
	-rm -rf autom4te*.cache
	-rm -rf build
#	-rm -rf Sparkle.framework

distdepend:: Sparkle.framework
distdepend:: update_plist_version

_debug: distdepend
	$(XCODEBUILD) $(ARCH) -scheme "$(TARGETS)" -configuration Debug   build

_release:: distdepend
	$(XCODEBUILD) $(ARCH) -scheme "$(TARGETS)" -configuration Release build

_release:: check_versions

debug::
	$(MAKE2) _debug > /dev/null

release::
	$(MAKE2) _release > /dev/null


Sparkle.framework:
	rm -rf bin sparkle-bin
	tar -vxjf ../archive/Sparkle-1.27.0.tar.xz \
	  --exclude CHANGELOG \
	  --exclude LICENSE \
	  --exclude SampleAppcast.xml \
	  --exclude Sparkle.framework.dSYM \
	  --exclude Sparkle\ Test\ App\*
	mv bin sparkle-bin

# Download and resize images from jwz.org.
# Largely duplicated in android/Makefile.
# This saves us having to include 4MB of images in the tar file that
# will only be used by a vast minority of people building from source.
# update-info-plist.pl runs "make" to do this as needed.

BASE = xscreensaver/screenshots/
URL = https://cdn.jwz.org/$(BASE)

# I find wget easier to deal with, but curl is usually installed by default.
#WGET = wget -q -U xscreensaver-build-osx --content-on-error=0 -O-
WGET = curl -sL -A xscreensaver-build-osx -f

# ImageMagick isn't installed by default, but neither is anything similar.
#
# Apple savers have "thumbnail.png" at 90x58 and "thumbnail@2x.png" at 180x116.
# System Preferences stretches those to fill a bordered 170x105 frame.
#
# This recipe takes our source images (720p) and fits them into a 180x116 box,
# cropping either horizontally or vertically as needed, with rounded corners
# in the alpha mask.
#
THUMB_SIZE=180x116
THUMB_CURVE=15
CVT  = -thumbnail $(THUMB_SIZE)'^' -gravity center -extent $(THUMB_SIZE) \
     \( +clone  -alpha extract \
        -draw \
          'fill black polygon 0,0 0,$(THUMB_CURVE) $(THUMB_CURVE),0 \
           fill white circle $(THUMB_CURVE),$(THUMB_CURVE) $(THUMB_CURVE),0' \
        \( +clone -flip \) -compose Multiply -composite \
        \( +clone -flop \) -compose Multiply -composite \
     \) -alpha off -compose CopyOpacity -composite \
    -colorspace sRGB \
    -strip \
    -quality 95 \
    +dither -colors 128

$(THUMBDIR)/%.png:
	@\
	FILE1=`echo "$@" | sed 's!^.*/\([^/]*\)\.png$$!\1.jpg!'` ;	\
	FILE2="$@" ;							\
	TMP="$$FILE2".tmp ;						\
	URL="$(URL)$$FILE1" ;						\
	if [ ! -d $(THUMBDIR) ]; then mkdir -p $(THUMBDIR) ; fi ;	\
	rm -f "$$FILE2" "$$TMP" ;					\
	set +e ;							\
	if [ -f "$$HOME/www/$(BASE)/$$FILE1" ]; then			\
	  cp -p "$$HOME/www/$(BASE)/$$FILE1" "$$TMP" ;			\
	else								\
	  echo "downloading $$URL..." ;					\
	  $(WGET) "$$URL" > "$$TMP" ;					\
	  if [ ! -s "$$TMP" ]; then					\
	    rm -f "$$TMP" ;						\
	    echo "failed: $$URL" ;					\
	    exit 1 ;							\
	  fi ;								\
	fi ;								\
	rm -f "$$FILE2" ;						\
	convert jpg:- $(CVT) "$$FILE2" < "$$TMP" ;			\
	if [ ! -s "$$FILE2" ]; then					\
	  echo "$$FILE2 failed" >&2 ;					\
	  rm -f "$$FILE2" "$$TMP" ;					\
	  exit 1 ;							\
	else								\
	  rm -f "$$TMP" ;						\
	fi

$(THUMBDIR)/randomxscreensaver.png:
	@\
	SRC="iSaverRunner1024.png" ;					\
	CVT2="-gravity center -background #249ff5 -extent 200%x100%" ;	\
	FILE1=`echo "$@" | sed 's!^.*/\([^/]*\)\.png$$!\1.jpg!'` ;	\
	FILE2="$@" ;							\
	TMP="$$FILE2".tmp.png ;						\
	if [ ! -d $(THUMBDIR) ]; then mkdir -p $(THUMBDIR) ; fi ;	\
	rm -f "$$FILE2" "$$TMP" ;					\
	set +e ;							\
	convert "$$SRC" $$CVT2 "$$TMP" ;				\
	convert "$$TMP" $(CVT) "$$FILE2" ;				\
	if [ ! -s "$$FILE2" ]; then					\
	  echo "$$FILE2 failed" >&2 ;					\
	  rm -f "$$FILE2" "$$TMP" ;					\
	  exit 1 ;							\
	fi


check_versions:
	@\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	\
  DIR=build/Release ;							\
  RESULT=0 ;								\
  for S in $$DIR/*.{saver,app} ; do					\
   for P in $$S/Contents/Info.plist ; do				\
     V2=`plutil -convert xml1 -o - "$$P" |				\
	 perl -0000 -n -e						\
	'm@<key>CFBundleVersion</key>\s*<string>(.*?)</string>@si 	\
	 && print $$1'` ;						\
     if [ "$$V2" != "$$V" ] ; then					\
       echo "Wrong version: $$S ($$V2)" ;				\
       RESULT=1 ;							\
     fi ;								\
   done ;								\
  done ;								\
  if [ "$$RESULT" = 0 ]; then echo "Versions match ($$V2)" ; fi ;	\
  exit $$RESULT


# Arrrrgh
ios-function-table.m::
	@./build-fntable.pl build/Debug-iphonesimulator/XScreenSaver.app $@


list_tarfiles:
	@( find .							\
	  \( \( -name '.??*' -o -name build -o -name CVS -o -name '*~*'	\
	     -o -name 'jwz.*' -o -name 'Screen Savers'			\
             -o -name xscreensaver.xcodeproj \)				\
	     -prune \)							\
	  -o \( -type f -o -type l \) -print ;				\
	  echo xscreensaver.xcodeproj/project.pbxproj ;			\
	  find xscreensaver.xcodeproj/xcshareddata -type f ;		\
	 )								\
	| sed 's@^\./@@'						\
	| sort

update_plist_version:
	@								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	      \
  T=/tmp/xs.$$$$ ;							      \
  for S in *.plist ; do							      \
    /bin/echo -n "Updating version number in $$S to \"$$V\"... " ;	      \
    KEYS="CFBundleVersion|CFBundleShortVersionString|CFBundleLongVersionString|CFBundleGetInfoString|NSHumanReadableCopyright" ; \
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


updates.xml::
	./updates.pl xscreensaver ../README ../archive ~/www/xscreensaver
	@$(MAKE2) test_sig

# This only knows how to test the DSA sig, which we no longer use.
# I don't know how to test the EdDSA sig.
test_sig::
	@								    \
  U=../utils/version.h ;						    \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ;]*\).*/\1/p' < $$U | head -1` ;	    \
  BASE="xscreensaver-$$V" ;						    \
  OUTDIR="../archive" ;							    \
  DMG="$$OUTDIR/$$BASE.dmg" ;						    \
  SIG1=`sed -n 's/^.*dsaSignature="\(.*\)".*/\1/p' updates.xml | head -1` ; \
  SIG2=`sed -n  's/^.*edSignature="\(.*\)".*/\1/p' updates.xml | head -1` ; \
  PUB1="sparkle_dsa_pub.pem" ;						    \
  NN="t.$$$$" ;								    \
  SIGB=/tmp/$$NN.sig ;							    \
  HASH=/tmp/$$NN.hash ;							    \
  rm -f "$$SIGB" "$$HASH" ;						    \
  if ( echo "$$SIG1" | grep -qi ERROR ); then				    \
    echo "No DSA signature in updates.xml" >&2 ; exit 1 ;		    \
  fi ;									    \
  if ( echo "$$SIG2" | grep -qi ERROR ); then				    \
    echo "No ED signature in updates.xml" >&2 ; exit 1 ;		    \
  fi ;									    \
									    \
  set -e ;								    \
  echo "$$SIG1 " | base64 -D  > "$$SIGB" ;				    \
  for OPENSSL in /usr/bin/openssl /opt/local/bin/openssl ; do		    \
    if [ -f "$$PUB1" ]; then						    \
      $$OPENSSL dgst -sha1 -binary  < "$$DMG"  > "$$HASH" ;		    \
      /bin/echo -n "DSA $$OPENSSL    	`$$OPENSSL version`:	" ;	    \
      $$OPENSSL dgst -sha1 -verify "$$PUB1" -signature "$$SIGB" "$$HASH" ;  \
    fi ;								    \
  done ;								    \
  echo "Warning: not verifying edSignature because I don't know how..." ;   \
									    \
  rm -f "$$SIGB" "$$HASH" ;						    \


# As of Xcode 13, I think it's no longer possible to check signatures on the
# saver bundles: they must be signed with "Developer ID Application" but as
# the individual savers are not notarized, "spctl --assess --type install"
# reports "rejected, Unnotarized Developer ID".
#
# Also, after running "productsign" on the pkg, "codesign" reports that it
# is unsigned, so WTF?
#
# "codesign -vv --deep --strict" complains: "resource fork, Finder
# information, or similar detritus not allowed".  Maybe it is complaining
# about the "Icon\r" file?

build/Release/installer.pkg: installer.rtf installer.xml installer.sh installer.png ../utils/version.h
	@\
  set -e ;								\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	\
									\
  DIST="installer.xml" ;						\
  STAGE="build/Release/pkg_stage" ;					\
  FINAL="$@" ;								\
  UNSIGNED="$$STAGE/contents.pkg" ;					\
  PRODUCT="$$STAGE/product_unsigned.pkg" ;				\
  SCRIPTS="$$STAGE/scripts" ;						\
  RES="$$STAGE/resources" ;						\
									\
  set -x ;								\
  rm -rf "$$STAGE" ;							\
  mkdir -p "$$SCRIPTS" "$$RES" ;					\
									\
  cp -p installer.sh "$$SCRIPTS/preinstall" ;				\
  cp -p installer.png "$$RES/background.png" ;				\
  cp -p installer.rtf "$$RES/welcome.rtf" ;				\
									\
  pkgbuild --identifier "$(PKGID)" --version "$$V"			\
	   --scripts "$$SCRIPTS" --nopayload "$$UNSIGNED" ;		\
									\
  productbuild --distribution "$$DIST" --resources "$$RES"		\
	       --package-path "$$STAGE" --version "$$V" "$$PRODUCT" ;	\
									\
  productsign --sign $(CERT) "$$PRODUCT" "$$FINAL" ;			\
									\
  if false; then							\
    spctl --assess --verbose=4 --type install "$$FINAL" ;		\
  fi ;									\
									\
  rm -rf "$$STAGE" ;							\


dmg:: distdepend check_versions
dmg:: build/Release/installer.pkg
dmg:: _dmg notarize staple updates.xml

_dmg::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	      \
  TMPDIR="build" ;							      \
  SRC="build/Release" ;							      \
  EXTRAS=../../xdaliclock/OSX/build/Release/*.saver ;			      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  TMPDMG="$$TMPDIR/tmp.dmg" ;						      \
  VOLNAME="XScreenSaver $$V" ;						      \
  STAGE="$$TMPDIR/dmg_stage" ;						      \
  DST="$$STAGE/Screen Savers" ;						      \
  PKG="$$STAGE/Install Everything.pkg" ;				      \
  rm -f "$$DMG" ;							      \
  sudo rm -rf "$$STAGE" ;						      \
  echo + mkdir -p "$$DST" ;						      \
         mkdir -p "$$DST" ;						      \
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
   f2=`basename "$$f"` ;						      \
   ok=yes ;								      \
   ff=`echo $$f | perl -e '$$_=<>; s@^.*/(.*)\..*$$@\L$$1@; print'`;	      \
   for r in $$retired ; do						      \
     if [ "$$ff" = "$$r" ]; then ok=no ; fi ;				      \
   done ;								      \
   if [ "$$ff" = testx11 ]; then ok=no ; fi ;				      \
   if [ "$$f2" = "XScreenSaverUpdater.app" ]; then			      \
     DST_HACK="XScreenSaver.updater" ;					      \
     echo + tar ... "$$DST/$$DST_HACK" ;				      \
     CWD=`pwd` ;							      \
     ( cd $$SRC ; tar -czf "$$CWD/$$DST/$$DST_HACK" "$$f2" ) ;		      \
     echo ' fuck you apple' >> $$DST/$$DST_HACK ;			      \
   elif [ "$$ok" = yes ]; then						      \
     echo + cp -pR "$$f" "$$DST/" ;					      \
            cp -pR "$$f" "$$DST/" ;					      \
   else									      \
     echo skipping "$$f" ;						      \
   fi ;									      \
  done ;								      \
									      \
  echo "Re-signing Apple2 and Phosphor..." ;				      \
  codesign --force --sign $(CERT2) --timestamp -o runtime "$$DST/"*.app/Contents/PlugIns/*.saver ; \
  codesign --force --sign $(CERT2) --timestamp -o runtime "$$DST/"*.app ;     \
									      \
  set -x ;								      \
  cp -p bindist.rtf "$$STAGE/Read Me.rtf" ;				      \
  cp -p build/Release/installer.pkg "$$PKG" ;				      \
  cp -p bindist-DS_Store "$$STAGE/.DS_Store" ;				      \
  cp -p bindist*.webloc "$$STAGE/" ;					      \
  cp -p XScreenSaverDMG.icns "$$STAGE/.VolumeIcon.icns" ;		      \
  ${SETFILE} -a C "$$STAGE" ;						      \
  ${SETFILE} -a E "$$STAGE"/*.{rtf,pkg,webloc} ;			      \
  $(SETICON) -d ../../xdaliclock/OSX/daliclockSaver.icns		      \
		"$$DST/DaliClock.saver" ;				      \
  $(SETICON) -d XScreenSaverFolder.icns "$$DST" ;			      \
  $(SETICON) -d XScreenSaver.icns "$$DST"/*.saver ;			      \
  $(SETICON) -d SaverRunner.icns "$$DST"/*.app ;			      \
  $(SETICON) -d XScreenSaverWebloc.icns        "$$STAGE"/bindist.webloc ;     \
  $(SETICON) -d XScreenSaverAndroidWebloc.icns "$$STAGE"/bindist2.webloc ;    \
  $(SETICON) -d XScreenSaverPkg.icns "$$STAGE"/*.pkg ;			      \
  mv "$$STAGE/bindist.webloc"  "$$STAGE/Get the iPhone:iPad Version.webloc" ; \
  mv "$$STAGE/bindist2.webloc" "$$STAGE/Get the Android Version.webloc" ;     \
									      \
  set +x ;								      \
									      \
  echo "Chowning..." ;							      \
  sudo chown -R root:wheel "$$STAGE/"* ;				      \
									      \
  echo "Checking signatures..." ;					      \
  if false; then							      \
    spctl --assess --type install "$$PKG" ;				      \
    spctl --assess --type execute "$$SRC/XScreenSaverUpdater.app" ;	      \
    spctl --assess --type execute "$$DST/"*.app ;			      \
    spctl --assess --type install "$$DST/"*.saver ;			      \
  else									      \
    true codesign -vv --deep --strict "$$PKG" ;				      \
    codesign -vv --deep --strict "$$SRC/XScreenSaverUpdater.app" ;	      \
    true codesign -vv --deep --strict "$$DST/"*.app ;			      \
    true codesign -vv --deep --strict "$$DST/"*.saver ;			      \
  fi ;									      \
									      \
  set -x ;								      \
									      \
  hdiutil makehybrid -quiet -ov -hfs -hfs-volume-name "$$VOLNAME"	      \
    -hfs-openfolder "$$STAGE" "$$STAGE" -o "$$TMPDMG" ;			      \
  echo "Deleting..." ;							      \
  sudo rm -rf "$$STAGE" ;						      \
									      \
  hdiutil convert -quiet -ov -format UDBZ -imagekey zlib-level=9	      \
    "$$TMPDMG" -o "$$DMG" ;						      \
  xattr -w com.apple.quarantine "0000;00000000;;" "$$DMG" ;		      \
  rm -f "$$TMPDMG" ;							      \
									      \
  codesign --sign $(CERT2) "$$DMG" ;					      \
  if false; then							      \
    spctl --assess --type install "$$DMG" ;				      \
  else									      \
    codesign -vvv --deep --strict "$$DMG" ;				      \
  fi ;									      \
									      \
  ls -ldhgF "$$DMG" ;							      \


# To set up notarization:
#  - Log in on https://appleid.apple.com/
#  - Generate App-Specific Password, "altool-notarizer"
#  - Keychain Access / New Password Item...
#  - Name: "altool-notarizer", Account: "jwz@jwz.org",
#    Pass: the one you just generated.
#
# As of Xcode 13, "altool" is obsolete because of course it is.
# The new way is "notarytool".
#
# "make notarize" will upload the DMG (slow).
#
# Note that if Sparkle.framework/.../Autoupdate.app is not independently
# signed, it won't pass. I had to add a build phase for that.
#
# - xcrun notarytool store-credentials --apple-id jwz@jwz.org
#   - Profile name: altool-notarizer
#   - App-specific password: [the above]
#   - Developer Team ID: 4627ATJELP
# - Credentials saved to Keychain.
#   To use them, specify `--keychain-profile "altool-notarizer"
#
#NOTARGS=-u "jwz@jwz.org" -p "@keychain:altool-notarizer"


# Send a notarization request, and then wait for the result.
#
notarize::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  CMD="xcrun notarytool submit $$DMG" ;					      \
  CMD="$$CMD --keychain-profile altool-notarizer" ;			      \
  CMD="$$CMD --apple-id jwz@jwz.org --team-id 4627ATJELP --wait" ;	      \
  echo "+ $$CMD" ;  							      \
  echo "" ;								      \
  RET=`$$CMD` ;  							      \
  echo "$$RET";  							      \
  echo "" ;								      \
  ID=`echo "$$RET" | sed -n 's/.*id: *\([^ ]*\).*/\1/p' | head -1` ;	      \
  STATUS=`echo "$$RET" | sed -n 's/.*status: *\([^ .]*\).*/\1/p' | head -1` ; \
  if [ "x$$ID" = "x" ]; then						      \
    echo "unparsable ID" ;						      \
    exit 1 ;								      \
  fi ;									      \
  if [ x"$$STATUS" != "xAccepted" ]; then				      \
    echo "ERROR: Bad status: $$STATUS" ;				      \
    echo "" ;								      \
    CMD="xcrun notarytool log $$ID --keychain-profile altool-notarizer" ;     \
    echo "+ $$CMD" ;  							      \
    $$CMD ;								      \
    exit 1 ;								      \
  fi ;									      \
  echo "Notarization success: $$STATUS" ;				      \
  exit 0


# Uploading the DMG to the notarizer generated a gatekeeper ticket for
# each enclosed item.  Staple those tickets to the DMG file.
#
# If we had uploaded a .zip, we would need to staple to the enclosed item
# and re-generate the .zip, but I think we can staple directly to a DMG
# without needing to re-generate it?
#
staple::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^ ]*\).*/\1/p' $$SRC | head -1` ;	      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  set -x ;								      \
  xcrun stapler staple "$$DMG" ;					      \
  xcrun stapler validate "$$DMG" ;					      \


# When debugging, sometimes I have to reset the preferences for all
# the savers.  Also I like FPS to be turned on, and them all to be
# pointed at the same image directory.
#
show_prefs::
	@cd build/Debug ;						      \
	for f in *.saver ; do						      \
	  f=`echo "$$f" | sed 's/\..*//'` ;				      \
	  echo "########################## $$f" ;			      \
	  defaults -currentHost read org.jwz.xscreensaver."$$f" 2>&- ;	      \
	done ;								      \
	for f in Apple2 Phosphor updater ; do				      \
	  echo "########################## $$f" ;			      \
	  defaults read org.jwz.xscreensaver."$$f" 2>&- ;		      \
	done

reset_prefs::
	@cd build/Debug ;						      \
	W1='defaults' ;							      \
	W2="$$W1 -currentHost write" ;					      \
	img='~/Pictures/Screensaver' ;					      \
	for f in *.saver ; do						      \
	  name=`echo "$$f" | sed 's/\..*//'` ;				      \
	  echo "########################## $$name" ;			      \
	  domain="org.jwz.xscreensaver" ;				      \
	  dd="$$domain.$$name" ;					      \
	  $$W1 -currentHost delete "$$dd" 2>&- ;			      \
	  $$W2 "$$dd" doFPS -bool true ;				      \
	  if [ -f $$f/Contents/Resources/xscreensaver-text ] ; then	      \
	    $$W2 "$$dd" textMode url ;					      \
	  fi ;								      \
	  if [ -f $$f/Contents/Resources/xscreensaver-getimage-file ] ; then  \
	    $$W2 "$$dd" chooseRandomImages -bool true ;			      \
	    $$W2 "$$dd" grabDesktopImages -bool false ;			      \
	    $$W2 "$$dd" imageDirectory "$$img" ;       			      \
	  fi ;								      \
	  if ( strings "$$f/Contents/MacOS/$$name" |			      \
		grep NSOpenGLContext >/dev/null ) ; then		      \
	    $$W2 "$$dd" multiSample -bool true ;			      \
	  fi ;								      \
	done ;								      \
									      \
	$$W1 delete "$$domain.Apple2" 2>&- ;				      \
	$$W1 delete "$$domain.Phosphor" 2>&- ;				      \
	$$W1 delete "$$domain.updater" 2>&- ;				      \
	$$W1 write  "$$domain.updater" SUScheduledCheckIntervalKey 86400;     \
									      \
	$$W2 "$$domain.BoxFit" grab -bool true ;			      \
	$$W2 "$$domain.FlipFlop" textured -bool true ;			      \
	$$W2 "$$domain.GLSlideshow" titles -bool true ;			      \
	$$W2 "$$domain.Photopile" titles -bool true ;			      \
	$$W2 "$$domain.SkyTentacles" mode cel ;				      \
	$$W2 "$$domain.Sonar" ping					      \
	  '/etc/hosts,$$HOME/.ssh/known_hosts,$$HOME/.ssh/known_hosts2' ;     \
	$$W2 "$$domain.XMatrix" matrixFont small ;			      \
	$$W2 "$$domain.XMatrix" textMode literal ;			      \
	$$W2 "$$domain.XMatrix" textLiteral "MONKEY BUTTER" ;		      \

# defaults -currentHost write org.jwz.xscreensaver.FontGlide debugMetrics -bool true
# defaults -currentHost write org.jwz.xscreensaver.StarWars debug -bool true
# defaults -currentHost write org.jwz.xscreensaver.StarWars textMode file
# defaults -currentHost write org.jwz.xscreensaver.StarWars textFile ~/src/xscreensaver/hacks/glx/zalgo.txt
