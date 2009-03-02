#!/usr/X11/bin/wish -f

#charles vidal 1998 <vidalc@club-intenet.fr>
# update Sun Oct 18 1998
# Add the menu file with load resources
# and the exit button
# Add the load_process procedure loading
# the ressource file ~/XLock
#

#function find in demo: mkStyles.tcl
# The procedure below inserts text into a given text widget and
# applies one or more tags to that text.  The arguments are:
#
# w             Window in which to insert
# text          Text to insert (it's inserted at the "insert" mark)
# args          One or more tags to apply to text.  If this is empty
#               then all tags are removed from the text.
set bgcolor ""
set fgcolor ""
set ftname ""
set mftname ""
set usernom ""
set passmot ""
set XLock_validate ""
set XLock_invalid ""
set XLock_program ""
set geometrie ""
set icogeometrie ""
set XLock_info ""
set indxhelp ""
set messagesfile ""
set messagefile ""
set bitmap ""

proc openfilesel { var  } {
upvar $var toto
set toto [ tk_getOpenFile -parent .]
return toto
}

proc insertWithTags {w text args} {
  set start [$w index insert]
  $w insert insert $text
  foreach tag [$w tag names $start] {
    $w tag remove $tag $start insert
  }
  foreach i $args {
    $w tag add $i $start insert
  }
}

# Function for the help
proc mkHelpCheck { w args } {
  set nbf 0
  foreach i $args {
    set nbf [ expr $nbf +1 ]
    $w insert insert  "\n"
    checkbutton $w.c$nbf -variable [lindex $i 0] -text [lindex $i 0]
    $w window create {end lineend} -window $w.c$nbf
    $w insert insert  " [lindex $i 1] "
  }
}

proc mkHelpEntry { w args } {
  set nbf 0
  foreach i $args {
    set nbf [ expr $nbf +1 ]
    insertWithTags  $w "\n [lindex $i 0] " underline
    entry $w.e$nbf -textvariable [lindex $i 1]
    $w window create {end lineend} -window $w.e$nbf
    $w insert insert  "\n[lindex $i 2] "
  }
}

#
proc whichcolor { which } {
  global fgcolor
  global bgcolor
  if {$which == "RESETFG"} {set fgcolor ""}
  if {$which == "RESETBG"} {set bgcolor ""}
  if ($tk_version>4) then {
    if {$which== "FG" } {set fgcolor [tk_chooseColor -initialcolor $fgcolor -title "foreground color"];
      puts $fgcolor
    }
  if {$which == "BG"} {set bgcolor [tk_chooseColor -initialcolor $bgcolor -title "background color"];
      puts $bgcolor
    }
  } else
  {
    if {$which== "FG" } {set fgcolor [.color.frame.names get [.color.frame.names curselection]];}
    if {$which == "BG"} {set bgcolor [.color.frame.names get [.color.frame.names curselection]];}
  }
}

proc mkColor { what } {
  if ($tk_version>4) then {
    whichcolor $what; destroy .color
  } else
  {
    toplevel .color
    wm title .color "Color"
    frame .color.frame -borderwidth 10
    frame .color.frame2 -borderwidth 10
    set w .color.frame
    label $w.msg0   -text "Color Options"
    pack $w.msg0 -side top
    foreach i {/usr/local/lib/X11/rgb.txt /usr/lib/X11/rgb.txt
        /X11/R5/lib/X11/rgb.txt /X11/R4/lib/rgb/rgb.txt} {
      if ![file readable $i] {
        continue;
      }
      set f [open $i]
      listbox .color.frame.names -yscrollcommand ".color.frame.scroll set" \
            -relief sunken -borderwidth 2 -exportselection false
      bind .color.frame.names <Double-1> {
            .color.test configure -bg [.color.frame.names get [.color.frame.names curselection]]
      }
      scrollbar .color.frame.scroll -orient vertical -command ".color.frame.names yview" \
        -relief sunken -borderwidth 2
      pack .color.frame.names  -side left
      pack .color.frame.scroll -side right -fill both
      pack .color.frame  -fill x
      while {[gets $f line] >= 0} {
        if {[llength $line] == 4} {
          .color.frame.names insert end [lindex $line 3]
        }
      }
      close $f
      label  .color.test -height 5 -width 20
      button .color.frame2.cancel -text Cancel -command "destroy .color"
      button .color.frame2.ok -text OK -command "whichcolor $what; destroy .color"
      button .color.frame2.reset -text Reset -command "whichcolor RESET$what; destroy .color"
      pack  .color.test
      pack .color.frame2.ok .color.frame2.cancel .color.frame2.reset -side left -fill x
      pack .color.frame2 -fill both
      break;
    }
  }
}

# moving in text by the list
proc moveintext { indx } {
upvar indxhelp t1
 .help.f.t yview  [ lindex $t1 $indx ]

}
#----------------------
# Help ...
proc Helpxlock {} {
  global indxhelp
  toplevel .help
  wm title .help "Help About Xlock"
  frame .help.f
  scrollbar .help.f.s -orient vertical -command {.help.f.t yview}
  pack .help.f -expand yes -fill both
  pack .help.f.s -side right -fill y -expand yes
  text .help.f.t -yscrollcommand {.help.f.s set} -wrap word -width 60 -height 20 \
    -setgrid 1

  listbox .help.f.names -width 20 -height 20

  bind .help.f.names <Double-1> {
    set titi [eval .help.f.names curselection]
    moveintext $titi
  }

  pack .help.f.names .help.f.t -expand y -fill both -side left
  set w  .help.f.t
  $w tag configure big -font -Adobe-Courier-Bold-R-Normal-*-140-*

  foreach i {{"Xlock Help" { Locks the X server still the user enters their pass\
word at the keyboard.  While xlock  is  running,  all  new\
server  connections are refused.  The screen saver is dis\
abled.  The mouse cursor is turned  off.   The  screen  is\
blanked and a changing pattern is put on the screen.  If a\
key or a mouse button is pressed then the user is prompted\
for the password of the user who started xlock.
If  the  correct  password  is  typed,  then the screen is\
unlocked and the X server is restored.   When  typing  the\
password  Control-U  and  Control-H are active as kill and\
erase respectively.  To return to the locked screen, click\
in the small icon version of the changing pattern.} 0 }
    {"Options" {The option sets  the  X11  display  to  lock.\
xlock  locks all available screens on a given server,\
and restricts you to locking only a local server such\
as  unix::00,,  localhost::00,,  or  ::00  unless you set the\
     -remote option.} 0 }\
    {"-name" {is used instead of XLock  when  looking for resources to configure xlock.} 1 }
    {"-mode" {As  of  this  writing there are 100+ display modes supported (plus one more for random selection of one  of the 100+).} 1 }
    {"-delay"  {It simply sets the number  of  microseconds to  delay
between  batches  of animations.  In blank mode, it is important to set this to
some small  number  of  microseconds,  because the keyboard and mouse are only checked after each delay, so you cannot set  the delay  too high, but a delay of
zero would needlessly consume cpu checking for mouse and keyboard input  in a tight loop, since blank mode has no work to do.} 1 }
    {"-saturation" {This option sets saturation  of  the  color ramp .  0 is grayscale and 1 is very rich color. 0.4 is a nice pastel.} 1 }
    {"-username" {text string to use for Name prompt} 1 }\
    {"-password" {text string to use for Password prompt} 1 }\
    {"-info" {text string to use for instructions} 1 }\
    {"-validate" {the message shown  while  validating  the password,
defaults to \"Validating login...\"} 1 }\
    {"-invalid" {the  message  shown  when  password  is invalid, defaults to \"Invalid login.\"} 1 }\
    {"-geometry"  {This option sets the size and offset  of the  lock
window  (normally the entire screen).  The entire screen format is still used for  entering  the password.   The  purpose  is  to  see the screen even though it is locked.  This should be used  with  caution since many of the modes will fail if the windows are far from square or are too small  (size  must  be greater
than  0x0).   This  should also be used with esaver to protect screen from phosphor burn.} 1 }\
    {"-icongeometry" {this option sets  the  size  of  the iconic screen (normally 64x64) seen when entering the password.  This should be  used  with  caution  since many  of  the  modes will fail if the windows are far from square
or are too small (size  must  be  greater than  0x0).   The  greatest  size  is
256x256.  There should be some limit  so  users  could  see  who  has locked  the  screen.  Position information of icon is ignored.} 1 }
    {"-font" { Ths option  sets  the  font  to be used on the prompt screen.} 1 }
    { "-fg "  { This option sets the color of the text on the password screen.} 1 }
    {"-bg" { This option sets the color of the background on the password screen.} 1 }
    {"-forceLogout" { This option sets the  auto-logout.  This  might not be enforced depending how your system is configured.} 1 }} {
    lappend indxhelp [$w index current]
    if {  [lindex $i 2] == 1 } then {.help.f.names insert end "    [lindex $i 0]"} else {.help.f.names insert end " [lindex $i 0]"}
    insertWithTags  $w "[lindex $i 0] " big
    $w insert end "\n"
    $w insert end [lindex $i 1]
    $w insert end "\n"
  }
  lappend indxhelp [$w index current]
  insertWithTags  $w "Options boolean" big
   $w insert end "\n"
  .help.f.names insert end "Options boolean"
  mkHelpCheck $w {XLock_mono {turn on/off monochrome override}}\
                {nolock {trun on/off no password required mode}}\
                {remote {turn on/off remote host access}}\
                {allowroot {turn on/off allow root password mode (ignored)}}\
                {enablesaver {turn on/off enable X server screen saver}}\
                {allowaccess {turn on/off access of the terminal X}}\
                {grabmouse {turn on/off grabbing of mouse and keyboard}}\
                {echokeys {turn on/off echo \'?\' for each password key}}\
                {usefirst {turn on/off using the first char typed in password}}\
                {verbose {turn on/off verbose mode}}\
                {inwindow {turn on/off making xlock run in a window}}\
                {inroot {turn on/off making xlock run in the root window}}\
                {timeelapsed {turn on/off clock}}\
                {install {whether to use private colormap if needed (yes/no)}}\
                {sound {whether to use sound if configured for it (yes/no}}\
                {timeelapsed {turn on/off clock}}\
                {usefirst {text string to use for Name prompt}}\
                {trackmouse {turn on/off the mouse interaction}}
        button .help.ok -text OK -command "destroy .help"
  pack  .help.ok
}

# Create toplevel Author and Maintainer.
proc mkAuthor {} {
  toplevel .author
  wm title .author "Author and Maintainer of xlock"
  frame .author.frame -borderwidth 10
  set w .author.frame

  label $w.msg0   -text "Author and Maintainer of xlock"
  label $w.msg1   -text "Maintained by: David A. Bagley (bagleyd@tux.org)"
  label $w.msg2   -text "Original Author: Patrick J. Naughton (naughton@eng.sun.com)"
  label $w.msg3   -text "Mailstop 21-14 Sun Microsystems Laboratories,"
  label $w.msg4   -text "Inc.  Mountain View, CA  94043 15//336-1080"
  label $w.msg5   -text "with many additional contributors"
  pack $w.msg0 $w.msg1 $w.msg2 $w.msg3 $w.msg4 $w.msg5 -side top

  label $w.msg6   -text "xlock.tcl\n created by charles VIDAL\n (author of flag mode and xmlock launcher )"
  pack $w.msg6 -side top

  button .author.ok -text OK -command "destroy .author"
  pack $w .author.ok
}

proc mkFileDialog { nom titre args }  {
  toplevel .$nom
  wm title .$nom "$titre"
  frame .$nom.frame -borderwidth 10
  frame .$nom.frame2 -borderwidth 10
  frame .$nom.frame.frame4 -borderwidth 10
  set w .$nom.frame
  set w2 .$nom.frame2
  set w4 .$nom.frame.frame4
  set nbf 0

  label $w.msg0   -text "$titre"
  pack $w.msg0 -side top
  foreach i $args {
    set nbf [ expr $nbf +1 ]
    frame $w4.f$nbf
    label $w4.f$nbf.l$nbf -text [lindex $i 0]
    entry $w4.f$nbf.e$nbf -textvariable [lindex $i 1]
    button $w4.f$nbf.b$nbf -text "..." -command "openfilesel [lindex $i 1]"
    pack $w4.f$nbf.l$nbf $w4.f$nbf.e$nbf $w4.f$nbf.b$nbf  -side left -expand yes
    pack $w4.f$nbf -expand yes
  }
  button $w2.ok -text OK -command "destroy .$nom"
  button $w2.cancel -text Cancel -command "destroy .$nom"
  pack $w -side top -expand yes
  pack $w4 -side right -expand yes
  pack $w2.ok $w2.cancel -side left -fill x -expand yes
  pack $w2  -side bottom -expand yes
}

proc mkDialog { nom titre args } {
  toplevel .$nom
  wm title .$nom "$titre"
  frame .$nom.frame -borderwidth 10
  frame .$nom.frame2 -borderwidth 10
  frame .$nom.frame.frame3 -borderwidth 10
  frame .$nom.frame.frame4 -borderwidth 10
  set w .$nom.frame
  set w2 .$nom.frame2
  set w3 .$nom.frame.frame3
  set w4 .$nom.frame.frame4
  set nbf 0

  label $w.msg0   -text "$titre"
  pack $w.msg0 -side top
  foreach i $args {
    set nbf [ expr $nbf +1 ]
    label $w3.l$nbf -text [lindex $i 0]
    entry $w4.e$nbf -textvariable [lindex $i 1]
    pack $w3.l$nbf
    pack $w4.e$nbf
  }
  button $w2.ok -text OK -command "destroy .$nom"
  button $w2.cancel -text Cancel -command "destroy .$nom"
  pack $w -side top
  pack $w3 -side left
  pack $w4 -side right
  pack $w2.ok $w2.cancel -side left -fill x
  pack $w2  -side bottom
}

proc mkMessage {} {
  global passmot
  global XLock_validate
  global XLock_invalid
  global XLock_info
  mkDialog message {Message Options} \
  {"message password" passmot} \
  {"validate string" XLock_validate} \
  {"invalid string" XLock_invalid} \
  {"info string" XLock_info}
}

proc mkGeometry {} {
  global geometrie
  global icogeometrie
  mkDialog geometry {Geometry Options} \
  {"geometry" geometrie} \
  {"icon geometry" icogeometrie}
}

proc mkFileOption {} {
  global messagesfile
  global messagefile
  global bitmap
  mkFileDialog fileoption {Files Options} \
  {"messagesfile" messagesfile} \
  {"messagefile" messagefile} \
  {"bitmap" bitmap}
}

proc whichfont { which } {
  global ftname
  global mftname
 if {$which== "FONT" } {set ftname [.font.frame.names get [.font.frame.names curselection]];}
 if {$which == "MFONT"} {set mftname [.font.frame.names get [.font.frame.names curselection]];}
 if {$which == "RESETFONT"} {set ftname ""}
 if {$which == "RESETMFONT"} {set mftname ""}
}

#this function should be erase in the newer version...
proc mkFont { What } {
  toplevel .font
  wm title .font "Font Options"
  label  .font.label -text "ABCDEFGH\nIJKabedfg\nhijkmnopq"
  frame .font.frame -borderwidth 10
  frame .font.frame2 -borderwidth 10
  set w .font.frame
  label $w.msg0   -text "Font Options"
  pack $w.msg0 -side top
  eval exec "xlsfonts \> /tmp/xlsfont.tmp"
  set f [open "/tmp/xlsfont.tmp"]
  listbox .font.frame.names -yscrollcommand ".font.frame.scroll set" \
    -xscrollcommand ".font.scroll2 set"  -setgrid 1 \
    -exportselection false
  bind .font.frame.names <Double-1> {
   .font.test configure -font [.font.frame.names get [.font.frame.names curselection]]
  }
  scrollbar .font.frame.scroll -orient vertical -command ".font.frame.names yview" \
    -relief sunken -borderwidth 2
  scrollbar .font.scroll2 -orient horizontal -command ".font.frame.names xview" \
    -relief sunken -borderwidth 2
  while {[gets $f line] >= 0} {
    .font.frame.names insert end $line
  }
  close $f

  eval exec "/bin/rm -f /tmp/xlsfont.tmp"
  pack .font.frame.names  -side left -expand y -fill both
  pack .font.frame.scroll -side right -fill both
  pack .font.frame  -fill x
  pack .font.scroll2 -fill both
  label  .font.test -text "ABCDEFGHIJKabedfghijkmnopq12345"
  pack .font.test

  button .font.frame2.cancel -text Cancel -command "destroy .font"
  button .font.frame2.reset -text Reset -command "whichfont RESET$What;destroy .font"
  button .font.frame2.ok -text OK -command "whichfont $What;destroy .font"
  pack .font.frame2.ok .font.frame2.cancel .font.frame2.reset -side left -fill both
  pack .font.frame2 -fill both

  #frame $w.fontname
  #label $w.fontname.l1 -text "font name"
  #entry $w.fontname.e1 -relief sunken
  #frame $w.specfont
  #label $w.specfont.l2 -text "specifique font name"
  #entry $w.specfont.e2 -relief sunken
  #pack $w.fontname $w.specfont
  #pack $w.fontname.l1 -side left
  #pack $w.specfont.l2 -side left
  #pack $w.fontname.e1 $w.specfont.e2  -side top -pady 5 -fill x
  #button .font.frame2.ok -text OK -command "destroy .font"
  #button .font.frame2.cancel -text Cancel -command "destroy .font"
  #pack $w .font.frame2.ok .font.frame2.cancel -side left -fill x
  #pack .font.frame2 -side bottom
}

proc mkEntry {} {
  global usernom
  global XLock_program
  mkDialog option {User Options} \
  {"user name" usernom} \
  {"program name" XLock_program}
}

proc Affopts { device } {

#options booleans
  global XLock_mono
  global nolock
  global remote
  global allowroot
  global enablesaver
  global allowaccess
  global grabmouse
  global echokeys
  global usefirst
  global install
  global sound
  global timeelapsed
  global usefirst
  global wireframe
  global use3d
  global trackmouse

  global fgcolor
  global bgcolor
  global ftname
  global mftname

  global usernom
  global passmot
  global XLock_validate
  global XLock_invalid
  global XLock_program
  global geometrie
  global icogeometrie
  global XLock_info
  global messagesfile
  global messagefile
  global bitmap

  set linecommand "xlock "

  if {$device == 1} {append linecommand "-inwindow "} elseif {$device == 2} {append linecommand "-inroot "}
  if {$bgcolor!=""} {append linecommand "-bg $bgcolor "}
  if {$fgcolor!=""} {append linecommand "-fg $fgcolor "}
  if {$ftname!=""} {append linecommand "-font $ftname "}
  if {$mftname!=""} {append linecommand "-messagefont $mftname "}
#entry action
  if {$usernom!=""} {append linecommand "-username $usernom "}
  if {$passmot!=""} {append linecommand "-password $passmot "}
  if {$XLock_validate!=""} {append linecommand "-validate $XLock_validate "}
  if {$XLock_invalid!=""} {append linecommand "-invalid $XLock_invalid "}
  if {$XLock_program!=""} {append linecommand "-program $XLock_program "}
  if {$geometrie!=""} {append linecommand "-geometry $geometrie "}
  if {$icogeometrie!=""} {append linecommand "-icongeometry $icogeometrie "}
  if {$messagesfile!=""} {append linecommand "-messagesfile $messagesfile "}
  if {$bitmap!=""} {append linecommand "-bitmap $bitmap "}
  if {$icogeometrie!=""} {append linecommand "-icongeometry $icogeometrie "}
  if {$XLock_info!=""} {append linecommand "-info $XLock_info "}
#check actions
  if { $XLock_mono == 1 } {append linecommand "-mono "}
  if { $install == 1 } {append linecommand "-install "}
  if { $sound == 1 } {append linecommand "-sound "}
  if { $timeelapsed == 1 } {append linecommand "-timeelapsed "}
  if { $usefirst == 1 } {append linecommand "-usefirst "}
  if { $wireframe == 1 } {append linecommand "-wireframe "}
  if { $use3d == 1 } {append linecommand "-use3d "}
  if { $trackmouse == 1 } {append linecommand "-trackmouse "}
  if { $nolock == 1 } {append linecommand "-nolock "}
  if { $remote == 1 } {append linecommand "-remote "}
  if { $allowroot == 1 } {append linecommand "-allowroot "}
  if { $enablesaver == 1 } {append linecommand "-enablesaver "}
  if { $allowaccess == 1 } {append linecommand "-allowaccess "}
  if { $grabmouse == 1 } {append linecommand "-grabmouse "}
  if { $echokeys == 1 } {append linecommand "-echokeys "}
  if { $usefirst == 1 } {append linecommand "-usefirst "}
  append linecommand "-mode "
  append linecommand [.listscrol.list get [eval .listscrol.list curselection]]
  puts $linecommand
  eval exec $linecommand
}

proc load_ressource { } {
  global XLock_invalid
  global XLock_validate
  global XLock_info
  global XLock_program

set filename ""

openfilesel filename

set f [ open $filename r ]
while { ! [eof $f ] } {
	gets $f line
       	switch -regexp $line {
	{^XLock\.[a-zA-Z]+:} {
		if { [ regexp -nocase {\: $} tyty ]} {
		}
	regsub {^} $line "set " line2
	regsub {XLock\.} $line2 "XLock_" line3
	regsub {: } $line3 " \"" line4
	regsub {on$} $line4 "1" line5
	regsub {off$} $line4 "0" line5
	eval "$line5\""
	}
	{^XLock\.[a-zA-Z]+\.[a-zA-Z]+:[ \t]*[a-zA-Z0-9]+$} {
	regsub {^} $line "set " line2
	regsub {XLock\.} $line2 "XLock_" line3
	regsub {\.} $line3 "(" line4
	regsub {: } $line4 ") \"" line5
	eval "$line5\""
	}
}
}
}

# Creation of GUI

wm title . "xlock launcher"
. configure -cursor top_left_arrow
frame .menu -relief raised -borderwidth 1
menubutton .menu.button -text "switches" -menu .menu.button.check
pack .menu -side top -fill x

global XLock_mono
global sound
global install
global nolock
global remote
global allowroot
global enablesaver
global allowaccess
global grabmouse
global echokeys
global usefirst

global usernom
global passmot
global geometrie
global icogeometrie
global XLock_info

# Creation of GUI

#Creation of  menu
set fileressource ""

menubutton .menu.buttonf -text "file" -menu .menu.buttonf.file
menu .menu.buttonf.file
set FILE .menu.buttonf.file
$FILE add command -label "Load ressource" -command "load_ressource"
$FILE add command -label "exit" -command "exit"

menu .menu.button.check
set CHECK .menu.button.check

#menu with les check buttons
$CHECK add check -label "mono" -variable XLock_mono
$CHECK add check -label "nolock" -variable nolock
$CHECK add check -label "remote" -variable remote
$CHECK add check -label "allowroot" -variable allowroot
$CHECK add check -label "enablesaver" -variable enablesaver
$CHECK add check -label "allowaccess" -variable allowaccess
$CHECK add check -label "grabmouse" -variable grabmouse
$CHECK add check -label "echokeys" -variable echokeys
$CHECK add check -label "usefirst" -variable usefirst
$CHECK add check -label "install" -variable install
$CHECK add check -label "sound" -variable sound
$CHECK add check -label "timeelapsed" -variable timeelapsed
$CHECK add check -label "usefirst" -variable usefirst
$CHECK add check -label "wireframe" -variable wireframe
$CHECK add check -label "use3d" -variable use3d
$CHECK add check -label "trackmouse" -variable trackmouse

menubutton .menu.button2 -text "options" -menu .menu.button2.options
menu .menu.button2.options
set OPTIONS .menu.button2.options
#les options
$OPTIONS add command -label "generals options" -command "mkEntry"
$OPTIONS add command -label "font to use for password prompt" -command "mkFont FONT"
$OPTIONS add command -label "font for a specific mode" -command "mkFont MFONT"


$OPTIONS add command -label "geometry options" -command "mkGeometry"
$OPTIONS add command -label "file options" -command "mkFileOption"
$OPTIONS add command -label "message options" -command "mkMessage"

#Color
menubutton .menu.button4 -text "color" -menu .menu.button4.color
menu .menu.button4.color
set COLOR .menu.button4.color
#if {$tk_version < 4} then {
#$COLOR add command -label "foreground options for password" -command "mkColor FG"
#$COLOR add command -label "background options for password" -command "mkColor BG"
#}
#else {
$COLOR add command -label "foreground options for password" -command "tk_chooseColor"
$COLOR add command -label "background options for password" -command "tk_chooseColor"
#}
menubutton .menu.button3 -text "help" -menu .menu.button3.help
menu .menu.button3.help
set HELP .menu.button3.help
$HELP add command -label "about xlock" -command "Helpxlock"
$HELP add command -label "about author" -command "mkAuthor"

pack .menu.buttonf .menu.button  .menu.button2 .menu.button4 -side left
pack .menu.button3 -side right

#---------------------------
#creation de la liste
#---------------------------
frame .listscrol -borderwidth 4 -relief ridge
set LISTSCROL .listscrol
scrollbar $LISTSCROL.scroll -relief sunken -command "$LISTSCROL.list yview"
listbox $LISTSCROL.list -yscroll  "$LISTSCROL.scroll set"

#---------------------------
#insert all modes in list
#---------------------------
$LISTSCROL.list  insert 0 \
ant\
atlantis\
ball\
bat\
blot\
bouboule\
bounce\
braid\
bubble\
bubble3d\
bug\
cage\
cartoon\
clock\
coral\
crystal\
daisy\
dclock\
decay\
deco\
demon\
dilemma\
discrete\
drift\
euler2d\
eyes\
fadeplot\
flag\
flame\
flow\
forest\
galaxy\
gears\
goop\
grav\
helix\
hop\
hyper\
ico\
ifs\
image\
invert\
juggle\
julia\
kaleid\
kumppa\
lament\
laser\
life\
life1d\
life3d\
lightning\
lisa\
lissie\
loop\
lyapunov\
mandelbrot\
marquee\
matrix\
maze\
moebius\
morph3d\
mountain\
munch\
nose\
pacman\
penrose\
petal\
pipes\
puzzle\
pyro\
qix\
roll\
rotor\
rubik\
shape\
sierpinski\
skewb\
slip\
solitare\
space\
sphere\
spiral\
spline\
sproingies\
stairs\
star\
starfish\
strange\
superquadrics\
swarm\
swirl\
t3d\
tetris\
thornbird\
tik_tak\
triangle\
tube\
turtle\
vines\
voters\
wator\
wire\
world\
worm\
xcl\
xjack\
blank\
bomb\
random

pack $LISTSCROL.scroll -side right -fill y
pack $LISTSCROL.list -side left -expand yes -fill both
pack $LISTSCROL  -fill both -expand yes

frame .buttons -borderwidth 4 -relief ridge
set BUTTON .buttons
button $BUTTON.launch -text "Launch"  -command "Affopts 0"
button $BUTTON.launchinW -text "Launch in Window" -command "Affopts 1"
button $BUTTON.launchinR -text "Launch in Root" -command "Affopts 2"
button $BUTTON.quit -text Quit -command "exit"
pack  $BUTTON.launch $BUTTON.launchinW $BUTTON.launchinR -side left
pack $BUTTON.quit -side right
pack $BUTTON -fill x -side bottom
