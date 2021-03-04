# If you're debugging xscreensaver and you are running a virtual root window
# manager, you'd better let the process handle these signals: it remaps the
# virtual root window when they arrive.  If you don't do this, your window
# manager will be hosed.
#
# Also, gdb copes badly with breakpoints in functions that are called on the
# other side of a fork().  The Trace/BPT traps cause the spawned process to
# die.
#
#handle 1 pass nostop
#handle 3 pass nostop
#handle 4 pass nostop
#handle 6 pass nostop
#handle 7 pass nostop
#handle 8 pass nostop
#handle 9 pass nostop
#handle 10 pass nostop
#handle 11 pass nostop
#handle 12 pass nostop
#handle 13 pass nostop
#handle 15 pass nostop
#handle 19 pass nostop
set env MallocGuardEdges 1
set env MallocPreScribble 1
set env MallocScribble 1
b exit
set args -debug
