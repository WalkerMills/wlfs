obj-m := wlfs.o
wlfs-objs := init.o

CFLAGS_init.o = -Wno-error=declaration-after-statement

all: ko mkfs-wlfs

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	$(RM) mkfs-wlfs

ko:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

mkfs-wlfs_SOURCES:
	mkfs-wlfs.c wlfs.h
