dnl Block device for testing (uses a loopback mount by default)
define(`m4_DEVICE', image)
dnl Location of mount point for test device
define(`m4_MOUNT', mount)
dnl Size of test device (only used when the test device is a file)
define(`m4_DEVICE_SIZE', 1073741824)
