/* $NetBSD$ */

/*-
 * Copyright (c) 2018 Jason R. Thorpe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MEDIATEK_MEDIATEK_UARTREG_H_
#define	_MEDIATEK_MEDIATEK_UARTREG_H_

#define	MTK_UART_RBR		0x00
#define	MTK_UART_THR		0x00
#define	MTK_UART_DLL		0x00
#define	MTK_UART_IER		0x04
#define	MTK_UART_DLM		0x04
#define	MTK_UART_IIR		0x08
#define	MTK_UART_FCR		0x08
#define	MTK_UART_ECR		0x08
#define	MTK_UART_LCR		0x0c
#define	MTK_UART_MCR		0x10
#define	MTK_UART_XON1		0x10
#define	MTK_UART_LSR		0x14
#define	MTK_UART_XON2		0x14
#define	MTK_UART_MSR		0x18
#define	MTK_UART_XOFF1		0x18
#define	MTK_UART_SCR		0x1c
#define	MTK_UART_XOFF2		0x1c
#define	MTK_UART_AUTOBAUD_EN	0x20
#define	MTK_UART_HIGHSPEED	0x24
#define	MTK_UART_SAMPLE_COUNT	0x28
#define	MTK_UART_SAMPLE_POINT	0x2c
#define	MTK_UART_AUTOBAUD_REG	0x30
#define	MTK_UART_GUARD		0x3c
#define	MTK_UART_ESCAPE_DAT	0x40
#define	MTK_UART_ESCAPE_EN	0x44
#define	MTK_UART_SLEEP_EN	0x48
#define	MTK_UART_DMA_EN		0x4c
#define	MTK_UART_RXTRI_AD	0x50
#define	MTK_UART_FRACDIV_L	0x54
#define	MTK_UART_FRACDIV_M	0x58
#define	MTK_UART_FCR_RD		0x5c
#define	MTK_UART_DEBUG0		0x60
#define	MTK_UART_DEBUG1		0x64
#define	MTK_UART_RX_SEL		0x90

#endif /* _MEDIATEK_MEDIATEK_UARTREG_H_ */
