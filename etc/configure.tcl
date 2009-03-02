#!/usr/local/bin/wish -f
#charles vidal 1997 <cvidal@newlog.com>
#should be in directory of configure ... but it does not work yet
set nbtotalecran 3

# titre de l'assistant
set titleassist "Wizard configure"

# nombre total d'ecran
#set nbtotalecran 6

# Declaration des variables globales
set nbwidget 0

# numero de l'ecran courant
set nb_ecran 0

proc goconfig { } {
global bitmapdir
global pixmapdir 
global incdir
global libdir
global soundprog
global wgcc
global  wpur 
global  woutxpm  
global  wmesa
global  wmotif 
global  weditres  
set commandline "configure "
append commandline $wgcc $wpur $woutxpm $wmesa $wmotif $weditres
if {$bitmapdir!=""} { append commandline " --enable-bitmapdir=$bitmapdir" } 
if {$pixmapdir!=""} { append commandline " --enable-pixmapdir=$pixmapdir" } 
if {$incdir!=""} {append commandline "  --x-includes=$incdir" } 
if {$libdir!=""} {append commandline "  --x-libraries=$libdir" } 
if {$soundprog!=""} {append commandline "  --enable-def-play=$soundprog" } 
puts $commandline
exit
}
proc openfilesel { var  } {
upvar $var toto
set toto [ tk_getOpenFile -parent .]
puts $toto
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
global nb_ecran
set tmp [expr $nb_ecran - 1]
if { $tmp > -1} {
		OutputEcran $nb_ecran
		pack forget .f$nb_ecran
		set nb_ecran [expr $nb_ecran - 1]
		InputEcran $nb_ecran
		pack .f$nb_ecran 
		}
}

# Callback pour aller a l'ecran suivant
proc next_ecran { } {
global nb_ecran
global nbtotalecran
if { $nb_ecran == $nbtotalecran }  {
		.f.b2 configure -text "Termine"
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

proc creationscreennb { n title desc} {
set currentarg 0
frame .f$n -height 10 -width 10
label .f$n.lt$n -text $title -font -Adobe-Courier-Bold-R-Normal-*-140-*
pack .f$n.lt$n 
frame .f$n.fdesc 
message  .f$n.mesg -text $desc -width 25c
pack .f$n.mesg -fill x
pack .f$n.fdesc
}

# Creation de deux label + entry avec variable texte
# Argument n:numero de l'ecran label1:label devant texte vtext1:variable text
# De meme pour les xxxx2

proc creationentry { n nbf  text variable} {
	frame .f$n.fr$nbf
	label .f$n.fr$nbf.lab1 -text $text
	entry .f$n.fr$nbf.e1 -textvariable  $variable
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
				ENTRY { creationentry  $n $nbwidget [lindex $i 1] [lindex $i 2]
					}
				LABEL { label  $w.label$nbwidget -text  [lindex $i 1]
					pack  $w.label$nbwidget
					}
				CHECK { checkbutton  $w.check$nbwidget -text  [lindex $i 1] -variable  [lindex $i 2] -onvalue  [lindex $i 3] -offvalue [lindex $i 4] 
					pack  $w.check$nbwidget 
					}
				RADIO { radiobutton  $w.radio$nbwidget -text  [lindex $i 1]
					pack  $w.radio$nbwidget
					}
				BUTTON { button  $w.button$nbwidget -text  [lindex $i 1]
					pack  $w.button$nbwidget 
					}
				SCALE { scale  $w.scale$nbwidget
					pack  $w.scale$nbwidget 
					}
				FILE { 
        				frame   $w.frame$nbwidget
        				set  wf $w.frame$nbwidget
        				label   $wf.label  -text [lindex $i 1]
        				entry   $wf.entry  -textvariable [lindex $i 2 ]
        				button $wf.button -text "File" -command "openfilesel [lindex $i 2]"
        				pack $wf
        				pack  $wf.label $wf.entry $wf.button  -side left
					}
				}
	set nbwidget [expr $nbwidget + 1]
		}
}
# 
creationscreennb  0 "wizard configure xlockmore" "This should help you to build xlockmore"
mkecran 0 \
{CHECK "without gcc" wgcc "--without-gcc " "" }\
{CHECK "with-purify" wpur "--with-purify " "" }\
{CHECK "without xpm" woutxpm  "--without-xpm " ""}\
{CHECK "without mesagl" wmesa "--without-mesagl " "" }\
{CHECK "without motif" wmotif "--without-motif" ""}\
{CHECK "without editres" weditres  "--without-editres " ""}\
 { }  
creationscreennb  1 "Wizard configure image path" "please enter the image path"
mkecran 1 \
{ENTRY  "Bitmap dir" bitmapdir }\
{ENTRY "Pixmaps dir" pixmapdir   }\
{ENTRY "sound player program" soundprog    }\
{CHECK "enable syslog logging" enasys   "--enable-syslog  " ""  }\
{CHECK "enable multiple root users"  emulroot   "--enable-multiple-root  " "" }\
 { }  
creationscreennb  2 "Wizard configure include and library path" "PLease enter the path"
mkecran 2 \
{ENTRY "path of X include          " incdir   }\
{ENTRY "path of X library          "  libdir }\
{ENTRY "user executables in        "  bindir }\
{ENTRY "system admin executables in"  bindir }\
{ENTRY "system admin executables in"  sbindir }\
{ENTRY "program executables in     " libexecdir }\
{ENTRY "info documentation in      " infodir }\
{ENTRY "man documentation in       " mandir }\
{ENTRY "find the sources in        " srcdir }\
 { }  
frame .f
button .f.b1 -text "previous" -command "prec_ecran"
button .f.b2 -text "next" -command "next_ecran"
button .f.b3 -text "cancel" -command "goconfig"
pack .f.b1 .f.b2 .f.b3 -side left
pack .f -side bottom
pack .f0
