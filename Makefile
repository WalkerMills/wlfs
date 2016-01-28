obj-m := wlfs.o
wlfs-objs := init.o super.o

KDIR := /lib/modules/$(shell uname -r)/build

CFLAGS_init.o = -Wno-error=declaration-after-statement

all: ko mkfs-wlfs

clean:
	make -C $(KDIR) M=$(PWD) clean
	$(RM) mkfs-wlfs

ko:
	make -C $(KDIR) M=$(PWD) modules

# mkfs-wlfs: private CFLAGS = -g
mkfs-wlfs: util.o
mkfs-wlfs_SOURCES:
	mkfs-wlfs.c wlfs.h disk.h
