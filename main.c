/*
 * main.c
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

/* Implementation of the Opera filesystem used by 3DO CD-ROMs.
 *
 * By Serge van den Boom <svdb@stack.nl>, October 2004
 *
 * With thanks to
 *   Dave Platt for designing the Opera file system.
 *   D. J. James for his 1994 posting to rec.games.video.3do:
 *      http://groups.google.com/groups?selm=1994Dec30.145918.26916%40fsd.com
 *   Alexander Troosh for his unCD-ROM program:
 *      http://troosh.pp.ru/3do/unCD-ROM14.zip
 *   Felix Lazarev of the FreeDO project, for providing some more
 *      3DO information.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/parser.h>
#include <linux/buffer_head.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/exportfs.h>
#include <linux/fs_struct.h>

#include "operafs.h"


//============================================================================


static int __init init_opera_fs(void);
static void __exit exit_opera_fs(void);
static struct dentry * opera_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data);
static int opera_init_inodecache(void);
static void opera_destroy_inodecache(void);
static void opera_inode_init_once(void *info_in);
static int opera_fill_super(struct super_block *sb, void *data,
		int silent);
static int parse_mount_options(char *optstr,
		struct opera_fs_options *options);
static int opera_make_root_inode(struct super_block *sb,
		const struct opera_disk_superblock *dsb, struct inode **inode_out,
		int silent);


//============================================================================


static struct file_system_type opera_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "opera",
	.mount		= opera_get_sb,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

struct kmem_cache *opera_inode_cache;

MODULE_DESCRIPTION("3DO 'Opera' filesystem support");
MODULE_AUTHOR("Serge van den Boom");
MODULE_LICENSE("GPL v2");
	

//============================================================================


module_init(init_opera_fs)
module_exit(exit_opera_fs)


static int
__init init_opera_fs(void)
{
	int err;

	err = opera_init_inodecache();
	if (err)
		return err;

	err = register_filesystem(&opera_fs_type);
	if (err)
		goto out_err;

	return 0;

out_err:
	opera_destroy_inodecache();
	return err;
}

static void
__exit exit_opera_fs(void)
{
	unregister_filesystem(&opera_fs_type);
	opera_destroy_inodecache();
}

static int
opera_init_inodecache(void)
{
	opera_inode_cache = kmem_cache_create("opera_inode_cache",
			sizeof(struct opera_inode_info), 0,
			SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT,
			opera_inode_init_once);
	if (opera_inode_cache == NULL)
		return -ENOMEM;
	return 0;
}

static void
opera_inode_init_once(void *info_in)
{
	struct opera_inode_info *info = (struct opera_inode_info *) info_in;

	inode_init_once(&info->vfs_inode);
}

static void
opera_destroy_inodecache(void)
{
	kmem_cache_destroy(opera_inode_cache);
}

enum {
	Opt_uid, Opt_gid, Opt_umask, Opt_dmask, Opt_fmask,
	Opt_showspecial, Opt_hidespecial, Opt_err
};

static match_table_t opera_option_tokens = {
	{ Opt_uid, "uid=%u" },
	{ Opt_gid, "gid=%u" },
	{ Opt_umask, "umask=%o" },
	{ Opt_dmask, "dmask=%o" },
	{ Opt_fmask, "fmask=%o" },
	{ Opt_showspecial, "showspecial" },
	{ Opt_hidespecial, "hidespecial" },
	{ Opt_err, NULL }
};

static int
parse_mount_options(char *optstr, struct opera_fs_options *options) {
	char *ptr;
	substring_t args[MAX_OPT_ARGS];
	int temp_int;

	while ((ptr = strsep(&optstr, ",")) != NULL) {
		int token;
		if (*ptr == '\0')
			continue;
	
		token = match_token(ptr, opera_option_tokens, args);
		switch (token) {
			case Opt_uid:
				if (match_int(&args[0], &temp_int))
					return 0;
				options->uid.val = temp_int;
				break;
			case Opt_gid:
				if (match_int(&args[0], &temp_int))
					return 0;
				options->gid.val = temp_int;
				break;
			case Opt_umask:
				if (match_int(&args[0], &temp_int))
					return 0;
				options->dmask = temp_int;
				options->fmask = temp_int;
				break;
			case Opt_dmask:
				if (match_int(&args[0], &temp_int))
					return 0;
				options->dmask = temp_int;
				break;
			case Opt_fmask:
				if (match_int(&args[0], &temp_int))
					return 0;
				options->fmask = temp_int;
				break;
			case Opt_showspecial:
				options->show_special = 1;
				break;
			case Opt_hidespecial:
				options->show_special = 0;
				break;
			default:
				printk(KERN_ERR "Opera: Unrecognised mount option "
						"\"%s\" or missing value.\n", ptr);
				return -EINVAL;
		}	
	}
	return 0;
}

static struct dentry *
opera_get_sb(struct file_system_type *fs_type, int flags,
		const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, opera_fill_super);
}

static int
opera_fill_super(struct super_block *sb, void *data, int silent)
{
	struct opera_sb_info *sbi;
	struct buffer_head *bh = NULL;
	const struct opera_disk_superblock *dsb;
	struct inode *root_inode = NULL;
	int error;

	sbi = kmalloc(sizeof (struct opera_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;

	memset(sbi, '\0', sizeof (struct opera_sb_info));
	sbi->sb = sb;
	sbi->options.uid = current->cred->uid;
	sbi->options.gid = current->cred->gid;
	sbi->options.fmask = current->fs->umask;
	sbi->options.dmask = current->fs->umask;
	sbi->options.show_special = OPERA_DEFAULT_SHOW_SPECIAL;
	error = parse_mount_options((char *) data, &sbi->options);
	if (error)
		goto out_err;

	sb->s_magic = OPERA_MAGIC;
	sb->s_flags |= MS_RDONLY;

	sb_set_blocksize(sb, 512);

	bh = sb_bread(sb, 0);
	if (bh == NULL) {
		if (!silent)
			printk(KERN_ERR "Opera: superblock read failed on device %s\n",
					sb->s_id);
		error = -EINVAL;
		goto out_err;
	}

	dsb = (const struct opera_disk_superblock *) bh->b_data;

	if (dsb->record_type != 0x01
			|| dsb->volume.sync[0] != 0x5A
			|| dsb->volume.sync[1] != 0x5A
			|| dsb->volume.sync[2] != 0x5A
			|| dsb->volume.sync[3] != 0x5A
			|| dsb->volume.sync[4] != 0x5A) {
		if (!silent)
			printk(KERN_ERR "Opera: no Opera superblock found on device %s\n",
					sb->s_id);
		error = -EINVAL;
		goto out_err;
	}

	if (dsb->volume.version != 1) {
		if (!silent)
			printk(KERN_ERR "Opera: superblock version %d not supported\n",
					dsb->volume.version);
		error = -EINVAL;
		goto out_err;
	}
	
	sbi->disk_id = be32_to_cpu(dsb->volume.id);
	
	{
		char buf[OPERA_LABEL_MAX + 1];
		strncpy(buf, dsb->volume.label, OPERA_LABEL_MAX);
		buf[OPERA_LABEL_MAX] = '\0';
		printk(KERN_DEBUG "Opera: Disk with label \"%s\" and id #%08X "
				"found\n", buf, sbi->disk_id);
	}

	sbi->block_size = be32_to_cpu(dsb->volume.block_size);
	if (sbi->block_size < 256) {
		if (!silent)
			printk(KERN_ERR "Opera: Error: block size is unrealistically "
					"small (%d bytes) (disk #%08X)\n", sbi->block_size,
					sbi->disk_id);
		error = -EINVAL;
		goto out_err;
	}
	if (((sbi->block_size - 1) & sbi->block_size) != 0) {
		if (!silent)
			printk(KERN_ERR "Opera: block size is %d, not a power "
					"of 2 (disk #%08X)\n", sbi->block_size, sbi->disk_id);
		error = -EINVAL;
		goto out_err;
	}
	sbi->block_shift = 0;
	while ((sbi->block_size >> (sbi->block_shift + 1)) != 0)
			sbi->block_shift++;
	sb_set_blocksize(sb, sbi->block_size);
	
	sbi->block_count = be32_to_cpu(dsb->volume.block_count);

	sbi->sb = sb;
	sb->s_fs_info = sbi;
	brelse(bh);
	bh = NULL;

	sb->s_op = &opera_super_ops;
	error = opera_make_root_inode(sb, dsb, &root_inode, silent);
	if (error)
		goto out_err;

	sb->s_root = d_make_root(root_inode);
	if (sb->s_root == NULL) {
		error = -ENOMEM;
		goto out_err;
	}
	
	return 0;

out_err:
	if (root_inode != NULL)
		iput(root_inode);
	if (bh != NULL)
		brelse(bh);
	if (sbi != NULL) {
		sb->s_fs_info = NULL;
		kfree(sbi);
	}
	return error;
}

static int
opera_make_root_inode(struct super_block *sb,
		const struct opera_disk_superblock *dsb, struct inode **inode_out,
		int silent) {
	struct inode *inode;
	struct opera_sb_info *sbi = OPERA_SB(sb);

	if (be32_to_cpu(dsb->root.block_size) != sbi->block_size) {
		if (!silent)
			printk(KERN_ERR "Opera: root directory block size (%d) "
					"differs from file system block size (%d) (disk #%08X)"
					".\n", be32_to_cpu(dsb->root.block_size),
					sbi->block_size, sbi->disk_id);
		return -EINVAL;
	}
	
	inode = new_inode(sb);
	if (inode == NULL)
		return -ENOMEM;
	
	inode->i_ino = OPERA_ROOT_INO;
	inode->i_uid = sbi->options.uid;
	inode->i_gid = sbi->options.gid;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_mode = (S_IRWXUGO & ~sbi->options.dmask) | S_IFDIR;
	inode->i_op = &opera_dir_inode_operations;
	inode->i_fop = &opera_dir_operations;
	inode->i_size = be32_to_cpu(dsb->root.block_count) *
			be32_to_cpu(dsb->root.block_size);
	inode->i_blocks = be32_to_cpu(dsb->root.block_count);
	OPERA_I(inode)->start_block = be32_to_cpu(dsb->root.copies[0]);
	
	set_nlink(inode, 2); 
	inc_nlink(opera_count_dirs(inode));
	
	*inode_out = inode;

	return 0;
}


