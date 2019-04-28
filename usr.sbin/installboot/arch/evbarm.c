/*	$NetBSD$	*/

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD$");
#endif  /* !__lint */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "installboot.h"
#include "evboards.h"

static int	evbarm_setboot(ib_params *);
static int	evbarm_clearboot(ib_params *);
static int	evbarm_editboot(ib_params *);

struct ib_mach ib_mach_evbarm = {
	.name		=	"evbarm",
	.setboot	=	evbarm_setboot,
	.clearboot	=	evbarm_clearboot,
	.editboot	=	evbarm_editboot,
	.valid_flags	=	IB_BOARD | IB_SOC,
	.mach_flags	=	MF_UBOOT,
};

static int meson_gxbb_setboot(ib_params *);

static const struct evboard_methods meson_gxbb_methods = {
	.name		=	"meson-gxbb",
	.setboot	=	meson_gxbb_setboot,
};

static int meson_gxl_setboot(ib_params *);

static const struct evboard_methods meson_gxl_methods = {
	.name		=	"meson-gxl",
	.setboot	=	meson_gxl_setboot,
};

static int rk3328_setboot(ib_params *);

static const struct evboard_methods rk3328_methods = {
	.name		=	"rk3328",
	.setboot	=	rk3328_setboot,
};

static int rk3399_setboot(ib_params *);

static const struct evboard_methods rk3399_methods = {
	.name		=	"rk3399",
	.setboot	=	rk3399_setboot,
};

static int sunxi_setboot(ib_params *);

static const struct evboard_methods sunxi_methods = {
	.name		=	"sunxi",
	.setboot	=	sunxi_setboot,
};

static const struct evboard_methods * const evbarm_methods[] = {
	&meson_gxbb_methods,
	&meson_gxl_methods,
	&rk3328_methods,
	&rk3399_methods,
	&sunxi_methods,
	NULL,
};

static int
evbarm_setboot(ib_params *params)
{
	const struct evboard_methods *m;

	m = evb_methods_lookup(params, evbarm_methods);
	if (m == NULL)
		return 0;

	return m->setboot(params);
}

static int
evbarm_clearboot(ib_params *params)
{
	const struct evboard_methods *m;

	m = evb_methods_lookup(params, evbarm_methods);
	if (m == NULL)
		return 0;

	if (m->clearboot != NULL)
		return m->clearboot(params);

	return no_clearboot(params);
}

static int
evbarm_editboot(ib_params *params)
{
	const struct evboard_methods *m;

	m = evb_methods_lookup(params, evbarm_methods);
	if (m == NULL)
		return 0;

	if (m->editboot != NULL)
		return m->editboot(params);

	return no_editboot(params);
}

/*
 * meson-gxbb methods
 */
static const struct evboard_uboot_desc meson_gxbb_uboot_description[] = {
	{ .filename = "bl1.bin.hardkernel",
	  .file_size = 442,
	  .flags = UB_PRESERVE,
	},
	{ .filename = "bl1.bin.hardkernel",
	  .file_offset = 512,
	  .image_offset = 512,
	},
	{ .filename = "u-boot.gxbb",
	  .image_offset = (97 * 512),
	},
	{ .filename = NULL }
};

static int
meson_gxbb_setboot(ib_params *params)
{

	return evb_uboot_setboot(params, meson_gxbb_uboot_description);
}

/*
 * meson-gxl methods
 */
static const struct evboard_uboot_desc meson_gxl_uboot_description_sd[] = {
	{ .filename = "u-boot.bin.sd.bin",
	  .file_size = 442,
	  .flags = UB_PRESERVE,
	},
	{ .filename = "u-boot.bin.sd.bin",
	  .file_offset = 512,
	  .image_offset = 512,
	},
	{ .filename = NULL }
};

static int
meson_gxl_setboot(ib_params *params)
{

	/* XXX Add code to select media type. */

	return evb_uboot_setboot(params, meson_gxl_uboot_description_sd);
}

/*
 * rk3328 methods
 */
static const struct evboard_uboot_desc rk3328_uboot_description[] = {
	{ .filename = "idbloader.img",
	  .image_offset = (32 * 1024),
	},
	{ .filename = NULL }
};

static int
rk3328_setboot(ib_params *params)
{

	return evb_uboot_setboot(params, rk3328_uboot_description);
}

/*
 * rk3399 methods
 */
static const struct evboard_uboot_desc rk3399_uboot_description_sd[] = {
	{ .filename = "rksd_loader.img",
	  .image_offset = (32 * 1024),
	},
	{ .filename = NULL }
};

static int
rk3399_setboot(ib_params *params)
{

	/* XXX Add code to select media type. */

	return evb_uboot_setboot(params, rk3399_uboot_description_sd);
}

/*
 * sunxi methods
 */
static const struct evboard_uboot_desc sunxi_uboot_description[] = {
	{ .filename = "u-boot-sunxi-with-spl.bin",
	  .image_offset = (8 * 1024),
	},
	{ .filename = NULL }
};

static int
sunxi_setboot(ib_params *params)
{

	return evb_uboot_setboot(params, sunxi_uboot_description);
}
