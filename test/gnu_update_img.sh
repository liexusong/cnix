#!/bin/sh

if ! test -e ./hdimg256M; then
	echo "create hdimg256M"
	dd if=/dev/zero of=hdimg256M bs=262080 count=1024

	echo "create MINIX partition for cnix"
	losetup /dev/loop0 ./hdimg256M
	fdisk /dev/loop0 < fdiskcmd > /dev/null 2>&1
	losetup -d /dev/loop0

	losetup -o 32256 /dev/loop0 ./hdimg256M

	echo "make file system"
	../tools/mkfs.minix -n 30 -v /dev/loop0
else
	losetup -o 32256 /dev/loop0 ./hdimg256M
fi

echo "mount hard image"
mount -t minix /dev/loop0 ./img 2>/dev/null

if ! test -d ./img/usr; then
	echo "mkdir and mknod"
	mkdir ./img/usr
	mkdir ./img/bin
	mkdir ./img/tmp
	mkdir ./img/dev
	./update_dev.sh
	mkdir ./img/var
	mkdir ./img/var/tmp
	mkdir ./img/etc
	cp termcap ./img/etc
	cp /etc/termcap ./img/etc/termcap.bak
	cp /etc/passwd ./img/etc
	mkdir ./img/root
	cp .bashrc ./img/root
	mkdir ./img/usr/cnixbin
	#mkdir ./img/usr/share
	#mkdir ./img/usr/share/vim
	#cp -rf /usr/share/vim/vim70 ./img/usr/share/vim
fi

echo "copy all programs to hard image"
rm -f ./img/bin/*
cp tmp/* ./img/bin/
rm -rf ./img/usr/cnixbin/*
cp -rf /usr/cnixbin/* ./img/usr/cnixbin

if ! test -d ../hdimg; then
	rm -f ./img/kernel
	cp ../hdimg ./img/kernel
fi

echo "unmount hard image"
umount /dev/loop0
losetup -d /dev/loop0

echo "tar hdimg256M to hdimg256M.tar.gz"
tar czvf hdimg256M.tar.gz hdimg256M

if [ "$1" == "-d" ]; then
echo "delete hdimg256M"
rm -f hdimg256M
fi
