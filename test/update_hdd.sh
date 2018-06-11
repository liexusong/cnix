#!/bin/sh
#
#echo "create MINIX partition for cnix"
#fdisk /dev/hdd < fdiskcmd > /dev/null 2>&1
#
#echo "make file system"
#../tools/mkfs.minix -n 30 -v /dev/hdd1
#
echo "mount hard image"
mount -t minix /dev/hdd1 ./img 2>/dev/null
#
#echo "mkdir and mknod"
#mkdir ./img/usr/
#mkdir ./img/bin/
#mkdir ./img/tmp/
#mkdir ./img/dev/
#mkdir ./img/etc/
#mkdir ./img/var/
#mkdir ./img/var/tmp/
#mkdir ./img/root/
#./update_dev.sh

echo "copy all programs to hard image"
cp tmp/* ./img/bin/
#
#echo "copy gcc & ld to hard image"
#cp -r /usr/cnixbin/ ./img/usr/
#
#echo "copy vim runtime file"
#if ! test -d ./img/usr/share ; then
#	mkdir -p ./img/usr/share/vim/
#fi
#cp -r /usr/share/vim/vim70 ./img/usr/share/vim/
#
#echo "copy system file"
#cp /etc/passwd ./img/etc
#cp /etc/group ./img/etc/group
#cp termcap ./img/etc/
#cp /etc/termcap ./img/etc/termcap.back
#
#echo "copy user info"
#cp ./.bashrc ./img/root/
#cp /root/.vimrc  ./img/root/
#
#echo "copy gcc & ld to bin directory"
#cp /usr/cnixbin/bin/i586-pc-cnix-gcc ./img/bin/gcc
#cp /usr/cnixbin/bin/ld ./img/bin/ld
#cp /usr/cnixbin/bin/objcopy ./img/bin/objcopy
#cp ./img/bin/bash ./img/bin/sh
#
echo "copy cnix source code"
rm -fr ./img/root/cnix
cp -r /home/qin/tmp/cnix ./img/root/
#
#echo "copy newlib src"
#cp -r /home/qin/newlib-1.15.0/ ./img/root/
#cp -r /home/qin/build-newlib ./img/root/
#
echo "unmount hard image"
umount /dev/hdd1
