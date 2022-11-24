/*
 * Copyright (c) 2015-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QEMU_PRIVATE_H
#define QEMU_PRIVATE_H

#include <stdint.h>

void qemu_configure_mmu_svc_mon(unsigned long total_base,
			unsigned long total_size,
			unsigned long code_start, unsigned long code_limit,
			unsigned long ro_start, unsigned long ro_limit,
			unsigned long coh_start, unsigned long coh_limit);

void qemu_configure_mmu_el1(unsigned long total_base, unsigned long total_size,
			unsigned long code_start, unsigned long code_limit,
			unsigned long ro_start, unsigned long ro_limit,
			unsigned long coh_start, unsigned long coh_limit);

void qemu_configure_mmu_el3(unsigned long total_base, unsigned long total_size,
			unsigned long code_start, unsigned long code_limit,
			unsigned long ro_start, unsigned long ro_limit,
			unsigned long coh_start, unsigned long coh_limit);

void plat_qemu_io_setup(void);
unsigned int plat_qemu_calc_core_pos(u_register_t mpidr);

void qemu_console_init(void);

void plat_qemu_gic_init(void);
void qemu_pwr_gic_on_finish(void);
void qemu_pwr_gic_off(void);

int qemu_set_tos_fw_info(uintptr_t config_base, uintptr_t log_addr,
			size_t log_size);

int qemu_set_nt_fw_info(
/*
 * Currently OP-TEE does not support reading DTBs from Secure memory
 * and this option should be removed when feature is supported.
 */
#ifdef SPD_opteed
			uintptr_t log_addr,
#endif
			size_t log_size,
			uintptr_t *ns_log_addr);

#if SPMD_SPM_AT_SEL2
void qemu_bl1_platform_setup(void);
#endif
#endif /* QEMU_PRIVATE_H */
