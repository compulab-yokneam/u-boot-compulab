#include <config.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <stdio.h>
#include <asm/global_data.h>
#include <efi_loader.h>

#if CONFIG_IS_ENABLED(EFI_HAVE_CAPSULE_SUPPORT)
struct efi_fw_image fw_images[] = {
	{
		.image_type_id = EFI_GUID(0x928b33bc, 0xe58b, 0x4247, 0x9f, 0x1d, 0x3b, 0xf1, 0xee, 0x1c, 0xda, 0xff),
		.fw_name = u"CLAB-IMX8MP-RAW",
		.image_index = 1,
	},
};

struct efi_capsule_update_info update_info = {
	.dfu_string = "mmc 2=flash-bin raw 0 0x2000 mmcpart 1",
	.images = fw_images,
};

u8 num_image_type_guids = ARRAY_SIZE(fw_images);
#endif /* EFI_HAVE_CAPSULE_SUPPORT */
