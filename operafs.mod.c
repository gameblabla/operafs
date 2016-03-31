#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x2e01622f, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc94d4201, __VMLINUX_SYMBOL_STR(kmem_cache_destroy) },
	{ 0x4467d24d, __VMLINUX_SYMBOL_STR(iget_failed) },
	{ 0x1d6d371b, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x71e456c, __VMLINUX_SYMBOL_STR(generic_file_llseek) },
	{ 0x6bf1c17f, __VMLINUX_SYMBOL_STR(pv_lock_ops) },
	{ 0xaa6950c, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0x44e9a829, __VMLINUX_SYMBOL_STR(match_token) },
	{ 0x4042ff64, __VMLINUX_SYMBOL_STR(inc_nlink) },
	{ 0x922422b4, __VMLINUX_SYMBOL_STR(block_read_full_page) },
	{ 0xf76baa96, __VMLINUX_SYMBOL_STR(mount_bdev) },
	{ 0x85df9b6c, __VMLINUX_SYMBOL_STR(strsep) },
	{ 0xbe6a7eb3, __VMLINUX_SYMBOL_STR(generic_read_dir) },
	{ 0x3924bca6, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x5ca251d5, __VMLINUX_SYMBOL_STR(__bread_gfp) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xf23ec957, __VMLINUX_SYMBOL_STR(d_rehash) },
	{ 0x449ad0a7, __VMLINUX_SYMBOL_STR(memcmp) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x64ccee9e, __VMLINUX_SYMBOL_STR(kmem_cache_free) },
	{ 0x2a0df1a4, __VMLINUX_SYMBOL_STR(set_nlink) },
	{ 0x4e3567f7, __VMLINUX_SYMBOL_STR(match_int) },
	{ 0x385e44b8, __VMLINUX_SYMBOL_STR(generic_file_read_iter) },
	{ 0x48c5ae5c, __VMLINUX_SYMBOL_STR(__brelse) },
	{ 0x2af9ce3a, __VMLINUX_SYMBOL_STR(inode_init_once) },
	{ 0x34e6a1e1, __VMLINUX_SYMBOL_STR(kmem_cache_alloc) },
	{ 0xa916b694, __VMLINUX_SYMBOL_STR(strnlen) },
	{ 0xa48750c2, __VMLINUX_SYMBOL_STR(generic_file_mmap) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x539dc4d2, __VMLINUX_SYMBOL_STR(unlock_new_inode) },
	{ 0x8846aee0, __VMLINUX_SYMBOL_STR(kill_block_super) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xfc6ed496, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xe259ae9e, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xb30aa6bc, __VMLINUX_SYMBOL_STR(kmem_cache_create) },
	{ 0xbd6b969b, __VMLINUX_SYMBOL_STR(register_filesystem) },
	{ 0x686f7800, __VMLINUX_SYMBOL_STR(generic_file_write_iter) },
	{ 0x3babab74, __VMLINUX_SYMBOL_STR(iter_file_splice_write) },
	{ 0xa3ff2878, __VMLINUX_SYMBOL_STR(iput) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xaf9daeec, __VMLINUX_SYMBOL_STR(sb_set_blocksize) },
	{ 0xeb1442, __VMLINUX_SYMBOL_STR(d_make_root) },
	{ 0x35026240, __VMLINUX_SYMBOL_STR(unregister_filesystem) },
	{ 0x61f9b6b0, __VMLINUX_SYMBOL_STR(new_inode) },
	{ 0xf4789ed, __VMLINUX_SYMBOL_STR(generic_file_splice_read) },
	{ 0x7b567ae9, __VMLINUX_SYMBOL_STR(d_instantiate) },
	{ 0xb13ee2bc, __VMLINUX_SYMBOL_STR(generic_block_bmap) },
	{ 0xe60746fd, __VMLINUX_SYMBOL_STR(iget_locked) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "D955EA483B2CE5FF99DDB38");
