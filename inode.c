/*
 * inode.c
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

#include "operafs.h"

//============================================================================


static struct dentry *opera_lookup(struct inode *dir, struct dentry *dentry,  unsigned int nd);
static int opera_lookup_callback(void *data, const char *name,
		size_t name_len, ino_t ino, unsigned int type);


//============================================================================


struct inode_operations opera_dir_inode_operations = {
	.lookup		= opera_lookup,
};

struct inode_operations opera_file_inode_operations = {

};


//============================================================================


struct lookup_arg {
	struct dentry *dentry;
	struct super_block *sb;
	int found_match;
			// Will be set if a match is found, to distinguish between
			// match found and error.
};

static struct dentry *opera_lookup(struct inode *dir, struct dentry *dentry, unsigned int nd)
{
	int res;
	struct lookup_arg arg;
	loff_t pos = 0;

	/*lock_kernel();*/
	arg.dentry = dentry;
	arg.sb = dir->i_sb;
	arg.found_match = 0;
	
	res = opera_for_all_entries(dir, &pos, opera_lookup_callback,
			(void *) &arg);
	if (res < 0) {
		// An error has occured. 'res' alredy contains the error.
		goto out;
	}

	// The value of res does not matter. It merely says how many entries
	// (matching or not) were encountered.
	res = 0;
	if (!arg.found_match) {
		// No match.
		d_add(dentry, NULL);
	} else {
		// A match was found. It is already added to dentry from within
		// opera_lookup_callback().
		// Nothing to do.
	}
	
out:
	/*unlock_kernel();*/

	(void) nd;  /* Unused variable - satisfy compiler */
	return ERR_PTR(res);
}

static int
opera_lookup_callback(void *data, const char *name, size_t name_len,
		ino_t ino, unsigned int type) {
	struct lookup_arg *arg = (struct lookup_arg *) data;
	struct inode *inode;

	if (arg->dentry->d_name.len != name_len ||
			memcmp(name, arg->dentry->d_name.name, name_len) != 0) {
		// no match
		return 0;  // continue looking
	}
	
	// We have found a match
	inode = operafs_iget(arg->sb, ino);
	if (IS_ERR(inode))
		return -1;  // Abort

	d_add(arg->dentry, inode);
	arg->found_match = 1;
	
	(void) type;  /* Unused variable - satisfy compiler */
	return 1;  // Stop looking, we're done.
}


