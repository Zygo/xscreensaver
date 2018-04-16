# hacks/images/Makefile --- xscreensaver, Copyright (c) 2018 Jamie Zawinski.

srcdir		= .
UTILS_SRC	= $(srcdir)/../../utils
RM		= rm -f

STAR		= *
TARFILES	= Makefile \
		  $(STAR).png \
		  $(STAR).gif \
		  $(STAR)/$(STAR).png \
		  $(STAR)/$(STAR).asm \
		  $(STAR)/$(STAR).pdb \
		  $(STAR)/$(STAR).pov \
		  $(STAR)/$(STAR).txt \

default:: all

all::
	@if [ ! -d gen ]; then mkdir gen ; fi
	@for f in *.png */*.png ; do					\
	  f2=`echo "$$f" |						\
	      sed -e 's@^.*/@@'						\
		  -e 's@^\(.*\)\.\([^./]*\)$$@gen/\1_\2.h@'` ;		\
	  if [ ! -f "$$f2" -o "$$f" -nt "$$f2" ]; then			\
	    echo $(UTILS_SRC)/bin2c "$$f" "$$f2" ;			\
	         $(UTILS_SRC)/bin2c "$$f" "$$f2" ;			\
	  fi ;								\
	done

clean::
	$(RM) -r gen

echo_tarfiles:
	@echo $(TARFILES)

install:
install-program:
install-man:
install-strip:
uninstall:
uninstall-program:
uninstall-man:
depend:
distdepend:
tags:
