#	$NetBSD: Makefile,v 1.11 2011/03/06 18:15:30 bouyer Exp $

.include <bsd.hostinit.mk>

HOSTPROGNAME=	${_TOOL_PREFIX}installboot
HOST_SRCDIR=	usr.sbin/installboot

LIBPROP_INC=	${.CURDIR}/../../common/include
HOST_CPPFLAGS+=	-I${LIBPROP_INC}

HOST_CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../mips-elf2ecoff
HOST_CPPFLAGS+=	-I${TOOLDIR}/include/nbinclude

HOST_CPPFLAGS+=	-DEVBOARDS_PLIST_BASE=\"${TOOLDIR}\"

LIBPROP_OBJ!=	cd ${.CURDIR}/../libprop && ${PRINTOBJDIR}
LDADD+=		-L${LIBPROP_OBJ} -lprop

.include "${.CURDIR}/../Makefile.nbincludes"
.include "${.CURDIR}/../Makefile.host"

