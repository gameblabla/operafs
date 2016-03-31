#
# Makefile for the 3DO 'Opera' filesystem routines.
#
# To compile manually:
# 	make -C /path/to/kernel_sources M=$PWD modules

#EXTRA_CFLAGS=-O0

#EXTRA_CFLAGS=-O2 -W -Wall -Wpointer-arith -Wshadow -pipe -Wbad-function-cast -Wcast-qual -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations -Wwrite-strings -Wimplicit -Wreturn-type -Wformat -Wswitch -Wcomment -Wchar-subscripts -Wparentheses -Wcast-align -Waggregate-return -Wnested-externs -Winline -fsigned-char -Wuninitialized
# -Werror

obj-m := operafs.o
#obj-$(CONFIG_OPERA_FS) += operafs.o

operafs-objs := main.o super.o dir.o file.o inode.o address.o misc.o


