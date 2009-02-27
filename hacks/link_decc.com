$! We fisrt test the version of DECW/Motif ; if 1.2 we need to link with new
$! X11R5 libraries
$@sys$update:decw$get_image_version sys$share:decw$xlibshr.exe decw$version
$ if f$extract(4,3,decw$version).eqs."1.2"
$ then
$! DECW/Motif 1.2 : link with X11R5 libraries
$ link/exe=attraction.exe  screenhack,attraction,vms_decc_12.opt/opt
$ link/exe=blitspin.exe    screenhack,blitspin,vms_decc_12.opt/opt
$ link/exe=decayscreen.exe screenhack,decayscreen,vms_decc_12.opt/opt
$ link/exe=flame.exe       screenhack,flame,vms_decc_12.opt/opt
$ link/exe=greynetic       screenhack,greynetic,vms_decc_12.opt/opt
$ link/exe=halo.exe        screenhack,halo,vms_decc_12.opt/opt
$ link/exe=helix.exe       screenhack,helix,vms_decc_12.opt/opt
$ link/exe=hopalong.exe    screenhack,hopalong,vms_decc_12.opt/opt
$ link/exe=hypercube.exe   screenhack,hypercube,vms_decc_12.opt/opt
$ link/exe=imsmap.exe      screenhack,imsmap,vms_decc_12.opt/opt
$ link/exe=lmorph.exe        screenhack,lmorph,vms_decc_12.opt/opt
$ link/exe=maze.exe        screenhack,maze,vms_decc_12.opt/opt
$ link/exe=noseguy.exe     screenhack,noseguy,vms_decc_12.opt/opt
$ link/exe=pedal.exe        screenhack,pedal,vms_decc_12.opt/opt
$ link/exe=pyro.exe        screenhack,pyro,vms_decc_12.opt/opt
$ link/exe=qix.exe         screenhack,qix,vms_decc_12.opt/opt
$ link/exe=rocks.exe       screenhack,rocks,vms_decc_12.opt/opt
$ link/exe=rorschach.exe   screenhack,rorschach,vms_decc_12.opt/opt
$ link/exe=slidescreen.exe screenhack,slidescreen,vms_decc_12.opt/opt
$ link/exe=xroger.exe      screenhack,xroger-hack,vms_decc_12.opt/opt
$ else
$! Else, link with X11R4 libraries
$ link/exe=attraction.exe  screenhack,attraction,vms_decc.opt/opt
$ link/exe=blitspin.exe    screenhack,blitspin,vms_decc.opt/opt
$ link/exe=decayscreen.exe screenhack,decayscreen,vms_decc.opt/opt
$ link/exe=flame.exe       screenhack,flame,vms_decc.opt/opt
$ link/exe=greynetic       screenhack,greynetic,vms_decc.opt/opt
$ link/exe=halo.exe        screenhack,halo,vms_decc.opt/opt
$ link/exe=helix.exe       screenhack,helix,vms_decc.opt/opt
$ link/exe=hopalong.exe    screenhack,hopalong,vms_decc.opt/opt
$ link/exe=hypercube.exe   screenhack,hypercube,vms_decc.opt/opt
$ link/exe=imsmap.exe      screenhack,imsmap,vms_decc.opt/opt
$ link/exe=lmorph.exe        screenhack,lmorph,vms_decc.opt/opt
$ link/exe=maze.exe        screenhack,maze,vms_decc.opt/opt
$ link/exe=noseguy.exe     screenhack,noseguy,vms_decc.opt/opt
$ link/exe=pedal.exe        screenhack,pedal,vms_decc.opt/opt
$ link/exe=pyro.exe        screenhack,pyro,vms_decc.opt/opt
$ link/exe=qix.exe         screenhack,qix,vms_decc.opt/opt
$ link/exe=rocks.exe       screenhack,rocks,vms_decc.opt/opt
$ link/exe=rorschach.exe   screenhack,rorschach,vms_decc.opt/opt
$ link/exe=slidescreen.exe screenhack,slidescreen,vms_decc.opt/opt
$ link/exe=xroger.exe      screenhack,xroger-hack,vms_decc.opt/opt
$ endif
