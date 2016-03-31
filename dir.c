/*
 * dir.c
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
/*#include <linux/smp_lock.h>*/
#include <linux/buffer_head.h>

#include "operafs.h"


//============================================================================


static int opera_readdir(struct file *f, void *dirent, filldir_t filldir);
static int opera_readdir_callback(void *data, const char *name, size_t len,
		ino_t ino, unsigned int type);


//============================================================================


struct file_operations opera_dir_operations = {
	.read = generic_read_dir,
	.iterate = opera_readdir,
	//.readdir = opera_readdir,
};


//============================================================================


struct opera_readdir_callback_arg {
	filldir_t filldir;
	void *dirent;
};

static int
opera_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct opera_readdir_callback_arg arg;
	int stored;
	int res;

	arg.filldir = filldir;
	arg.dirent = dirent;

	/*lock_kernel();*/
			// XXX: What are we protecting anyhow?
	
	stored = 0;
	switch (file->f_pos) {
		case 0:  // Make an entry "."
			res = filldir(dirent, ".", 1, inode->i_ino, inode->i_ino
					/* The inode is the position */, DT_DIR);
			if (res < 0)
				goto out;
			stored++;
			file->f_pos = 1;
			/* fallthrough */
		case 1: {  // Make an entry ".."
			ino_t pi = parent_ino(file->f_path.dentry);
			res = filldir(dirent, "..", 2, pi, pi
					/* The inode is the position */, DT_DIR);
			if (res < 0)
				goto out;
			stored++;
			file->f_pos = 2;
			break;
		}
	}

	res = opera_for_all_entries(inode, &file->f_pos,
			opera_readdir_callback, (void *) &arg);
	if (res >= 0) {
		// res is the number of entries stored in opera_for_all_entries()
		res += stored;
	} else {
		// An error occured.
		// Leaving res alone.
	}

out:
	/*unlock_kernel();*/
	return res;
}

static int
opera_readdir_callback(void *data, const char *name, size_t len,
		ino_t ino, unsigned int type) {
	struct opera_readdir_callback_arg *arg =
			(struct opera_readdir_callback_arg *) data;

	return arg->filldir(arg->dirent, name, len, ino, ino, type);
			// The inode number is the position.
}


