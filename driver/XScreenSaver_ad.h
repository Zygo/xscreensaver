"#error Do not run app-defaults files through xrdb!",
"#error That does not do what you might expect.",
"#error Put this file in /usr/lib/X11/app-defaults/XScreenSaver instead.",
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
"*chooseRandomImages:	False",
"*imageDirectory:	",
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
"*overlayTextForeground:	#FFFF00",
"*overlayTextBackground:	#000000",
"*overlayStderr:		True",
"*font:			*-medium-r-*-140-*-m-*",
"*sgiSaverExtension:	True",
"*mitSaverExtension:	False",
"*xidleExtension:	True",
"*procInterrupts:	True",
"*demoCommand: xscreensaver-demo",
"*prefsCommand: xscreensaver-demo -prefs",
"*helpURL: http://www.jwz.org/xscreensaver/man.html",
"*loadURL: netscape -remote 'openURL(%s)' || netscape '%s'",
"*manualCommand: gnome-help-browser 'man:%s'",
"*dateFormat:		%d-%b-%y (%a); %I:%M %p",
"*installColormap:	True",
"*programs:								      \
		 \"Qix (solid)\" 	qix -root -solid -segments 100		    \\n\
	   \"Qix (transparent)\" 	qix -root -count 4 -solid -transparent	    \\n\
		\"Qix (linear)\" 	qix -root -count 5 -solid -transparent	      \
				  -linear -segments 250 -size 100	    \\n\
- mono: 	   \"Qix (xor)\" 	qix -root -linear -count 5 -size 200	      \
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
				hyperball -root				    \\n\
				halo -root				    \\n\
				maze -root				    \\n\
				noseguy -root				    \\n\
				flame -root				    \\n\
				lmorph -root				    \\n\
				deco -root				    \\n\
				moire -root				    \\n\
				moire2 -root				    \\n\
				lightning -root				    \\n\
				strange -root				    \\n\
				spiral -root				    \\n\
				laser -root				    \\n\
				grav -root				    \\n\
	       \"Grav (trails)\" 	grav -root -trail -decay		    \\n\
				drift -root				    \\n\
				ifs -root				    \\n\
				julia -root				    \\n\
				penrose -root				    \\n\
				sierpinski -root			    \\n\
				braid -root				    \\n\
				galaxy -root				    \\n\
				bouboule -root				    \\n\
				swirl -root				    \\n\
				flag -root				    \\n\
				sphere -root				    \\n\
				forest -root				    \\n\
				lisa -root				    \\n\
				lissie -root				    \\n\
				goop -root -max-velocity 0.5 -elasticity      \
				  0.9					    \\n\
				starfish -root				    \\n\
	     \"Starfish (blob)\" 	starfish -root -blob			    \\n\
				munch -root				    \\n\
				fadeplot -root				    \\n\
				coral -root -delay 0			    \\n\
				mountain -root				    \\n\
				triangle -root -delay 1			    \\n\
				worm -root				    \\n\
				rotor -root				    \\n\
				ant -root				    \\n\
				demon -root				    \\n\
				loop -root				    \\n\
				vines -root				    \\n\
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
				critical -root				    \\n\
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
  color: 			bubbles -root				    \\n\
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
	   GL:			atlantis -root				    \\n\
	   GL:			lament -root				    \\n\
	   GL:			bubble3d -root				    \\n\
	   GL:			glplanet -root				    \\n\
	   GL:			pulsar -root				    \\n\
-	   GL:	   \"Pulsar (textures)\"					      \
				  pulsar -root -texture -mipmap		      \
				  -texture_quality -light -fog		    \\n\
	   GL:			extrusion -root				    \\n\
	   GL:			sierpinski3d -root			    \\n\
	   GL:			menger -root				    \\n\
	   GL:	 \"GFlux\"	gflux -root				    \\n\
	   GL:	 \"GFlux (grab)\"	gflux -root -mode grab			    \\n\
	   GL:			stonerview -root			    \\n\
	   GL:			starwars -root				    \\n\
	   GL:			gltext -root				    \\n\
	   GL:	\"GLText (clock)\" gltext -text \"%A%n%d %b %Y%n%r\" -root	    \\n\
	   GL:	 \"Molecule\"		molecule -root			    \\n\
	   GL:	 \"Molecule (lumpy)\"	molecule -root -no-bonds -no-labels \\n\
	   GL:			dangerball -root			    \\n\
	   GL:			circuit -root				    \\n\
	   GL:			engine -root				    \\n\
	   GL:			flipscreen3d -root			    \\n\
	   GL:			glsnake -root				    \\n\
	   GL:			boxed -root				    \\n\
	   GL:			glforestfire -root			    \\n\
	   GL:			sballs -root				    \\n\
	   GL:			cubenetic -root				    \\n\
	   GL:			spheremonics -root			    \\n\
	   GL:			lavalite -root				    \\n\
	   GL:			queens -root				    \\n\
	   GL:			endgame -root				    \\n\
									      \
-				xdaliclock -root -builtin3 -cycle	    \\n\
- default-n:			xearth -nofork -nostars -ncolors 50	      \
				  -night 3 -wait 0 -timewarp 400.0 -pos	      \
				  sunrel/38/-30				    \\n\
-				xplanetbg -xscreensaver -moonside             \
                                  -markerfile earth -wait 1 -timewarp 400   \\n\
-				xmountains -b -M -Z 0 -r 1		    \\n\
-	\"XMountains (top)\"	xmountains -b -M -Z 0 -r 1 -m		    \\n\
-                               xaos -fullscreen -autopilot                   \
                                  -incoloring -1 -outcoloring -1            \\n\
-				xfishtank -d -s                             \\n\
-				xsnow                                       \\n\
-				goban -root                                 \\n\
-				electricsheep                               \\n\
-				cosmos -root                                \\n\
-	   GL:                  sphereEversion --root                       \\n",
"XScreenSaver.pointerPollTime:		0:00:05",
"XScreenSaver.initialDelay:		0:00:00",
"XScreenSaver.windowCreationTimeout:	0:00:30",
"XScreenSaver.bourneShell:		/bin/sh",
"*Dialog.headingFont:		*-times-bold-r-*-*-*-180-*-*-*-iso8859-1",
"*Dialog.bodyFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.labelFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.buttonFont:		*-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"*Dialog.dateFont:		*-courier-medium-r-*-*-*-80-*-*-*-iso8859-1",
"*Dialog.foreground:		#000000",
"*Dialog.background:		#D6D6D6",
"*Dialog.Button.foreground:	#000000",
"*Dialog.Button.background:	#EAEAEA",
"*Dialog.text.foreground:	#000000",
"*Dialog.text.background:	#FFFFFF",
"*passwd.thermometer.foreground:	#FF0000",
"*passwd.thermometer.background:	#FFFFFF",
"*Dialog.topShadowColor:		#FFFFFF",
"*Dialog.bottomShadowColor:	#666666",
"*Dialog.logo.width:		210",
"*Dialog.logo.height:		210",
"*Dialog.internalBorderWidth:	30",
"*Dialog.borderWidth:		1",
"*Dialog.shadowThickness:	2",
"*passwd.heading.label:		XScreenSaver %s",
"*passwd.body.label:		This display is locked.",
"*passwd.user.label:		User:",
"*passwd.passwd.label:		Password:",
"*passwd.passwdFont:		*-courier-medium-r-*-*-*-140-*-*-*-iso8859-1",
"*passwd.thermometer.width:	8",
"*splash.heading.label:		XScreenSaver %s",
"*splash.body.label:		Copyright © 1991-2002 by",
"*splash.body2.label:		Jamie Zawinski <jwz@jwz.org>",
"*splash.demo.label:		Settings",
"*splash.help.label:		Help",
"*fontList:                       *-helvetica-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*demoDialog*label1.fontList:     *-helvetica-medium-r-*-*-*-140-*-*-*-iso8859-1",
"*cmdText.fontList:                 *-courier-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*label0.fontList:                  *-helvetica-bold-r-*-*-*-140-*-*-*-iso8859-1",
"XScreenSaver*doc.fontList:       *-helvetica-medium-r-*-*-*-100-*-*-*-iso8859-1",
"*foreground:			#000000",
"*background:			#C0C0C0",
"*XmTextField.foreground:	#000000",
"*XmTextField.background:	#FFFFFF",
"*list.foreground:		#000000",
"*list.background:		#FFFFFF",
"*ApplicationShell.title:	XScreenSaver",
"*warning.title:			XScreenSaver",
"*warning_popup.title:		XScreenSaver",
"*allowShellResize:		True",
"*autoUnmanage:			False",
"*menubar*file.labelString:	File",
"*menubar*file.mnemonic:		F",
"*file.blank.labelString:	Blank Screen Now",
"*file.blank.mnemonic:		B",
"*file.lock.labelString:		Lock Screen Now",
"*file.lock.mnemonic:		L",
"*file.kill.labelString:		Kill Daemon",
"*file.kill.mnemonic:		K",
"*file.restart.labelString:	Restart Daemon",
"*file.restart.mnemonic:		R",
"*file.exit.labelString:		Exit",
"*file.exit.mnemonic:		E",
"*menubar*edit.labelString:	Edit",
"*menubar*edit.mnemonic:		E",
"*edit.cut.labelString:		Cut",
"*edit.cut.mnemonic:		u",
"*edit.copy.labelString:		Copy",
"*edit.copy.mnemonic:		C",
"*edit.paste.labelString:	Paste",
"*edit.paste.mnemonic:		P",
"*menubar*help.labelString:	Help",
"*menubar*help.mnemonic:		H",
"*help.about.labelString:	About...",
"*help.about.mnemonic:		A",
"*help.docMenu.labelString:	Documentation...",
"*help.docMenu.mnemonic:		D",
"*demoTab.marginWidth:		10",
"*optionsTab.marginWidth:	10",
"*XmScrolledWindow.topOffset:	10",
"*XmScrolledWindow.leftOffset:	10",
"*demoTab.topOffset:		4",
"*form1.bottomOffset:		10",
"*form3.leftOffset:		10",
"*form3.rightOffset:		10",
"*frame.topOffset:		10",
"*frame.bottomOffset:		10",
"*enabled.topOffset:		10",
"*visLabel.topOffset:		10",
"*combo.topOffset:		10",
"*form4.bottomOffset:		4",
"*hr.bottomOffset:		4",
"*XmComboBox.marginWidth:	0",
"*XmComboBox.marginHeight:	0",
"*demo.marginWidth:		30",
"*demo.marginHeight:		4",
"*man.marginWidth:		10",
"*man.marginHeight:		4",
"*down.leftOffset:		40",
"*down.marginWidth:		4",
"*down.marginHeight:		4",
"*up.marginWidth:		4",
"*up.marginHeight:		4",
"*frame.traversalOn:		False",
"*list.automaticSelection:	True",
"*list.visibleItemCount:		20",
"*doc.columns:			60",
"*combo.columns:			11",
"*demoTab.labelString:		Graphics Demos",
"*optionsTab.labelString:	Screensaver Options",
"*down.labelString:		\\\\/ ",
"*up.labelString:		/\\\\ ",
"*frameLabel.labelString:	",
"*cmdLabel.labelString:		Command Line:",
"*cmdLabel.alignment:		ALIGNMENT_BEGINNING",
"*enabled.labelString:		Enabled",
"*visLabel.labelString:		Visual:",
"*visLabel.alignment:		ALIGNMENT_END",
"*visLabel.leftOffset:		20",
"*demo.labelString:		Demo",
"*man.labelString:		Documentation...",
"*done.labelString:		Quit",
"*preferencesLabel.labelString:	XScreenSaver Parameters",
"*timeoutLabel.labelString:	Saver Timeout",
"*cycleLabel.labelString:	Cycle Timeout",
"*fadeSecondsLabel.labelString:	Fade Duration",
"*fadeTicksLabel.labelString:	Fade Ticks",
"*lockLabel.labelString:		Lock Timeout",
"*passwdLabel.labelString:	Password Timeout",
"*preferencesForm*XmTextField.columns:	8",
"*verboseToggle.labelString:	Verbose",
"*cmapToggle.labelString:	Install Colormap",
"*fadeToggle.labelString:	Fade Colormap",
"*unfadeToggle.labelString:	Unfade Colormap",
"*lockToggle.labelString:	Require Password",
"*OK.marginWidth:		30",
"*OK.marginHeight:		4",
"*OK.leftOffset:			10",
"*OK.bottomOffset:		10",
"*Cancel.marginWidth:		30",
"*Cancel.marginHeight:		4",
"*Cancel.rightOffset:		10",
"*Cancel.bottomOffset:		10",
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
"*hacks.xspirograph.name:    XSpiroGraph",
"*hacks.nerverot.name:       NerveRot",
"*hacks.webcollage.name:     WebCollage",
"*hacks.vidwhacker.name:     VidWhacker",
"*hacks.morph3d.name:        Morph3D",
"*hacks.bubble3d.name:       Bubble3D",
"*hacks.glplanet.name:       GLPlanet",
"*hacks.sierpinski3d.name:   Sierpinski3D",
"*hacks.gflux.name:          GFlux",
"*hacks.xrayswarm.name:      XRaySwarm",
"*hacks.whirlwindwarp.name:  WhirlwindWarp",
"*hacks.rotzoomer.name:      RotZoomer",
"*hacks.stonerview.name:     StonerView",
"*hacks.starwars.name:       StarWars",
"*hacks.gltext.name:         GLText",
"*hacks.dangerball.name:     DangerBall",
"*hacks.whirlygig.name:      WhirlyGig",
"*hacks.speedmine.name:      SpeedMine",
"*hacks.glsnake.name:        GLSnake",
"*hacks.glforestfire.name:   GLForestFire",
"*hacks.sballs.name:         SBalls",
"*hacks.xdaliclock.name:     XDaliClock",
"*hacks.xplanetbg.name:      XPlanet",
"*hacks.xaos.name:           XaoS",
"*hacks.xfishtank.name:      XFishTank",
"*hacks.electricsheep.name:  ElectricSheep",
"*hacks.sphereEversion.name: SphereEversion",
"*hacks.fluidballs.name:     FluidBalls",
"*hacks.documentation.isInstalled: True",
