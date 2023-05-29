#
# Makefile:
#
# Copyright (c) 2001 James McKenzie <james@fishsoup.dhs.org>,
# All rights reserved.
#
# $Id: Makefile,v 1.4 2002/10/14 18:48:57 root Exp root $
#
# $Log: Makefile,v $
# Revision 1.4  2002/10/14 18:48:57  root
# #
#
# Revision 1.3  2002/10/14 18:48:17  root
# *** empty log message ***
#
# Revision 1.2  2002/10/14 18:04:14  root
# *** empty log message ***
#
# Revision 1.1  2002/10/14 16:47:11  root
# Initial revision
#
# Revision 1.1  2001/12/20 21:41:40  root
# Initial revision
#
#

# edit the path where you want the undither binary installed
PROJECT=ppmdescreen

CC=gcc
CI=ci
BINDIR=/software/bin

# edit the path to where you installed fftw
FFTW=/software

FFTWINC=-I${FFTW}/include
FFTWLIB=-L${FFTW}/lib -lrfftw -lfftw

RM=/bin/rm -f
INSTALL=install
INDENT=indent
MD=makedepend

ifeq (.depend,$(wildcard .depend))
default:all
include .depend
else
default:depend
	${MAKE} default
endif

OPT=-O

CSRCS=${wildcard src/*.c}

INCLUDE=${FFTWINC} -Isrc
CFLAGS=${INCLUDE} ${OPT}
LIBS=${FFTWLIB} -lm

all: ${PROJECT}

${PROJECT}: ${CSRCS}
	${CC} ${CFLAGS} $^ ${LIBS} -o $@ -s

clean:
	${RM} a.out core *.BAK *% *~ ${PROJECT}
	${RM} wisdom  .depend

%:%.c
	${CC} ${CFLAGS} -o $@ $< ${LIBS}

checkin: 
	${CI} -m# -l *.c Makefile 

install: ${PROJECT}
	${INSTALL} -c -m 755  $< ${BINDIR}/$<

tidy: checkin
	${INDENT} -i2 -ts0 *.c 

depend: 
	${MD} -f- ${INCLUDE} ${CSRCS} > .depend


