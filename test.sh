#!/bin/bash

BLOCK_SIZE=0
echo $BLOCK_SIZE
if [ -b M4_DEVICE ]
then
    BLOCK_SIZE=$(lsblk -o NAME,PHY-SEC | 
                 grep $(basename M4_DEVICE) | 
                 awk '{print $2}')
else
    BLOCK_SIZE=$(lsblk -o MOUNTPOINT,PHY-SEC | 
                 grep "$(df $(dirname M4_DEVICE) --output=target | 
                         tail -n 1)" |  
                 awk '{print $2}')
fi
echo $BLOCK_SIZE
BLOCKS=$((M4_DEVICE_SIZE / $BLOCK_SIZE))

# Zero and reformat the test device
dd if=/dev/zero of=m4_DEVICE bs=$BLOCK_SIZE count=$BLOCKS
./mkfs-wlfs -b $BLOCK_SIZE m4_DEVICE
# Perform validation testing for mkfs-wlfs
test/test_mkfs

# Create the mount point if it doesn't exist
if [ ! -d m4_MOUNT ]
then
    mkdir m4_MOUNT
fi

MODULE=$(sed -rn 's/.*\.name = "(.*?)".*/\1/p' init.c)
# Reload the kernel module if necessary
if [ -n "$(lsmod | grep $MODULE)" ]
then
    sudo rmmod $MODULE
fi
sudo insmod ${MODULE}.ko

# Run validations tests
if ! $(sudo mount -o loop -t $MODULE m4_DEVICE m4_MOUNT)
then
    echo "Mounting unsuccessful"
    exit 1
fi

# Clean up
sudo umount m4_MOUNT
sudo rmmod $MODULE
