/*
 * misc.c
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


// ============================================================================


static int opera_count_dirs_callback(void *data, const char *name,
		size_t name_len, ino_t ino, unsigned int type);


// ============================================================================


// Iterate through all entries and call callback for all of them.
// If the callback function returns a value unequal to 0, the iteration is
// ended. If the value is less than 0, start_pos will be unchanged, and
// 0 will be returned.
// *start_pos is updated for each entry read.
// This function does not lock the kernel. That's up to the caller.
int
opera_for_all_entries(struct inode *inode, loff_t *start_pos,
		opera_for_all_callback callback, void *data)
{
	int stored = 0;
			// number of directory entries stored this call so far
	struct super_block *sb = inode->i_sb;
	struct opera_sb_info *sbi = OPERA_SB(sb);
	struct opera_inode_info *info = OPERA_I(inode);
	struct buffer_head *bh = NULL;
	const struct opera_disk_dir_header *tddh;
	unsigned int num_blocks;
			// number of blocks in the directory
	uint32_t next_block, prev_block;
	uint32_t first_free;
			// first unused byte in the block
	uint32_t blocknr;
			// number of the current block of a directory
	uint32_t pos;
			// position within a block
	int error;
	struct opera_disk_dirent *tdd;
	uint32_t entry_flags;
			// flags of the current directory entry
	uint32_t entry_size;
			// size of the current directory entry
	unsigned int type;
			// file type of the current directory entry
	unsigned int last_dirent_in_dir;
			// flag - is this the last directory entry in the dir?

	num_blocks = inode->i_size >> sbi->block_shift;
	blocknr = *start_pos >> sbi->block_shift;
	if (blocknr >= num_blocks) {
		// We're already done.
		return 0;
	}

	pos = *start_pos & OPERA_BLOCK_MASK(sbi->block_shift);
	for (;;) {
		bh = sb_bread(sb, info->start_block + blocknr);
		if (bh == NULL) {
			printk(KERN_ERR "Opera: could not read block %d "
					"(block_size=%d, disk #%08X).\n",
					info->start_block + blocknr, sbi->block_size,
					sbi->disk_id);
			error = -EINVAL;
			goto out_err;
		}
	
		tddh = (const struct opera_disk_dir_header *) bh->b_data;

		next_block = be32_to_cpu(tddh->next_block);
		prev_block = be32_to_cpu(tddh->prev_block);
		first_free = be32_to_cpu(tddh->first_free);
		if (((blocknr == 0) !=  (prev_block == 0xffffffff)) ||
				(blocknr > 0 && blocknr != prev_block + 1) ||
				((blocknr + 1 == num_blocks) != (next_block == 0xffffffff)) ||
				(next_block != 0xffffffff && next_block != blocknr + 1) ||
				first_free > sbi->block_size) {
			// It may or may not be possible that non-consecutive directory
			// blocks can occur. At any rate, this code cannot handle it.
			// (I'm not actually sure about the precise meaning of next_block
			// and prev_block).
			printk(KERN_ERR "Opera: bad directory header in block %d "
					"(block_size=%d, disk #%08X).\n",
					info->start_block + blocknr, sbi->block_size,
					sbi->disk_id);
			error = -EINVAL;
			goto out_err;
		}

		if (pos <= 2)
			pos = be32_to_cpu(tddh->first_entry);

		for (;;) {
			if (pos > sbi->block_size || ((pos & 0x03) != 0x00)) {
				// position outside the block, or not aligned.
				printk(KERN_ERR "Opera: Bad start of directory in block %d"
						"(block_size=%d, disk #%08X).\n",
						info->start_block + blocknr, sbi->block_size,
						sbi->disk_id);
				error = -EBADF;
				goto out_err;
			}

			tdd = (struct opera_disk_dirent *)
					(((uint8_t *) bh->b_data) + pos);
			
			entry_size = 72 + 4 * be32_to_cpu(tdd->last_copy);
			if (pos + entry_size > first_free) {
				// The entire entry does not fit in the block.
				// That should not happen.
				printk(KERN_ERR "Opera: Bad directory entry in block %d "
						"(pos=%d, block_size=%d, disk #%08X).\n",
						info->start_block + blocknr, pos, sbi->block_size,
						sbi->disk_id);
				error = -EBADF;
				goto out_err;
			}

			entry_flags = be32_to_cpu(tdd->flags);

			if (be32_to_cpu(tdd->block_size) != sbi->block_size) {
				printk(KERN_WARNING "Opera: Block size for directory entry "
						"in block %d (%d) differs from the file system "
						"block size (pos=%d, block_size=%d, disk #%08X). "
						"Entry skipped.\n", info->start_block + blocknr,
						be32_to_cpu(tdd->block_size), pos, sbi->block_size,
						sbi->disk_id);
				goto next_entry;
			}
		
			switch (OPERA_DIRENT_TYPE(entry_flags)) {
				case OPERA_DIRENT_FILE:  // Regular file
					type = DT_REG;
					break;
				case OPERA_DIRENT_SPECIAL:  // Special File
					if (sbi->options.show_special) {
						type = DT_REG;
					} else
						goto next_entry;
					break;
				case OPERA_DIRENT_DIR:
					type = DT_DIR;
					break;
				default:
					printk(KERN_WARNING "Opera: Unrecognised directory "
							"entry type %d in block %d (ignored) (pos=%d, "
							"block_size=%d, disk #%08X).\n",
							entry_flags & 0xff, info->start_block + blocknr,
							pos, sbi->block_size, sbi->disk_id);
					goto next_entry;
			};
					
			error = callback(data, tdd->name,
					strnlen(tdd->name, OPERA_NAME_MAX),
					(info->start_block + blocknr) * sbi->block_size + pos,
					type);
			if (error) {
				if (error > 0) {
					stored++;
					pos += entry_size;
				}
				brelse(bh);
				goto out;
			}
			stored++;

next_entry:
			pos += entry_size;

			if (entry_flags & OPERA_LAST_DIRENT_IN_BLOCK)
				break;
		}

		last_dirent_in_dir = entry_flags & OPERA_LAST_DIRENT_IN_DIR;
		brelse(bh);
		blocknr++;
		pos = 0;

		if (last_dirent_in_dir)
			break;
		
		if (blocknr >= num_blocks) {
			// We should have encountered an entry with the
			// end-of-directory flag set by now.
			printk(KERN_ERR "Opera: missing end-of-directory flag "
					"(num_blocks = %d, block_size=%d, disk #%08X).\n",
					num_blocks, sbi->block_size, sbi->disk_id);
			break;
		}
	}

out:
	*start_pos = (blocknr * sbi->block_size) + pos;
	return stored;

out_err:
	if (bh)
		brelse(bh);
	return error;
}

struct opera_count_dirs_arg {
	uint32_t num_dirs;
};

struct inode *
opera_count_dirs(struct inode *inode) {
	struct opera_count_dirs_arg arg;
	int res;
	loff_t pos;
	
	arg.num_dirs = 0;
	pos = 0;
	res = opera_for_all_entries(inode, &pos, opera_count_dirs_callback,
			&arg);
	if (res < 0) {
		// Something went wrong.
		return 0;
	}
	return arg.num_dirs;
}

static int
opera_count_dirs_callback(void *data, const char *name, size_t name_len,
		ino_t ino, unsigned int type) {
	struct opera_count_dirs_arg *arg =
			(struct opera_count_dirs_arg *) data;
	
	if (type == DT_DIR)
		arg->num_dirs++;

	(void) name;  /* Unused variable - satisfy compiler */
	(void) name_len;  /* Unused variable - satisfy compiler */
	(void) ino;  /* Unused variable - satisfy compiler */
	return 0;  // continue counting
}


