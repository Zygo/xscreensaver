# XScreenSaver for Android
# The rest of this Makefile is in xscreensaver/jni/Android.mk

export TERM=dumb
GRADLE = ./gradlew
THUMBDIR = .

default:: debug
all:: release

# Your sdk.dir and ndk.dir might go in here; gradle wants the file to exist.
local.properties:
	touch local.properties

clean::
	rm -rf .gradle
	rm -rf xscreensaver/.idea
	rm -f  android.iml
	rm -rf build
	rm -rf gen
	rm -rf xscreensaver/build
	rm -rf xscreensaver/libs
	rm -rf xscreensaver/obj
	rm -rf xscreensaver/res/values
	rm -rf xscreensaver/res/xml
	rm -rf xscreensaver/src/org/jwz/xscreensaver/gen
	rm -f  xscreensaver/AndroidManifest.xml
	rm -f  GLWallpaperService/build


# "make clean" should not require a functional gradle.
#	$(GRADLE) clean

distdepend:: local.properties

# Set this to the set of platforms you want to compile for in debug mode.
# E.g., if you are running an x86 emulator, there's no point in compiling
# for a dozen other platforms.  Release builds override this to "all".
#
export APP_ABI = all


# TODO:
# check_versions:


# These hacks have interdependencies with others, so we can't build without
# including them or there are link errors.
# See also $LOCAL_SRC_FILES in xscreensaver/jni/Android.mk
#
ANDROID_BASE_HACKS=		\
	apple2			\
	bubble3d		\
	pacman			\
	polyhedra		\
	sonar			\
	sproingies		\

# These are the ones that currently work, at least to some degree:
#
export ANDROID_HACKS=		\
	$(ANDROID_BASE_HACKS)	\
	abstractile		\
	anemone			\
	anemotaxis		\
	antinspect		\
	antmaze			\
	antspotlight		\
	apollonian		\
	atlantis		\
	attraction		\
	atunnel			\
	barcode			\
	beats			\
	binaryhorizon		\
	binaryring		\
	blaster			\
	blinkbox		\
	blitspin		\
	blocktube		\
	boing			\
	bouboule		\
	bouncingcow		\
	boxed			\
	boxfit			\
	braid			\
	bsod			\
	bumps			\
	cage			\
	carousel		\
	ccurve			\
	celtic			\
	chompytower		\
	circuit			\
	cityflow		\
	cloudlife		\
	companioncube		\
	compass			\
	coral			\
	covid19			\
	crackberg		\
	crumbler		\
	crystal			\
	cube21			\
	cubenetic		\
	cubestack		\
	cubestorm		\
	cubetwist		\
	cubicgrid		\
	cubocteversion		\
	cwaves			\
	cynosure		\
	dangerball		\
	decayscreen		\
	deco			\
	deepstars		\
	deluxe			\
	demon			\
	discoball		\
	discrete		\
	distort			\
	dnalogo			\
	drift			\
	droste			\
	dumpsterfire		\
	dymaxionmap		\
	endgame			\
	energystream		\
	engine			\
	epicycle		\
	eruption		\
	esper			\
	etruscanvenus		\
	euler2d			\
	fadeplot		\
	fiberlamp		\
	filmleader		\
	fireworkx		\
	flame			\
	flipflop		\
	flipscreen3d		\
	fliptext		\
	flow			\
	fluidballs		\
	flyingtoasters		\
	fontglide		\
	fuzzyflakes		\
	galaxy			\
	gears			\
	geodesic		\
	geodesicgears		\
	gflux			\
	gibson			\
	glblur			\
	glcells			\
	gleidescope		\
	glhanoi			\
	glknots			\
	glmatrix		\
	glplanet		\
	glschool		\
	glslideshow		\
	glsnake			\
	gltext			\
	goop			\
	grav			\
	gravitywell		\
	greynetic		\
	halftone		\
	halo			\
	handsy			\
	headroom		\
	helix			\
	hexadrop		\
	hexstrut		\
	hextrail		\
	highvoltage		\
	hilbert			\
	hopalong		\
	hopffibration		\
	hypertorus		\
	hypnowheel		\
	ifs			\
	imsmap			\
	interaggregate		\
	interference		\
	intermomentary		\
	jigglypuff		\
	jigsaw			\
	juggler3d		\
	julia			\
	kaleidescope		\
	kaleidocycle		\
	kallisti		\
	klein			\
	klondike		\
	kumppa			\
	lament			\
	lavalite		\
	loop			\
	m6502			\
	marbling		\
	maze			\
	maze3d			\
	memscroller		\
	menger			\
	metaballs		\
	mirrorblob		\
	moebius			\
	moebiusgears		\
	moire			\
	moire2			\
	molecule		\
	morph3d			\
	mountain		\
	munch			\
	nakagin			\
	nerverot		\
	noof			\
	noseguy			\
	papercube		\
	peepers			\
	penetrate		\
	penrose			\
	petri			\
	photopile		\
	piecewise		\
	pinion			\
	pipes			\
	platonicfolding		\
	polyominoes		\
	polytopes		\
	pong			\
	popsquares		\
	projectiveplane		\
	providence		\
	pulsar			\
	pyro			\
	quasicrystal		\
	queens			\
	raverhoop		\
	razzledazzle		\
	rdbomb			\
	ripples			\
	rocks			\
	romanboy		\
	rorschach		\
	rotzoomer		\
	rubik			\
	rubikblocks		\
	sballs			\
	scooter			\
	shadebobs		\
	sierpinski		\
	sierpinski3d		\
	skulloop		\
	skytentacles		\
	slidescreen		\
	slip			\
	sphereeversion		\
	spheremonics		\
	splitflap		\
	splodesic		\
	spotlight		\
	squiral			\
	squirtorus		\
	stairs			\
	starwars		\
	stonerview		\
	strange			\
	substrate		\
	superquadrics		\
	surfaces		\
	swirl			\
	tangram			\
	tessellimage		\
	timetunnel		\
	topblock		\
	triangle		\
	tronbit			\
	truchet			\
	twang			\
	unicrud			\
	unknownpleasures	\
	vermiculate		\
	vfeedback		\
	vigilance		\
	voronoi			\
	wander			\
	whirlwindwarp		\
	winduprobot		\
	wormhole		\
	xanalogtv		\
	xflame			\
	xjack			\
	xlyap			\
	xmatrix			\
	xrayswarm		\
	xspirograph		\
	zoom			\


# These don't work well enough to turn on by default:
#
ANDROID_TODO=			\
	pedal			\
	phosphor		\
	qix			\
	speedmine		\
	starfish		\


# If we are making the m6502 hack, create the header file for Android
m6502.h::
	../hacks/m6502.sh ../hacks/m6502.h ../hacks/images/m6502/*.asm

molecules.h::
	cd ../hacks/glx && \
	./molecules.sh molecules.h ../images/molecules/*.pdb

images_png_h:
	cd ../hacks/images && make


# Download and resize images from jwz.org.
# Largely duplicated in OSX/Makefile.
# This saves us having to include 4MB of images in the tar file that
# will only be used by a vast minority of people building from source.
# Android actually wants these to be 160x160 but our source is 720p.

BASE = xscreensaver/screenshots/
URL = https://cdn.jwz.org/$(BASE)

# I find wget easier to deal with, but curl is usually installed by default.
#WGET = wget -q -U xscreensaver-build-android --content-on-error=0 -O-
WGET = curl -sL -A xscreensaver-build-android -f

# ImageMagick isn't installed by default, but neither is anything similar.
#
# This recipe takes our source images (720p) and fits them into a 512x512 box,
# cropping either horizontally or vertically as needed, with rounded corners
# in the alpha mask.
#
# At one time I thought that Android wanted 150x150 resolution for these (the
# android:previewImage property in <dream> and the android:thumbnail property
# in <wallpaper>) but screen shots of the emulator demonstrate that 256x256
# images are being scaled up.  The APK must be compressing the shit out of
# the files, though:
#
#   150x150      2.3 MB of PNGs,  67 MB APK
#   256x256      4.7 MB of PNGs,  70 MB APK
#   512x512     14.0 MB of PNGs,  79 MB APK
#   1024x1024  110.0 MB of PNGs, 110 MB APK
#   (but at 1024, loading the list takes forever and/or crashes)
#
THUMB_SIZE=512x512
THUMB_CURVE=52
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

xscreensaver/res/drawable/%.png:
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

thumbs::
	@for f in $(ANDROID_HACKS) $(ANDROID_TODO) ; do			\
	  $(MAKE) xscreensaver/res/drawable/$$f.png ;			\
	done

clean_thumbs::
	@\
	for f in $(ANDROID_HACKS) $(ANDROID_TODO) ; do			\
	  rm -f xscreensaver/res/drawable/$$f.png ;			\
	done

distclean:: clean_thumbs clean
	rm -rf .gradle


EXTRA_TARFILES = xscreensaver/res/drawable/thumbnail.png \

list_tarfiles:
	@find . $(EXTRA_TARFILES) \( \(		\
		   -name .DS_Store		\
		-o -name '*~'			\
		-o -name '*.jks'		\
		-o -name '*.keystore'		\
		-o -name '*_dream.xml'		\
		-o -name '*_settings.xml'	\
		-o -name '*_wallpaper.xml'	\
		-o -name AndroidManifest.xml	\
		-o -name .gitignore		\
		-o -name .gradle		\
		-o -name .idea			\
		-o -name attrs.xml		\
		-o -name build			\
		-o -name caches			\
		-o -name drawable		\
		-o -name gen			\
		-o -name libs			\
		-o -name local.properties	\
		-o -name obj			\
		-o -name settings.xml		\
		-o -name strings.xml		\
		\) -prune \)			\
		-o \( \( -type f -o -type l \)	\
		      -print \)			\
		| sed 's@^\./@@' 		\
		| sort

run_check::
	../hacks/check-configs.pl --build-android $(ANDROID_HACKS)

debug::   local.properties m6502.h molecules.h run_check images_png_h
	$(GRADLE) assembleDebug
release:: local.properties m6502.h molecules.h run_check images_png_h
	$(GRADLE) assembleRelease
	if [ -f xscreensaver/build/outputs/apk/release/xscreensaver-release-unsigned.apk ]; then echo "Signing release APK..."; $(MAKE) sign_release; else echo "APK build failed, skipping signing"; fi


KEYSTORE = xscreensaver.jks

DEBUG_KEYSTORE = debug.keystore

$(DEBUG_KEYSTORE):
	keytool -genkey -v -keystore $@ \
	-alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
	-storepass android -keypass android \
	-dname "CN=Android Debug,O=Android,C=US"

$(DEBUG_KEYSTORE):
APK_DIR       = xscreensaver/build/outputs/apk/release/
APK_UNSIGNED  = $(APK_DIR)xscreensaver-release-unsigned.apk
APK_UNALIGNED = $(APK_DIR)xscreensaver-release-unaligned.apk
APK_SIGNED    = $(APK_DIR)xscreensaver-release.apk

  TOOLDIR = $(ANDROID_HOME)/build-tools/34.0.0
 ZIPALIGN = $(TOOLDIR)/zipalign
APKSIGNER = $(TOOLDIR)/apksigner

sign_release:: $(DEBUG_KEYSTORE)
	rm -f $(APK_UNALIGNED)
	rm -f $(APK_SIGNED)
	$(ZIPALIGN) -v 4 $(APK_UNSIGNED) $(APK_SIGNED)
	$(APKSIGNER) sign --ks $(DEBUG_KEYSTORE) --ks-pass pass:android --key-pass pass:android $(APK_SIGNED)
	$(APKSIGNER) verify $(APK_SIGNED) 2>/dev/null
	@ls -lF $(APK_SIGNED)
	stty -echo

apk:: release
	@\
  SRC=../utils/version.h ;						    \
  VERS=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. \"]*\).*/\1/p' < $$SRC |	    \
	 head -1`;							    \
  HEAD="xscreensaver-$$VERS" ;						    \
  if [ ! -s $(APK_SIGNED) -o $(APK_UNSIGNED) -nt $(APK_SIGNED) ]; then	\
    $(MAKE) sign_release; else echo "APK build failed, skipping signing"; fi ;						\
  fi ;									\
  set -x ;								\
  cp -p $(APK_SIGNED) ../archive/$$HEAD.apk


## #### Pare it down for faster debugging...
#export APP_ABI = armeabi-v7a
#export APP_ABI = x86
#
#export ANDROID_HACKS=		\
#	$(ANDROID_BASE_HACKS)	\
#  bsod				\
#  apollonian			\
#  engine			\
#  dnalogo			\
#  twang			\
#  memscroller			\
#  phosphor			\
#  discoball			\
#  cubetwist			\
#  cubestack			\
#  splodesic			\
