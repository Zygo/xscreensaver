# XScreenSaver for Android

export TERM=dumb
GRADLE = cd project ; ./gradlew

default:: debug
all:: release

clean::
	$(GRADLE) clean

distdepend::

# Set this to the set of platforms you want to compile for in debug mode.
# E.g., if you are running an x86 emulator, there's no point in compiling
# for a dozen other platforms.  Release builds override this to "all".
#
export APP_ABI = all


# TODO:
# check_versions:


# These hacks have interdependencies with others, so we can't build without
# including them or there are link errors:
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
	apollonian		\
	attraction		\
	atunnel			\
	blaster			\
	blinkbox		\
	blocktube		\
	boing			\
	bouncingcow		\
	boxed			\
	boxfit			\
	braid			\
	bsod			\
	cage			\
	ccurve			\
	cloudlife		\
	companioncube		\
	compass			\
	coral			\
	crystal			\
	cubestack		\
	cubestorm		\
	cubetwist		\
	cwaves			\
	cynosure		\
	dangerball		\
	decayscreen		\
	deco			\
	demon			\
	discrete		\
	discoball		\
	distort			\
	dnalogo			\
	drift			\
	energystream		\
	engine			\
	epicycle		\
	eruption		\
	euler2d			\
	fadeplot		\
	fiberlamp		\
	fireworkx		\
	flame			\
	flipflop		\
	flow			\
	fluidballs		\
	flyingtoasters		\
	fuzzyflakes		\
	galaxy			\
	gears			\
	geodesic		\
	geodesicgears		\
	glcells			\
	glknots			\
	glmatrix		\
	glschool		\
	glsnake			\
	gltext			\
	goop			\
	grav			\
	greynetic		\
	helix			\
	hexadrop		\
	hexstrut		\
	hilbert			\
	hopalong		\
	hypnowheel		\
	ifs			\
	imsmap			\
	interference		\
	intermomentary		\
	julia			\
	kaleidescope		\
	kaleidocycle		\
	klein			\
	kumppa			\
	lament			\
	lavalite		\
	loop			\
	m6502			\
	maze			\
	memscroller		\
	menger			\
	metaballs		\
	moebiusgears		\
	moire			\
	morph3d			\
	nerverot		\
	noof			\
	penetrate		\
	penrose			\
	petri			\
	piecewise		\
	pinion			\
	polytopes		\
	pong			\
	popsquares		\
	providence		\
	pyro			\
	quasicrystal		\
	raverhoop		\
	rd-bomb			\
	ripples			\
	rocks			\
	romanboy		\
	rorschach		\
	rotzoomer		\
	rubik			\
	rubikblocks		\
	sballs			\
	shadebobs		\
	sierpinski		\
	sierpinski3d		\
	slidescreen		\
	splodesic		\
	squiral			\
	stairs			\
	starfish		\
	stonerview		\
	strange			\
	substrate		\
	superquadrics		\
	swirl			\
	tangram			\
	thornbird		\
	timetunnel		\
	topblock		\
	triangle		\
	tronbit			\
	twang			\
	unknownpleasures	\
	vermiculate		\
	vigilance		\
	voronoi			\
	wander			\
	whirlwindwarp		\
	winduprobot		\
	wormhole		\
	xflame			\
	xlyap			\
	xspirograph		\


# These don't work well enough to turn on by default:
#
ANDROID_TODO=			\
	antinspect		\
	antmaze			\
	antspotlight		\
	atlantis		\
	barcode			\
	binaryring		\
	blitspin		\
	bouboule		\
	bumps			\
	carousel		\
	celtic			\
	circuit			\
	cityflow		\
	crackberg		\
	cube21			\
	cubenetic		\
	cubicgrid		\
	deluxe			\
	dymaxionmap		\
	endgame			\
	flipscreen3d		\
	fliptext		\
	fontglide		\
	gflux			\
	glblur			\
	gleidescope		\
	glhanoi			\
	glplanet		\
	glslideshow		\
	halftone		\
	halo			\
	hypertorus		\
	interaggregate		\
	jigglypuff		\
	jigsaw			\
	juggler3d		\
	mirrorblob		\
	moebius			\
	moire2			\
	molecule		\
	mountain		\
	munch			\
	noseguy			\
	pedal			\
	phosphor		\
	photopile		\
	pipes			\
	polyominoes		\
	projectiveplane		\
	pulsar			\
	qix			\
	queens			\
	skytentacles		\
	slip			\
	speedmine		\
	spheremonics		\
	splitflap		\
	spotlight		\
	starwars		\
	surfaces		\
	tessellimage		\
	testx11			\
	truchet			\
	unicrud			\
	xanalogtv		\
	xjack			\
	xmatrix			\
	xrayswarm		\
	zoom			\


# Download and resize images from jwz.org.
# This saves us having to include 4MB of images in the tar file
# that will only be used by a vast minority of people building
# from source.
# Android actually wants these to be 160x160 but our source is 200x150.

URL = https://www.jwz.org/xscreensaver/screenshots/
WGET = wget -q -U xscreensaver-build-android
CVT  = -thumbnail '150x150^' -gravity center -extent 150x150 \
     \( +clone  -alpha extract \
        -draw 'fill black polygon 0,0 0,15 15,0 fill white circle 15,15 15,0' \
        \( +clone -flip \) -compose Multiply -composite \
        \( +clone -flop \) -compose Multiply -composite \
     \) -alpha off -compose CopyOpacity -composite \
    -colorspace sRGB \
    -strip \
    -quality 95 \
    +dither -colors 128

# If we are making the m6502 hack, create the header file for Android
m6502.h::
	@for h in $(ANDROID_HACKS) ; do \
		if [ $${h} = "m6502" ] ; then \
			echo "Making $${h} header ..."; \
			../hacks/m6502.sh ../hacks/m6502.h ../hacks/images/m6502/*.asm ; \
			echo "Made $${h} header"; \
		fi; \
	done

project/xscreensaver/res/drawable/%.png:
	@\
	FILE1=`echo "$@" | sed 's!^.*/\([^/]*\)\.png$$!\1.jpg!'` ;	\
	FILE2="$@" ;							\
	FILE1=`echo "$$FILE1" | sed s/rdbomb/rd-bomb/` ;		\
	FILE2=`echo "$$FILE2" | sed s/rd-bomb/rdbomb/` ;		\
	URL="$(URL)$$FILE1" ;						\
	echo "converting $$URL..." ;					\
	rm -f "$$FILE2" ;						\
	$(WGET) -O- "$$URL" |						\
	convert jpg:- $(CVT) "$$FILE2" ;				\
	if [ ! -s "$$FILE2" ]; then					\
	  echo "$$FILE2 failed" >&2 ;					\
	  exit 1 ;							\
	fi

thumbs::
	@for f in $(ANDROID_HACKS) $(ANDROID_TODO) ; do			\
	  $(MAKE) project/xscreensaver/res/drawable/$$f.png ;		\
	done

clean_thumbs::
	@\
	for f in $(ANDROID_HACKS) $(ANDROID_TODO) ; do			\
	  rm -f project/xscreensaver/res/drawable/$$f.png ;		\
	done

distclean:: clean_thumbs clean


EXTRA_TARFILES = project/xscreensaver/res/drawable/thumbnail.png \

echo_tarfiles:
	@FILES=`find . $(EXTRA_TARFILES) \( \(	\
		   -name .DS_Store		\
		-o -name '*~'			\
		-o -name '*.keystore'		\
		-o -name '*_dream.xml'		\
		-o -name '*_settings.xml'	\
		-o -name '*_wallpaper.xml'	\
		-o -name AndroidManifest.xml	\
		-o -name strings.xml		\
		-o -name settings.xml		\
		-o -name attrs.xml		\
		-o -name .gitignore		\
		-o -name .gradle		\
		-o -name drawable		\
		-o -name build			\
		-o -name gen			\
		-o -name libs			\
		-o -name obj			\
		\) -prune \)			\
		-o \( \( -type f -o -type l \)	\
		      -print \)			\
		| sed 's@^\./@@' 		\
		| sort` ;			\
	echo $$FILES

run_check::
	../hacks/check-configs.pl --build-android $(ANDROID_HACKS)

debug:: m6502.h run_check
	$(GRADLE) assembleDebug
release:: m6502.h run_check
	export APP_ABI=all ; \
	$(GRADLE) assembleRelease


KEYSTORE = xscreensaver.keystore
$(KEYSTORE):
	keytool -genkey -v -keystore $@ \
	-alias xscreensaver -keyalg RSA -keysize 2048 -validity 10000

APK_DIR       = project/xscreensaver/build/outputs/apk/
APK_UNSIGNED  = $(APK_DIR)/xscreensaver-release-unsigned.apk
APK_UNALIGNED = $(APK_DIR)/xscreensaver-release-unaligned.apk
APK_SIGNED    = $(APK_DIR)/xscreensaver-release.apk

  TOOLDIR = $(shell echo $$HOME/Library/Android/sdk/build-tools/* | tail -1)
 ZIPALIGN = $(TOOLDIR)/zipalign
JARSIGNER = jarsigner

sign_release::
	cp -p $(APK_UNSIGNED) $(APK_UNALIGNED)
	$(JARSIGNER) -verbose -sigalg SHA1withRSA -digestalg SHA1 \
		     -keystore $(KEYSTORE) $(APK_UNALIGNED) xscreensaver
	rm -f $(APK_SIGNED)
	$(ZIPALIGN) -v 4 $(APK_UNALIGNED) $(APK_SIGNED)
	rm -f $(APK_UNALIGNED)
	$(JARSIGNER) -verify -verbose -certs $(APK_SIGNED)
	@ls -lF $(APK_SIGNED)

apk:: release
	@\
  VERS=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' ../utils/version.h` ; \
  HEAD="xscreensaver-$$VERS" ;						      \
  if [ ! -s $(APK_SIGNED) -o $(APK_UNSIGNED) -nt $(APK_SIGNED) ]; then	\
    $(MAKE) sign_release ;						\
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
