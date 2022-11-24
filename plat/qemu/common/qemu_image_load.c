/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/bl_common.h>
#include <common/desc_image_load.h>
#if SPMD_SPM_AT_SEL2
#include <plat/arm/common/fconf_arm_sp_getter.h>
#endif

/*******************************************************************************
 * This function is a wrapper of a common function which flushes the data
 * structures so that they are visible in memory for the next BL image.
 ******************************************************************************/
void plat_flush_next_bl_params(void)
{
	flush_bl_params_desc();
}

#if SPMD_SPM_AT_SEL2
/*******************************************************************************
 * This function appends Secure Partitions to list of loadable images.
 ******************************************************************************/
static void plat_add_sp_images_load_info(struct bl_load_info *load_info)
{
	bl_load_info_node_t *curr_node = load_info->head;
	bl_load_info_node_t *prev_node;

	/* Shortcut for empty SP list */
	if (sp_mem_params_descs[0].image_id == 0) {
		ERROR("No Secure Partition Image available\n");
		return;
	}

	/* Traverse through the bl images list */
	do {
		curr_node = curr_node->next_load_info;
	} while (curr_node->next_load_info != NULL);

	prev_node = curr_node;

	for (unsigned int index = 0; index < MAX_SP_IDS; index++) {
		if (sp_mem_params_descs[index].image_id == 0) {
			return;
		}
		curr_node = &sp_mem_params_descs[index].load_node_mem;
		/* Populate the image information */
		curr_node->image_id = sp_mem_params_descs[index].image_id;
		curr_node->image_info = &sp_mem_params_descs[index].image_info;

		prev_node->next_load_info = curr_node;
		prev_node = curr_node;
	}

	INFO("Reached Max number of SPs\n");
}
#endif

/*******************************************************************************
 * This function is a wrapper of a common function which returns the list of
 * loadable images.
 ******************************************************************************/
bl_load_info_t *plat_get_bl_image_load_info(void)
{
#if SPMD_SPM_AT_SEL2
	bl_load_info_t *bl_load_info;

	bl_load_info = get_bl_load_info_from_mem_params_desc();
	plat_add_sp_images_load_info(bl_load_info);

	return bl_load_info;
#else
	return get_bl_load_info_from_mem_params_desc();
#endif
}

/*******************************************************************************
 * This function is a wrapper of a common function which returns the data
 * structures of the next BL image.
 ******************************************************************************/
bl_params_t *plat_get_next_bl_params(void)
{
	return get_next_bl_params_from_mem_params_desc();
}
