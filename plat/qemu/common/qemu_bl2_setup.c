/*
 * Copyright (c) 2015-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <string.h>

#include <libfdt.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <common/fdt_fixup.h>
#include <lib/optee_utils.h>
#include <lib/utils.h>
#include <plat/common/platform.h>
#if SPMD_SPM_AT_SEL2
#include <lib/fconf/fconf.h>
#include <lib/fconf/fconf_dyn_cfg_getter.h>
#endif

#include "qemu_private.h"

#if SPMD_SPM_AT_SEL2
/* Base address of fw_config received from BL1 */
static uintptr_t config_base;
#endif

/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

void bl2_early_platform_setup2(u_register_t arg0, u_register_t arg1,
			       u_register_t arg2, u_register_t arg3)
{
#if SPMD_SPM_AT_SEL2
	config_base = (uintptr_t)arg0;
#endif
	meminfo_t *mem_layout = (void *)arg1;

	/* Initialize the console to provide early debug support */
	qemu_console_init();

	/* Setup the BL2 memory layout */
	bl2_tzram_layout = *mem_layout;

	plat_qemu_io_setup();
}

static void security_setup(void)
{
	/*
	 * This is where a TrustZone address space controller and other
	 * security related peripherals, would be configured.
	 */
}

static void update_dt(void)
{
	int ret;
	void *fdt = (void *)(uintptr_t)ARM_PRELOADED_DTB_BASE;

	ret = fdt_open_into(fdt, fdt, PLAT_QEMU_DT_MAX_SIZE);
	if (ret < 0) {
		ERROR("Invalid Device Tree at %p: error %d\n", fdt, ret);
		return;
	}

	if (dt_add_psci_node(fdt)) {
		ERROR("Failed to add PSCI Device Tree node\n");
		return;
	}

	if (dt_add_psci_cpu_enable_methods(fdt)) {
		ERROR("Failed to add PSCI cpu enable methods in Device Tree\n");
		return;
	}

	ret = fdt_pack(fdt);
	if (ret < 0)
		ERROR("Failed to pack Device Tree at %p: error %d\n", fdt, ret);
}

#if SPMD_SPM_AT_SEL2
/*
 * BL2 utility function to initialize dynamic configuration specified by
 * FW_CONFIG. Populate the bl_mem_params_node_t of other FW_CONFIGs if
 * specified in FW_CONFIG.
 */
void qemu_bl2_dyn_cfg_init(void)
{
	unsigned int i;
	bl_mem_params_node_t *cfg_mem_params = NULL;
	uintptr_t image_base;
	uint32_t image_size;
	const unsigned int config_ids[] = {
			HW_CONFIG_ID,
			SOC_FW_CONFIG_ID,
			NT_FW_CONFIG_ID,
			TOS_FW_CONFIG_ID
	};

	const struct dyn_cfg_dtb_info_t *dtb_info;

	/* Iterate through all the fw config IDs */
	for (i = 0; i < ARRAY_SIZE(config_ids); i++) {
		cfg_mem_params = get_bl_mem_params_node(config_ids[i]);
		if (cfg_mem_params == NULL) {
			INFO("%s config_id %d in bl_mem_params_node\n",
				"Couldn't find", config_ids[i]);
			continue;
		}

		/* Get the config load address and size from FW_CONFIG */
		dtb_info = FCONF_GET_PROPERTY(dyn_cfg, dtb, config_ids[i]);
		if (dtb_info == NULL) {
			INFO("%sconfig_id %d load info in FW_CONFIG\n",
				"Couldn't find ", config_ids[i]);
			continue;
		}

		image_base = dtb_info->config_addr;
		image_size = dtb_info->config_max_size;

		/*
		 * Do some runtime checks on the load addresses of soc_fw_config,
		 * tos_fw_config, nt_fw_config. This is not a comprehensive check
		 * of all invalid addresses but to prevent trivial porting errors.
		 */
		if (config_ids[i] != HW_CONFIG_ID) {

			if (check_uptr_overflow(image_base, image_size)) {
				continue;
			}
#ifdef	BL31_BASE
			/* Ensure the configs don't overlap with BL31 */
			if ((image_base >= BL31_BASE) &&
			    (image_base <= BL31_LIMIT)) {
				continue;
			}
#endif
			/* Ensure the configs are loaded in a valid address */
			if (image_base < BL_RAM_BASE) {
				continue;
			}
#ifdef BL32_BASE
			/*
			 * If BL32 is present, ensure that the configs don't
			 * overlap with it.
			 */
			if ((image_base >= BL32_BASE) &&
			    (image_base <= BL32_LIMIT)) {
				continue;
			}
#endif
		}

		cfg_mem_params->image_info.image_base = image_base;
		cfg_mem_params->image_info.image_max_size = (uint32_t)image_size;

		/*
		 * Remove the IMAGE_ATTRIB_SKIP_LOADING attribute from
		 * HW_CONFIG or FW_CONFIG nodes
		 */
		cfg_mem_params->image_info.h.attr &= ~IMAGE_ATTRIB_SKIP_LOADING;
	}
}
#endif

void bl2_platform_setup(void)
{
	security_setup();
	update_dt();
#if SPMD_SPM_AT_SEL2
	qemu_bl2_dyn_cfg_init();
#endif

	/* TODO Initialize timer */
}

#ifdef __aarch64__
#define QEMU_CONFIGURE_BL2_MMU(...)	qemu_configure_mmu_el1(__VA_ARGS__)
#else
#define QEMU_CONFIGURE_BL2_MMU(...)	qemu_configure_mmu_svc_mon(__VA_ARGS__)
#endif

void bl2_plat_arch_setup(void)
{
#if SPMD_SPM_AT_SEL2
	const struct dyn_cfg_dtb_info_t *tb_fw_config_info;
#endif
	QEMU_CONFIGURE_BL2_MMU(bl2_tzram_layout.total_base,
			      bl2_tzram_layout.total_size,
			      BL_CODE_BASE, BL_CODE_END,
			      BL_RO_DATA_BASE, BL_RO_DATA_END,
			      BL_COHERENT_RAM_BASE, BL_COHERENT_RAM_END);

#if SPMD_SPM_AT_SEL2
	/* Fill the properties struct with the info from the config dtb */
	fconf_populate("FW_CONFIG", config_base);

	/* TB_FW_CONFIG was also loaded by BL1 */
	tb_fw_config_info = FCONF_GET_PROPERTY(dyn_cfg, dtb, TB_FW_CONFIG_ID);
	assert(tb_fw_config_info != NULL);

	fconf_populate("TB_FW", tb_fw_config_info->config_addr);
#endif
}

/*******************************************************************************
 * Gets SPSR for BL32 entry
 ******************************************************************************/
static uint32_t qemu_get_spsr_for_bl32_entry(void)
{
#ifdef __aarch64__
	/*
	 * The Secure Payload Dispatcher service is responsible for
	 * setting the SPSR prior to entry into the BL3-2 image.
	 */
	return 0;
#else
	return SPSR_MODE32(MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE,
			   DISABLE_ALL_EXCEPTIONS);
#endif
}

/*******************************************************************************
 * Gets SPSR for BL33 entry
 ******************************************************************************/
static uint32_t qemu_get_spsr_for_bl33_entry(void)
{
	uint32_t spsr;
#ifdef __aarch64__
	unsigned int mode;

	/* Figure out what mode we enter the non-secure world in */
	mode = (el_implemented(2) != EL_IMPL_NONE) ? MODE_EL2 : MODE_EL1;

	/*
	 * TODO: Consider the possibility of specifying the SPSR in
	 * the FIP ToC and allowing the platform to have a say as
	 * well.
	 */
	spsr = SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);
#else
	spsr = SPSR_MODE32(MODE32_svc,
		    plat_get_ns_image_entrypoint() & 0x1,
		    SPSR_E_LITTLE, DISABLE_ALL_EXCEPTIONS);
#endif
	return spsr;
}

static int qemu_bl2_handle_post_image_load(unsigned int image_id)
{
	int err = 0;
	bl_mem_params_node_t *bl_mem_params = get_bl_mem_params_node(image_id);
#if defined(SPD_opteed) || defined(AARCH32_SP_OPTEE) || defined(SPMC_OPTEE)
	bl_mem_params_node_t *pager_mem_params = NULL;
	bl_mem_params_node_t *paged_mem_params = NULL;
#endif
#if defined(SPD_spmd)
#if SPMD_SPM_AT_SEL2
        bl_mem_params_node_t *tos_fw_config_mem_params = NULL;
        bl_mem_params_node_t *hw_config_mem_params = NULL;
        unsigned int mode_rw = MODE_RW_64;

        /* return if the image is SP image */
        if ((bl_mem_params == NULL) && (image_id > MAX_IMAGE_IDS))
                return err;
#else
	unsigned int mode_rw = MODE_RW_64;
	uint64_t pagable_part = 0;
#endif
#endif

	assert(bl_mem_params);

	switch (image_id) {
	case BL32_IMAGE_ID:
#if defined(SPD_opteed) || defined(AARCH32_SP_OPTEE) || defined(SPMC_OPTEE)
		pager_mem_params = get_bl_mem_params_node(BL32_EXTRA1_IMAGE_ID);
		assert(pager_mem_params);

		paged_mem_params = get_bl_mem_params_node(BL32_EXTRA2_IMAGE_ID);
		assert(paged_mem_params);

		err = parse_optee_header(&bl_mem_params->ep_info,
					 &pager_mem_params->image_info,
					 &paged_mem_params->image_info);
		if (err != 0) {
			WARN("OPTEE header parse error.\n");
		}
#if defined(SPD_spmd)
		mode_rw = bl_mem_params->ep_info.args.arg0;
		pagable_part = bl_mem_params->ep_info.args.arg1;
#endif
#endif

#if defined(SPD_spmd)
#if SPMD_SPM_AT_SEL2
                mode_rw = bl_mem_params->ep_info.args.arg0;

                tos_fw_config_mem_params = get_bl_mem_params_node(TOS_FW_CONFIG_ID);
                assert(tos_fw_config_mem_params);

                hw_config_mem_params = get_bl_mem_params_node(HW_CONFIG_ID);
                assert(hw_config_mem_params);

                bl_mem_params->ep_info.args.arg0 = tos_fw_config_mem_params->image_info.image_base;
                bl_mem_params->ep_info.args.arg1 = hw_config_mem_params->image_info.image_base;
                bl_mem_params->ep_info.args.arg2 = mode_rw;
                bl_mem_params->ep_info.args.arg3 = ARM_PRELOADED_DTB_BASE;
#else
		bl_mem_params->ep_info.args.arg0 = ARM_PRELOADED_DTB_BASE;
		bl_mem_params->ep_info.args.arg1 = pagable_part;
		bl_mem_params->ep_info.args.arg2 = mode_rw;
		bl_mem_params->ep_info.args.arg3 = 0;
#endif
#elif defined(SPD_opteed)
		/*
		 * OP-TEE expect to receive DTB address in x2.
		 * This will be copied into x2 by dispatcher.
		 */
		bl_mem_params->ep_info.args.arg3 = ARM_PRELOADED_DTB_BASE;
#elif defined(AARCH32_SP_OPTEE)
		bl_mem_params->ep_info.args.arg0 =
					bl_mem_params->ep_info.args.arg1;
		bl_mem_params->ep_info.args.arg1 = 0;
		bl_mem_params->ep_info.args.arg2 = ARM_PRELOADED_DTB_BASE;
		bl_mem_params->ep_info.args.arg3 = 0;
#endif
		bl_mem_params->ep_info.spsr = qemu_get_spsr_for_bl32_entry();
		break;

	case BL33_IMAGE_ID:
#ifdef AARCH32_SP_OPTEE
		/* AArch32 only core: OP-TEE expects NSec EP in register LR */
		pager_mem_params = get_bl_mem_params_node(BL32_IMAGE_ID);
		assert(pager_mem_params);
		pager_mem_params->ep_info.lr_svc = bl_mem_params->ep_info.pc;
#endif

#if ARM_LINUX_KERNEL_AS_BL33
		/*
		 * According to the file ``Documentation/arm64/booting.txt`` of
		 * the Linux kernel tree, Linux expects the physical address of
		 * the device tree blob (DTB) in x0, while x1-x3 are reserved
		 * for future use and must be 0.
		 */
		bl_mem_params->ep_info.args.arg0 =
			(u_register_t)ARM_PRELOADED_DTB_BASE;
		bl_mem_params->ep_info.args.arg1 = 0U;
		bl_mem_params->ep_info.args.arg2 = 0U;
		bl_mem_params->ep_info.args.arg3 = 0U;
#else
		/* BL33 expects to receive the primary CPU MPID (through r0) */
		bl_mem_params->ep_info.args.arg0 = 0xffff & read_mpidr();
#endif

		bl_mem_params->ep_info.spsr = qemu_get_spsr_for_bl33_entry();
		break;
	default:
		/* Do nothing in default case */
		break;
	}

	return err;
}

/*******************************************************************************
 * This function can be used by the platforms to update/use image
 * information for given `image_id`.
 ******************************************************************************/
int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	return qemu_bl2_handle_post_image_load(image_id);
}

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return NS_IMAGE_OFFSET;
}
