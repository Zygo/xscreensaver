#!/bin/sh
#charles vidal 1998 <vidalc@club-internet.fr>
# Thu Jul 30 06:21:57 MET DST 1998
#Projet Wizard in french Assistant
#http://www.chez.com/vidalc/assist/
#Sun Jul  4 01:35:05 MET DST 1999
# Add filevents find in setup.tcl of the xap ( X Application Panel )
# thank's to rasca, berlin 1999
#
# Sat Jul 29 20:18:28 JST 2000
#   Fix font dialog and button action.
#   Add I18N system
#      By: YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>
#

# the next line restart wish \
exec wish "$0" "$@"

# Setup I18N system
set auto_path [linsert $auto_path end .]
set_catalogue [set_language]

set titleassist [_ "Wizard configure"]
wm title . $titleassist

# Declaration des variables globales
set nbwidget 0

# number of the current screen
set nb_ecran 0

set lang " "

proc cont {w input} {
    if [eof $input] {
	close $input
    } else {
	gets $input line
	$w insert end "$line\n"
	$w see end
    }
}

proc read_pipe { command w } {
    $w delete 1.0 end
    $w insert end "$command\n"
    set fileid [open "|$command" r]
    fileevent $fileid readable "cont $w $fileid"
}

proc action { } {
    global bitmapdir pixmapdir
    global incdir libdir
    global soundprog
    global wgcc wmotif weditres wpurify wxpm wopengl wmesagl wdtsaver wdpms
    global esyslog emulroot emuluser eunstable
    global nb_ecran nbtotalecran
    global lang

    if { $nb_ecran == [expr $nbtotalecran -1 ]}  {
	# Press "configure" button
	set commandline "./configure"
	append commandline $wgcc $wpurify $wmotif $weditres $wxpm $wopengl $wmesagl $wdtsaver \
		$esyslog $emuluser $emulroot $eunstable
	if {$bitmapdir!=""} { append commandline " --enable-bitmapdir=$bitmapdir" }
	if {$pixmapdir!=""} { append commandline " --enable-pixmapdir=$pixmapdir" }
	if {$incdir!=""}    { append commandline " --x-includes=$incdir" }
	if {$libdir!=""}    { append commandline " --x-libraries=$libdir" }
	if {$soundprog!=""} { append commandline " --enable-def-play=$soundprog" }
	append commandline " $lang"
	if { [getyesno [_ "Do you really want to launch configure?"]] == "yes" } {
	    puts $commandline
	    pack forget .f$nb_ecran

	    frame .fconf
	    button .f.make -text [_ "Make"] -command {
		set commandline "make"
		read_pipe $commandline  .fconf.t
	    }
	    button .f.install -text [_ "Make Install"] -command {
		set commandline "make install"
		read_pipe $commandline .fconf.t
	    }
	    button .f.exit -text [_ "Exit"] -command exit
	    scrollbar .fconf.s -orient vertical -command {.fconf.t yview}

	    text .fconf.t -yscrollcommand {.fconf.s set} \
		    -wrap word -width 50 -height 10 \
		    -setgrid 1

	    pack .fconf.s -side right  -fill y
	    pack .fconf.t -expand yes -side right -fill both
	    pack .fconf -side top -expand yes -fill both
	    pack .f.make  .f.install .f.exit  -side left
	    pack forget .f.b1 .f.b2 .f.b3

	    update
	    read_pipe $commandline  .fconf.t
	}
    } else {
	# Press "abort" button
	exit
    }
}

# get a dialog yes or no
proc getyesno { text } {
    return [ tk_messageBox -parent . -title [_ "Launch Configure"] -type yesno \
	    -icon warning \
	    -message $text ]
}

set tmpfnt ""

# Declaration des variables globales
set nbwidget 0

# numero de l'ecran courant
set nb_ecran 0

# open font dialog
proc openfont { var } {
    upvar $var toto
    global tmpfnt

    set w .font

    toplevel $w
    wm title $w [_ "Font Options"]

    frame $w.f
    label $w.f.msg0 -text [_ "Font Options"]

    listbox $w.f.names -yscrollcommand "$w.f.scroll set" \
	-xscrollcommand "$w.f.scroll2 set"  -setgrid 1 \
         -exportselection false
    bind $w.f.names <Double-1> "fontselect_action $w"
    scrollbar $w.f.scroll -orient vertical -command "$w.f.names yview" \
            -relief sunken -borderwidth 2
    scrollbar $w.f.scroll2 -orient horizontal -command "$w.f.names xview" \
            -relief sunken -borderwidth 2

    label $w.f.test -text [_ "ABCDEFGHIJKabedfghijkmnopq12345"]

    set fd [open "|xlsfonts" r]
    while {[gets $fd line]>=0} {
	$w.f.names insert end $line
    }
    close $fd

    grid rowconfigure $w 0 -weight 100
    grid columnconfigure $w 0 -weight 100

    grid $w.f -column 0 -row 0 -sticky nsew
    grid $w.f.msg0 -column 0 -row 0 -sticky ew
    grid $w.f.names -column 0 -row 1 -sticky nsew
    grid $w.f.scroll -column 1 -row 1 -sticky ns
    grid $w.f.scroll2 -column 0 -row 2 -sticky ew
    grid $w.f.test -column 0 -row 3 -sticky ew
    grid rowconfigure $w.f 1 -weight 100
    grid columnconfigure $w.f 0 -weight 100

    frame $w.f2
    button $w.f2.cancel -text [_ "Cancel"] -command "destroy $w"
    button $w.f2.ok -text [_ "OK"] -command "set $var \$tmpfnt; destroy $w"

    grid $w.f2 -column 0 -row 1
    pack $w.f2.ok $w.f2.cancel -side left
}

proc fontselect_action {w} {
    global tmpfnt

    set fname [$w.f.names get [$w.f.names curselection]]
    $w.f.test configure -font $fname
    set tmpfnt $fname
}

# open color dialog
proc opencolorsel {titre var wf} {
    upvar $var toto
    set toto [tk_chooseColor -title $titre]
    $wf.label configure -foreground $toto
}

# open file dialog
proc openfilesel { var } {
    upvar $var toto
    set toto [ tk_getOpenFile -parent .]
}

# Configure button state for "previous" button and "next" button
proc state_configure {} {
    global nbtotalecran nb_ecran

    if {$nb_ecran != 0} {
	.f.b1 configure -state active
    } else {
	.f.b1 configure -state disabled
    }

    if {$nb_ecran != [expr $nbtotalecran - 1]} {
	.f.b2 configure -state active
    } else {
	.f.b2 configure -state disabled
    }
}


# Callback pour revenir a l'ecran precedent
proc prec_ecran { } {
    global nbtotalecran nb_ecran
    set tmp [expr $nb_ecran - 1]
    if { $tmp >= 0} {
	pack forget .f$nb_ecran
	set nb_ecran [expr $nb_ecran - 1]
	pack .f$nb_ecran
    }

    state_configure

    if { $nb_ecran != [expr $nbtotalecran - 1 ]}  {
	.f.b3 configure -text [_ "Abort"]
    }
}

# Callback pour aller a l'ecran suivant
proc next_ecran { } {
    global nb_ecran nbtotalecran
    set tmp [expr $nb_ecran + 1]
    if { $tmp < $nbtotalecran} {
	pack forget .f$nb_ecran
	set nb_ecran [expr $nb_ecran + 1]
	pack .f$nb_ecran
    }

    state_configure

    if { $nb_ecran == [expr $nbtotalecran - 1 ]}  {
	.f.b3 configure -text [_ "Configure"]
    }

    if { $nb_ecran != [expr $nbtotalecran - 1 ]}  {
	.f.b3 configure -text [_ "Abort"]
    }
}

# Creation du label titre et du texte explicatif
# Argument n:numero de l'ecran title:titre desc description

proc creationscreennb { n title desc icon} {
    set currentarg 0
    frame .f$n -height 10 -width 10
    image create bitmap  image$n -file bitmaps/$icon
    label .f$n.lt$n -text [_ $title] -font [_ -Adobe-Courier-Bold-R-Normal-*-140-*]
    label .f$n.li$n -image image$n
    pack .f$n.lt$n  .f$n.li$n
    frame .f$n.fdesc
    message  .f$n.mesg -text [_ $desc] -width 25c
    pack .f$n.mesg -fill x
    pack .f$n.fdesc
}

# Creation de deux label + entry avec variable texte
# Argument n:numero de l'ecran label1:label devant texte vtext1:variable text
# De meme pour les xxxx2

proc creationentry { n nbf  text variable value} {
    frame .f$n.fr$nbf
    label .f$n.fr$nbf.lab1 -text [_ $text]
    entry .f$n.fr$nbf.e1 -textvariable  $variable
    .f$n.fr$nbf.e1 insert 0 $value
    pack .f$n.fr$nbf
    pack .f$n.fr$nbf.e1  -side right
    pack .f$n.fr$nbf.lab1  -side left
}

proc mkecran { n args } {
    global nbwidget
    set nbf 0
    set w .f$n
    foreach i $args {
	switch -regexp  [lindex $i 0] {
	    LIST {
		listbox  $w.list$nbwidget
		pack  $w.list$nbwidget
	    }
	    ENTRY {
		creationentry  $n $nbwidget [lindex $i 1] [lindex $i 2] [lindex $i 3]
	    }
	    LABEL {
		label  $w.label$nbwidget -text [_ [lindex $i 1]]
		pack  $w.label$nbwidget
	    }
	    CHECK {
		checkbutton  $w.check$nbwidget \
			-text [_ [lindex $i 1]] \
			-variable  [lindex $i 2] \
			-onvalue  [lindex $i 3] \
			-offvalue [lindex $i 4]
		pack  $w.check$nbwidget
	    }
	    RADIO {
		frame $w.fr$nbwidget
		image create photo   [lindex $i 4] -file [lindex $i 5]
		label  $w.fr$nbwidget.labrad  -image [lindex $i 4]
		radiobutton  $w.fr$nbwidget.radio$nbwidget \
			-text [_ [lindex $i 1]] \
			-variable  [lindex $i 2] \
			-value [lindex $i 3]
		pack   $w.fr$nbwidget.labrad  $w.fr$nbwidget.radio$nbwidget -side left
		pack   $w.fr$nbwidget
	    }
	    BUTTON {
		button  $w.button$nbwidget -text [_ [lindex $i 1]]
		pack  $w.button$nbwidget
	    }
	    SCALE {
		scale  $w.scale$nbwidget -from [lindex $i 1] -to [lindex $i 2] -orient horizontal
		pack  $w.scale$nbwidget
	    }
	    FILE {
		frame   $w.frame$nbwidget
		set  wf $w.frame$nbwidget
		label   $wf.label  -text [_ [lindex $i 1]]
		entry   $wf.entry  -textvariable [lindex $i 2]
		$wf.entry insert 0 [lindex $i 3]
		button $wf.button -text [_ "File"] -command "openfilesel [lindex $i 2]"
		pack $wf
		pack  $wf.label $wf.entry $wf.button -side left
	    }
	    COLOR {
		frame   $w.frame$nbwidget
		set  wf $w.frame$nbwidget
		if { [lindex $i 4] !="" } {
		    label $wf.label  -text [_ [lindex $i 1]] -foreground [lindex $i 4]
		} else {
		    label $wf.label  -text [_ [lindex $i 1]]
		}
		entry   $wf.entry  -textvariable [lindex $i 2]
		$wf.entry insert 0 [lindex $i 4]
		button $wf.button -text [_ "Color"] -command "opencolorsel [lindex $i 3] [lindex $i 2] $wf"
		pack $wf
		pack  $wf.label $wf.entry $wf.button  -side left
	    }
	    FONT {
		frame   $w.frame$nbwidget
		set  wf $w.frame$nbwidget
		label   $wf.label  -text [_ [lindex $i 1]]
		entry   $wf.entry  -textvariable [lindex $i 2]
		$wf.entry insert 0 [lindex $i 3]
		button $wf.button -text [_ "Font"] -command "openfont [lindex $i 2]"
		pack $wf
		pack  $wf.label $wf.entry $wf.button -side left
	    }
	}
	set nbwidget [expr $nbwidget + 1]
    }
}

#-----------------------------------------------------------------------------

# number total of screens
set nbtotalecran 0

creationscreennb  0 "wizard configure xlockmore" "This should help you to build xlockmore" m-xlock.xbm
mkecran 0 \
	{CHECK "without gcc    " wgcc     " --without-gcc" "" }\
	{CHECK "with-purify    " wpurify  " --with-purify" "" }\
	{CHECK "without motif  " wmotif   " --without-motif" ""}\
	{CHECK "without editres" weditres " --without-editres" ""}\
	{CHECK "without xpm    " wxpm     " --without-xpm" ""}\
	{CHECK "without opengl " wopengl  " --without-opengl" "" }\
	{CHECK "without mesagl " wmesagl  " --without-mesagl" "" }\
	{CHECK "without dtsaver" wdtsaver " --without-dtsaver" "" }\
	{CHECK "without dpms   " wdpms    " --without-dpms" "" }\
	{ }
incr nbtotalecran

creationscreennb  1 "Wizard configure image path" "please enter the image path"  m-xlock.xbm
mkecran 1 \
	{ENTRY  "Bitmap dir" bitmapdir }\
	{ENTRY "Pixmaps dir" pixmapdir }\
	{ENTRY "sound player program" soundprog    }\
	{CHECK "enable syslog logging      " esyslog   " --enable-syslog" "" }\
	{CHECK "enable multiple users      " emuluser  " --enable-multiple-user" "" }\
	{CHECK "enable multiple root users " emulroot  " --enable-multiple-root" "" }\
	{CHECK "enable unstable            " eunstable " --enable-unstable" "" }\
	{ }
incr nbtotalecran

creationscreennb  2 "Wizard configure include and library path" "Please enter the path"  m-x11.xbm
mkecran 2 \
	{ENTRY "path of X include          " incdir   }\
	{ENTRY "path of X library          " libdir }\
	{ENTRY "user executables in        " bindir }\
	{ENTRY "system admin executables in" sbindir }\
	{ENTRY "program executables in     " libexecdir }\
	{ENTRY "info documentation in      " infodir }\
	{ENTRY "man documentation in       " mandir }\
	{ENTRY "find the sources in        " srcdir }\
	{ }
incr nbtotalecran

creationscreennb  3 "Wizard configure language" "Please choice your language "  m-xlock.xbm
mkecran 3 \
	{RADIO "Dutch   " lang  " --with-lang=nl" fdutch etc/gif/nlflag.gif}\
	{RADIO "English " lang " " fenglish etc/gif/ukflag.gif}\
	{RADIO "French  " lang  " --with-lang=fr" ffrench etc/gif/frflag.gif}\
	{RADIO "German  " lang  " --with-lang=de" fgerman etc/gif/deflag.gif}\
	{RADIO "Japanese" lang  " --with-lang=jp" fjapanese etc/gif/jpflag.gif}\
	{}
incr nbtotalecran

#-----------------------------

frame .f

button .f.b1 -text [_ "< Previous"] -command {prec_ecran} -state disabled
button .f.b2 -text [_ "Next >"] -command {next_ecran}
button .f.b3 -text [_ "Abort"] -command {action}

pack .f.b1 .f.b2  .f.b3  -padx 2m -pady 2m -side left
pack .f -side bottom
pack .f0

# end
