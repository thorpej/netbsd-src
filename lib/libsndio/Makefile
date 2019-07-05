#	$NetBSD$

LIB=	sndio

SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c

CPPFLAGS+= -DDEBUG

MAN=	sio_open.3 mio_open.3 sndio.7

INCSDIR= /usr/include
INCS=	sndio.h

.include <bsd.lib.mk>
