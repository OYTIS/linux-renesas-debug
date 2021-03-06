/*
 * SMP support for r8a7790
 *
 * Copyright (C) 2014 Renesas Electronics Corporation
 * Copyright (C) 2012-2013 Renesas Solutions Corp.
 * Copyright (C) 2012 Takashi Yoshii <takashi.yoshii.ze@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/smp_plat.h>
#include <mach/platsmp-apmu.h>
#include <mach/platsmp-rst.h>

#include "common.h"
#include "pm-rcar.h"
#include "r8a7790.h"
#include "rcar-gen2.h"

#define CCI_BASE	0xf0090000
#define CCI_SLAVE3	0x4000
#define CCI_SLAVE4	0x5000
#define CCI_SNOOP	0x0000
#define CCI_STATUS	0x000c
#define APMU		0xe6151000
#define CA7DBGRCR	0x0180
#define CA15DBGRCR	0x1180

#define CA15RESCNT	0x0040
#define CA7RESCNT	0x0044

static struct rcar_sysc_ch r8a7790_ca15_scu = {
	.chan_offs = 0x180, /* PWRSR5 .. PWRER5 */
	.isr_bit = 12, /* CA15-SCU */
};

static struct rcar_sysc_ch r8a7790_ca7_scu = {
	.chan_offs = 0x100, /* PWRSR3 .. PWRER3 */
	.isr_bit = 21, /* CA7-SCU */
};

static struct rcar_apmu_config r8a7790_apmu_config[] = {
	{
		.iomem = DEFINE_RES_MEM(0xe6152000, 0x188),
		.cpus = { 0, 1, 2, 3 },
	},
	{
		.iomem = DEFINE_RES_MEM(0xe6151000, 0x188),
		.cpus = { 0x100, 0x0101, 0x102, 0x103 },
	}
};

static struct rcar_rst_config r8a7790_rst_config[] = {
	{
		.rescnt = CA15RESCNT,
		.rescnt_magic = 0xa5a50000,
	},
	{
		.rescnt = CA7RESCNT,
		.rescnt_magic = 0x5a5a0000,
	}
};

static void __init r8a7790_smp_prepare_cpus(unsigned int max_cpus)
{
	void __iomem *p;
	u32 val;
	unsigned int k;

	/* let APMU code install data related to shmobile_boot_vector */
	shmobile_smp_apmu_prepare_cpus(max_cpus,
				       r8a7790_apmu_config,
				       ARRAY_SIZE(r8a7790_apmu_config));

	/* setup for debug mode */
	if (rcar_gen2_read_mode_pins() & MD(21)) {
		p = ioremap_nocache(APMU, 0x2000);
		val = readl_relaxed(p + CA15DBGRCR);
		writel_relaxed((val | 0x01f80000), p + CA15DBGRCR);
		val = readl_relaxed(p + CA7DBGRCR);
		writel_relaxed((val | 0x01f83330), p + CA7DBGRCR);
		iounmap(p);
	}

	/* turn on power to SCU */
	r8a7790_pm_init();
	rcar_sysc_power_up(&r8a7790_ca15_scu);
	rcar_sysc_power_up(&r8a7790_ca7_scu);

	/* keep secondary CPU cores in reset */
	r8a779x_init_reset(r8a7790_rst_config);
	for (k = 1; k < max_cpus; k++)
		r8a779x_assert_reset(k);

	/* enable snoop and DVM */
	p = ioremap_nocache(CCI_BASE, 0x8000);
	writel_relaxed(readl_relaxed(p + CCI_SLAVE3 + CCI_SNOOP) | 0x3,
		p + CCI_SLAVE3 + CCI_SNOOP);    /* ca15 */
	writel_relaxed(readl_relaxed(p + CCI_SLAVE4 + CCI_SNOOP) | 0x3,
		p + CCI_SLAVE4 + CCI_SNOOP);    /* ca7 */
	while (__raw_readl(p + CCI_STATUS));
	/* wait for pending bit low */
	iounmap(p);
}

struct smp_operations r8a7790_smp_ops __initdata = {
	.smp_prepare_cpus	= r8a7790_smp_prepare_cpus,
	.smp_boot_secondary	= shmobile_smp_apmu_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= shmobile_smp_cpu_disable,
	.cpu_die		= shmobile_smp_apmu_cpu_die,
	.cpu_kill		= shmobile_smp_apmu_cpu_kill,
#endif
};
