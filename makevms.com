$! Build Xscreensaver under OpenVMS V 6.x system  with DEC C 5.x compiler
$!
$ copy /log CONFIG.H-VMS CONFIG.H
$!
$! Architecture type test (VAX < 1024 <= Alpha AXP)
$!
$ if f$getsyi("HW_MODEL").ge.1024
$ then
$! 
$! Alpha AXP
$! build Utils library
$!
$   set def [.UTILS]
$   @COMPILE_AXP
$   set def [-]
$!
$! build graphics hacks
$!
$   set def [.HACKS]
$   @COMPILE_AXP
$   @LINK_AXP
$   set def [-]
$!
$! Build Xscreensaver & Xscreensaver-command
$!
$   set def [.DRIVER]
$   @COMPILE_AXP
$   @LINK_AXP
$   set def [-]
$ else
$! 
$! Good old VAX
$! build Utils library
$!
$   set def [.UTILS]
$   @COMPILE_DECC
$   set def [-]
$!
$! build graphics hacks
$!
$   set def [.HACKS]
$   @COMPILE_DECC
$   @LINK_DECC
$   set def [-]
$!
$! Build Xscreensaver & Xscreensaver-command
$!
$   set def [.DRIVER]
$   @COMPILE_DECC
$   @LINK_DECC
$   set def [-]
$ endif
$!
$! DCL symbols definition 
$!
$ @SETUP
$ exit
