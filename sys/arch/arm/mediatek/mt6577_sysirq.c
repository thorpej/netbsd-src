/*	$NetBSD$	*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/kmem.h>

#include <dev/fdt/fdtvar.h>

struct mt6577_sysirq_intrpol_block {
	bus_space_handle_t	b_bsh;
	u_int			b_first_irq;
	u_int			b_last_irq;
};

struct mt6577_sysirq_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bst;

	int			sc_intr_parent;

	struct mt6577_sysirq_intrpol_block *sc_blocks;
	u_int			sc_nblocks;

	kmutex_t		sc_mutex;
};

#define	NCELLS	3U	/* expected "#interrupt-cells" */

static int	mt6577_sysirq_match(device_t, cfdata_t, void *);
static void	mt6577_sysirq_attach(device_t, device_t, void *);

static void *	mt6577_sysirq_fdt_intr_establish(device_t, u_int *, int, int,
		    int (*func)(void *), void *);
static void	mt6577_sysirq_fdt_intr_disestablish(device_t, void *);
static bool	mt6577_sysirq_fdt_intr_intrstr(device_t, u_int *, char *,
		    size_t);

static struct fdtbus_interrupt_controller_func mt6577_sysirq_fdt_intrfuncs = {
	.establish = mt6577_sysirq_fdt_intr_establish,
	.disestablish = mt6577_sysirq_fdt_intr_disestablish,
	.intrstr = mt6577_sysirq_fdt_intr_intrstr,
};

CFATTACH_DECL_NEW(mt6577_sysirq, sizeof(struct mt6577_sysirq_softc),
    mt6577_sysirq_match, mt6577_sysirq_attach, NULL, NULL);

static const char * const compatible[] = {
	"mediatek,mt6577-sysirq",
	NULL,
};

static int
mt6577_sysirq_match(device_t parent, cfdata_t cf, void *aux)
{
	struct fdt_attach_args * const faa = aux;

	return of_match_compatible(faa->faa_phandle, compatible);
}

static void
mt6577_sysirq_attach(device_t parent, device_t self, void *aux)
{
	struct mt6577_sysirq_softc * const sc = device_private(self);
	struct fdt_attach_args * const faa = aux;
	bus_addr_t addr;
	bus_size_t size;
	int error;
	u_int i, next_irq;
	uint32_t ncells;

	const int phandle = faa->faa_phandle;

	/* First, count the number of cells we have to manage. *
	for (sc->sc_nblocks = 0;
	     fdtbus_get_reg(phandle, sc->sc_nblocks, NULL, NULL) == 0;
	     sc->sc_nncells++) {
		/* doop de doo. */;
	}
	if (ncells == 0) {
		aprint_error(": unable to find any INTRPOL register cells\n");
		return;
	}

	/* Fetch our interrupt parent. */
	sc->sc_intr_parent = fdtbus_get_phandle(phandle, "interrupt-parent");
	if (sc->sc_intr_parent <= 0) {
		aprint_error(": unable to find interrupt-parent\n");
		return;
	}

	/*
	 * DT bindings say that we use the same interrupt specifier
	 * format as "arm,gic", and the DT bindings specify 3 cells
	 * for that.  We make assumptions about that format, so assert
	 * one of our assumptions here.
	 */
	if (of_getprop_uint32(phandle, "#interrupt-cells", &ncells) != 0) {
		aprint_error(": unable to find #interrupt-cells\n");
		return;
	}
	if (ncells != NCELLS) {
		aprint_error(": incorrect #interrupt-cells (%u != %u)\n",
		    ncells, NCELLS);
		return;
	}
	if (of_getprop_uint32(sc->sc_intr_parent, "#interrupt-cells",
			      &ncells) != 0) {
		aprint_error(": unable to find parent #interrupt-cells\n");
		return;
	}
	if (ncells != NCELLS) {
		aprint_error(": incorrect parent #interrupt-cells (%u != %u)\n",
		    ncells, NCELLS);
		return;
	}

	aprint_naive("\n");
	aprint_normal(": Interrupt polarity controller\n");

	sc->sc_dev = self;
	sc->sc_bst = faa->faa_bst;
	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);

	sc->sc_blocks = kmem_zalloc(sc->sc_nncells * sizeof(*sc->sc_cells),
				   KM_SLEEP);

	/* Now map each individual cell. */
	for (next_irq = 0, i = 0; i < sc->sc_nblocks; i++) {
		error = fdtbus_get_reg(phandle, i, &addr, &size);
		if (error) {
			aprint_error_dev(self,
			    "unable to get registers for cell #%u (error=%d)\n",
			    i, error);
			return;
		}
		error = bus_space_map(sc->sc_bst, addr, size, 0,
				      &sc->sc_blocks[i].b_bsh);
		if (error) {
			aprint_error_dev(self,
			    "unable to map regsiters for cell #%u (error=%d)\n",
			    i, error);
			return;
		}

		cell->b_first_irq = next_irq;
		next_irq += size * NBBY;
		cell->b_last_irq = next_irq - 1;
	}

	fdtbus_register_interrupt_controller(self, phandle,
	    mt6577_sysirq_fdt_intrfuncs);
}
