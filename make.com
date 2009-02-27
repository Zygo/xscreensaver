$ set verify
$ if p1.eqs."DEBUG" .or. p2.eqs."DEBUG"
$ then
$   cc=="CC/DEB/NOOPT"
$   link=="LINK/DEB"
$ endif
$ if p1.nes."LINK"
$ then
$ define sys sys$library
$!
$ set verify
$ copy [.bitmap]maze-x11.xbm maze.xbm
$ copy [.bitmap]life-x11.xbm life.xbm
$ copy [.bitmap]image-x11.xbm image.xbm
$ cc/noopt xlock.c
$ cc resource.c
$ cc usleep.c
$ cc hsbramp.c
$ cc logout.c
$ cc hopalong.c
$ cc qix.c
$ cc life.c
$ cc swarm.c
$ cc rotor.c
$ cc pyro.c
$ cc flame.c
$ cc worm.c
$ cc spline.c
$ cc maze.c
$ cc sphere.c
$ cc hyper.c
$ cc helix.c
$ cc rock.c
$ cc blot.c
$ cc grav.c
$ cc bounce.c
$ cc world.c
$ cc rect.c
$ cc bat.c
$ cc galaxy.c
$ cc kaleid.c
$ cc wator.c
$ cc life3d.c
$ cc swirl.c
$ cc image.c
$ cc bomb.c
$ cc blank.c
$ set noverify
$ endif
$!
$ set verify
$ link/map xlock,hsbramp,resource,usleep,-
       hopalong,qix,life,-
       swarm,rotor,pyro,flame,worm,-
       spline,maze,sphere,hyper,-
       helix,rock,blot,-
       grav,bounce,world,rect,bat,-
       galaxy,kaleid,wator,life3d,-
       swirl,image,bomb,blank,-
       sys$library:vaxcrtl/lib,-
       sys$library:ucx$ipc/lib,-
       sys$input/opt
sys$share:decw$dxmlibshr/share
sys$share:decw$xlibshr/share
$ set noverify
$ exit
