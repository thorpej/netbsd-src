/*	$NetBSD: copy.S,v 1.30 2018/07/14 14:29:40 maxv Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/asm.h>
#include <machine/frameasm.h>

#define GET_CURPCB(reg)	\
	movq	CPUVAR(CURLWP),reg; \
	movq	L_PCB(reg),reg

/*
 * These are arranged so that the abnormal case is a forwards
 * conditional branch - which will be predicted not-taken by
 * both Intel and AMD processors.
 */
#define DEFERRED_SWITCH_CHECK \
	CHECK_DEFERRED_SWITCH			; \
	jnz	99f				; \
98:

#define DEFERRED_SWITCH_CALL \
99:						; \
	call	_C_LABEL(do_pmap_load)		; \
	jmp	98b

/*
 * The following primitives are to copy regions of memory.
 * Label must be before all copy functions.
 */
	.text

x86_copyfunc_start:	.globl	x86_copyfunc_start

/*
 * Handle deferred pmap switch.  We must re-enable preemption without
 * making a function call, so that the program counter is visible to
 * cpu_kpreempt_exit().  It can then know if it needs to restore the
 * pmap on returning, because a preemption occurred within one of the
 * copy functions.
 */
ENTRY(do_pmap_load)
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rdi
	pushq	%rsi
	pushq	%rdx
	pushq	%rcx
	pushq	%rbx
	movq	CPUVAR(CURLWP),%rbx
1:
	incl	L_NOPREEMPT(%rbx)
	call	_C_LABEL(pmap_load)
	decl	L_NOPREEMPT(%rbx)
	jnz	2f
	cmpl	$0,L_DOPREEMPT(%rbx)
	jz	2f
	xorq	%rdi,%rdi
	call	_C_LABEL(kpreempt)
2:
	cmpl	$0,CPUVAR(WANT_PMAPLOAD)
	jnz	1b
	popq	%rbx
	popq	%rcx
	popq	%rdx
	popq	%rsi
	popq	%rdi
	leaveq
	ret
END(do_pmap_load)

/*
 * Copy routines from and to userland, plus a few more. See the
 * section 9 manpages for info. Some cases can be optimized more.
 *
 * I wonder if it's worthwhile to make these use SSE2 registers?
 * (dsl) Not from info I've read from the AMD guides.
 *
 * Also note that the setup time for 'rep movs' is horrid - especially on P4
 * netburst - but on my AMD X2 it manages one copy (read+write) per clock
 * which can be achieved with a code loop, but is probably impossible to beat.
 * However the use of 'rep movsb' for the final bytes should be killed.
 *
 * Newer Intel cpus have a much lower setup time, and may (someday)
 * be ably to do cache-line size copies....
 */

/*
 * int kcopy(const void *from, void *to, size_t len);
 * Copy len bytes from and to kernel memory, and abort on fault.
 */
ENTRY(kcopy)
	xchgq	%rdi,%rsi
	movq	%rdx,%rcx
.Lkcopy_start:
	movq	%rdi,%rax
	subq	%rsi,%rax
	cmpq	%rcx,%rax		/* overlapping? */
	jb	1f
	/* nope, copy forward */
	shrq	$3,%rcx			/* copy by 64-bit words */
	rep
	movsq

	movq	%rdx,%rcx
	andl	$7,%ecx			/* any bytes left? */
	rep
	movsb

	xorq	%rax,%rax
	ret

/*
 * Using 'rep movs' to copy backwards is not as fast as for forwards copies
 * and ought not be done when the copy doesn't acually overlap.
 * However kcopy() isn't used any that looks even vaguely used often.
 * I'm also not sure it is ever asked to do overlapping copies!
 */

1:	addq	%rcx,%rdi		/* copy backward */
	addq	%rcx,%rsi
	std
	andq	$7,%rcx			/* any fractional bytes? */
	decq	%rdi
	decq	%rsi
	rep
	movsb
	movq	%rdx,%rcx		/* copy remainder by 64-bit words */
	shrq	$3,%rcx
	subq	$7,%rsi
	subq	$7,%rdi
	rep
	movsq
	cld
.Lkcopy_end:
	xorq	%rax,%rax
	ret
END(kcopy)

ENTRY(copyout)
	DEFERRED_SWITCH_CHECK

	xchgq	%rdi,%rsi		/* kernel address to %rsi, user to %rdi */
	movq	%rdx,%rax		/* save transfer length (bytes) */

	addq	%rdi,%rdx		/* end address to %rdx */
	jc	_C_LABEL(copy_efault)	/* jump if wraps */
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)	/* jump if end in kernel space */

	SMAP_DISABLE
.Lcopyout_start:
	movq	%rax,%rcx		/* length */
	shrq	$3,%rcx			/* count of 8-byte words */
	rep
	movsq				/* copy from %rsi to %rdi */
	movb	%al,%cl
	andb	$7,%cl			/* remaining number of bytes */
	rep
	movsb				/* copy remaining bytes */
.Lcopyout_end:
	SMAP_ENABLE

	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL
END(copyout)

ENTRY(copyin)
	DEFERRED_SWITCH_CHECK

	xchgq	%rdi,%rsi
	movq	%rdx,%rax

	addq	%rsi,%rdx		/* check source address not wrapped */
	jc	_C_LABEL(copy_efault)
	movq	$VM_MAXUSER_ADDRESS,%r8
	cmpq	%r8,%rdx
	ja	_C_LABEL(copy_efault)	/* j if end in kernel space */

	SMAP_DISABLE
.Lcopyin_start:
3:	/* bcopy(%rsi, %rdi, %rax); */
	movq	%rax,%rcx
	shrq	$3,%rcx
	rep
	movsq
	movb	%al,%cl
	andb	$7,%cl
	rep
	movsb
.Lcopyin_end:
	SMAP_ENABLE

	xorl	%eax,%eax
	ret
	DEFERRED_SWITCH_CALL
END(copyin)

ENTRY(copy_efault)
	movq	$EFAULT,%rax
	ret
END(copy_efault)

ENTRY(kcopy_fault)
	ret
END(kcopy_fault)

ENTRY(copy_fault)
	SMAP_ENABLE
	ret
END(copy_fault)

ENTRY(copyoutstr)
	DEFERRED_SWITCH_CHECK
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rdi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rdi,%rax
	jc	_C_LABEL(copystr_efault)
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8
1:	incq	%rdx

	SMAP_DISABLE
.Lcopyoutstr_start:
1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b
.Lcopyoutstr_end:
	SMAP_ENABLE

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* rdx is zero -- return EFAULT or ENAMETOOLONG. */
	SMAP_ENABLE
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rdi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL
END(copyoutstr)

ENTRY(copyinstr)
	DEFERRED_SWITCH_CHECK
	xchgq	%rdi,%rsi
	movq	%rdx,%r8
	movq	%rcx,%r9

	/*
	 * Get min(%rdx, VM_MAXUSER_ADDRESS-%rsi).
	 */
	movq	$VM_MAXUSER_ADDRESS,%rax
	subq	%rsi,%rax
	jc	_C_LABEL(copystr_efault)
	cmpq	%rdx,%rax
	jae	1f
	movq	%rax,%rdx
	movq	%rax,%r8
1:	incq	%rdx

	SMAP_DISABLE
.Lcopyinstr_start:
1:	decq	%rdx
	jz	2f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b
.Lcopyinstr_end:
	SMAP_ENABLE

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorq	%rax,%rax
	jmp	copystr_return

2:	/* rdx is zero -- return EFAULT or ENAMETOOLONG. */
	SMAP_ENABLE
	movq	$VM_MAXUSER_ADDRESS,%r11
	cmpq	%r11,%rsi
	jae	_C_LABEL(copystr_efault)
	movq	$ENAMETOOLONG,%rax
	jmp	copystr_return
	DEFERRED_SWITCH_CALL
END(copyinstr)

ENTRY(copystr_efault)
	movl	$EFAULT,%eax
	jmp	copystr_return
END(copystr_efault)

ENTRY(copystr_fault)
	SMAP_ENABLE
copystr_return:
	/* Set *lencopied and return %eax. */
	testq	%r9,%r9
	jz	8f
	subq	%rdx,%r8
	movq	%r8,(%r9)
8:	ret
END(copystr_fault)

ENTRY(copystr)
	xchgq	%rdi,%rsi
	movq	%rdx,%r8

	incq	%rdx

1:	decq	%rdx
	jz	4f
	lodsb
	stosb
	testb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached. */
	decq	%rdx
	xorl	%eax,%eax
	jmp	6f

4:	/* rdx is zero -- return ENAMETOOLONG. */
	movl	$ENAMETOOLONG,%eax

6:	/* Set *lencopied and return %eax. */
	testq	%rcx,%rcx
	jz	7f
	subq	%rdx,%r8
	movq	%r8,(%rcx)

7:	ret
END(copystr)

/**************************************************************************/

#define	UFETCHSTORE_PROLOGUE						\
	movq	$VM_MAXUSER_ADDRESS-1,%r11			;	\
	cmpq	%r11,%rdi					;	\
	ja	_C_LABEL(copy_efault)				;	\
	GET_CURPCB(%rcx)					;	\
	xorq	%r10,%r10					;	\
	leaq	_C_LABEL(ufetchstoreerr)(%rip),%r11		;	\
	movq	%r11,PCB_ONFAULT(%rcx)

#define	UFETCHSTORE_PROLOGUE_INTRSAFE					\
	movq	$VM_MAXUSER_ADDRESS-1,%r11			;	\
	cmpq	%r11,%rdi					;	\
	ja	_C_LABEL(copy_efault)				;	\
	GET_CURPCB(%rcx)					;	\
	leaq	_C_LABEL(ufetchstoreerr_intrsafe)(%rip),%r11	;	\
	movq	PCB_ONFAULT(%rcx),%r10				;	\
	movq	%r11,PCB_ONFAULT(%rcx)

#define	UFETCHSTORE_EPILOGUE						\
	movq	%r10,PCB_ONFAULT(%rcx)

ENTRY(ufetch_uint8)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movb	(%rdi),%al
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	movb	%al,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ufetch_uint8)
STRONG_ALIAS(ufetch_uchar,ufetch_uint8)

ENTRY(ufetch_uint8_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ufetch_uint8_intrsafe)
STRONG_ALIAS(ufetch_uchar_intrsafe,ufetch_uint8_intrsafe)

ENTRY(ufetch_uint16)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movw	(%rdi),%ax
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	movw	%ax,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ufetch_uint16)
STRONG_ALIAS(ufetch_ushort,ufetch_uint16)

ENTRY(ufetch_uint16_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ufetch_uint16_intrsafe)
STRONG_ALIAS(ufetch_ushort_intrsafe,ufetch_uint16_intrsafe)

ENTRY(ufetch_uint32)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movl	(%rdi),%eax
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	movl	%eax,(%rsi)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ufetch_uint32)
STRONG_ALIAS(ufetch_uint,ufetch_uint32)

ENTRY(ufetch_uint32_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ufetch_uint32_intrsafe)
STRONG_ALIAS(ufetch_uint_intrsafe,ufetch_uint32_intrsafe)

ENTRY(ufetch_uint64)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movq	(%rdi),%rax
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	movq	%rax,(%rsi)
	xorq	%rax,%rax
	req
	DEFERRED_SWITCH_CALL
END(ufetch_uint64)
STRONG_ALIAS(ufetch_ulong,ufetch_uint64)
STRONG_ALIAS(ufetch_ptr,ufetch_uint64)

ENTRY(ufetch_uint64_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ufetch_uint64_intrsafe)
STRONG_ALIAS(ufetch_ulong_intrsafe,ufetch_uint64_intrsafe)
STRONG_ALIAS(ufetch_ptr_intrsafe,ufetch_uint64_intrsafe)

ENTRY(ustore_uint8)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movb	%sil,(%rdi)
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ustore_uint8)
STRONG_ALIAS(ustore_uchar,ustore_uint8)

ENTRY(ustore_uint8_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ustore_uint8_intrsafe)
STRONG_ALIAS(ustore_uchar_intrsafe,ustore_uint8_intrsafe)

ENTRY(ustore_uint16)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movw	%si,(%rdi)
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ustore_uint16)
STRONG_ALIAS(ustore_ushort,ustore_uint16)

ENTRY(ustore_uint16_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ustore_uint16_intrsafe)
STRONG_ALIAS(ustore_ushort_intrsafe,ustore_uint16_intrsafe)

ENTRY(ustore_uint32)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movl	%esi,(%rdi)
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ustore_uint32)
STRONG_ALIAS(ustore_uint,ustore_uint32)

ENTRY(ustore_uint32_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ustore_uint32_intrsafe)
STRONG_ALIAS(ustore_uint_intrsafe,ustore_uint32_intrsafe)

ENTRY(ustore_uint64)
	DEFERRED_SWITCH_CHECK
	UFETCHSTORE_PROLOGUE

9:	SMAP_DISABLE
	movq	%rsi,(%rdi)
	SMAP_ENABLE

	UFETCHSTORE_EPILOGUE
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ustore_uint64)
STRONG_ALIAS(ustore_ulong,ustore_uint64)
STRONG_ALIAS(ustore_ptr,ustore_uint64)

ENTRY(ustore_uint64_intrsafe)
	UFETCHSTORE_PROLOGUE_INTRSAFE
	jmp	9b
END(ustore_uint64_intrsafe)
STRONG_ALIAS(ustore_ulong_intrsafe,ustore_uint64_intrsafe)
STRONG_ALIAS(ustore_ptr_intrsafe,ustore_uint64_intrsafe)

NENTRY(ufetchstoreerr)
9:	SMAP_ENABLE
	UFETCHSTORE_EPILOGUE
	ret
END(ufetchstoreerr)

NENTRY(ufetchstoreerr_intrsafe)
	jmp	9b
END(ufetchstoreerr_intrsafe)

/**************************************************************************/

/*
 * Compare-and-swap the 64-bit integer in the user-space.
 *
 * int	ucas_64(volatile int64_t *uptr, int64_t old, int64_t new, int64_t *ret);
 */
ENTRY(ucas_64)
	DEFERRED_SWITCH_CHECK
	/* Fail if kernel-space */
	movq	$VM_MAXUSER_ADDRESS-8,%r8
	cmpq	%r8,%rdi
	ja	_C_LABEL(ucas_efault)
	movq	%rsi,%rax

	SMAP_DISABLE
.Lucas64_start:
	/* Perform the CAS */
	lock
	cmpxchgq %rdx,(%rdi)
.Lucas64_end:
	SMAP_ENABLE

	/*
	 * Note: %rax is "old" value.
	 * Set the return values.
	 */
	movq	%rax,(%rcx)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ucas_64)

/*
 * int	ucas_32(volatile int32_t *uptr, int32_t old, int32_t new, int32_t *ret);
 */
ENTRY(ucas_32)
	DEFERRED_SWITCH_CHECK
	/* Fail if kernel-space */
	movq	$VM_MAXUSER_ADDRESS-4,%r8
	cmpq	%r8,%rdi
	ja	_C_LABEL(ucas_efault)
	movl	%esi,%eax

	SMAP_DISABLE
.Lucas32_start:
	/* Perform the CAS */
	lock
	cmpxchgl %edx,(%rdi)
.Lucas32_end:
	SMAP_ENABLE

	/*
	 * Note: %eax is "old" value.
	 * Set the return values.
	 */
	movl	%eax,(%rcx)
	xorq	%rax,%rax
	ret
	DEFERRED_SWITCH_CALL
END(ucas_32)

ENTRY(ucas_efault)
	movq	$EFAULT,%rax
	ret
END(ucas_efault)

ENTRY(ucas_fault)
	SMAP_ENABLE
	ret
END(ucas_fault)

/*
 * int	ucas_ptr(volatile void **uptr, void *old, void *new, void **ret);
 * int	ucas_int(volatile int *uptr, int old, intnew, int *ret);
 */
STRONG_ALIAS(ucas_ptr, ucas_64)
STRONG_ALIAS(ucas_int, ucas_32)

/*
 * Label must be after all copy functions.
 */
x86_copyfunc_end:	.globl	x86_copyfunc_end

/*
 * Fault table of copy functions for trap().
 */
	.section ".rodata"
	.globl _C_LABEL(onfault_table)

_C_LABEL(onfault_table):
	.quad .Lcopyin_start
	.quad .Lcopyin_end
	.quad _C_LABEL(copy_fault)

	.quad .Lcopyout_start
	.quad .Lcopyout_end
	.quad _C_LABEL(copy_fault)

	.quad .Lkcopy_start
	.quad .Lkcopy_end
	.quad _C_LABEL(kcopy_fault)

	.quad .Lcopyoutstr_start
	.quad .Lcopyoutstr_end
	.quad _C_LABEL(copystr_fault)

	.quad .Lcopyinstr_start
	.quad .Lcopyinstr_end
	.quad _C_LABEL(copystr_fault)

	.quad .Lucas64_start
	.quad .Lucas64_end
	.quad _C_LABEL(ucas_fault)

	.quad .Lucas32_start
	.quad .Lucas32_end
	.quad _C_LABEL(ucas_fault)

	.quad 0	/* terminate */

	.text
