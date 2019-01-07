/* $NetBSD$ */

/*-
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/bus.h>

#include <arm/mediatek/mtk_cru.h>

int
mtk_cru_clk_mux_set_parent(struct mtk_cru_softc *sc,
    struct mtk_cru_clk *clk, const char *name)
{
	struct mtk_cru_clk_mux *mux = &clk->u.mux;
	uint32_t val;
	u_int index;

	KASSERT(clk->type == MTK_CLK_MUX);

	if (mux->sel == 0)
		return ENODEV;
	
	for (index = 0; index < mux->nparents; index++) {
		if (mux->parents[index] != NULL &&
		    strcmp(mux->parents[index], name) == 0)
		    	break;
	}
	if (index == mux->nparents)
		return EINVAL;

	mutex_enter(&sc->sc_mutex);
	val = CRU_READ(sc, mux->reg);
	val &= ~mux->sel;
	val |= __SHIFTIN(index, mux->sel);
	CRU_WRITE(sc, mux->reg, val);
	mutex_exit(&sc->sc_mutex);

	return 0;
}

const char *
mtk_cru_clk_mux_get_parent(struct mtk_cru_softc *sc,
    struct mtk_cru_clk *clk)
{
	struct mtk_cru_clk_mux *mux = &clk->u.mux;
	uint32_t val;
	u_int index;

	KASSERT(clk->type == MTK_CLK_MUX);

	val = CRU_READ(sc, mux->reg);
	index = __SHIFTOUT(val, mux->sel);

	return mux->parents[index];
}
