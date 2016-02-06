#!/bin/bash

MODULE=$(sed -rn 's/.*name = "(.*?)".*/\1/p' init.c)

DEV_SIZE=1073741824
BLOCK_SIZE=$(lsblk -o MOUNTPOINT,PHY-SEC | 
             grep "$(df . --output=target | tail -n 1)" | awk '{print $2}')
BLOCKS=$(($DEV_SIZE / $BLOCK_SIZE))

FILE=image
MOUNT=mount

make

if [ ! -f $FILE ]
then
    echo "Creating wlfs image"
    dd if=/dev/zero of=$FILE bs=$BLOCK_SIZE count=$BLOCKS
    ./mkfs-wlfs -b $BLOCK_SIZE $FILE
fi
if [ ! -d $MOUNT ]
then
    echo "Creating mount point"
    mkdir $MOUNT
fi

echo "Loading $MODULE module"
if $(lsmod | grep wlfs)
then
    sudo rmmod $MODULE
fi
sudo insmod ${MODULE}.ko
 
echo "Mounting $FILE on $MOUNT"
if $(sudo mount -o loop -t $MODULE $FILE $MOUNT)
then
    echo "Mounting $FILE successful!"
    sudo umount $MOUNT
else
    echo "Mounting unsuccessful"
fi
sudo rmmod $MODULE
