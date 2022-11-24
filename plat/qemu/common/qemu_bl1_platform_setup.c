/*
 * Copyright (c) 2021-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <arch.h>
#include <bl1/bl1.h>
#include <common/bl_common.h>
#include <lib/fconf/fconf.h>
#include <lib/fconf/fconf_dyn_cfg_getter.h>
#include <lib/utils.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

/*
 * To enable FW_CONFIG to be loaded by BL1, define the corresponding base
 * and limit. Leave enough space of BL2 meminfo.
 */
#define ARM_FW_CONFIG_BASE             (BL_RAM_BASE + sizeof(meminfo_t))
#define ARM_FW_CONFIG_LIMIT            ((BL_RAM_BASE + PAGE_SIZE) \
		+ (PAGE_SIZE / 2U))
void qemu_bl1_platform_setup(void)
{
	const struct dyn_cfg_dtb_info_t *fw_config_info;
	image_desc_t *desc;
	uint32_t fw_config_max_size;
	int err = -1;

	/* Set global DTB info for fixed fw_config information */
	fw_config_max_size = ARM_FW_CONFIG_LIMIT - ARM_FW_CONFIG_BASE;
	set_config_info(ARM_FW_CONFIG_BASE, ~0UL, fw_config_max_size, FW_CONFIG_ID);

	/* Fill the device tree information struct with the info from the config dtb */
	err = fconf_load_config(FW_CONFIG_ID);
	if (err < 0) {
		ERROR("Loading of FW_CONFIG failed %d\n", err);
		/*plat_error_handler(err);*/
		return;
	}

	/*
	 * FW_CONFIG loaded successfully. If FW_CONFIG device tree parsing
	 * is successful then load TB_FW_CONFIG device tree.
	 */
	fw_config_info = FCONF_GET_PROPERTY(dyn_cfg, dtb, FW_CONFIG_ID);
	if (fw_config_info != NULL) {
		err = fconf_populate_dtb_registry(fw_config_info->config_addr);
		if (err < 0) {
			ERROR("Parsing of FW_CONFIG failed %d\n", err);
			plat_error_handler(err);
		}
		/* load TB_FW_CONFIG */
		err = fconf_load_config(TB_FW_CONFIG_ID);
		if (err < 0) {
			ERROR("Loading of TB_FW_CONFIG failed %d\n", err);
			plat_error_handler(err);
		}
	} else {
		ERROR("Invalid FW_CONFIG address\n");
		plat_error_handler(err);
	}

	/* The BL2 ep_info arg0 is modified to point to FW_CONFIG */
	desc = bl1_plat_get_image_desc(BL2_IMAGE_ID);
	assert(desc != NULL);
	desc->ep_info.args.arg0 = fw_config_info->config_addr;
}
