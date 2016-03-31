/*
 * operafs.h
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

#ifndef _OPERAFS_H
#define _OPERAFS_H

#define OPERA_COMMENT_MAX 32
		// Maximum comment length

#define OPERA_LABEL_MAX 32
		// Maximum volume label length

#define OPERA_NAME_MAX 32
		// Maximum directory entry name length

#define NUM_COPIES_ROOT 8
		// Number of copies of the root directory.


// The 'superblock' as on the CDROM.
struct opera_disk_superblock {
	uint8_t record_type;  // Always 1
	struct {
		uint8_t sync[5];  // Synchronisation bytes. All 0x5a. (padding?)
		uint8_t version;  // Should always be 1.
		uint8_t flags;  // ?
		uint8_t comment[OPERA_COMMENT_MAX];  // Comment about the volume
		uint8_t label[OPERA_LABEL_MAX];  // Volume label
		uint32_t id;  // unique identifier for the disk
		uint32_t block_size;  // block size (always 2048?)
		uint32_t block_count;  // number of blocks on the disk
	} __attribute__((packed)) volume;
	struct {
		uint32_t id;  // unique identifier for the root directory
		uint32_t block_count;  // number of blocks in the root directory
		uint32_t block_size;
				// block size for the root dir. (always the same as for the
				// volume?)
		uint32_t last_copy;  // number of copies - 1
		uint32_t copies[NUM_COPIES_ROOT];
				// locations of the copies, in blocks, counted from the
				// beginning of the disk
	} __attribute__((packed)) root;
} __attribute__((packed));

// A directory header as on the CDROM
struct opera_disk_dir_header {
	int32_t next_block;
			// Next block in this directory, 0xffffffff if this is the
			// last block.
			// Offset in blocks from the first block in the dir?
			// (if this is true, it can't be -1, as that is 0xfffffff)
	int32_t prev_block;
			// Previous block in this directory, 0xfffffff if this is
			// the first block.
			// Offset in blocks from the first block in the dir?
	uint32_t flags;
			// directory flags (details unknown)
	uint32_t first_free;
			// u32  offset from the beginning of the block to the first
			// unused byte in the block
	uint32_t first_entry;
			// offset from the beginning of the block to the first
			// directory entry in this block (always (?) 0x14)
} __attribute__((packed));

// A directory entry as on the CDROM
struct opera_disk_dirent {
	uint32_t flags;
			// directory entry flags:
#define OPERA_DIRENT_FILE         0x00000002
#define OPERA_DIRENT_SPECIAL      0x00000006
#define OPERA_DIRENT_DIR          0x00000007
#define OPERA_DIRENT_TYPE_MASK    0x000000ff
		// Not sure about the mask.
#define OPERA_DIRENT_TYPE(flags) ((flags) & OPERA_DIRENT_TYPE_MASK)
#define OPERA_LAST_DIRENT_IN_BLOCK 0x40000000
#define OPERA_LAST_DIRENT_IN_DIR   0x80000000
	uint32_t id;  // unique identifier for the entry
	uint8_t type[4];
			// file type ("*dir" for directory, "*lbl" for volume header
			// "*zap" for catapult file)
	uint32_t block_size;
			// block size (always the same as the volume block size?)
	uint32_t byte_count;  // length of entry in bytes
	uint32_t block_count;  // length of entry in blocks
	uint32_t burst;  // function unknown
	uint32_t gap;  // function unknown
	uint8_t name[OPERA_NAME_MAX];
			// file/dir name. Padded with '\0'. Not sure whether it is
			// always '\0'-terminated.
	uint32_t last_copy;  // number of copies - 1
	uint32_t copies[1];
			// Offsets to all copies (in blocks from the beginning of the
			// disk). We ignore all but the first. The array runs to
			// last_copy, not necessarilly 1.
} __attribute__((packed));

struct opera_fs_options {
	kuid_t uid;  // uid of files and directories
	kgid_t gid;  // gid of files and directories
	mode_t fmask;  // file mask
	mode_t dmask;  // directory mask
	int show_special: 1;  // show special files?
#define OPERA_DEFAULT_SHOW_SPECIAL 0
		// Show special files if no options are passed?
};

struct opera_sb_info {
	struct super_block *sb;

	struct opera_fs_options options;

	uint32_t block_size;
	uint32_t block_count;
	uint32_t block_shift;

	uint32_t disk_id;
};
#define OPERA_BLOCK_MASK(shift) ((1 << (shift)) - 1)

struct opera_inode_info {
	uint32_t start_block;
			// In case of multiple copies, the first one is used.
	struct inode vfs_inode;
};
#define OPERA_ROOT_INO 84
		// Using the disk position of the dir entry as inode number.


#define OPERA_MAGIC  0x4455434B /* DUCK */
		// Identifying to linux with this value for the file system type.

static inline struct opera_sb_info *
OPERA_SB(struct super_block *sb)
{
	return (struct opera_sb_info *) sb->s_fs_info;
}

static inline struct opera_inode_info *
OPERA_I(struct inode *inode)
{
	return container_of(inode, struct opera_inode_info, vfs_inode);
}

// From main.h:
extern struct kmem_cache *opera_inode_cache;

// From super.c:
extern struct super_operations opera_super_ops;
struct inode *operafs_iget(struct super_block *sb, unsigned long ino);

// From dir.c:
extern struct file_operations opera_dir_operations;

// From file.c:
extern struct file_operations opera_file_operations;

// From inode.c:
extern struct inode_operations opera_dir_inode_operations;
extern struct inode_operations opera_file_inode_operations;

// From address.c:
extern struct address_space_operations opera_address_operations;

// From misc.c:
typedef int (*opera_for_all_callback)(void *data, const char *name,
		size_t len, ino_t ino, unsigned int type);
extern int opera_for_all_entries(struct inode *inode, loff_t *start_pos,
		opera_for_all_callback callback, void *data);
extern struct inode * opera_count_dirs(struct inode *inode);

#endif  /* _OPERAFS_H */

