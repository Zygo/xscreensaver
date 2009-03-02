$! Xscreensaver - definition of various DCL symbols
$ set NOON
$ set def [.HACKS]
$ mydisk = f$trnlmn("SYS$DISK")
$ mydir  = mydisk+f$directory()
$ attrac*tion :== $'mydir'attraction
$ blitspin    :== $'mydir'blitspin
$ bouboule    :== $'mydir'bouboule
$ braid       :== $'mydir'braid
$ bubbles     :== $'mydir'bubbles
$ decays*creen:== $'mydir'decayscreen
$ deco        :== $'mydir'deco
$ drift       :== $'mydir'drift
$ fadeplot    :== $'mydir'fadeplot
$ flag        :== $'mydir'flag
$ flame       :== $'mydir'flame
$ forest      :== $'mydir'forest
$ fract       :== $'mydir'fract
$ galaxy      :== $'mydir'galaxy 
$ goop        :== $'mydir'goop
$ grav        :== $'mydir'grav
$ greyne*tic  :== $'mydir'greynetic
$ halo        :== $'mydir'halo
$ helix       :== $'mydir'helix
$ hopalong    :== $'mydir'hopalong
$ hypercube   :== $'mydir'hypercube
$ imsmap      :== $'mydir'imsmap
$ ifs         :== $'mydir'ifs
$ julia       :== $'mydir'julia
$ kaleidescope:== $'mydir'kaleidescope
$ laser       :== $'mydir'laser
$ lightning   :== $'mydir'lightning
$ lisa        :== $'mydir'lisa
$ lmorph      :== $'mydir'lmorph
$ maze        :== $'mydir'maze
$ moire       :== $'mydir'moire
$ munch       :== $'mydir'munch
$ noseguy     :== $'mydir'noseguy
$ pedal       :== $'mydir'pedal
$ penrose     :== $'mydir'penrose
$ pyro        :== $'mydir'pyro
$ qix         :== $'mydir'qix
$ rocks       :== $'mydir'rocks
$ rorsch*ach  :== $'mydir'rorschach
$ sierpinski  :== $'mydir'sierpinski
$ slidescr*een:== $'mydir'slidescreen
$ slip        :== $'mydir'slip
$ sphere      :== $'mydir'sphere
$ spiral      :== $'mydir'spiral
$ starfish    :== $'mydir'starfish
$ strange     :== $'mydir'strange
$ swirl       :== $'mydir'swirl
$ xroger      :== $'mydir'xroger
$ set def [-.DRIVER]
$ mydir  = mydisk+f$directory()
$ xscreensaver :== $'mydir'xscreensaver
$ xscreen*command :== $'mydir'xscreensaver-command
$ set def [-]
$ exit 
