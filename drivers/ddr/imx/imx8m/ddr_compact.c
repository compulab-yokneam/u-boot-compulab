// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <errno.h>
#include <imx8m_ddr_compact.h>

static u32 compact_le(const u8 *p, unsigned int bytes)
{
	u32 value = 0;
	unsigned int i;

	for (i = 0; i < bytes; i++)
		value |= (u32)p[i] << (8 * i);
	return value;
}

void ddr_compact_iter_init(struct dram_compact_iter *it,
			   const struct dram_compact_table *table)
{
	memset(it, 0, sizeof(*it));
	it->table = table;
	it->cursor = table->data;
}

int ddr_compact_next(struct dram_compact_iter *it, struct dram_cfg_param *entry)
{
	const struct dram_compact_table *t = it->table;
	unsigned int command, offset, count;
	const u8 *p;

	if (t->width != 5 && t->width != 6)
		return -EINVAL;
	if (it->emitted == t->count)
		return it->remaining || it->cursor != t->data + t->size ? -EINVAL : 0;
	if (!t->base) {
		if (t->size != t->count * t->width)
			return -EINVAL;
		p = it->cursor;
		it->cursor += t->width;
	} else {
		if (!it->remaining) {
			if (t->data + t->size - it->cursor < 2)
				return -EINVAL;
			command = compact_le(it->cursor, 2);
			it->cursor += 2;
			count = command & 0x7fff;
			if (!count || count > (unsigned int)(t->count - it->emitted))
				return -EINVAL;
			if (command & 0x8000) {
				/* Literal run immediately follows the command. */
				if (t->data + t->size - it->cursor < count * t->width)
					return -EINVAL;
				it->run = it->cursor;
				it->cursor += count * t->width;
			} else {
				/* Copy a contiguous range of the packed base table. */
				if (t->data + t->size - it->cursor < 2)
					return -EINVAL;
				offset = compact_le(it->cursor, 2);
				it->cursor += 2;
				if (offset + count > t->base_count)
					return -EINVAL;
				it->run = t->base + offset * t->width;
			}
			it->remaining = count;
		}
		p = it->run;
		it->run += t->width;
		it->remaining--;
	}
	if (t->width == 6) {
		entry->reg = 0x3d400000 + compact_le(p, 2);
		entry->val = compact_le(p + 2, 4);
	} else {
		entry->reg = compact_le(p, 3);
		entry->val = compact_le(p + 3, 2);
	}
	it->emitted++;
	return 1;
}

int ddr_compact_write(const struct dram_compact_table *table, bool phy)
{
	struct dram_compact_iter it;
	struct dram_cfg_param entry;
	int ret;

	ddr_compact_iter_init(&it, table);
	while ((ret = ddr_compact_next(&it, &entry)) > 0) {
		if (phy)
			dwc_ddrphy_apb_wr(entry.reg, entry.val);
		else
			reg32_write(entry.reg, entry.val);
	}
	return ret;
}

static int compact_save_table(const struct dram_compact_table *table,
			      struct dram_cfg_param **destination)
{
	struct dram_compact_iter it;
	struct dram_cfg_param entry;
	int ret;

	ddr_compact_iter_init(&it, table);
	while ((ret = ddr_compact_next(&it, &entry)) > 0)
		*(*destination)++ = entry;
	return ret;
}

int ddr_compact_save(const struct dram_compact_timing *timing, ulong address)
{
	struct dram_timing_info *saved = (void *)address;
	struct dram_cfg_param *cfg = (void *)(saved + 1);
	int ret;

	/* Publish ordinary expanded tables, never OCRAM pointers or bytecode. */
	memset(saved, 0, sizeof(*saved));
	memcpy(saved->fsp_table, timing->fsp_table, sizeof(saved->fsp_table));
	saved->ddrc_cfg = cfg;
	saved->ddrc_cfg_num = timing->ddrc_cfg->count;
	ret = compact_save_table(timing->ddrc_cfg, &cfg);
	if (ret)
		return ret;
	saved->ddrphy_cfg = cfg;
	saved->ddrphy_cfg_num = timing->ddrphy_cfg->count;
	ret = compact_save_table(timing->ddrphy_cfg, &cfg);
	if (ret)
		return ret;
	saved->ddrphy_trained_csr = cfg;
	saved->ddrphy_trained_csr_num = ddrphy_trained_csr_num;
	memcpy(cfg, ddrphy_trained_csr, ddrphy_trained_csr_num * sizeof(*cfg));
	cfg += ddrphy_trained_csr_num;
	saved->ddrphy_pie = cfg;
	saved->ddrphy_pie_num = timing->ddrphy_pie->count;
	return compact_save_table(timing->ddrphy_pie, &cfg);
}
