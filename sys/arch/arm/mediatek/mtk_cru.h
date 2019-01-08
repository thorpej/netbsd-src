/* $NetBSD$ */

/*-
 * Copyright (c) 2019 Jason R. Thorpe
 * Copyright (c) 2017 Jared McNeill <jmcneill@invisible.ca>
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

#ifndef _MEDIATEK_MTK_CRU_H_
#define	_MEDIATEK_MTK_CRU_H_

#include <sys/mutex.h>
#include <dev/clk/clk_backend.h>

struct mtk_cru_softc;
struct mtk_cru_clk;
struct mtk_cru_reset;

/*
 * Resets
 */

struct mtk_cru_reset {
	bus_size_t	reg;
	uint32_t	mask;
};

#define	MTK_CRU_RESET(_id, _reg, _bit)				\
	[(_id)] = {						\
		.reg = (_reg),					\
		.mask = __BIT(_bit),				\
	}

/*
 * Clocks
 */

typedef enum {
	MTK_CLK_UNKNOWN		= 0,
	MTK_CLK_FIXED,
	MTK_CLK_FACTOR,
	MTK_CLK_GATE,
	MTK_CLK_MUX,
	MTK_CLK_MUXGATE,
} mtk_cru_clk_type_t;

struct mtk_cru_clk_fixed {
	const char	*parent;
	u_int		rate;
};

u_int	mtk_cru_clk_fixed_get_rate(struct mtk_cru_softc *,
				   struct mtk_cru_clk *);
const char *mtk_cru_clk_fixed_get_parent(struct mtk_cru_softc *,
					 struct mtk_cru_clk *);

#define	MTK_CLK_FIXED(_id, _name, _parent, _rate)		\
	[(_id)] = {						\
		.type = MTK_CLK_FIXED,				\
		.base.name = (_name),				\
		.base.flags = 0,				\
		.u.fixed.parent = (_parent),			\
		.u.fixed.rate = (_rate),			\
		.get_rate = mtk_cru_clk_fixed_get_rate,		\
		.get_parent = mtk_cru_clk_fixed_get_parent,	\
	}

struct mtk_cru_clk_factor {
	const char	*parent;
	u_int		mul;
	u_int		div;
};

u_int	mtk_cru_clk_factor_get_rate(struct mtk_cru_softc *,
				    struct mtk_cru_clk *);
const char *mtk_cru_clk_factor_get_parent(struct mtk_cru_softc *,
					  struct mtk_cru_clk *);

#define	MTK_CLK_FACTOR(_id, _name, _parent, _mul, _div)		\
	 [(_id)] = {						\
	 	.type = MTK_CLK_FACTOR,				\
		.base.name = (_name),				\
		.base.flags = 0,				\
		.u.factor.parent = (_parent),			\
		.u.factor.mul = (_mul),				\
		.u.factor.div = (_div),				\
		.get_rate = mtk_cru_clk_factor_get_rate,	\
		.get_parent = mtk_cru_clk_factor_get_parent,	\
	}

#define	MTK_CLK_FMUL(_id, _name, _parent, _mul)			\
	MTK_CLK_FACTOR(_id, _name, _parent, _mul, 1)

#define	MTK_CLK_FDIV(_id, _name, _parent, _div)			\
	MTK_CLK_FACTOR(_id, _name, _parent, 1, _div)

struct mtk_cru_clk_gate {
	const bus_size_t *regs;
	const char	*parent;
	uint32_t	mask;
	u_int		flags;
};

#define	MTK_CLK_GATE_REG_SET		0
#define	MTK_CLK_GATE_REG_CLR		1
#define	MTK_CLK_GATE_REG_STA		2

#define	MTK_CLK_GATE_ACT_LOW		0x01

int	mtk_cru_clk_gate_enable(struct mtk_cru_softc *, struct mtk_cru_clk *,
				int);
const char *mtk_cru_clk_gate_get_parent(struct mtk_cru_softc *,
					struct mtk_cru_clk *);

#define	MTK_CLK_GATE(_id, _name, _pname, _regs, _mask, _flags)	\
	[(_id)] = {						\
		.type = MTK_CLK_GATE,				\
		.base.name = (_name),				\
		.base.flags = CLK_SET_RATE_PARENT,		\
		.u.gate.parent = (_pname),			\
		.u.gate.regs = (_regs),				\
		.u.gate.mask = (_mask),				\
		.u.gate.flags = (_flags),			\
		.enable = mtk_cru_clk_gate_enable,		\
		.get_parent = mtk_cru_clk_gate_get_parent,	\
	}

struct mtk_cru_clk_mux {
	bus_size_t	reg;
	uint32_t	sel;
	const char	**parents;
	u_int		nparents;
};

int	mtk_cru_clk_mux_set_parent(struct mtk_cru_softc *,
				   struct mtk_cru_clk *, const char *);
const char *mtk_cru_clk_mux_get_parent(struct mtk_cru_softc *,
				       struct mtk_cru_clk *);

#define	MTK_CLK_MUX(_id, _name, _pnames, _reg, _sel)		\
	[(_id)] = {						\
		.type = MTK_CLK_MUX,				\
		.base.name = (_name),				\
		.base.flags = CLK_SET_RATE_PARENT,		\
		.u.mux.parents = (_pnames),			\
		.u.mux.nparents = __arraycount(_pnames),	\
		.u.mux.reg = (_reg),				\
		.u.mux.sel = (_sel),				\
		.set_parent = mtk_cru_clk_mux_set_parent,	\
		.get_parent = mtk_cru_clk_mux_get_parent,	\
	}

struct mtk_cru_clk_muxgate {
	const bus_size_t *regs;
	const char	**parents;
	u_int		nparents;
	uint32_t	sel;
	uint32_t	mask;
	u_int		flags;
};

#define	MTK_CLK_MUXGATE_CLKF(_id, _name, _pnames, _regs, _sel, 	\
			     _mask, _flags, _clkf)		\
	[(_id)] = {						\
		.type = MTK_CLK_MUXGATE,			\
		.base.name = (_name),				\
		.base.flags = (_clkf),				\
		.u.muxgate.parents = (_pnames),			\
		.u.muxgate.nparents = __arraycount(_pnames),	\
		.u.muxgate.regs = (_regs),			\
		.u.muxgate.sel = (_sel),			\
		.u.muxgate.mask = (_mask),			\
		.u.muxgate.flags = (_flags),			\
		.enable = mtk_cru_clk_gate_enable,		\
		.set_parent = mtk_cru_clk_mux_set_parent,	\
		.get_parent = mtk_cru_clk_mux_get_parent,	\
	}

#define	MTK_CLK_MUXGATE(_id, _name, _pnames, _regs, _sel, _mask, _flags) \
	MTK_CLK_MUXGATE_CLKF(_id, _name, _pnames, _regs, _sel,	\
			     _mask, _flags, CLK_SET_RATE_PARENT)

struct mtk_cru_clk {
	struct clk		base;
	mtk_cru_clk_type_t	type;
	union {
		struct mtk_cru_clk_fixed fixed;
		struct mtk_cru_clk_factor factor;
		struct mtk_cru_clk_gate gate;
		struct mtk_cru_clk_mux mux;
		struct mtk_cru_clk_muxgate muxgate;
	} u;

	int	(*enable)(struct mtk_cru_softc *, struct mtk_cru_clk *, int);
	u_int	(*get_rate)(struct mtk_cru_softc *, struct mtk_cru_clk *);
	int	(*set_rate)(struct mtk_cru_softc *, struct mtk_cru_clk *, u_int);
	u_int	(*round_rate)(struct mtk_cru_softc *, struct mtk_cru_clk *, u_int);
	const char *
		(*get_parent)(struct mtk_cru_softc *, struct mtk_cru_clk *);
	int	(*set_parent)(struct mtk_cru_softc *, struct mtk_cru_clk *,
			      const char *);
};

struct mtk_cru_softc {
	device_t		sc_dev;
	int			sc_phandle;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;

	struct clk_domain	sc_clkdom;

	struct mtk_cru_reset	*sc_resets;
	u_int			sc_nresets;

	struct mtk_cru_clk	*sc_clks;
	u_int			sc_nclks;

	kmutex_t		sc_mutex;
};

int	mtk_cru_attach(struct mtk_cru_softc *);
struct mtk_cru_clk *mtk_cru_clock_find(struct mtk_cru_softc *, const char *);
void	mtk_cru_print(struct mtk_cru_softc *);

#define	CRU_READ(sc, reg)	\
	bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define	CRU_WRITE(sc, reg, val)	\
	bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

#endif /* _MEDIATEK_MTK_CRU_H_ */
