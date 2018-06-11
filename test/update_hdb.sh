#!/bin/sh

echo "create MINIX partition for cnix"
fdisk /dev/hdb < fdiskcmd > /dev/null 2>&1

echo "make file system"
../tools/mkfs.minix -n 30 -v /dev/hdb1

echo "mount hard image"
mount -t minix /dev/hdb1 ./img 2>/dev/null

echo "mkdir and mknod"
mkdir ./img/usr
mkdir ./img/bin
mkdir ./img/tmp
mkdir ./img/dev
./update_dev.sh
mkdir ./img/var
mkdir ./img/var/tmp
mkdir ./img/etc

echo "copy all programs to hard image"
rm -f ./img/bin/*
cp tmp/* ./img/bin/
cp termcap ./img/etc
cp /etc/termcap ./img/etc/termcap.bak
cp /etc/passwd ./img/etc
mkdir ./img/root
cp .bashrc ./img/root
mkdir ./img/usr/cnixbin
cp -rf /usr/cnixbin/* ./img/usr/cnixbin

if ! test -d ../hdimg; then
	rm -f ./img/kernel
	cp ../hdimg ./img/kernel
fi

echo "unmount hard image"
umount /dev/hdb1
