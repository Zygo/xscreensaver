"*timeout: 10",
"*cycle: 10",
"*lockTimeout: 0",
"*passwdTimeout: 30",
"*nice:	 10",
"*lock:		False",
"*verbose:	False",
"*fade:		True",
"*unfade:	False",
"*fadeSeconds: 1",
"*fadeTicks: 75",
"*installColormap: False",
"*programs:	qix -root						\\n\
		qix -root -solid -delay 0 -segments 100			\\n\
		qix -root -linear -count 10 -size 100 -segments 200	\\n\
		attraction -root -mode balls				\\n\
		attraction -root -mode lines -points 3 -segments 200	\\n\
		attraction -root -mode splines -segments 300		\\n\
		attraction -root -mode lines -radius 300		\
			-orbit -vmult 0.5				\\n\
		pyro -root						\\n\
		helix -root						\\n\
		rorschach -root -offset 7				\\n\
		hopalong -root						\\n\
		greynetic -root	-delay 1000     			\\n\
		xroger -root						\\n\
		imsmap -root						\\n\
		slidescreen -root					\\n\
		decayscreen -root					\\n\
		hypercube -root						\\n\
		halo -root						\\n\
		maze -root						\\n\
		flame -root						\\n",
"*monoPrograms:	qix -root -linear -count 5 -size 200 -spread 30		\
			-segments 75 -solid -xor			\\n\
		rocks -root						\\n\
		noseguy -root						\\n",
"*colorPrograms:	qix -root -count 4 -solid -transparent			\\n\
		qix -root -count 5 -solid -transparent -linear		\
			-segments 250 -size 100				\\n\
		attraction -root -mode polygons				\\n\
		attraction -root -mode filled-splines -segments 0	\\n\
		attraction -root -glow -points 10			\\n\
		rocks -root  -fg  lightblue				\\n\
		noseguy -root -fg yellow -bg gray30			\\n",
"*fontList:                       *-helvetica-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*demoDialog*label1.fontList:     *-helvetica-medium-r-*-*-*-140-*-*-*-iso8859-1",
"*passwdDialog*fontList:          *-helvetica-medium-r-*-*-*-140-*-*-*-iso8859-1",
"*XmList.fontList:                  *-courier-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*XmTextField.fontList:             *-courier-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*passwdDialog.passwdText.fontList: *-courier-medium-r-*-*-*-120-*-*-*-iso8859-1",
"*XmDialogShell*foreground:		black",
"*XmDialogShell*background:		gray90",
"*XmDialogShell*XmTextField.foreground:	black",
"*XmDialogShell*XmTextField.background:	white",
"*XmDialogShell*demoList.foreground:	black",
"*XmDialogShell*demoList.background:	white",
"*XmDialogShell*rogerLabel.foreground:	red3",
"*XmDialogShell*rogerLabel.background:	white",
"*XmDialogShell.title:		XScreenSaver",
"*allowShellResize:		True",
"*autoUnmanage:			False",
"*demoDialog.maxWidth:		600",
"*label1.labelString:		XScreenSaver %s",
"*label2.labelString: Copyright © 1991-1994 by Jamie Zawinski <jwz@mcom.com>",
"*demoList.visibleItemCount:	10",
"*demoList.automaticSelection:	True",
"*next.labelString:		Run Next",
"*prev.labelString:		Run Previous",
"*edit.labelString:		Edit Parameters",
"*done.labelString:		Exit Demo Mode",
"*restart.labelString:		Reinitialize",
"*resourcesLabel.labelString:	XScreenSaver Parameters",
"*timeoutLabel.labelString:	Saver Timeout",
"*cycleLabel.labelString:	Cycle Timeout",
"*fadeSecondsLabel.labelString:	Fade Duration",
"*fadeTicksLabel.labelString:	Fade Ticks",
"*lockLabel.labelString:		Lock Timeout",
"*passwdLabel.labelString:	Password Timeout",
"*resourcesForm*XmTextField.columns:	8",
"*verboseToggle.labelString:	Verbose",
"*cmapToggle.labelString:	Install Colormap",
"*fadeToggle.labelString:	Fade Colormap",
"*unfadeToggle.labelString:	Unfade Colormap",
"*lockToggle.labelString:	Require Password",
"*resourcesDone.labelString:	Done",
"*resourcesCancel.labelString:	Cancel",
"*passwdDialog.title:		Password",
"*passwdLabel1.labelString:	XScreenSaver %s",
"*passwdLabel2.labelString:	This display is locked.",
"*passwdLabel3.labelString:	Please type %s's password to unlock it.",
"*passwdDone.labelString:	Done",
"*passwdCancel.labelString:	Cancel",
"*passwdLabel1.alignment:	ALIGNMENT_BEGINNING",
"*passwdLabel2.alignment:	ALIGNMENT_BEGINNING",
"*passwdLabel3.alignment:	ALIGNMENT_BEGINNING",
"*rogerLabel.width:		150",
"*pointerPollTime: 5",
"*initialDelay: 30",
"*windowCreationTimeout: 30",
"*bourneShell:		/bin/sh",
