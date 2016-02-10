# WLFS
A log-based filesystem

## Build environment
You must download the Linux kernel headers to compile this kernel module; Google Test & the m4 macro processor to build & run the tests. wlfs is developed on Arch Linux & Debian without regard for backwards compatibility; if it breaks on earlier kernel versions, submit a pull request.  To install build deps on Arch Linux:
```
# pacman -S linux-headers m4 gtest
```
On Debian:
```
# apt-get install linux-headers m4 libgtest-dev
```
You must also copy the kernel configuration to the build directory.  To copy the configuration of the currently running kernel:
```
# zcat /proc/config.gz > /path/to/wlfs/src/.config
# chown your_user:optional_group /path/to/wlfs/src/.config
```

## Testing
Running `make test` executes all unit & validation tests

