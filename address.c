/*
 * address.c
 * Copyright 2004-2008  Serge van den Boom (svdb@stack.nl)
 *
 * This file is part of the Opera file system driver for Linux.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "operafs.h"


//============================================================================


static int opera_readpage(struct file *file, struct page *page);
static sector_t opera_bmap(struct address_space *mapping, sector_t block);
static int opera_get_block(struct inode *inode, sector_t iblock,
		struct buffer_head *bh_result, int create);


//============================================================================


struct address_space_operations opera_address_operations = {
	.readpage = opera_readpage,
	.bmap = opera_bmap,
};


//============================================================================


static int
opera_readpage(struct file *file, struct page *page)
{
	(void) file;  /* Unused variable - satisfy compiler */
	return block_read_full_page(page, opera_get_block);
}


static sector_t
opera_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping, block, opera_get_block);
}

static int
opera_get_block(struct inode *inode, sector_t iblock,
		struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct opera_sb_info *sbi = OPERA_SB(sb);
	struct opera_inode_info *info = OPERA_I(inode);
	uint32_t block = iblock + info->start_block;
	
	if (block >= sbi->block_count)
		return 0;

	map_bh(bh_result, sb, block);
	
	(void) create;  /* Unused variable - satisfy compiler */
	return 0;
}


