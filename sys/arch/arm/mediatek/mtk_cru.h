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
	MTK_CLK_GATE,
	MTK_CLK_MUX,
} mtk_cru_clk_type_t;

struct mtk_cru_clk_gate {
	bus_size_t	reg;
	uint32_t	mask;
	const char	*parent;
};

#define	MTK_CLK_GATE(_id, _name, _pname, _reg, _bit)		\
	[(_id)] = {						\
		.type = MTK_CLK_GATE,				\
		.base.name = (_name),				\
		.base.flags = CLK_SET_RATE_PARENT,		\
		.u.gate.parent = (_pname),			\
		.u.gate.reg = (_reg),				\
		.u.gate.mask = __BIT(_bit),			\
		.enable = mtk_cru_clk_gate_enable,		\
		.get_parent = mtk_cru_clk_gate_get_parent,	\
	}

struct mtk_cru_clk_mux {
	bus_size_t	reg;
	uint32_t	mask;
	const char	*parent;
};

#define	MTK_CLK_MUX(_id, _name, _pname, _reg, _bit)		\
	[(_id)] = {						\
		.type = MTK_CLK_MUX,				\
		.base.name = (_name),				\
		.base.flags = CLK_SET_RATE_PARENT,		\
		.u.gate.parent = (_pname),			\
		.u.gate.reg = (_reg),				\
		.u.gate.mask = __BIT(_bit),			\
		.enable = mtk_cru_clk_mux_enable,		\
		.get_parent = mtk_cru_clk_mux_get_parent,	\
	}

struct mtk_cru_clk {
	struct clk	base;
	mtk_cru_clk_type_t	type;
	union {
		struct mtk_cru_clk_gate gate;
		struct mtk_cru_clk_mux mux;
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

	const struct mtk_cru_reset *sc_resets;
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
