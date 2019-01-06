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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/fdt/fdtvar.h>

#include <arm/mediatek/mtk_cru.h>
#include <arm/mediatek/mt2701_pericfg.h>

/* Relative to PERICFG Base address */
#define	PERI_GLOBALCON_RST0		0x000
#define	PERI_GLOBALCON_RST1		0x004
#define	PERI_GLOBALCON_PDN0_SET		0x008
#define	PERI_GLOBALCON_PDN1_SET		0x00c
#define	PERI_GLOBALCON_PDN0_CLR		0x010
#define	PERI_GLOBALCON_PDN1_CLR		0x014
#define	PERI_GLOBALCON_PDN0_STA		0x018
#define	PERI_GLOBALCON_PDN1_STA		0x01c
#define	PERI_GLOBALCON_PDN_MD1_SET	0x020
#define	PERI_GLOBALCON_PDN_MD2_SET	0x024
#define	PERI_GLOBALCON_PDN_MD1_CLR	0x028
#define	PERI_GLOBALCON_PDN_MD2_CLR	0x02c
#define	PERI_GLOBALCON_PDN_MD1_STA	0x030
#define	PERI_GLOBALCON_PDN_MD2_STA	0x034
#define	PERI_GLOBALCON_PDN_MD_MASK	0x038
#define	PERI_GLOBALCON_DCMCTL		0x050
#define	PERI_GLOBALCON_DCMDBC		0x054
#define	PERI_GLOBALCON_DCMFSEL		0x058
#define	PERI_GLOBALCON_CKSEL		0x05c
#define	PERIAXI_BUS_CTL1		0x200
#define	PERIAXI_BUS_CTL2		0x204
#define	PERIAXI_BUS_SI0_CTL		0x208
#define	PERIAXI_BUS_SI1_CTL		0x20c
#define	PERIAXI_MI_STA			0x210
#define	PERIAXI_AHB_LMT_CON1		0x300
#define	PERIAXI_AHB_LMT_CON2		0x304
#define	PERIAXI_AHB_LMT_CON3		0x308
#define	PERIAXI_AHB_LMT_CON4		0x30c
#define	PERIAXI_AHB_LMT_CON5		0x310
#define	PERIAXI_AHB_LMT_CON6		0x314
#define	PERIAXI_AHB_LMT_CON7		0x318
#define	PERIAXI_AHB_LMT_CON8		0x31c
#define	PERIAXI_AXI_LMT_CON1		0x320
#define	PERIAXI_AXI_LMT_CON2		0x324
#define	PERIAXI_AXI_LMT_CON3		0x328
#define	PERIAXI_AXI_LMT_CON4		0x32c
#define	PERIAXI_AXI_LMT_CON5		0x330
#define	PERIAXI_AXI_LMT_CON6		0x334
#define	PERIAXI_AXI_LMT_CON7		0x338
#define	PERIAXI_AXI_LMT_CON8		0x33c
#define	PERI_USB_WAKEUP_DEC_CON0	0x400
#define	PERI_UART_CLK_SOURCE_SEL	0x40c
#define	PERI_ETH_NIC_CON		0x420
#define	PERI_NFI_CK_SOURCE_SEL		0x424
#define	PERI_NFI_MAC_CTRL		0x428

static int	mt2701_pericfg_match(device_t, cfdata_t, void *);
static void	mt2701_pericfg_attach(device_t, device_t, void *);

static const char * compatible[] = {
	"mediatek,mt2701-pericfg",
	NULL
};

struct mt2701_pericfg_softc {
	struct mtk_cru_softc	sc_mtk_cru;
	/* XXX Register with fdt syscon? */
};

CFATTACH_DECL_NEW(mt2701_pericfg, sizeof(struct mt2701_pericfg_softc),
    mt2701_pericfg_match, mt2701_pericfg_attach, NULL, NULL);

static const struct mtk_cru_reset mt2701_pericfg_resets[] = {
	MTK_CRU_RESET(MT2701_RST_PERI_UART0_SW_RST, PERI_GLOBALCON_RST0, 0),
	MTK_CRU_RESET(MT2701_RST_PERI_UART1_SW_RST, PERI_GLOBALCON_RST0, 1),
	MTK_CRU_RESET(MT2701_RST_PERI_UART2_SW_RST, PERI_GLOBALCON_RST0, 2),
	MTK_CRU_RESET(MT2701_RST_PERI_UART3_SW_RST, PERI_GLOBALCON_RST0, 3),
	MTK_CRU_RESET(MT2701_RST_PERI_GCPU_SW_RST, PERI_GLOBALCON_RST0, 5),
	MTK_CRU_RESET(MT2701_RST_PERI_BTIF_SW_RST, PERI_GLOBALCON_RST0, 6),
	MTK_CRU_RESET(MT2701_RST_PERI_PWM_SW_RST, PERI_GLOBALCON_RST0, 8),
	MTK_CRU_RESET(MT2701_RST_PERI_AUXADC_SW_RST, PERI_GLOBALCON_RST0, 10),
	MTK_CRU_RESET(MT2701_RST_PERI_DMA_SW_RST, PERI_GLOBALCON_RST0, 11),
	MTK_CRU_RESET(MT2701_RST_PERI_NFI_SW_RST, PERI_GLOBALCON_RST0, 14),
	MTK_CRU_RESET(MT2701_RST_PERI_NLI_SW_RST, PERI_GLOBALCON_RST0, 15),
	MTK_CRU_RESET(MT2701_RST_PERI_THERM_SW_RST, PERI_GLOBALCON_RST0, 16),
	MTK_CRU_RESET(MT2701_RST_PERI_MSDC2_SW_RST, PERI_GLOBALCON_RST0, 17),
	MTK_CRU_RESET(MT2701_RST_PERI_MSDC0_SW_RST, PERI_GLOBALCON_RST0, 19),
	MTK_CRU_RESET(MT2701_RST_PERI_MSDC1_SW_RST, PERI_GLOBALCON_RST0, 20),
	MTK_CRU_RESET(MT2701_RST_PERI_I2C0_SW_RST, PERI_GLOBALCON_RST0, 22),
	MTK_CRU_RESET(MT2701_RST_PERI_I2C1_SW_RST, PERI_GLOBALCON_RST0, 23),
	MTK_CRU_RESET(MT2701_RST_PERI_I2C2_SW_RST, PERI_GLOBALCON_RST0, 24),
	MTK_CRU_RESET(MT2701_RST_PERI_I2C3_SW_RST, PERI_GLOBALCON_RST0, 25),
	MTK_CRU_RESET(MT2701_RST_PERI_USB_SW_RST, PERI_GLOBALCON_RST0, 28),
	MTK_CRU_RESET(MT2701_RST_PERI_ETH_SW_RST, PERI_GLOBALCON_RST0, 29),

	MTK_CRU_RESET(MT2701_RST_PERI_SPI0_SW_RST, PERI_GLOBALCON_RST1, 1),
};

static struct mtk_cru_clk mt2701_pericfg_clks[] = {
};

static int
mt2701_pericfg_match(device_t parent, cfdata_t cf, void *aux)
{
	struct fdt_attach_args * const faa = aux;

	return of_match_compatible(faa->faa_phandle, compatible);
}

static void
mt2701_pericfg_attach(device_t parent, device_t self, void *aux)
{
	struct mt2701_pericfg_softc * const pericfg_sc = device_private(self);
	struct mtk_cru_softc * const sc = &pericfg_sc->sc_mtk_cru;
	struct fdt_attach_args * const faa = aux;

	sc->sc_dev = self;
	sc->sc_phandle = faa->faa_phandle;
	sc->sc_bst = faa->faa_bst;

	sc->sc_resets = mt2701_pericfg_resets;
	sc->sc_nresets = __arraycount(mt2701_pericfg_resets);

	sc->sc_clks = mt27901_pericfg_clks;
	sc->sc_nclks = __arraycount(mt27901_pericfg_clks);

	if (mtk_cru_attach(sc) != 0)
		return;

	aprint_naive("\n");
	aprint_normal(": MT2701 Peripheral Controller\n");

	mtk_cru_print(sc);
}
