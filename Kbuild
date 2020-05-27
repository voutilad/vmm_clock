# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2020 Dave Voutila <dave@sisu.io>. All rights reserved.

ccflags-y := -O2 -Wall
ccflags-y += -DCONFIG_HZ=$(CONFIG_HZ)
ccflags-$(CONFIG_VMM_CLOCK_DEBUG) += -DDEBUG -g

obj-m := vmm_clock.o
vmm_clock-objs := vmm_clock_main.o pvclock.o
