/* $NetBSD$ */

/*-
 * Copyright (c) 2019 Jason R. Thorpe
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ARM_MEDIATEK_MTK_GPIO_H_
#define _ARM_MEDIATEK_MTK_GPIO_H_

struct mtk_gpio_drive {
	const uint8_t *sel_to_mA;
	size_t nsel;
};

#define	MTK_IES_SMT_BIT_IES	0
#define	MTK_IES_SMT_BIT_SMT	1

struct mtk_ies_smt_group {
	bus_size_t	ies_reg;
	bus_size_t	smt_reg;
	uint16_t	first_pin;
	uint16_t	last_pin;
	uint16_t	bits[2];
};

#define	_IES_SMT(_fp, _lp, _ireg, _sreg, _ival, _sval)			\
	{								\
		.first_pin = (_fp),					\
		.last_pin = (_lp),					\
		.ies_reg = (_ireg),					\
		.smt_reg = (_sreg),					\
		.bits[MTK_IES_SMT_BIT_IES] = (_ival),			\
		.bits[MTK_IES_SMT_BIT_SMT] = (_sval),			\
	}

#define	IES_SMT(_fp, _lp, _ireg, _sreg, _bit)				\
	_IES_SMT(_fp, _lp, _ireg, _sreg, __BIT(_bit), __BIT(_bit))

#define	IES(_fp, _lp, _ireg, _bit)					\
	_IES_SMT(_fp, _lp, _ireg, 0, __BIT(_bit), 0)

#define	SMT(_fp, _lp, _sreg, _bit)					\
	_IES_SMT(_fp, _lp, 0, _sreg, 0, __BIT(_bit))

#define	MTK_GPIO_MAXFUNC	8

struct mtk_gpio_pin {
	const char *name;
	const char *functions[MTK_GPIO_MAXFUNC];
	struct {
		const struct mtk_gpio_drive *params;
		bus_size_t reg;
		uint16_t sel;
		uint16_t sr;		/* slew rate control bit */
	} drive;
	struct {
		bus_size_t reg;
		uint16_t pupd;		/* 0=pull-up, 1=pull-down bit */
		uint16_t r1;		/* 50K resistor control bit */
		uint16_t r0;		/* 10K resistor control bit */
	} pupdr1r0_smt;
};

#define	_DRIVE(_params, _reg, _sel, _srval)				\
	.drive = {							\
		.params = &mt2701_gpio_drive_ ## _params,		\
		.reg = (_reg),						\
		.sel = (_sel),						\
		.sr = (_srval),
	}

#define	DRIVE(_params, _reg, _sel)					\
	_DRIVE(_params, _reg, _sel, 0)

#define	DRIVE_SLEW(_params, _reg, _sel, _sr)				\
	_DRIVE(_params, _reg, _sel, __BIT(_sr))

#define	PUPDR1R0(_reg, _pupd, _r1, _r0)					\
	.pupdr1r0_smt = {						\
		.reg = (_reg),						\
		.pupd = __BIT(_pupd),					\
		.r1 = __BIT(_r1),					\
		.r0 = __BIT(_r0),					\
	}

struct mtk_gpio_conf {
	const struct mtk_gpio_pin * const pins;
	size_t npins;
	const struct mtk_ies_smt_group * const ies_smt_groups;
	size_t nies_smt_groups;
};

#endif /* _ARM_MEDIATEK_MTK_GPIO_H_ */
