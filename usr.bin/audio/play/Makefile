#	$NetBSD: Makefile,v 1.4 2009/04/14 22:15:17 lukem Exp $

PROG=	audioplay
SRCS=	play.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

# XXXJRT
LDADD+= -lsndio

.include <bsd.prog.mk>
