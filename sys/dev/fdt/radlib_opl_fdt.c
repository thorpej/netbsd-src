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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/gpio.h>
#include <sys/intr.h>

#include <dev/fdt/fdtvar.h>

#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#define	RADLIB_OPL_DATA_NPINS	8

struct radlib_opl_softc {
	struct opl_softc	sc_opl;
	kmutex_t		sc_lock;
	int			sc_phandle;

	struct fdtbus_gpio_pin	*sc_Dx_pins[RADLIB_OPL_DATA_NPINS];
	struct fdtbus_gpio_pin	*sc_a0_pin;
	struct fdtbus_gpio_pin	*sc_read_pin;
	struct fdtbus_gpio_pin	*sc_cs_pin;
	struct fdtbus_gpio_pin	*sc_rst_pin;
};

static const char *compatible[] = {
	"thorpej,radlib-opl2",
	NULL
};

static uint8_t	radlib_opl_read_status(struct opl_softc *, int);
static void	radlib_opl_send_command(struct opl_softc *, int, int, int);

static int
radlib_opl_match(device_t parent, cfdata_t match, void *aux)
{
	struct fdt_attach_args * const faa = aux;

	return of_match_compatible(faa->faa_phandle, compatible);
}

static void
radlib_opl_attach(device_t parent, device_t self, void *aux)
{
	struct radlib_opl_softc *sc = device_private(self);
	struct opl_softc *opl = &sc->sc_opl;
	struct fdt_attach_args * const faa = aux;
	int i;

	aprint_naive("\n");
	aprint_normal(": RAdLib OPL2 FM Synthesizer\n");

	sc->sc_phandle = faa->faa_phandle;

	opl->dev = self;
	opl->offs = 0;
	opl->model = OPL_2;
	opl->lock = &sc->sc_lock;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);

	opl->read_status = radlib_opl_read_status;
	opl->send_command = radlib_opl_send_command;

	/* Reset pin. */
	sc->sc_rst_pin = fdtbus_gpio_acquire_index(sc->sc_phandle,
	    "rst-gpios", 0, GPIO_PIN_OUTPUT);
	if (sc->sc_rst_pin == NULL) {
		aprint_error_dev(self, "unable to acquire /RST pin\n");
		return;
	}

	/* Hold the OPL2 in reset. */
	fdtbus_gpio_write(sc->sc_rst_pin, GPIO_PIN_HIGH);

	/* Configure all the control pins. */
	if (fdtbus_pinctrl_set_config(sc->sc_phandle, "control-outputs")) {
		aprint_error_dev(self, "unable to configure control-outputs\n");
		return;
	}

	/* A0 pin. */
	sc->sc_a0_pin = fdtbus_gpio_acquire_index(sc->sc_phandle,
	    "a0-gpios", 0, GPIO_PIN_OUTPUT);
	if (sc->sc_a0_pin == NULL) {
		aprint_error_dev(self, "unable to acquire A0 pin\n");
		return;
	}
	fdtbus_gpio_write(sc->sc_a0_pin, GPIO_PIN_LOW);

	/* READ pin. */
	sc->sc_read_pin = fdtbus_gpio_acquire_index(sc->sc_phandle,
	    "read-gpios", 0, GPIO_PIN_OUTPUT);
	if (sc->sc_read_pin == NULL) {
		aprint_error_dev(self, "unable to acquire READ pin\n");
		return;
	}
	fdtbus_gpio_write(sc->sc_read_pin, GPIO_PIN_LOW);

	/* Chip Select pin. */
	sc->sc_cs_pin = fdtbus_gpio_acquire_index(sc->sc_phandle,
	    "cs-gpios", 0, GPIO_PIN_OUTPUT);
	if (sc->sc_cs_pin == NULL) {
		aprint_error_dev(self, "unable to acquire /CS pin\n");
		return;
	}
	fdtbus_gpio_write(sc->sc_cs_pin, GPIO_PIN_LOW);

	/* Configure the rest of the pins. */
	if (fdtbus_pinctrl_set_config(sc->sc_phandle, "irq-inputs")) {
		aprint_error_dev(self, "unable to configure irq-inputs\n");
		return;
	}
	if (fdtbus_pinctrl_set_config(sc->sc_phandle, "data-outputs")) {
		aprint_error_dev(self, "unable to configure data-outputs\n");
		return;
	}

	/* Data pins. */
	for (i = 0; i < RADLIB_OPL_DATA_NPINS; i++) {
		sc->sc_Dx_pins[i] = fdtbus_gpio_acquire_index(sc->sc_phandle,
		    "data-gpios", i, GPIO_PIN_OUTPUT);
		if (sc->sc_Dx_pins[i] == NULL) {
			aprint_error_dev(self,
			    "unable to acquire D%d pin\n", i);
			return;
		}
		fdtbus_gpio_write(sc->sc_Dx_pins[i], GPIO_PIN_LOW);
	}

	/* Release the OPL2 from reset. */
	fdtbus_gpio_write(sc->sc_rst_pin, GPIO_PIN_LOW);

	opl_attach(&sc->sc_opl);
}

CFATTACH_DECL_NEW(radlib_opl_fdt, sizeof(struct radlib_opl_softc),
    radlib_opl_match, radlib_opl_attach, NULL, NULL);

static void
radlib_opl_set_data_input(struct radlib_opl_softc *sc)
{
	if (fdtbus_pinctrl_set_config(sc->sc_phandle, "data-inputs")) {
		aprint_error_dev(sc->sc_opl.dev,
		    "unable to set data-inputs\n");
	}
}

static void
radlib_opl_set_data_output(struct radlib_opl_softc *sc)
{
	if (fdtbus_pinctrl_set_config(sc->sc_phandle, "data-outputs")) {
		aprint_error_dev(sc->sc_opl.dev,
		    "unable to set data-outputs\n");
	}
}

static inline void
radlib_opl_set_a0(struct radlib_opl_softc *sc, bool val)
{
	fdtbus_gpio_write(sc->sc_a0_pin, val);
}

static inline void
radlib_opl_set_read(struct radlib_opl_softc *sc, bool val)
{
	fdtbus_gpio_write(sc->sc_read_pin, val);
}

static inline void
radlib_opl_set_cs(struct radlib_opl_softc *sc, bool val)
{
	fdtbus_gpio_write(sc->sc_cs_pin, val);
}

static uint8_t
radlib_opl_read(struct radlib_opl_softc *sc)
{
	/*
	 * Read timing:
	 *
	 * Address setup: 10ns min
	 * Chip select pulse: 200ns min
	 * Read pulse: 200ns min
	 * Read data access: 200ns max once both /CS and /RD are settled low
	 * Read data hold: 10ns min
	 *
	 * Note we set the D[0-7] pins to inputs BEFORE setting the
	 * read signal to avoid having both the host and the RAdLib
	 * driving the pins simultaneously.
	 */

	radlib_opl_set_a0(sc, false);
	radlib_opl_set_data_input(sc);

	/*
	 * Device lock is held, but we want to disable interrupts and
	 * preemption on the current CPU while we perform the timing-
	 * critical operation.
	 */
	int s = splhigh();

	radlib_opl_set_read(sc, true);
	radlib_opl_set_cs(sc, true);
	delay(1);

#define	RADLIB_GET_DPIN(x)						\
	(fdtbus_gpio_read(sc->sc_Dx_pins[(x)]) << (x))

	const uint8_t rv =
	    RADLIB_GET_DPIN(0) | RADLIB_GET_DPIN(1) | RADLIB_GET_DPIN(2) |
	    RADLIB_GET_DPIN(3) | RADLIB_GET_DPIN(4) | RADLIB_GET_DPIN(5) |
	    RADLIB_GET_DPIN(6) | RADLIB_GET_DPIN(7);

#undef RADLIB_GET_DPIN

	radlib_opl_set_cs(sc, false);
	radlib_opl_set_read(sc, false);
	delay(1);

	/* Timing-critical section complete. */
	splx(s);

	/* Reads are rare; quiesce back into "write" mode. */
	radlib_opl_set_data_output(sc);

	return rv;
}

static void
radlib_opl_write(struct radlib_opl_softc *sc, int addr, int data)
{
	/*
	 * Write timing:
	 *
	 * Address setup: 10ns min
	 * Chip select pulse: 100ns min
	 * Write pulse: 100ns min
	 * Write data setup: 20ns min once both /CS and /WR are settled low
	 * Write data hold: 30ns min
	 *
	 * Note we set the D[0-7] pins to inputs BEFORE setting the
	 * read signal to avoid having both the host and the RAdLib
	 * driving the pins simultaneously.
	 */

	radlib_opl_set_a0(sc, addr & 1);

#define	RADLIB_SET_DPIN(x)						\
	fdtbus_gpio_write(sc->sc_Dx_pins[(x)], (data & (1U << (x))) ? 1 : 0)

	RADLIB_SET_DPIN(0); RADLIB_SET_DPIN(1); RADLIB_SET_DPIN(2);
	RADLIB_SET_DPIN(3); RADLIB_SET_DPIN(4); RADLIB_SET_DPIN(5);
	RADLIB_SET_DPIN(6); RADLIB_SET_DPIN(7);

#undef RADLIB_SET_DPIN

	/* See comment in radlib_opl_read(). */
	int s = splhigh();

	radlib_opl_set_cs(sc, true);
	delay(1);
	radlib_opl_set_cs(sc, false);

	splx(s);
}

static uint8_t
radlib_opl_read_status(struct opl_softc *opl, int offs __unused)
{
	return radlib_opl_read((struct radlib_opl_softc *)opl);
}

static void
radlib_opl_send_command(struct opl_softc *opl, int offs __unused,
			 int addr, int data)
{
	struct radlib_opl_softc *sc = (struct radlib_opl_softc *)opl;

	radlib_opl_write(sc, OPL_ADDR, addr);
	delay(10);

	radlib_opl_write(sc, OPL_DATA, data);
	delay(30);
}
