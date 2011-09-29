"#error Do not run app-defaults files through xrdb!",
"#error That does not do what you might expect.",
"#error Put this file in /usr/lib/X11/app-defaults/XScreenSaver instead.",
"*mode:			random",
"*timeout:		0:10:00",
"*cycle:			0:10:00",
"*lockTimeout:		0:00:00",
"*passwdTimeout:		0:00:30",
"*dpmsEnabled:		False",
"*dpmsQuickoffEnabled:	False",
"*dpmsStandby:		2:00:00",
"*dpmsSuspend:		2:00:00",
"*dpmsOff:		4:00:00",
"*grabDesktopImages:	True",
"*grabVideoFrames:	False",
"*chooseRandomImages:	True",
"*imageDirectory:	/Library/Desktop Pictures/",
"*nice:			10",
"*memoryLimit:		0",
"*lock:			False",
"*verbose:		False",
"*timestamp:		True",
"*fade:			True",
"*unfade:		False",
"*fadeSeconds:		0:00:03",
"*fadeTicks:		20",
"*splash:		True",
"*splashDuration:	0:00:05",
"*visualID:		default",
"*captureStderr: 	True",
"*ignoreUninstalledPrograms: False",
"*textMode:		file",
"*textLiteral:		XScreenSaver",
"*textFile:		/usr/X11/share/X11/doc/README.sgml",
"*textProgram:		fortune",
"*textURL:		http://twitter.com/statuses/public_timeline.atom",
"*overlayTextForeground:	#FFFF00",
"*overlayTextBackground:	#000000",
"*overlayStderr:		True",
"*font:			*-medium-r-*-140-*-m-*",
"*sgiSaverExtension:	True",
"*xidleExtension:	True",
"*procInterrupts:	True",
"*xinputExtensionDev:	False",
"GetViewPortIsFullOfLies: False",
"*demoCommand: xscreensaver-demo",
"*prefsCommand: xscreensaver-demo -prefs",
"*helpURL: http://www.jwz.org/xscreensaver/man.html",
"*loadURL: gnome-open '%s'",
"*manualCommand: gnome-terminal --title '%s manual' \
		--command '/bin/sh -c \"man %s; read foo\"'",
"*dateFormat:		%d-%b-%y (%a); %I:%M %p",
"*installColormap:	True",
"*programs:								      \
				maze -root				    \\n\
  GL: 				superquadrics -root			    \\n\
				attraction -root			    \\n\
				blitspin -root				    \\n\
				greynetic -root				    \\n\
				helix -root				    \\n\
				hopalong -root				    \\n\
				imsmap -root				    \\n\
-				noseguy -root				    \\n\
-				pyro -root				    \\n\
				qix -root				    \\n\
-				rocks -root				    \\n\
				rorschach -root				    \\n\
				decayscreen -root			    \\n\
				flame -root				    \\n\
				halo -root				    \\n\
				slidescreen -root			    \\n\
				pedal -root				    \\n\
				bouboule -root				    \\n\
-				braid -root				    \\n\
				coral -root				    \\n\
				deco -root				    \\n\
				drift -root				    \\n\
-				fadeplot -root				    \\n\
				galaxy -root				    \\n\
				goop -root				    \\n\
				grav -root				    \\n\
				ifs -root				    \\n\
  GL: 				jigsaw -root				    \\n\
				julia -root				    \\n\
-				kaleidescope -root			    \\n\
  GL: 				moebius -root				    \\n\
				moire -root				    \\n\
  GL: 				morph3d -root				    \\n\
				mountain -root				    \\n\
				munch -root				    \\n\
				penrose -root				    \\n\
  GL: 				pipes -root				    \\n\
				rd-bomb -root				    \\n\
  GL: 				rubik -root				    \\n\
-				sierpinski -root			    \\n\
				slip -root				    \\n\
  GL: 				sproingies -root			    \\n\
				starfish -root				    \\n\
				strange -root				    \\n\
				swirl -root				    \\n\
				triangle -root				    \\n\
				xjack -root				    \\n\
				xlyap -root				    \\n\
  GL: 				atlantis -root				    \\n\
				bsod -root				    \\n\
  GL: 				bubble3d -root				    \\n\
  GL: 				cage -root				    \\n\
-				crystal -root				    \\n\
				cynosure -root				    \\n\
				discrete -root				    \\n\
				distort -root				    \\n\
				epicycle -root				    \\n\
				flow -root				    \\n\
- GL: 				glplanet -root				    \\n\
				interference -root			    \\n\
				kumppa -root				    \\n\
  GL: 				lament -root				    \\n\
				moire2 -root				    \\n\
  GL: 				sonar -root				    \\n\
  GL: 				stairs -root				    \\n\
				truchet -root				    \\n\
-				vidwhacker -root			    \\n\
				blaster -root				    \\n\
				bumps -root				    \\n\
				ccurve -root				    \\n\
				compass -root				    \\n\
				deluxe -root				    \\n\
-				demon -root				    \\n\
- GL: 				extrusion -root				    \\n\
-				loop -root				    \\n\
				penetrate -root				    \\n\
				petri -root				    \\n\
				phosphor -root				    \\n\
  GL: 				pulsar -root				    \\n\
				ripples -root				    \\n\
				shadebobs -root				    \\n\
  GL: 				sierpinski3d -root			    \\n\
				spotlight -root				    \\n\
				squiral -root				    \\n\
				wander -root				    \\n\
-				webcollage -root			    \\n\
				xflame -root				    \\n\
				xmatrix -root				    \\n\
  GL: 				gflux -root				    \\n\
-				nerverot -root				    \\n\
				xrayswarm -root				    \\n\
				xspirograph -root			    \\n\
  GL: 				circuit -root				    \\n\
  GL: 				dangerball -root			    \\n\
- GL: 				dnalogo -root				    \\n\
  GL: 				engine -root				    \\n\
  GL: 				flipscreen3d -root			    \\n\
  GL: 				gltext -root				    \\n\
  GL: 				menger -root				    \\n\
  GL: 				molecule -root				    \\n\
				rotzoomer -root				    \\n\
				speedmine -root				    \\n\
  GL: 				starwars -root				    \\n\
  GL: 				stonerview -root			    \\n\
				vermiculate -root			    \\n\
				whirlwindwarp -root			    \\n\
				zoom -root				    \\n\
				anemone -root				    \\n\
				apollonian -root			    \\n\
  GL: 				boxed -root				    \\n\
  GL: 				cubenetic -root				    \\n\
  GL: 				endgame -root				    \\n\
				euler2d -root				    \\n\
				fluidballs -root			    \\n\
  GL: 				flurry -root				    \\n\
- GL: 				glblur -root				    \\n\
  GL: 				glsnake -root				    \\n\
				halftone -root				    \\n\
  GL: 				juggler3d -root				    \\n\
  GL: 				lavalite -root				    \\n\
-				polyominoes -root			    \\n\
  GL: 				queens -root				    \\n\
- GL: 				sballs -root				    \\n\
  GL: 				spheremonics -root			    \\n\
-				thornbird -root				    \\n\
				twang -root				    \\n\
- GL: 				antspotlight -root			    \\n\
				apple2 -root				    \\n\
  GL: 				atunnel -root				    \\n\
				barcode -root				    \\n\
  GL: 				blinkbox -root				    \\n\
  GL: 				blocktube -root				    \\n\
  GL: 				bouncingcow -root			    \\n\
				cloudlife -root				    \\n\
  GL: 				cubestorm -root				    \\n\
				eruption -root				    \\n\
  GL: 				flipflop -root				    \\n\
  GL: 				flyingtoasters -root			    \\n\
				fontglide -root				    \\n\
  GL: 				gleidescope -root			    \\n\
  GL: 				glknots -root				    \\n\
  GL: 				glmatrix -root				    \\n\
- GL: 				glslideshow -root			    \\n\
  GL: 				hypertorus -root			    \\n\
- GL: 				jigglypuff -root			    \\n\
				metaballs -root				    \\n\
  GL: 				mirrorblob -root			    \\n\
				piecewise -root				    \\n\
  GL: 				polytopes -root				    \\n\
				pong -root				    \\n\
				popsquares -root			    \\n\
  GL: 				surfaces -root				    \\n\
				xanalogtv -root				    \\n\
-				abstractile -root			    \\n\
				anemotaxis -root			    \\n\
- GL: 				antinspect -root			    \\n\
				fireworkx -root				    \\n\
				fuzzyflakes -root			    \\n\
				interaggregate -root			    \\n\
				intermomentary -root			    \\n\
				memscroller -root			    \\n\
  GL: 				noof -root				    \\n\
				pacman -root				    \\n\
  GL: 				pinion -root				    \\n\
  GL: 				polyhedra -root				    \\n\
- GL: 				providence -root			    \\n\
				substrate -root				    \\n\
				wormhole -root				    \\n\
- GL: 				antmaze -root				    \\n\
  GL: 				boing -root				    \\n\
				boxfit -root				    \\n\
  GL: 				carousel -root				    \\n\
				celtic -root				    \\n\
  GL: 				crackberg -root				    \\n\
  GL: 				cube21 -root				    \\n\
				fiberlamp -root				    \\n\
  GL: 				fliptext -root				    \\n\
  GL: 				glhanoi -root				    \\n\
  GL: 				tangram -root				    \\n\
  GL: 				timetunnel -root			    \\n\
  GL: 				glschool -root				    \\n\
  GL: 				topblock -root				    \\n\
  GL: 				cubicgrid -root				    \\n\
				cwaves -root				    \\n\
  GL: 				gears -root				    \\n\
  GL: 				glcells -root				    \\n\
  GL: 				lockward -root				    \\n\
				m6502 -root				    \\n\
  GL: 				moebiusgears -root			    \\n\
  GL: 				voronoi -root				    \\n\
  GL: 				hypnowheel -root			    \\n\
  GL: 				klein -root				    \\n\
-				lcdscrub -root				    \\n\
  GL: 				photopile -root				    \\n\
  GL: 				skytentacles -root			    \\n\
  GL: 				rubikblocks -root			    \\n\
  GL: 				companioncube -root			    \\n\
  GL: 				hilbert -root				    \\n\
  GL: 				tronbit -root				    \\n",
"XScreenSaver.pointerPollTime:		0:00:05",
"XScreenSaver.pointerHysteresis:		10",
"XScreenSaver.initialDelay:		0:00:00",
"XScreenSaver.windowCreationTimeout:	0:00:30",
"XScreenSaver.bourneShell:		/bin/sh",
"*Dialog.headingFont:		*-helvetica-bold-r-*-*-*-180-*-*-*-iso8859-1",
"*Dialog.bodyFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.labelFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.unameFont:		*-helvetica-bold-r-*-*-*-120-*-*-*-iso8859-1",
"*Dialog.buttonFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.dateFont:		*-helvetica-medium-r-*-*-*-80-*-*-*-iso8859-1",
"*passwd.passwdFont:		*-courier-medium-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.foreground:		#000000",
"*Dialog.background:		#E6E6E6",
"*Dialog.Button.foreground:	#000000",
"*Dialog.Button.background:	#F5F5F5",
"*Dialog.text.foreground:	#000000",
"*Dialog.text.background:	#FFFFFF",
"*passwd.thermometer.foreground:	#4464AC",
"*passwd.thermometer.background:	#FFFFFF",
"*Dialog.topShadowColor:		#FFFFFF",
"*Dialog.bottomShadowColor:	#CECECE",
"*Dialog.logo.width:		210",
"*Dialog.logo.height:		210",
"*Dialog.internalBorderWidth:	24",
"*Dialog.borderWidth:		1",
"*Dialog.shadowThickness:	2",
"*passwd.heading.label:		XScreenSaver %s",
"*passwd.body.label:		This screen is locked.",
"*passwd.unlock.label:		OK",
"*passwd.login.label:		New Login",
"*passwd.user.label:		Username:",
"*passwd.thermometer.width:	8",
"*passwd.asterisks:              True",
"*passwd.uname:                  True",
"*splash.heading.label:		XScreenSaver %s",
"*splash.body.label:		Copyright © 1991-2010 by",
"*splash.body2.label:		Jamie Zawinski <jwz@jwz.org>",
"*splash.demo.label:		Settings",
"*splash.help.label:		Help",
"*hacks.antinspect.name:     AntInspect",
"*hacks.antmaze.name:        AntMaze",
"*hacks.antspotlight.name:   AntSpotlight",
"*hacks.blinkbox.name:       BlinkBox",
"*hacks.blitspin.name:       BlitSpin",
"*hacks.blocktube.name:      BlockTube",
"*hacks.bouncingcow.name:    BouncingCow",
"*hacks.boxfit.name:         BoxFit",
"*hacks.bsod.name:           BSOD",
"*hacks.bubble3d.name:       Bubble3D",
"*hacks.ccurve.name:         CCurve",
"*hacks.cloudlife.name:      CloudLife",
"*hacks.companioncube.name:  CompanionCube",
"*hacks.cubestorm.name:      CubeStorm",
"*hacks.cubicgrid.name:      CubicGrid",
"*hacks.cwaves.name:         CWaves",
"*hacks.dangerball.name:     DangerBall",
"*hacks.decayscreen.name:    DecayScreen",
"*hacks.dnalogo.name:        DNA Logo",
"*hacks.euler2d.name:        Euler2D",
"*hacks.fadeplot.name:       FadePlot",
"*hacks.flipflop.name:       FlipFlop",
"*hacks.flipscreen3d.name:   FlipScreen3D",
"*hacks.fliptext.name:       FlipText",
"*hacks.fluidballs.name:     FluidBalls",
"*hacks.flyingtoasters.name: FlyingToasters",
"*hacks.fontglide.name:      FontGlide",
"*hacks.fuzzyflakes.name:    FuzzyFlakes",
"*hacks.gflux.name:          GFlux",
"*hacks.gleidescope.name:    Gleidescope",
"*hacks.glforestfire.name:   GLForestFire",
"*hacks.hyperball.name:      HyperBall",
"*hacks.hypercube.name:      HyperCube",
"*hacks.ifs.name:            IFS",
"*hacks.imsmap.name:         IMSMap",
"*hacks.jigglypuff.name:     JigglyPuff",
"*hacks.juggler3d.name:      Juggler3D",
"*hacks.lcdscrub.name:       LCDscrub",
"*hacks.lmorph.name:         LMorph",
"*hacks.m6502.name:          m6502",
"*hacks.memscroller.name:    MemScroller",
"*hacks.metaballs.name:      MetaBalls",
"*hacks.mirrorblob.name:     MirrorBlob",
"*hacks.moebiusgears.name:   MoebiusGears",
"*hacks.morph3d.name:        Morph3D",
"*hacks.nerverot.name:       NerveRot",
"*hacks.noseguy.name:        NoseGuy",
"*hacks.popsquares.name:     PopSquares",
"*hacks.rd-bomb.name:        RDbomb",
"*hacks.rdbomb.name:         RDbomb",
"*hacks.rotzoomer.name:      RotZoomer",
"*hacks.rubikblocks.name:    RubikBlocks",
"*hacks.sballs.name:         SBalls",
"*hacks.shadebobs.name:      ShadeBobs",
"*hacks.sierpinski3d.name:   Sierpinski3D",
"*hacks.skytentacles.name:   SkyTentacles",
"*hacks.slidescreen.name:    SlideScreen",
"*hacks.speedmine.name:      SpeedMine",
"*hacks.starwars.name:       StarWars",
"*hacks.stonerview.name:     StonerView",
"*hacks.t3d.name:            T3D",
"*hacks.timetunnel.name:     TimeTunnel",
"*hacks.topblock.name:       TopBlock",
"*hacks.tronbit.name:        TronBit",
"*hacks.vidwhacker.name:     VidWhacker",
"*hacks.webcollage.name:     WebCollage",
"*hacks.whirlwindwarp.name:  WhirlWindWarp",
"*hacks.xanalogtv.name:      XAnalogTV",
"*hacks.xrayswarm.name:      XRaySwarm",
"*hacks.documentation.isInstalled: True",
