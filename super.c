/*
 * super.c
 * Copyright 2004-2010  Serge van den Boom (svdb@stack.nl)
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
#include <linux/mount.h>
#include <linux/statfs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include "operafs.h"


//============================================================================


static struct inode *opera_alloc_inode(struct super_block *sb);
static void opera_destroy_inode(struct inode *inode);
static void opera_put_super(struct super_block *sb);
static int opera_statfs(struct dentry *dentry, struct kstatfs *buf);
static int opera_remount(struct super_block *sb, int *flags, char *data);
static int opera_show_options(struct seq_file *out, struct vfsmount *mnt);


//============================================================================


struct super_operations opera_super_ops = {
	.alloc_inode = opera_alloc_inode,
	.destroy_inode = opera_destroy_inode,
	.put_super = opera_put_super,
	.statfs = opera_statfs,
	.remount_fs = opera_remount,
	.show_options = opera_show_options,
};


//============================================================================


static struct inode *
opera_alloc_inode(struct super_block *sb)
{
	struct opera_inode_info *info;
	info = (struct opera_inode_info *) kmem_cache_alloc(
			opera_inode_cache, GFP_KERNEL);
	if (!info)
		return NULL;
	(void) sb;  /* Unused variable - satisfy compiler */
	return &info->vfs_inode;
}

static void
opera_destroy_inode(struct inode *inode)
{
	kmem_cache_free(opera_inode_cache, OPERA_I(inode));
}

struct inode *
operafs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	struct opera_sb_info *sbi = OPERA_SB(sb);
	struct buffer_head *bh;
	uint32_t block;
	uint32_t off;
	const struct opera_disk_dirent *tdd;
	int ret;

	inode = iget_locked(sb, ino);
	if (inode == NULL)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW)) {
		// We already have an inode for 'ino'.
		return inode;
	}
	
	// NB: inode->i_ino is the offset to the to the directory entry
	//     from the beginning of the disk.

	if (ino == OPERA_ROOT_INO) {
		// XXX: The root inode was a special case, and it was set up
		//      from opera_fill_super().
		//      I am assuming here that the root inode never gets lost.
		//      If that is an incorrect assumption, please let me know.
		//      It would need special handling.
		printk(KERN_ERR "Opera: opera_read_inode called for the root "
				"inode. That wasn't supposed to happen.\n");
		ret = -EIO;
		goto out_err;
	}
	
	block = inode->i_ino >> sbi->block_shift;
	off = inode->i_ino & OPERA_BLOCK_MASK(sbi->block_shift);

	bh = sb_bread(sb, block);
	if (bh == NULL) {
		printk(KERN_ERR "Opera: could not read block %d "
				"(block_size=%d, disk #%08X).\n", block, sbi->block_size,
				sbi->disk_id);
		ret = -EIO;
		goto out_err;
	}

	// inode->i_ino must have been acquired from operafs_readdir()
	// or operafs_lookup(), which means the validity of the block is
	// already verified. It also means the entry is either a directory,
	// or a (possibly special) file.

	tdd = (const struct opera_disk_dirent *) ((char *) bh->b_data + off);

	inode->i_uid = sbi->options.uid;
	inode->i_gid = sbi->options.gid;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;
	
	inode->i_blocks = be32_to_cpu(tdd->block_count);

	OPERA_I(inode)->start_block = be32_to_cpu(tdd->copies[0]);
	if (OPERA_DIRENT_TYPE(be32_to_cpu(tdd->flags)) == OPERA_DIRENT_DIR) {
		// is a directory
		inode->i_mode = (S_IRWXUGO & ~sbi->options.dmask) | S_IFDIR;
		inode->i_op = &opera_dir_inode_operations;
		inode->i_fop = &opera_dir_operations;
		inode->i_size = be32_to_cpu(tdd->block_count) *
				be32_to_cpu(tdd->block_size);
		set_nlink(inode, 2); 
		inc_nlink(opera_count_dirs(inode));
	} else {
		// is a file (possibly a special file)
		set_nlink(inode, 1); 
		inode->i_mode = ((S_IRUGO | S_IWUGO) & ~sbi->options.fmask) | S_IFREG;
		inode->i_op = &opera_file_inode_operations;
		inode->i_fop = &opera_file_operations;
		inode->i_size = be32_to_cpu(tdd->byte_count);
		inode->i_mapping->a_ops = &opera_address_operations;
	}

	brelse(bh);
	unlock_new_inode(inode);
	return inode;

out_err:
	iget_failed(inode);
	return ERR_PTR(ret);
}

static void
opera_put_super(struct super_block *sb)
{
	struct opera_sb_info *sbi = OPERA_SB(sb);

	sb->s_fs_info = NULL;
	kfree(sbi);
}
	

static int
opera_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	struct opera_sb_info *sbi = OPERA_SB(sb);

	buf->f_type = OPERA_MAGIC;
	buf->f_bsize = sbi->block_size;
	buf->f_blocks = sbi->block_count;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_ffree = 0;
	buf->f_namelen = OPERA_NAME_MAX;
	return 0;
}

static int
opera_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	(void) sb;  /* Unused variable - satisfy compiler */
	(void) data;  /* Unused variable - satisfy compiler */
	return 0;
}

static int
opera_show_options(struct seq_file *out, struct vfsmount *mnt)
{
	struct super_block *sb = mnt->mnt_sb;
	struct opera_sb_info *sbi = OPERA_SB(sb);
	struct opera_fs_options *options = &sbi->options;
	
	/*if (options->uid != 0)*/
		seq_printf(out, ",uid=%u", options->uid);
	/*if (options->gid != 0)*/
		seq_printf(out, ",gid=%u", options->gid);
	seq_printf(out, ",fmask=%04o", options->fmask);
	seq_printf(out, ",dmask=%04o", options->dmask);
	if (options->show_special != OPERA_DEFAULT_SHOW_SPECIAL) {
		if (options->show_special) {
			seq_printf(out, ",showspecial");
		} else
			seq_printf(out, ",hidespecial");
	}
	return 0;
}


