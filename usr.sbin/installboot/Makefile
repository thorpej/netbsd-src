#	$NetBSD: Makefile,v 1.51 2017/01/11 12:19:44 joerg Exp $
#

.include <bsd.own.mk>

PROG=	installboot
MAN=	installboot.8
SRCS=	installboot.c sum.c machines.c fstypes.c

ARCH_XLAT= amd64-i386.c news68k-news.c newsmips-news.c
ARCH_XLAT+= sun2-sun68k.c sun3-sun68k.c

.if !defined(SMALLPROG) && !defined(ARCH_FILES)
ARCH_FILES=  alpha.c amiga.c
ARCH_FILES+= emips.c evbarm.c ews4800mips.c
ARCH_FILES+= hp300.c hppa.c
ARCH_FILES+= i386.c
ARCH_FILES+= landisk.c
ARCH_FILES+= macppc.c
ARCH_FILES+= news.c next68k.c
ARCH_FILES+= pmax.c
ARCH_FILES+= sparc.c sparc64.c sun68k.c
ARCH_FILES+= vax.c
ARCH_FILES+= x68k.c
.else
ARCH_FILES?= ${ARCH_XLAT:M${MACHINE}-*:S/${MACHINE}-//}
.if empty(ARCH_FILES)
ARCH_FILES= ${MACHINE}.c
.endif
.endif

SRCS+=${ARCH_FILES}

# XXX include only for evbarm
SRCS+=evboards.c

.if empty(ARCH_FILES:C/(macppc|news|sparc|sun68k|x68k)/stg2/:Mstg2.c)
CPPFLAGS	+= -DNO_STAGE2
.else
SRCS+= bbinfo.c

# fstypes are only needed for 'stage2' and then only from bbinfo.
SRCS+= ffs.c
.if SMALLPROG
CPPFLAGS+=	-DNO_FFS_SWAP
.else
SRCS+= ffs_bswap.c
.endif
#SRCS+= ext2fs.c ext2fs_bswap.c
.endif

UFSSRC=		${NETBSDSRCDIR}/sys/ufs
CPPFLAGS+=	-I${.CURDIR} -I.
.PATH:		${.CURDIR}/arch ${UFSSRC}/ffs ${UFSSRC}/ext2fs

.if !defined(HOSTPROGNAME)
.for f in i386 macppc
COPTS.${f}.c+=  -Wno-pointer-sign
.endfor

# XXX include only if evboards.c is being built
LDADD+=	-lprop
DPADD+=	${LIBPROP}

LDADD+= -lutil
DPADD+= ${LIBUTIL}
.endif

CWARNFLAGS.clang+=	-Wno-error=address-of-packed-member

.include <bsd.prog.mk>
