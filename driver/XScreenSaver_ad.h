"#error Do not run app-defaults files through xrdb!",
"#error That does not do what you might expect.",
"#error Put this file in /usr/lib/X11/app-defaults/XScreenSaver instead.",
"*mode:			random",
"*timeout:		0:10:00",
"*cycle:			0:10:00",
"*lockTimeout:		0:00:00",
"*passwdTimeout:		0:00:30",
"*dpmsEnabled:		False",
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
"*lockVTs:		True",
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
"*textFile:		/usr/X11R6/README",
"*textProgram:		fortune",
"*textURL:		http://www.livejournal.com/stats/latest-rss.bml",
"*overlayTextForeground:	#FFFF00",
"*overlayTextBackground:	#000000",
"*overlayStderr:		True",
"*font:			*-medium-r-*-140-*-m-*",
"*sgiSaverExtension:	True",
"*xidleExtension:	True",
"*procInterrupts:	True",
"GetViewPortIsFullOfLies: False",
"*demoCommand: xscreensaver-demo",
"*prefsCommand: xscreensaver-demo -prefs",
"*helpURL: http://www.jwz.org/xscreensaver/man.html",
"*loadURL: firefox '%s' || mozilla '%s' || netscape '%s'",
"*manualCommand: xterm -sb -fg black -bg gray75 -T '%s manual' \
		    -e /bin/sh -c 'man \"%s\" ; read foo'",
"*dateFormat:		%d-%b-%y (%a); %I:%M %p",
"*installColormap:	True",
"*programs:								      \
		 \"Qix (solid)\" 	qix -root -solid -segments 100		    \\n\
	   \"Qix (transparent)\" 	qix -root -count 4 -solid -transparent	    \\n\
		\"Qix (linear)\" 	qix -root -count 5 -solid -transparent	      \
				  -linear -segments 250 -size 100	    \\n\
-		   \"Qix (xor)\" 	qix -root -linear -count 5 -size 200	      \
				  -spread 30 -segments 75 -solid -xor	    \\n\
									      \
	  \"Attraction (balls)\" 	attraction -root -mode balls		    \\n\
	  \"Attraction (lines)\" 	attraction -root -mode lines -points 3	      \
				  -segments 200				    \\n\
-	   \"Attraction (poly)\" 	attraction -root -mode polygons		    \\n\
	\"Attraction (splines)\" 	attraction -root -mode splines -segments      \
				  300					    \\n\
	\"Attraction (orbital)\" 	attraction -root -mode lines -radius 300      \
				  -orbit -vmult 0.5			    \\n\
									      \
				pyro -root				    \\n\
				rocks -root				    \\n\
				helix -root				    \\n\
				pedal -root				    \\n\
				rorschach -root -offset 7		    \\n\
				hopalong -root				    \\n\
				greynetic -root				    \\n\
				imsmap -root				    \\n\
				slidescreen -root			    \\n\
				decayscreen -root			    \\n\
				jigsaw -root				    \\n\
				blitspin -root -grab			    \\n\
				slip -root				    \\n\
				distort -root				    \\n\
				spotlight -root				    \\n\
	      \"Ripples (oily)\"	ripples -root -oily -light 2		    \\n\
	      \"Ripples (stir)\"	ripples -root -oily -light 2 -stir	    \\n\
	   \"Ripples (desktop)\"	ripples -root -water -light 6		    \\n\
				hypercube -root				    \\n\
-				hyperball -root				    \\n\
				halo -root				    \\n\
				maze -root				    \\n\
				noseguy -root				    \\n\
				flame -root				    \\n\
-				lmorph -root				    \\n\
				deco -root				    \\n\
				moire -root				    \\n\
				moire2 -root				    \\n\
				lightning -root				    \\n\
				strange -root				    \\n\
-				spiral -root				    \\n\
				laser -root				    \\n\
				grav -root				    \\n\
	       \"Grav (trails)\" 	grav -root -trail -decay		    \\n\
				drift -root				    \\n\
				ifs -root				    \\n\
				julia -root				    \\n\
				penrose -root				    \\n\
-				sierpinski -root			    \\n\
				braid -root				    \\n\
				galaxy -root				    \\n\
				bouboule -root				    \\n\
				swirl -root				    \\n\
				flag -root				    \\n\
				sphere -root				    \\n\
				forest -root				    \\n\
-				lisa -root				    \\n\
-				lissie -root				    \\n\
				goop -root -max-velocity 0.5 -elasticity      \
				  0.9					    \\n\
				starfish -root				    \\n\
	     \"Starfish (blob)\" 	starfish -root -blob			    \\n\
				munch -root				    \\n\
				mismunch -root				    \\n\
				fadeplot -root				    \\n\
				coral -root -delay 0			    \\n\
				mountain -root				    \\n\
				triangle -root -delay 1			    \\n\
-				worm -root				    \\n\
-				rotor -root				    \\n\
-				demon -root				    \\n\
-				loop -root				    \\n\
-				vines -root				    \\n\
				kaleidescope -root			    \\n\
				xjack -root				    \\n\
				xlyap -root -randomize			    \\n\
				cynosure -root				    \\n\
				flow -root				    \\n\
				epicycle -root				    \\n\
				interference -root			    \\n\
				truchet -root -randomize		    \\n\
				bsod -root				    \\n\
				crystal -root				    \\n\
				discrete -root				    \\n\
				kumppa -root				    \\n\
				rd-bomb -root				    \\n\
	    \"RD-Bomb (mobile)\" 	rd-bomb -root -speed 1 -size 0.1	    \\n\
				sonar -root				    \\n\
				t3d -root				    \\n\
				penetrate -root				    \\n\
				deluxe -root				    \\n\
				compass -root				    \\n\
				squiral -root				    \\n\
				xflame -root				    \\n\
				wander -root				    \\n\
	      \"Wander (spots)\" 	wander -root -advance 0 -size 10 -circles     \
				  -length 10000 -reset 100000		    \\n\
-				critical -root				    \\n\
				phosphor -root				    \\n\
				xmatrix -root				    \\n\
				petri -root -size 2 -count 20		    \\n\
		     \"Petri 2\" 	petri -root -minlifespeed 0.02		      \
				  -maxlifespeed 0.03 -minlifespan 1	      \
				  -maxlifespan 1 -instantdeathchan 0	      \
				  -minorchan 0 -anychan 0.3		    \\n\
				shadebobs -root				    \\n\
				ccurve -root				    \\n\
				blaster -root				    \\n\
				bumps -root				    \\n\
				xteevee -root				    \\n\
				xanalogtv -root				    \\n\
				xspirograph -root			    \\n\
				nerverot -root				    \\n\
-	    \"NerveRot (dense)\"	nerverot -root -count 1000		    \\n\
-	    \"NerveRot (thick)\"	nerverot -root -count 100 -line-width 4       \
			        -max-nerve-radius 0.8 -nervousness 0.5 -db  \\n\
				xrayswarm -root				    \\n\
-	       \"Zoom (Fatbits)\"	zoom -root				    \\n\
	       \"Zoom (Lenses)\"	zoom -root -lenses			    \\n\
				rotzoomer -root				    \\n\
-	   \"RotZoomer (mobile)\" rotzoomer -root -move			    \\n\
-	   \"RotZoomer (sweep)\"  rotzoomer -root -sweep			    \\n\
				whirlwindwarp -root			    \\n\
 	            \"WhirlyGig\"	whirlygig -root				    \\n\
 	            \"SpeedMine\"	speedmine -root				    \\n\
 	            \"SpeedWorm\"	speedmine -root -worm			    \\n\
 	                	vermiculate -root			    \\n\
 	                	twang -root				    \\n\
 	                	apollonian -root			    \\n\
 	                	euler2d -root				    \\n\
	     \"Euler2d (dense)\"	euler2d -root -count 4000 -eulertail 400      \
				  -ncolors 230				    \\n\
- 	                	juggle -root				    \\n\
 	                	polyominoes -root			    \\n\
- 	                	thornbird -root				    \\n\
 	                	fluidballs -root			    \\n\
 	                	anemone -root				    \\n\
 	                	halftone -root				    \\n\
 	                	metaballs -root				    \\n\
 	                	eruption -root				    \\n\
 	                	popsquares -root			    \\n\
 	                	barcode -root				    \\n\
 	                	piecewise -root				    \\n\
 	                	cloudlife -root				    \\n\
		   \"FontGlide\"	fontglide -root -page			    \\n\
	\"FontGlide (scroller)\"	fontglide -root -scroll			    \\n\
				apple2 -root				    \\n\
                                bubbles -root				    \\n\
				pong -root				    \\n\
				wormhole -root				    \\n\
				pacman -root				    \\n\
				fuzzyflakes -root			    \\n\
				anemotaxis -root			    \\n\
				memscroller -root			    \\n\
				substrate -root				    \\n\
	 \"Substrate (circles)\"  substrate -root -circle-percent 33          \\n\
				intermomentary -root			    \\n\
				interaggregate -root			    \\n\
				fireworkx -root				    \\n\
				fiberlamp -root				    \\n\
				boxfit -root				    \\n\
				celtic -root				    \\n\
- default-n:			webcollage -root			    \\n\
- default-n:  \"WebCollage (whacked)\"					      \
				webcollage -root -filter		      \
				  'vidwhacker -stdin -stdout'		    \\n\
- default-n:			vidwhacker -root			    \\n\
									      \
	   GL:			gears -root				    \\n\
	   GL:	\"Gears (planetary)\" gears -root -planetary		    \\n\
	   GL:			superquadrics -root			    \\n\
	   GL:			morph3d -root				    \\n\
	   GL:			cage -root				    \\n\
	   GL:			moebius -root				    \\n\
	   GL:			stairs -root				    \\n\
	   GL:			pipes -root				    \\n\
	   GL:			sproingies -root			    \\n\
	   GL:			rubik -root				    \\n\
	   GL:			atlantis -root -gradient		    \\n\
	   GL:			lament -root				    \\n\
	   GL:			bubble3d -root				    \\n\
	   GL:			glplanet -root				    \\n\
	   GL:			flurry -root -preset random		    \\n\
	   GL:			pulsar -root				    \\n\
-	   GL:	   \"Pulsar (textures)\"					      \
				  pulsar -root -texture -mipmap		      \
				  -texture_quality -light -fog		    \\n\
-	   GL:			extrusion -root				    \\n\
	   GL:			sierpinski3d -root			    \\n\
	   GL:			menger -root				    \\n\
	   GL:	 \"GFlux\"	gflux -root				    \\n\
	   GL:	 \"GFlux (grab)\"	gflux -root -mode grab			    \\n\
	   GL:			stonerview -root			    \\n\
	   GL:			starwars -root				    \\n\
	   GL:			gltext -root				    \\n\
	   GL:	\"GLText (clock)\" gltext -text \"%A%n%d %b %Y%n%r\" -root	    \\n\
	   GL:	 		molecule -root -shells			    \\n\
	   GL:			dangerball -root			    \\n\
	   GL:			circuit -root				    \\n\
	   GL:			engine -root				    \\n\
	   GL:			flipscreen3d -root			    \\n\
	   GL:			glsnake -root				    \\n\
	   GL:			boxed -root				    \\n\
-	   GL:	\"GLForestFire\"		glforestfire -root		    \\n\
-	   GL:	\"GLForestFire (rain)\"	glforestfire -root -count 0	    \\n\
-	   GL:			sballs -root				    \\n\
	   GL:			cubenetic -root				    \\n\
	   GL:			spheremonics -root			    \\n\
	   GL:			lavalite -root				    \\n\
	   GL:			queens -root				    \\n\
	   GL:			endgame -root				    \\n\
-	   GL:			glblur -root				    \\n\
	   GL:			atunnel -root				    \\n\
	   GL:			flyingtoasters -root			    \\n\
	   GL:			bouncingcow -root			    \\n\
	   GL:			jigglypuff -root -random		    \\n\
	   GL:			klein -root -random			    \\n\
	   GL:	\"HyperTorus (striped)\" hypertorus -root			    \\n\
	   GL:	\"HyperTorus (solid)\"   hypertorus -root -solid -transparent \\n\
	   GL:			glmatrix -root				    \\n\
	   GL:			cubestorm -root				    \\n\
	   GL:			glknots -root				    \\n\
	   GL:			blocktube -root				    \\n\
	   GL:			flipflop -root				    \\n\
	   GL:			antspotlight -root			    \\n\
-	   GL:			glslideshow -root			    \\n\
	   GL:			polytopes -root				    \\n\
	   GL:			gleidescope -root			    \\n\
- 	   GL:			mirrorblob -root			    \\n\
	   GL:	    \"MirrorBlob (color only)\"				      \
                                mirrorblob -root -colour -no-texture	    \\n\
	   GL:			blinkbox -root				    \\n\
	   GL:			noof -root				    \\n\
	   GL:			polyhedra -root				    \\n\
-	   GL:                  antinspect -root                            \\n\
	   GL:			providence -root			    \\n\
	   GL:	\"Pinion (large gears)\"	pinion -root			    \\n\
	   GL:	\"Pinion (small gears)\"	pinion -root -size 0.2 -scroll 0.3  \\n\
	   GL:			boing -root -lighting -smooth		    \\n\
-	   GL:                  carousel -root                              \\n\
	   GL:			fliptext -root				    \\n\
-	   GL:                  antmaze -root                               \\n\
	   GL:			tangram -root				    \\n\
	   GL:			crackberg -root -flat -lit -crack	      \
				 -color random				    \\n\
	   GL:			glhanoi -root				    \\n\
	   GL:			cube21 -root -colormode six		    \\n\
	   GL:			timetunnel -root			    \\n\
	   GL:			juggler3d -root				    \\n\
									      \
-				xdaliclock -root -builtin3 -cycle	    \\n\
- default-n:			xearth -nofork -nostars -ncolors 50	      \
				  -night 3 -wait 0 -timewarp 400.0 -pos	      \
				  sunrel/38/-30				    \\n\
-				xplanet -vroot -wait 1 -timewarp 90000        \
                                  -label -origin moon			    \\n\
-				xmountains -b -M -Z 0 -r 1		    \\n\
-	\"XMountains (top)\"	xmountains -b -M -Z 0 -r 1 -m		    \\n\
-                               xaos -root -autopilot -nogui -delay 10000     \
                                  -maxframerate 30                            \
                                  -incoloring -1 -outcoloring -1            \\n\
-				xfishtank -d -s                             \\n\
-				xsnow                                       \\n\
-				goban -root                                 \\n\
-				electricsheep                               \\n\
-				cosmos -root                                \\n\
-	   GL:                  sphereEversion --root                       \\n\
-	   GL:                  fireflies -root                             \\n",
"XScreenSaver.pointerPollTime:		0:00:05",
"XScreenSaver.pointerHysteresis:		10",
"XScreenSaver.initialDelay:		0:00:00",
"XScreenSaver.windowCreationTimeout:	0:00:30",
"XScreenSaver.bourneShell:		/bin/sh",
"*Dialog.headingFont:		*-helvetica-bold-r-*-*-*-180-*-*-*-iso8859-1",
"*Dialog.bodyFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.labelFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
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
"*passwd.body.label:		Please enter your password.",
"*passwd.login.label:		New Login",
"*passwd.user.label:		Username:",
"*passwd.passwd.label:		Password:",
"*passwd.thermometer.width:	8",
"*passwd.asterisks:              True",
"*splash.heading.label:		XScreenSaver %s",
"*splash.body.label:		Copyright © 1991-2005 by",
"*splash.body2.label:		Jamie Zawinski <jwz@jwz.org>",
"*splash.demo.label:		Settings",
"*splash.help.label:		Help",
"*hacks.imsmap.name:         IMSmap",
"*hacks.slidescreen.name:    SlideScreen",
"*hacks.decayscreen.name:    DecayScreen",
"*hacks.blitspin.name:       BlitSpin",
"*hacks.lmorph.name:         LMorph",
"*hacks.ifs.name:            IFS",
"*hacks.fadeplot.name:       FadePlot",
"*hacks.bsod.name:           BSOD",
"*hacks.rd-bomb.name:        RD-Bomb",
"*hacks.t3d.name:            T3D",
"*hacks.shadebobs.name:      ShadeBobs",
"*hacks.ccurve.name:         C Curve",
"*hacks.xteevee.name:        XTeeVee",
"*hacks.xanalogtv.name:      XAnalogTV",
"*hacks.xspirograph.name:    XSpiroGraph",
"*hacks.nerverot.name:       NerveRot",
"*hacks.webcollage.name:     WebCollage",
"*hacks.vidwhacker.name:     VidWhacker",
"*hacks.morph3d.name:        Morph3D",
"*hacks.bubble3d.name:       Bubble3D",
"*hacks.sierpinski3d.name:   Sierpinski3D",
"*hacks.gflux.name:          GFlux",
"*hacks.xrayswarm.name:      XRaySwarm",
"*hacks.whirlwindwarp.name:  WhirlwindWarp",
"*hacks.rotzoomer.name:      RotZoomer",
"*hacks.stonerview.name:     StonerView",
"*hacks.starwars.name:       StarWars",
"*hacks.dangerball.name:     DangerBall",
"*hacks.whirlygig.name:      WhirlyGig",
"*hacks.speedmine.name:      SpeedMine",
"*hacks.glforestfire.name:   GLForestFire",
"*hacks.sballs.name:         SBalls",
"*hacks.xdaliclock.name:     XDaliClock",
"*hacks.xplanetbg.name:      XPlanet",
"*hacks.xplanet.name:        XPlanet",
"*hacks.xaos.name:           XaoS",
"*hacks.xfishtank.name:      XFishTank",
"*hacks.electricsheep.name:  ElectricSheep",
"*hacks.sphereEversion.name: SphereEversion",
"*hacks.fluidballs.name:     FluidBalls",
"*hacks.flyingtoasters.name: FlyingToasters",
"*hacks.bouncingcow.name:    BouncingCow",
"*hacks.jigglypuff.name:     JigglyPuff",
"*hacks.hypertorus.name:     HyperTorus",
"*hacks.cubestorm.name:      CubeStorm",
"*hacks.blocktube.name:      BlockTube",
"*hacks.flipflop.name:       FlipFlop",
"*hacks.antspotlight.name:   AntSpotlight",
"*hacks.fontglide.name:      FontGlide",
"*hacks.mirrorblob.name:     MirrorBlob",
"*hacks.blinkbox.name:       BlinkBox",
"*hacks.fuzzyflakes.name:    FuzzyFlakes",
"*hacks.memscroller.name:    MemScroller",
"*hacks.boxfit.name:         BoxFit",
"*hacks.fliptext.name:       FlipText",
"*hacks.glhanoi.name:        GLHanoi",
"*hacks.documentation.isInstalled: True",
