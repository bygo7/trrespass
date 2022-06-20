#include "addr-mapper.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static size_t g_rmap_len = 0;
static size_t g_base_row = 0;
static size_t g_bks = 0;
static size_t g_rows = 0;
#define BY_DEBUG

RowMap get_row_map(ADDRMapper * mapper, DRAMAddr * d_addr)
{
	size_t idx =
	    (d_addr->row - g_base_row) * get_banks_cnt() + d_addr->bank;
#ifdef BY_DEBUG
	fprintf(stdout, "d_addr->row:%d, g_base_row:%d, d_addr->bank:%dd, idx:%d, g_bks:%d, g_rows:%d\n", d_addr->row, g_base_row, d_addr->bank, idx, g_bks, g_rows);
#endif
	assert(idx < g_bks * g_rows);
	return mapper->row_maps[idx];
}

DRAM_pte get_dram_pte(ADDRMapper * mapper, DRAMAddr * d_addr)
{
	RowMap rmap = get_row_map(mapper, d_addr);
	return rmap.lst[(d_addr->col) >> 6];
}

RowMap gen_row_map(DRAMAddr d_src, MemoryBuffer * mem)
{
	RowMap rmap;
	DRAM_pte *dst = (DRAM_pte *) malloc(sizeof(DRAM_pte) * g_rmap_len);
	d_src.col = 0;

	for (size_t col = 0; col < g_rmap_len; col++, d_src.col += (1 << 6)) {
		dst[col].d_addr = d_src;
		dst[col].v_addr = phys_2_virt(dram_2_phys(d_src), mem);
#ifdef BY_DEBUG
		fprintf(stderr, "phys_2_virt: %s", dst[col].v_addr);
#endif
	}	
#ifdef BY_DEBUG
	uint64_t dir_col = d_src.col >> 14;
        fprintf(stdout, "gen_row_map bank:%d, row:%d, d_src.row: %d, col:%ld, dir_col::%d, v_addr:%s \n", dst[dir_col].d_addr.bank, dst[dir_col].d_addr.row, d_src.row, dst[dir_col].d_addr.col, dir_col, *dst[dir_col].v_addr);
#endif
	rmap.lst = dst;
	rmap.len = g_rmap_len;
	return rmap;

}

size_t rmap_idx(size_t bk, size_t row)
{
	return row * get_banks_cnt() + bk;
}

void init_addr_mapper(ADDRMapper * mapper, MemoryBuffer * mem,
		      DRAMAddr * d_base, size_t h_rows)
{
	mapper->row_maps =
	    (RowMap *) malloc(sizeof(RowMap) * h_rows * get_banks_cnt());
	mapper->base_row = d_base->row;
	g_base_row = d_base->row;
	g_bks = get_banks_cnt();
	g_rows = h_rows;
	// set global rmap_len
	g_rmap_len = ROW_SIZE / CL_SIZE;

	// create ptes list for every
	DRAMAddr d_tmp = {.bank = 0,.row = 0,.col = 0 };
	for (size_t bk = 0; bk < get_banks_cnt(); bk++) {
#ifdef BY_DEBUG
		fprintf(stdout, "\ninit_addr_mapper bkno :%d total row: %d, base_row: %d", bk, h_rows, g_base_row);
#endif
		d_tmp.bank = bk;
		for (size_t row = 0; row < h_rows; row++) {
#ifdef BY_DEBUG
			fprintf(stdout, "row: %d , d_tmp.row: %d \n", row, g_base_row + row);
#endif
			d_tmp.row = g_base_row + row;
			mapper->row_maps[rmap_idx(bk, row)] =
			    gen_row_map(d_tmp, mem);
		}
	}
}

void tear_down_addr_mapper(ADDRMapper * mapper)
{
	for (int i = 1; i < g_rows * g_bks; i++) {
		free(mapper->row_maps[i].lst);
	}
	free(mapper->row_maps);
}
