#!/usr/local/bin/wish -f

#charles vidal 1997 <cvidal@newlog.com>

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
set prgname ""
set geometrie ""
set icogeometrie ""
set passstring ""
set infostring ""
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

proc whichcolor { which } {
global fgcolor
global bgcolor
 if {$which== "FG" } {set fgcolor [.color.frame.names get [.color.frame.names curselection]];}
  if {$which == "BG"} {set bgcolor [.color.frame.names get [.color.frame.names curselection]];}
 if {$which == "RESETFG"} {set fgcolor ""}
 if {$which == "RESETBG"} {set bgcolor ""}
}
proc mkColor { what } {
     toplevel .color
	wm title .color "Color"
    frame .color.frame -borderwidth 10
    frame .color.frame2 -borderwidth 10
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
# Help ...
proc Helpxlock {} {
     toplevel .help
	wm title .help "Help About Xlock"
scrollbar .help.s -orient vertical -command {.help.t yview}
pack .help.s -side right -fill y
text .help.t -yscrollcommand {.help.s set} -wrap word -width 60 -height 30 \
        -setgrid 1
pack .help.t -expand y -fill both
.help.t tag configure big -font -Adobe-Courier-Bold-R-Normal-*-140-*
.help.t tag configure verybig -font -Adobe-Courier-Bold-R-Normal-*-240-*
.help.t tag configure color2 -foreground red
.help.t tag configure underline -underline on
insertWithTags .help.t  "Xlock Help\n"  verybig
insertWithTags .help.t "\n"
.help.t insert end { Locks the X server still the user enters their pass\
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
in the small icon version of the changing pattern.
}
.help.t insert end "\n"
insertWithTags .help.t  "OPTIONS\n"  big
insertWithTags .help.t  "\n\n-display "  underline
insertWithTags .help.t {The option sets  the  X11  display  to  lock.\
            xlock  locks all available screens on a given server,\
            and restricts you to locking only a local server such\
            as  unix::00,,  localhost::00,,  or  ::00  unless you set the\
     -remote option.}
.help.t insert end "\n"
insertWithTags .help.t  "\n\n-name "  underline
insertWithTags .help.t  { is used instead of XLock  when  looking\
            for resources to configure xlock.}
.help.t insert end "\n"
insertWithTags .help.t  "\n\n-mode "  underline
insertWithTags .help.t {As  of  this  writing there are 48 display modes sup\
            ported (plus one more for random selection of one  of\
            the 48).}
insertWithTags .help.t  "\n\n-delay "  underline
insertWithTags .help.t {It simply sets the number  of  microseconds to  delay  between  batches  of animations.  In blank mode, it is important to set this to some small  number  of  microseconds,  because the keyboard and mouse are only checked after each delay, so you cannot set  the delay  too high, but a delay of zero would needlessly consume cpu checking for mouse and keyboard input  in a tight loop, since blank mode has no work to do.}
insertWithTags .help.t  "\n\n-saturation"  underline
insertWithTags .help.t { This option sets saturation  of  the  color
 ramp .  0 is grayscale and 1 is very rich color. 0.4 is a nice pastel.}
insertWithTags .help.t  "\n\n-nice"  underline
insertWithTags .help.t { This option sets system nicelevel  of  the  xlock .}
insertWithTags .help.t  "\n\n-timeout "  underline
insertWithTags .help.t { This option sets the number of second before the password screen will time out.}
insertWithTags .help.t  "\n\n-lockdelay "  underline
insertWithTags .help.t { This  option  sets  the  number  of second before  the  screen  needs a password to be unlocked.  Good for use with an autolocking mechanism like xautolock(1).}
insertWithTags .help.t  "\n\n-font "  underline
insertWithTags .help.t { Ths option  sets  the  font  to be used on the prompt screen.}
insertWithTags .help.t  "\n\n-fg "  underline
insertWithTags .help.t { This option sets the color of the text on the password screen.}
insertWithTags .help.t  "\n\n-bg "  underline
insertWithTags .help.t { This option sets the color of the background on the password screen.}
insertWithTags .help.t  "\n\n-usename "  underline
insertWithTags .help.t { usename is shown in front of user  name,  defaults to "Name: ".}
insertWithTags .help.t  "\n\n-password "  underline
insertWithTags .help.t { the password prompt string, defaults to "Password: ".}
insertWithTags .help.t  "\n\n-info "  underline
insertWithTags .help.t { the informational message  to  tell  the user  what  to  do,  defaults  to  "Enter password to
 unlock; select icon to lock.".}
insertWithTags .help.t  "\n\n-validate "  underline
insertWithTags .help.t { the message shown  while  validating  the password, defaults to "Validating login..."}
insertWithTags .help.t  "\n\n-invalid"  underline
insertWithTags .help.t {
the  message  shown  when  password  is invalid, defaults to "Invalid login."}
insertWithTags .help.t  "\n\n-geometry "  underline
insertWithTags .help.t { This option sets the size and offset  of the  lock  window  (normally the entire screen).  The entire screen format is still used for  entering  the password.   The  purpose  is  to  see the screen even though it is locked.  This should be used  with  caution since many of the modes will fail if the windows are far from square or are too small  (size  must  be greater  than  0x0).   This  should also be used with esaver to protect screen from phosphor burn.}
insertWithTags .help.t  "\n\n-icongeometry "  underline
insertWithTags .help.t { this option sets  the  size  of  the iconic screen (normally 64x64) seen when entering the password.  This should be  used  with  caution  since many  of  the  modes will fail if the windows are far from square or are too small (size  must  be  greater than  0x0).   The  greatest  size  is 256x256.  There should be some limit  so  users  could  see  who  has locked  the  screen.  Position information of icon is ignored.}
insertWithTags .help.t  "\n\n-forceLogout "  underline
insertWithTags .help.t { This option sets the  auto-logout.  This  might not be enforced depending how your system is configured.}
insertWithTags .help.t  "\n\nOPTIONS BOOLEAN\n"  big
    insertWithTags .help.t "\n-/+mono " underline
    insertWithTags .help.t " turn on/off monochrome override"
    insertWithTags .help.t "\n-/+nolock" underline    
     insertWithTags .help.t " turn on/off no password required mode"
    insertWithTags .help.t "\n-/+remote"  underline           
insertWithTags .help.t " turn on/off remote host access"
    insertWithTags .help.t "\n-/+allowroot" underline      
     insertWithTags .help.t " turn on/off allow root password mode (ignored)"
    insertWithTags .help.t "\n-/+enablesaver"  underline      
  insertWithTags .help.t " turn on/off enable X server screen saver"
    insertWithTags .help.t "\n-/+allowaccess"  underline    
    insertWithTags .help.t " turn on/off allow new clients to connect"
    insertWithTags .help.t "\n-/+grabmouse  "  underline      
  insertWithTags .help.t " turn on/off grabbing of mouse and keyboard"
    insertWithTags .help.t "\n-/+echokeys "  underline      
    insertWithTags .help.t " turn on/off echo '?' for each password key"
    insertWithTags .help.t "\n-/+usefirst"  underline        
   insertWithTags .help.t " turn on/off using the first char typed in password"
    insertWithTags .help.t "\n-/+v  "   underline         
      insertWithTags .help.t " turn on/off verbose mode"
    insertWithTags .help.t "\n-/+inwindow"  underline       
    insertWithTags .help.t " turn on/off making xlock run in a window"
    insertWithTags .help.t "\n-/+mono "   underline           
  insertWithTags .help.t " turn on/off monochrome override"
    insertWithTags .help.t "\n-/+inroot"  underline        
     insertWithTags .help.t " turn on/off making xlock run in the root window"
    insertWithTags .help.t "\n-/+timeelapsed "  underline      
 insertWithTags .help.t " turn on/off clock"
    insertWithTags .help.t "\n-/+install " underline        
 insertWithTags .help.t " whether to use private colormap if needed (yes/no)"

        button .help.ok -text OK -command "destroy .help"
	pack  .help.ok
}
# Create toplevel Author and Maintener.
proc mkAuthor {} {
     toplevel .author
    wm title .author "Author and Maintener of xlock"
    frame .author.frame -borderwidth 10
    set w .author.frame

    label $w.msg1   -text "Maintained by: David A. Bagley (bagleyd@hertz.njit.edu)"
label $w.msg2   -text "Original Author: Patrick J. Naughton (naughton@eng.sun.com)" 
label $w.msg3   -text "Mailstop 21-14 Sun Microsystems Laboratories,"
 label $w.msg4   -text "Inc.  Mountain View, CA  94043 15//336-1080"
label $w.msg5   -text "with many additional contributors"
        pack $w.msg1 $w.msg2 $w.msg3 $w.msg4 $w.msg5 -side top

        label $w.msg6   -text "xlock.tcl\n created by charles VIDAL\n (author of flag mode and xmlock launcher )"
        pack $w.msg6 -side top

    button .author.ok -text OK -command "destroy .author"
    pack $w .author.ok 
}

#no influence with xlock
proc mkMessage {} {
global passstring 
global infostring 
     toplevel .message
	wm title .message "Message Options"
    frame .message.frame -borderwidth 10
    frame .message.frame2 -borderwidth 10
    set w .message.frame

    frame $w.messagename
    label $w.messagename.l1 -text "password string" 
    entry $w.messagename.e1 -relief sunken -textvariable passstring
    frame $w.specmessage
    label $w.specmessage.l2 -text "info string" 
    entry $w.specmessage.e2 -relief sunken -textvariable infostring
    pack $w.messagename $w.specmessage 
    pack $w.messagename.l1 -side left 
    pack $w.specmessage.l2 -side left 
    pack $w.messagename.e1 $w.specmessage.e2  -side top -pady 5 -fill x
    button .message.frame.ok -text OK -command "destroy .message"
    button .message.frame.cancel -text Cancel -command "destroy .message"
    pack $w .message.frame.ok .message.frame.cancel -side left -fill x
	pack .message.frame2  -side bottom

}
#no influence with xlock
proc mkGeometry {} {
	global geometrie
	global icogeometrie
     toplevel .geometry
	wm title .geometry "Geometry  Options"
    frame .geometry.frame -borderwidth 10
    frame .geometry.frame2 -borderwidth 10
    set w .geometry.frame

    frame $w.geometryname
    label $w.geometryname.l1 -text "geometry" 
    entry $w.geometryname.e1 -relief sunken -textvariable geometrie
    frame $w.specgeometry
    label $w.specgeometry.l2 -text "icon geometry" 
    entry $w.specgeometry.e2 -relief sunken -textvariable icogeometrie
    pack $w.geometryname $w.specgeometry 
    pack $w.geometryname.l1 -side left 
    pack $w.specgeometry.l2 -side left 
    pack $w.geometryname.e1 $w.specgeometry.e2  -side top -pady 5 -fill x
    button .geometry.frame2.ok -text OK -command "destroy .geometry"
    button .geometry.frame2.cancel -text Cancel -command "destroy .geometry"
    pack $w .geometry.frame2.ok .geometry.frame2.cancel -side left -fill x 
	pack .geometry.frame2 -side bottom
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
	label  .font.label -text "ABCDEFGHIJKabedfghijkmnopq" 
    frame .font.frame -borderwidth 10 
    frame .font.frame2 -borderwidth 10
    set w .font.frame
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
	pack .font.frame2.ok .font.frame2.cancel .font.frame2.reset -side left -fill x
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

#no influence with xlock
proc mkEntry {} {
global usernom
global passmot
global prgname
     toplevel .option
	wm title .option "Entry options"
    frame .option.frame -borderwidth 10
    frame .option.frame2 -borderwidth 10
    set w .option.frame

    frame $w.username
    label $w.username.l1 -text "user name" 
    entry $w.username.e1 -relief sunken  -textvariable usernom
    frame $w.password
    label $w.password.l2 -text "password" 
    entry $w.password.e2 -relief sunken -textvariable passmot
    frame $w.info
    label $w.info.l3 -text "program name" 
    entry $w.info.e3 -relief sunken -textvariable prgname
    pack $w.username $w.password $w.info 
    pack $w.username.l1 -side left 
    pack $w.password.l2 -side left 
    pack $w.info.l3 -side left 
    pack $w.username.e1 $w.password.e2 $w.info.e3 -side top -pady 5 -fill x
    button .option.frame2.ok -text OK -command "destroy .option"
    button .option.frame2.cancel -text Cancel -command "destroy .option"
    pack $w .option.frame2.ok .option.frame2.cancel -side left -fill x 

	pack .option.frame2 -side bottom
}

proc Affopts { device } {

#options booleans
global mono
global nolock
global remote
global allowroot
global enablesaver
global allowaccess
global grabmouse
global echokeys
global usefirst



global fgcolor
global bgcolor
global ftname
global mftname

global usernom 
global passmot 
global prgname
global geometrie 
global icogeometrie 
global passstring 
global infostring 

set linecommand "xlock "

if {$device == 1} {append linecommand "-inwindow "} elseif {$device == 2} {append linecommand "-inroot "}
if {$bgcolor!=""} {append linecommand "-bg $bgcolor "}
if {$fgcolor!=""} {append linecommand "-fg $fgcolor "}
if {$ftname!=""} {append linecommand "-font $ftname "}
if {$mftname!=""} {append linecommand "-mfont $mftname "}
if {$usernom!=""} {append linecommand "-username $usernom "}
if {$passmot!=""} {append linecommand "-password $passmot "}
if {$prgname!=""} {append linecommand "-program $prgname "}
if {$geometrie!=""} {append linecommand "-geometry $geometrie "}
if {$icogeometrie!=""} {append linecommand "-icongeometry $icogeometrie "}
if {$passstring!=""} {append linecommand "-password $passstring "}
if {$infostring!=""} {append linecommand "-info $infostring "}
if { $mono == 1 } {append linecommand "-mono "}
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

# Creation of GUI 

wm title . "xlock launcher"
. configure -cursor top_left_arrow
frame .menu -relief raised -borderwidth 1
menubutton .menu.button -text "switches" -menu .menu.button.check 
pack .menu -side top -fill x 

global mono
global nolock
global remote
global allowroot
global enablesaver
global allowaccess
global grabmouses
global echokeys
global usefirst

global usernom 
global passmot
global geometrie
global icogeometrie
global passstring 
global infostring 

# Creation of GUI 

#Creation of  menu 

menu .menu.button.check
set CHECK .menu.button.check

#menu with les check buttons
$CHECK add check -label "mono" -variable mono
$CHECK add check -label "nolock" -variable nolock
$CHECK add check -label "remote" -variable remote
$CHECK add check -label "allowroot" -variable allowroot
$CHECK add check -label "enablesaver" -variable enablesaver
$CHECK add check -label "allowaccess" -variable allowaccess
$CHECK add check -label "grabmouse" -variable grabmouse 
$CHECK add check -label "echokeys" -variable echokeys
$CHECK add check -label "usefirst" -variable usefirst

menubutton .menu.button2 -text "options" -menu .menu.button2.options
menu .menu.button2.options
set OPTIONS .menu.button2.options
#les options
$OPTIONS add command -label "generals options" -command "mkEntry"
$OPTIONS add command -label "font to use for password prompt" -command "mkFont FONT"
$OPTIONS add command -label "font for a specific mode" -command "mkFont MFONT"


$OPTIONS add command -label "geometry options" -command "mkGeometry"
$OPTIONS add command -label "message options" -command "mkMessage"

#Color
menubutton .menu.button4 -text "color" -menu .menu.button4.color
menu .menu.button4.color
set COLOR .menu.button4.color
$COLOR add command -label "Foreground options for password" -command "mkColor FG"
$COLOR add command -label "Background options for password" -command "mkColor BG"

menubutton .menu.button3 -text "help" -menu .menu.button3.help
menu .menu.button3.help
set HELP .menu.button3.help
$HELP add command -label "About xlock" -command "Helpxlock"
$HELP add command -label "About author" -command "mkAuthor"

pack .menu.button  .menu.button2 .menu.button4 -side left
pack .menu.button3 -side right

#creation de la liste 
frame .listscrol -borderwidth 4 -relief ridge 
set LISTSCROL .listscrol
scrollbar $LISTSCROL.scroll -relief sunken -command "$LISTSCROL.list yview"
listbox $LISTSCROL.list -yscroll  "$LISTSCROL.scroll set" 
pack $LISTSCROL.scroll -side right -fill y
pack $LISTSCROL.list -side left -expand yes -fill both 
pack $LISTSCROL -fill both

$LISTSCROL.list  insert 0 \
ant ball bat blot \
bouboule bounce braid bug \
cartoon clock crystal \
daisy dclock demon drift escher eyes \
fadeplot flag flame forest \
galaxy gears geometry grav \
helix hop hyper \
ico ifs image julia kaleid \
laser life life1d life3d \
lightning lisa lissie loop \
marquee maze morph3d mountain munch nose \
pacman penrose petal pipes puzzle pyro \
qix roll rotor rubik \
shape sierpinski slip \
sphere spiral spline sproingies \
star strange superquadrics swarm swirl \
triangle tube turtle vines voters \
wator wire world worm \
blank bomb random

frame .buttons -borderwidth 4 -relief ridge
set BUTTON .buttons
button $BUTTON.launch -text "Launch"  -command "Affopts 0"
button $BUTTON.launchinW -text "Launch in Window" -command "Affopts 1"
button $BUTTON.launchinR -text "Launch in Root" -command "Affopts 2"
button $BUTTON.quit -text Quit -command "exit"
pack  $BUTTON.launch $BUTTON.launchinW $BUTTON.launchinR -side left
pack $BUTTON.quit -side right
pack $BUTTON -fill x -side bottom

