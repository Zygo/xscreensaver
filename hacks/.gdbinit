set args -geom =800x600+0+0 -sync
set env MallocScribble 1
set env MallocPreScribble 1
set env MallocErrorAbort 1
set env MallocCheckHeapAbort 1
set env MallocGuardEdges 1
#set env MallocCheckHeapStart 130000
#set env MallocCheckHeapEach 1
b screenhack_ehandler
b malloc_error_break
b exit
b abort
