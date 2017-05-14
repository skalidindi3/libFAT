#!/bin/bash -e

IMAGE_NAME="TESTFAT32"
LOCAL_IMG="./floppy.img"

# make blank 64MB image
dd if=/dev/zero bs=1m count=64 of=$LOCAL_IMG

# mount blank image, format as FAT32, & detach
FLOPPY_DISK=`hdid -nomount $LOCAL_IMG`
newfs_msdos -F 32 -v "$IMAGE_NAME" $FLOPPY_DISK
hdiutil detach $FLOPPY_DISK -force

# remount this image post-modification, create files, & detach
FLOPPY_DISK=`hdid $LOCAL_IMG | cut -d ' ' -f 1`
mkdir -p "/Volumes/$IMAGE_NAME"/nest/doublenest/
echo "vampire" > "/Volumes/$IMAGE_NAME"/octopus.txt
echo "lion's mane" > "/Volumes/$IMAGE_NAME"/nest/jellyfishsuperlongnamefile.txt
echo "turtles all the way down" > "/Volumes/$IMAGE_NAME"/nest/doublenest/turtles.txt
hdiutil detach $FLOPPY_DISK -force
