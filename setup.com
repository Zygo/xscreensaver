$! Xscreensaver - definition of various DCL symbols
$ set NOON
$ set def [.HACKS]
$ mydisk = f$trnlmn("SYS$DISK")
$ mydir  = mydisk+f$directory()
$ attrac*tion :== $'mydir'attraction
$ blitspin    :== $'mydir'blitspin
$ decays*creen:== $'mydir'decayscreen
$ flame       :== $'mydir'flame
$ greyne*tic  :== $'mydir'greynetic
$ halo        :== $'mydir'halo
$ helix       :== $'mydir'helix
$ hopalong    :== $'mydir'hopalong
$ hypercube   :== $'mydir'hypercube
$ imsmap      :== $'mydir'imsmap
$ lmorph      :== $'mydir'lmorph
$ maze        :== $'mydir'maze
$ noseguy     :== $'mydir'noseguy
$ pedal       :== $'mydir'pedal
$ pyro        :== $'mydir'pyro
$ qix         :== $'mydir'qix
$ rocks       :== $'mydir'rocks
$ rorsch*ach  :== $'mydir'rorschach
$ slidescr*een:== $'mydir'slidescreen
$ xroger      :== $'mydir'xroger
$ set def [-.DRIVER]
$ mydir  = mydisk+f$directory()
$ xscreensaver :== $'mydir'xscreensaver
$ xscreen*command :== $'mydir'xscreensaver-command
$ set def [-]
$ exit 
