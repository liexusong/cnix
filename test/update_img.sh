#!/bin/sh

diskfile=hdimg128M

if ! test -e ./$diskfile; then
	echo "create hdimg160M"
	dd if=/dev/zero of=$diskfile bs=163840 count=1024

	echo "create MINIX partition for cnix"
	losetup /dev/loop0 ./$diskfile
	fdisk /dev/loop0 < fdiskcmd > /dev/null 2>&1
	losetup -d /dev/loop0

	losetup -o 32256 /dev/loop0 ./$diskfile

	echo "make file system"
	../tools/mkfs.minix -n 30 -v /dev/loop0
else
	losetup -o 32256 /dev/loop0 ./$diskfile
fi

echo "mount hard image"
mount -t minix /dev/loop0 ./img 2>/dev/null

#exit 0

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
	#mkdir ./img/usr/share
	#mkdir ./img/usr/share/vim
	#cp -rf /usr/share/vim/vim70 ./img/usr/share/vim
fi

#mkdir -p ./img/disk2/usr/cross/bin
#cp /disk2/usr/cross/bin/* ./img/disk2/usr/cross/bin/
#rm -fr ./img/disk2/usr
#mkdir ./img/disk2/usr
#cp -r /disk2/usr/cross/ ./img/disk2/usr/
#cp /tmp/b.c ./img/
#mkdir ./img/usr/share
#cp -r /usr/share/vim	./img/usr/share
rm ./img/usr/share/vim/vim70/syntax/*
cp /usr/share/vim/vim70/syntax/c.vim ./img/usr/share/vim/vim70/syntax/
cp /usr/share/vim/vim70/syntax/syntax.vim ./img/usr/share/vim/vim70/syntax/
cp /usr/share/vim/vim70/syntax/nosyntax.vim ./img/usr/share/vim/vim70/syntax/
cp /usr/share/vim/vim70/syntax/synload.vim ./img/usr/share/vim/vim70/syntax/
cp /usr/share/vim/vim70/syntax/syncolor.vim ./img/usr/share/vim/vim70/syntax/
cp ./termcap ./img/etc
cp .bashrc ./img/root
cp ~/.vimrc ./img/root
cp ~/.viminfo ./img/root

echo "copy all programs to hard image"
rm -f ./img/bin/*
cp tmp/* ./img/bin/
ln ./img/bin/ex ./img/bin/vi

echo "unmount hard image"
umount /dev/loop0
losetup -d /dev/loop0

echo "tar $diskfile to $diskfile.tar.gz"
tar czvf $diskfile.tar.gz $diskfile

if [ "$1" == "-d" ]; then
echo "delete $diskfile"
rm -f $diskfile
fi
