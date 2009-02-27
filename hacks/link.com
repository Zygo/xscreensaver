$! We fisrt test the version of DECW/Motif ; if 1.2 we need to link with new
$! X11R5 libraries
$@sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$ if f$extract(4,3,decw$version).eqs."1.2"
$ then
$! DECW/Motif 1.2 : link with X11R5 libraries
$ link/exe=attraction.exe  screenhack,attraction,vms_12.opt/opt
$ link/exe=blitspin.exe    screenhack,blitspin,vms_12.opt/opt
$ link/exe=decayscreen.exe screenhack,decayscreen,vms_12.opt/opt
$ link/exe=flame.exe       screenhack,flame,vms_12.opt/opt
$ link/exe=greynetic       screenhack,greynetic,vms_12.opt/opt
$ link/exe=halo.exe        screenhack,halo,vms_12.opt/opt
$ link/exe=helix.exe       screenhack,helix,vms_12.opt/opt
$ link/exe=hopalong.exe    screenhack,hopalong,vms_12.opt/opt
$ link/exe=hypercube.exe   screenhack,hypercube,vms_12.opt/opt
$ link/exe=imsmap.exe      screenhack,imsmap,vms_12.opt/opt
$ link/exe=lmorph.exe        screenhack,lmorph,vms_12.opt/opt
$ link/exe=maze.exe        screenhack,maze,vms_12.opt/opt
$ link/exe=noseguy.exe     screenhack,noseguy,vms_12.opt/opt
$ link/exe=pedal.exe        screenhack,pedal,vms_12.opt/opt
$ link/exe=pyro.exe        screenhack,pyro,vms_12.opt/opt
$ link/exe=qix.exe         screenhack,qix,vms_12.opt/opt
$ link/exe=rocks.exe       screenhack,rocks,vms_12.opt/opt
$ link/exe=rorschach.exe   screenhack,rorschach,vms_12.opt/opt
$ link/exe=slidescreen.exe screenhack,slidescreen,vms_12.opt/opt
$ link/exe=xroger.exe      screenhack,xroger-hack,vms_12.opt/opt
$ else
$! Else, link with X11R4 libraries
$ link/exe=attraction.exe  screenhack,attraction,vms.opt/opt
$ link/exe=blitspin.exe    screenhack,blitspin,vms.opt/opt
$ link/exe=decayscreen.exe screenhack,decayscreen,vms.opt/opt
$ link/exe=flame.exe       screenhack,flame,vms.opt/opt
$ link/exe=greynetic       screenhack,greynetic,vms.opt/opt
$ link/exe=halo.exe        screenhack,halo,vms.opt/opt
$ link/exe=helix.exe       screenhack,helix,vms.opt/opt
$ link/exe=hopalong.exe    screenhack,hopalong,vms.opt/opt
$ link/exe=hypercube.exe   screenhack,hypercube,vms.opt/opt
$ link/exe=imsmap.exe      screenhack,imsmap,vms.opt/opt
$ link/exe=lmorph.exe        screenhack,lmorph,vms.opt/opt
$ link/exe=maze.exe        screenhack,maze,vms.opt/opt
$ link/exe=noseguy.exe     screenhack,noseguy,vms.opt/opt
$ link/exe=pedal.exe        screenhack,pedal,vms.opt/opt
$ link/exe=pyro.exe        screenhack,pyro,vms.opt/opt
$ link/exe=qix.exe         screenhack,qix,vms.opt/opt
$ link/exe=rocks.exe       screenhack,rocks,vms.opt/opt
$ link/exe=rorschach.exe   screenhack,rorschach,vms.opt/opt
$ link/exe=slidescreen.exe screenhack,slidescreen,vms.opt/opt
$ link/exe=xroger.exe      screenhack,xroger-hack,vms.opt/opt
$ endif
