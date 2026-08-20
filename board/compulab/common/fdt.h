#ifndef _COMMON_FDT_H__
#define _COMMON_FDT_H__

int fdt_set_sn(void *blob);
int fdt_set_env_addr(void *blob);
int fdt_fixup_jailhouse_memory(void *blob);

#endif /* _FDT_H__ */
