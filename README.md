# WLFS
A log-based filesystem

## Build environment
You must download the Linux kernel source to compile this kernel module.  On Arch Linux:
```
# pacman -S linux-headers
# cd /usr/lib/$(uname -r)/build
# make mrproper
# cp .config /path/to/project/src
# chown your_user:optional_group /path/to/project/src/.config
```
You can also get the source from [github](https://github.com/torvalds/linux), just make sure to checkout the appropriate tag.  At the time of writing, the stable Arch kernel is 4.3.3

