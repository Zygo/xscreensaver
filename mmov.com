$ write sys$output "Ignore all informationals when compiling readavi.c"
$ define/nolog machine 		   MMOV$INCLUDE:,[]
$ define/nolog mme 		   MMOV$INCLUDE:,[]
$ define/nolog x11 		   MMOV$INCLUDE:,DECW$INCLUDE
$ define/nolog decc$user_include   mme,decw$include:
$ define/nolog sys 		   decc$user_include
$ define/nolog decc$system_include decc$user_include
$ cc/define=(_VMS_V6_SOURCE,__VMS,mme_BLD,IPC_VMS,Long_bit=64,DEC)/-
    standard=vaxc readavi
$ cc/define=(_VMS_V6_SOURCE,__VMS,mme_BLD,IPC_VMS,Long_bit=64,DEC)/-
    standard=vaxc [-.xlock]vms_mmov
$ define/nolog x11 		   DECW$INCLUDE
