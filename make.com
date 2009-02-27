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
$ copy [.bitmap]mazeicon.gen mazeicon.bit
$ copy [.bitmap]lifeicon.gen lifeicon.bit
$ cc bat.c
$ cc blank.c
$ cc blot.c
$ cc bounce.c
$ cc flame.c
$ cc galaxy.c
$ cc grav.c
$ cc helix.c
$ cc hopalong.c
$ cc hsbramp.c
$ cc hyper.c
$ cc kaleid.c
$ cc life.c
$ cc maze.c
$ cc pyro.c
$ cc qix.c
$ cc rect.c
$ cc resource.c
$ cc rock.c
$ cc rotor.c
$ cc sphere.c
$ cc spline.c
$ cc swarm.c
$ cc world.c
$ cc worm.c
$ cc/noopt xlock.c
$ cc usleep.c
$ set noverify
$ endif
$!
$ set verify
$ link/map xlock,hsbramp,resource,usleep,-
       hopalong,qix,life,blank,-
       swarm,rotor,pyro,flame,worm,-
       spline,maze,sphere,hyper,-
       helix,rock,blot,-
       grav,bounce,world,rect,bat,-
       sys$library:vaxcrtl/lib,-
       sys$library:ucx$ipc/lib,-
       sys$input/opt
sys$share:decw$dxmlibshr/share
sys$share:decw$xlibshr/share
$ set noverify
$ exit
