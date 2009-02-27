            TOP = ./../..
             MV = mv
             RM = rm -f
        UTILSRC = $(TOP)/util
       IMAKESRC = $(UTILSRC)/imake
       IRULESRC = $(UTILSRC)/imake.includes
          IMAKE = $(IMAKESRC)/imake
      IMAKE_CMD = $(NEWTOP)$(IMAKE) -TImake.tmpl \
                        -I$(NEWTOP)$(IRULESRC) \
                        -s Makefile

Makefile:: Imakefile
        -$(RM) Makefile.bak; $(MV) Makefile Makefile.bak
        $(IMAKE_CMD) -DTOPDIR=$(TOP)
