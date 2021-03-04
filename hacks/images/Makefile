# hacks/images/Makefile.in --- xscreensaver, Copyright © 2018-2021 Jamie Zawinski.
# the `../configure' script generates `hacks/images/Makefile' from this file.


srcdir		= .
VPATH		= ${srcdir} ${srcdir}/bubbles ${srcdir}/m6502 \
		  ${srcdir}/molecules ${srcdir}/noseguy
top_srcdir	= ../..
UTILS_SRC	= $(top_srcdir)/utils
RM		= rm -f

STAR		= *
TARFILES	= Makefile \
		  Makefile.in \
		  $(STAR).png \
		  $(STAR).gif \
		  $(STAR)/$(STAR).png \
		  $(STAR)/$(STAR).asm \
		  $(STAR)/$(STAR).pdb \
		  $(STAR)/$(STAR).pov \
		  $(STAR)/$(STAR).txt \

PNG_HDRS	= $(shell find $(srcdir) -name \*.png | \
		    sed 's@^.*/\(.*\)\.\(png\)$$@gen/\1_\2.h@')

default: all

all: gen $(PNG_HDRS)

gen:
	mkdir gen

# %.png dependency below works because VPATH contains subdirs
# of 'images' dir not only 'images' itself.
#
# This rule only works in GNU make, not BSD make, and I'm not sure what
# to do about that.
#
gen/%_png.h: %.png
	$(UTILS_SRC)/bin2c "$<" "$@"

clean:
	$(RM) -r gen

# Note: don't remove the generated Makefile.  We include that in the tar file
# so that OSX and Android can run 'make' in this directory without 'configure'.
distclean:: clean
	-rm -f TAGS *~ "#"*

list_tarfiles:
	@find $(TARFILES) -type f -print | sort

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
