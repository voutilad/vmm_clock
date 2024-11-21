// SPDX-License-Identifier: GPL-2.0-or-later
/*  paravirtual clock -- common code used by kvm/xen
 *
 *  This code taken from the kernel tree v5.4.42.
 */
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <asm/msr.h>

#include "pvclock.h"

static u8 valid_flags __read_mostly = 0;
static struct pvclock_vsyscall_time_info *pvti_cpu0_va __read_mostly;

void pvclock_set_flags(u8 flags)
{
	valid_flags = flags;
}

static atomic64_t last_value = ATOMIC64_INIT(0);

void pvclock_resume(void)
{
	atomic64_set(&last_value, 0);
}

u8 pvclock_read_flags(struct pvclock_vcpu_time_info *src)
{
	unsigned version;
	u8 flags;

	do {
		version = pvclock_read_begin(src);
		flags = src->flags;
	} while (pvclock_read_retry(src, version));

	return flags & valid_flags;
}

/*
 * This function was rewritten to mimic the exact logic in OpenBSD's
 * pvclock(4) implementation.
 */
u64 pvclock_clocksource_read(struct pvclock_vcpu_time_info *src)
{
	unsigned version;
	u64 last;
	u8 flags;

	int shift;
	u64 tsc_timestamp, system_time, delta, ctr;
	u32 mul_frac;

	do {
		version = pvclock_read_begin(src);
		flags = src->flags;
		system_time = src->system_time;
		tsc_timestamp = src->tsc_timestamp;
		mul_frac = src->tsc_to_system_mul;
		shift = src->tsc_shift;
	} while (pvclock_read_retry(src, version));

	// via OpenBSD's pvclock.c
	delta = rdtsc() - tsc_timestamp;
	if (shift < 0)
		delta >>= -shift;
	else
		delta <<= shift;
	ctr = ((delta * mul_frac) >> 32) + system_time;

	if ((valid_flags & PVCLOCK_TSC_STABLE_BIT) &&
		(flags & PVCLOCK_TSC_STABLE_BIT))
		return ctr;

	last = atomic64_read(&last_value);
	do {
		if (ctr < last)
			return last;
		last = atomic64_cmpxchg(&last_value, last, ctr);
	} while (unlikely(last != ctr));

	return ctr;
}

void pvclock_set_pvti_cpu0_va(struct pvclock_vsyscall_time_info *pvti)
{
	pvti_cpu0_va = pvti;
}
