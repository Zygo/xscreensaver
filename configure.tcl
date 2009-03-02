#!/usr/bin/X11/wish -f
#charles vidal 1998 <vidalc@club-internet.fr>
# Thu Jul 30 06:21:57 MET DST 1998
#Projet Wizard in french Assistant
#http://www.chez.com/vidalc/assist/
#Sun Jul  4 01:35:05 MET DST 1999
# Add filevents find in setup.tcl of the xap ( X Application Panel ) 
# thank's to rasca, berlin 1999

# the next line restart wish \
exec wish "$0" "$@"

# number total of screens
set nbtotalecran 4

# titre de l'assistant
set titleassist "Wizard configure"

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
                $w insert end $line\n
                $w see end
        }
}

proc read_pipe { command w } {
	$w delete 1.0 end
        $w insert end "$command\n"
	set fileid [open |$command r]
        fileevent $fileid readable "cont $w $fileid"
}

proc action { } {
global bitmapdir
global pixmapdir 
global incdir
global libdir
global soundprog
global wgcc
global wmotif
global weditres
global wpurify
global wxpm
global wopengl
global wmesagl
global wdtsaver
global wdpms
global esyslog
global emulroot
global emuluser
global eunstable
global nb_ecran
global nbtotalecran
global lang
if { $nb_ecran == [expr $nbtotalecran -1 ]}  {
	set commandline "./configure"
	append commandline $wgcc $wpurify $wmotif $weditres $wxpm $wopengl $wmesagl $wdtsaver $esyslog $emuluser $emulroot $eunstable
	if {$bitmapdir!=""} { append commandline " --enable-bitmapdir=$bitmapdir" } 
	if {$pixmapdir!=""} { append commandline " --enable-pixmapdir=$pixmapdir" } 
	if {$incdir!=""} {append commandline " --x-includes=$incdir" } 
	if {$libdir!=""} {append commandline " --x-libraries=$libdir" } 
	if {$soundprog!=""} {append commandline " --enable-def-play=$soundprog" } 
	append commandline " $lang" 
	if { [getyesno "Do you really want to launch configure"] == "yes" } {
		set nb_ecran= [ expr $nb_ecran + 1 ]
		puts $commandline
#		wm withdraw .
#		toplevel .config
		pack forget .f$nb_ecran
		frame .fconf
		pack  .fconf
		set w .fconf
		button .f.make -text "Make" -command { set commandline "make"
					read_pipe $commandline  .fconf.t
			}
		button .f.install -text "Make Install"\
				 -command { set commandline "make install"
					read_pipe $commandline .fconf.t
		}
		button .f.exit -text "Exit" -command exit
#		button $w.goback -text "Go back" -command goback
		button $w.exit -text "Exit" -command exit
		scrollbar $w.s -orient vertical -command {.fconf.t yview} 
		pack $w.s -side right  -fill y
		text $w.t -yscrollcommand {.fconf.s set} \
			-wrap word -width 50 -height 10 \
        		-setgrid 1
		pack $w.t -expand yes -side right -fill both
		pack $w -side top -expand yes -fill both
		pack .f.make  .f.install .f.exit  -side left
#		pack .f.goback .f.exit  -side left
		pack forget .f.b1 .f.b2 .f.b3
		update
		read_pipe $commandline  $w.t
	}
   } else  exit
}
#
# get a dialog yes ok
#
proc getyesno { text  } {
return [ tk_messageBox -parent . -title {Launch Configure} -type yesno\
	-icon warning \
	-message $text ] 
}

proc OutputEcran { n } { 
global nomuser 
global nomreel 
global homedir 
 switch  $n { 
	}
}

proc InputEcran { n } {
global nomuser 
global homedir
 switch  $n {
	}
}
# titre de l'assistant
set titleassist "Assistant xxxx"

set tmpfnt ""

global tmpfnt

# nombre total d'ecran
#set nbtotalecran 6

# Declaration des variables globales
set nbwidget 0

# numero de l'ecran courant
set nb_ecran 0


proc openfont { var } {
upvar $var toto
global tmpfnt
     toplevel .font 
     set Fnt .font
	wm title $Fnt "Font Options"
	label  $Fnt.label -text "ABCDEFGH\nIJKabedfg\nhijkmnopq" 

    frame $Fnt.frame -borderwidth 10 
    frame $Fnt.frame2 -borderwidth 10
    set w $Fnt.frame
    label $w.msg0   -text "Font Options" 
    pack $w.msg0 -side top
    eval exec "xlsfonts \> /tmp/xlsfont.tmp"
    set f [open "/tmp/xlsfont.tmp"]
    listbox $Fnt.frame.names -yscrollcommand ".font.frame.scroll set" \
	-xscrollcommand "$Fnt.scroll2 set"  -setgrid 1 \
         -exportselection false 
    bind $Fnt.frame.names <Double-1> {
            .font.test configure -font [.font.frame.names get [.font.frame.names curselection]]
    set tmpfnt  [.font.frame.names get [.font.frame.names curselection]]
    }
    scrollbar $Fnt.frame.scroll -orient vertical -command ".font.frame.names 
yview" \
            -relief sunken -borderwidth 2
    scrollbar $Fnt.scroll2 -orient horizontal -command "$Fnt.frame.names xview" \
            -relief sunken -borderwidth 2
    while {[gets $f line] >= 0} {
            $Fnt.frame.names insert end $line 
    }
    close $f

    eval exec "/bin/rm -f /tmp/xlsfont.tmp"
    pack $Fnt.frame.names  -side left -expand y -fill both
    pack $Fnt.frame.scroll -side right -fill both
    pack $Fnt.frame  -fill x
    pack $Fnt.scroll2 -fill both
	label  $Fnt.test -text "ABCDEFGHIJKabedfghijkmnopq12345" 
        pack $Fnt.test

    button $Fnt.frame2.cancel -text Cancel -command "destroy $Fnt"
    button $Fnt.frame2.ok -text OK -command "set $var $tmpfnt; destroy $Fnt"
	pack $Fnt.frame2.ok $Fnt.frame2.cancel  -side left -fill both
	pack $Fnt.frame2 -fill both
}
proc opencolorsel {titre  var wf } {
upvar $var toto
set toto [tk_chooseColor -title $titre]
 $wf.label configure -foreground $toto
}
proc openfilesel { var  } {
upvar $var toto
set toto [ tk_getOpenFile -parent .]
}

proc OutputEcran { n } { 
global nomuser 
global nomreel 
global homedir 
 switch  $n { 
	}
}

proc InputEcran { n } {
global nomuser 
global homedir
 switch  $n {
	}
}

# Callback pour revenir a l'ecran precedent
proc prec_ecran { } {
global nbtotalecran
global nb_ecran
set tmp [expr $nb_ecran - 1]
if { $tmp > -1} {
		OutputEcran $nb_ecran
		pack forget .f$nb_ecran
		set nb_ecran [expr $nb_ecran - 1]
		InputEcran $nb_ecran
		pack .f$nb_ecran 
		}
if { $nb_ecran != [expr $nbtotalecran - 1 ]}  {
		.f.b3 configure -text "Cancel"
		}
}

# Callback pour aller a l'ecran suivant
proc next_ecran { } {
global nb_ecran
global nbtotalecran
if { $nb_ecran == [expr $nbtotalecran - 1 ]}  {
		.f.b3 configure -text "Configure"
		}
if { $nb_ecran != [expr $nbtotalecran - 1 ]}  {
		.f.b3 configure -text "Cancel"
		}
set tmp [expr $nb_ecran + 1]
if { $tmp < $nbtotalecran} {
		OutputEcran $nb_ecran
		pack forget .f$nb_ecran
		set nb_ecran [expr $nb_ecran + 1]
		InputEcran $nb_ecran
		pack .f$nb_ecran 
		}
}

# Creation du label titre et du texte explicatif
# Argument n:numero de l'ecran title:titre desc description

proc creationscreennb { n title desc icon} {
set currentarg 0
frame .f$n -height 10 -width 10
image create bitmap  image$n -file bitmaps/$icon
label .f$n.lt$n -text $title -font -Adobe-Courier-Bold-R-Normal-*-140-*
label .f$n.li$n -image image$n
pack .f$n.lt$n  .f$n.li$n
frame .f$n.fdesc 
message  .f$n.mesg -text $desc -width 25c
pack .f$n.mesg -fill x
pack .f$n.fdesc
}

# Creation de deux label + entry avec variable texte
# Argument n:numero de l'ecran label1:label devant texte vtext1:variable text
# De meme pour les xxxx2

proc creationentry { n nbf  text variable value} {
	frame .f$n.fr$nbf
	label .f$n.fr$nbf.lab1 -text $text
	entry .f$n.fr$nbf.e1 -textvariable  $variable
	.f$n.fr$nbf.e1 insert 0 $value
	pack .f$n.fr$nbf 
	pack .f$n.fr$nbf.e1  -side right
	pack .f$n.fr$nbf.lab1  -side left
}

proc mkecran { n args } {
  global nbwidget
  set nbf 0
  set w  .f$n
  foreach i $args {
		switch -regexp  [lindex $i 0] {
				LIST { listbox  $w.list$nbwidget 
					pack  $w.list$nbwidget 
					}
				ENTRY { 
					creationentry  $n $nbwidget [lindex $i 1] [lindex $i 2] [lindex $i 3] 
					}
				LABEL { label  $w.label$nbwidget -text  [lindex $i 1]
					pack  $w.label$nbwidget
					}
				CHECK { checkbutton  $w.check$nbwidget -text  [lindex $i 1] -variable  [lindex $i 2] -onvalue  [lindex $i 3] \
-offvalue [lindex $i 4]
					pack  $w.check$nbwidget 
					}
				RADIO { 
						frame $w.fr$nbwidget 
						image create photo   [lindex $i 4] -file [lindex $i 5] 
						label  $w.fr$nbwidget.labrad  -image [lindex $i 4]
						radiobutton  $w.fr$nbwidget.radio$nbwidget -text  [lindex $i 1] -variable  [lindex $i 2] -value [lindex $i 3] 
					pack   $w.fr$nbwidget.labrad  $w.fr$nbwidget.radio$nbwidget -side left
					pack   $w.fr$nbwidget
					}
				BUTTON { button  $w.button$nbwidget -text  [lindex $i 1]
					pack  $w.button$nbwidget 
					}
				SCALE { scale  $w.scale$nbwidget -from [lindex $i 1] -to [lindex $i 2]  -orient 
horizontal 
					pack  $w.scale$nbwidget 
					}
				FILE { 
        				frame   $w.frame$nbwidget
        				set  wf $w.frame$nbwidget
        				label   $wf.label  -text [lindex $i 1]
        				entry   $wf.entry  -textvariable [lindex $i 2 ]
	 				$wf.entry insert 0 [lindex $i 3] 
        				button $wf.button -text "File" -command 
"openfilesel [lindex $i 2]"
        				pack $wf
        				pack  $wf.label $wf.entry $wf.button  
-side left
					}
				COLOR { 
        				frame   $w.frame$nbwidget
        				set  wf $w.frame$nbwidget
					if { [lindex $i 4] !="" } then { label $wf.label  -text [lindex $i 1] -foreground [lindex 
$i 4] 
					} else { label $wf.label  -text [lindex $i 1] }
        				entry   $wf.entry  -textvariable [lindex $i 2 ]
	 				$wf.entry insert 0 [lindex $i 4] 
        				button $wf.button -text "Color" -command "opencolorsel [lindex $i 3] [lindex $i 
2] $wf"
        				pack $wf
        				pack  $wf.label $wf.entry $wf.button  -side left
					}
				FONT { 
        				frame   $w.frame$nbwidget
        				set  wf $w.frame$nbwidget
        				label   $wf.label  -text [lindex $i 1]
        				entry   $wf.entry  -textvariable [lindex $i 2 ]
	 				$wf.entry insert 0 [lindex $i 3] 
        				button $wf.button -text "Font" -command 
"openfont [lindex $i 2] "
        				pack $wf
        				pack  $wf.label $wf.entry $wf.button  
-side left
					}
				}
	set nbwidget [expr $nbwidget + 1]
		}
}
# 
# 
# --with-lang=lang        use a foreign language (de/fr/nl)
creationscreennb  0 "wizard configure xlockmore" "This should help you to 
build xlockmore" m-xlock.xbm
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
creationscreennb  2 "Wizard configure include and library path" "Please enter the path"  m-x11.xbm
mkecran 2 \
{ENTRY "path of X include          " incdir   }\
{ENTRY "path of X library          " libdir }\
{ENTRY "user executables in        " bindir }\
{ENTRY "system admin executables in" bindir }\
{ENTRY "system admin executables in" sbindir }\
{ENTRY "program executables in     " libexecdir }\
{ENTRY "info documentation in      " infodir }\
{ENTRY "man documentation in       " mandir }\
{ENTRY "find the sources in        " srcdir }\
 { }  
creationscreennb  3 "Wizard configure language" "Please choice your language "  m-xlock.xbm
mkecran 3 \
{RADIO "Dutch   " lang  " --with-lang=nl" fdutch etc/gif/nlflag.gif}\
{RADIO "English " lang " " fenglish etc/gif/ukflag.gif}\
{RADIO "French  " lang  " --with-lang=fr" ffrench etc/gif/frflag.gif}\
{RADIO "German  " lang  " --with-lang=de" fgerman etc/gif/deflag.gif}\
{}
frame .f
button .f.b1 -text "< Previous" -command "prec_ecran" -under 2
button .f.b2 -text "Next >" -command "next_ecran" -under 0
button .f.b3 -text "Cancel" -command "action"
pack .f.b1 .f.b2  .f.b3  -padx 2m -pady 2m -side left
pack .f -side bottom
pack .f0
