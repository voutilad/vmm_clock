// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Implementation of an OpenBSD VMM paravirtualized clock source based
 *  on kvm-clock.
 *
 *  Copyright 2020 Dave Voutila
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/clocksource.h>
#include <linux/mm.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/set_memory.h>

#include <asm/cpufeature.h>
#include <asm/msr.h>
#include <asm/kvm_para.h>
#include <asm/mem_encrypt.h>
#include <asm/timer.h>

#include "pvclock.h"

static int msr_vmm_system_time __ro_after_init = MSR_KVM_SYSTEM_TIME;
static u64 vmm_sched_clock_offset __ro_after_init;

////////////////////////////////////
/*
 * From kvmclock.c
 */
#define HV_CLOCK_SIZE	(sizeof(struct pvclock_vsyscall_time_info) * NR_CPUS)
#define HVC_BOOT_ARRAY_SIZE \
	(PAGE_SIZE / sizeof(struct pvclock_vsyscall_time_info))

static struct pvclock_vsyscall_time_info
			hv_clock_boot[HVC_BOOT_ARRAY_SIZE] __bss_decrypted __aligned(PAGE_SIZE);
//static struct pvclock_wall_clock wall_clock __bss_decrypted;
static DEFINE_PER_CPU(struct pvclock_vsyscall_time_info *, hv_clock_per_cpu);
static struct pvclock_vsyscall_time_info *hvclock_mem;

static inline struct pvclock_vcpu_time_info *this_cpu_pvti(void)
{
	return &this_cpu_read(hv_clock_per_cpu)->pvti;
}

static inline struct pvclock_vsyscall_time_info *this_cpu_hvclock(void)
{
	return this_cpu_read(hv_clock_per_cpu);
}

static u64 vmm_clock_read(void)
{
	u64 ret;

	preempt_disable_notrace();
	ret = pvclock_clocksource_read(this_cpu_pvti());
	preempt_enable_notrace();
	return ret;
}

static u64 vmm_clock_get_cycles(struct clocksource *cs)
{
	return vmm_clock_read();
}


static inline void vmm_sched_clock_init(bool stable)
{
	vmm_sched_clock_offset = vmm_clock_read();

	pr_info("%s: using sched offset of %llu cycles",
	    __func__, vmm_sched_clock_offset);
}

static void vmmclock_init_mem(void)
{
	unsigned long ncpus;
	unsigned int order;
	struct page *p;
	int r;

	pr_info("%s: starting", __func__);
	// todo: simplify to 1
	if (HVC_BOOT_ARRAY_SIZE >= num_possible_cpus())
		return;

	ncpus = num_possible_cpus() - HVC_BOOT_ARRAY_SIZE;
	order = get_order(ncpus * sizeof(*hvclock_mem));

	p = alloc_pages(GFP_KERNEL, order);
	if (!p) {
		pr_warn("%s: failed to alloc %d pages", __func__, (1U << order));
		return;
	}

	hvclock_mem = page_address(p);

	/*
	 * hvclock is shared between the guest and the hypervisor, must
	 * be mapped decrypted.
	 */
	if (sev_active()) {
		r = set_memory_decrypted((unsigned long) hvclock_mem,
					 1UL << order);
		if (r) {
			__free_pages(p, order);
			hvclock_mem = NULL;
			pr_warn("%s: set_memory_decrypted() failed. Disabling",
			    __func__);
			return;
		}
	}

	memset(hvclock_mem, 0, PAGE_SIZE << order);
}

////////////////////////// END kvmclock.c copy pasta

/*
 * Detect if we're running under VMM(4) or not.
 */
static uint32_t vmm_detect(void)
{
	pr_info("%s: checking if we're running under OpenBSD VMM", __func__);
	if (boot_cpu_data.cpuid_level < 0)
		return 0;

	if (boot_cpu_has(X86_FEATURE_HYPERVISOR))
		return hypervisor_cpuid_base("OpenBSDVMM58", 0);

	return 0;
}

struct clocksource vmm_clock = {
	.name	= "vmm-clock",
	.read	= vmm_clock_get_cycles,
	.rating = 400,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};
EXPORT_SYMBOL_GPL(vmm_clock);

/*
 * Another kvmclock.c copy pasta
 */
static void vmm_register_clock(char *txt)
{
	struct pvclock_vsyscall_time_info *src = this_cpu_hvclock();
	u64 pa;

	if (!src)
		return;

	pa = slow_virt_to_phys(&src->pvti) | 0x01ULL;
	wrmsrl(msr_vmm_system_time, pa);
	pr_info("%s: cpu %d, msr %llx, %s\n", __func__, smp_processor_id(), pa, txt);
}

static int vmm_setup_vsyscall_timeinfo(void)
{
	u8 flags;

	pr_info("%s: starting\n", __func__);
	if (!per_cpu(hv_clock_per_cpu, 0))
		return 0;

	flags = pvclock_read_flags(&hv_clock_boot[0].pvti);
	if (!(flags & PVCLOCK_TSC_STABLE_BIT))
		return 0;

	vmmclock_init_mem();

	return 0;
}

/*
 * Based heavily on the logic from kvmclock.c
 */
static int __init vmm_clock_init(void)
{
	u8 flags;

	if (!vmm_detect()) {
		pr_info("%s: failed to detect OpenBSD VMM hypervisor\n", __func__);
		return 1;
	}

	pr_info("%s: starting to attempt attachment\n", __func__);

	//// ATTACHING LOGIC
	msr_vmm_system_time = MSR_KVM_SYSTEM_TIME_NEW;

	vmm_setup_vsyscall_timeinfo();

	this_cpu_write(hv_clock_per_cpu, &hv_clock_boot[0]);
	vmm_register_clock("primary cpu clock");
	pvclock_set_pvti_cpu0_va(hv_clock_boot);

	// We can assume stable bit
	pvclock_set_flags(PVCLOCK_TSC_STABLE_BIT);

	flags = pvclock_read_flags(&hv_clock_boot[0].pvti);
	vmm_sched_clock_init(flags & PVCLOCK_TSC_STABLE_BIT);

	pr_info("%s: attempting to register clocksource\n", __func__);
	clocksource_register_hz(&vmm_clock, NSEC_PER_SEC);

	return 0;
}

static void __exit vmm_clock_exit(void)
{
	pr_info("%s: unregistering clocksource\n", __func__);
	clocksource_unregister(&vmm_clock);
}

module_init(vmm_clock_init);
module_exit(vmm_clock_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dave Voutila <voutilad@gmail.com>");
MODULE_DESCRIPTION("OpenBSD VMM Paravirt Clock");
MODULE_VERSION("1");
