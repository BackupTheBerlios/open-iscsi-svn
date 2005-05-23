#!/bin/bash
#
# Open-iSCSI Regression Test Utility
# Copyright (C) 2004 Dmitry Yusupov
# maintained by open-iscsi@googlegroups.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# See the file COPYING included with this distribution for more details.
#

PATH=$PATH:.

trap regress_signal INT QUIT TERM
regress_signal() {
    printf "\nterminating, restore defaults: "
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.ImmediateData -v Yes
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.InitialR2T -v No
./iscsiadm -m node -r $record -o update \
	-n node.conn[0].iscsi.HeaderDigest -v None,CRC32C
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.FirstBurstLength -v $((256*1024))
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.MaxBurstLength -v $((16*1024*1024-1024))
./iscsiadm -m node -r $record -o update \
	-n node.conn[0].iscsi.MaxRecvDataSegmentLength -v $((128*1024))
    printf "done\n"
    exit 0
}

function update_cfg() {
./iscsiadm -m node -r $record -u
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.ImmediateData -v $imm_data_en
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.InitialR2T -v $initial_r2t_en
./iscsiadm -m node -r $record -o update \
	-n node.conn[0].iscsi.HeaderDigest -v $hdrdgst_en
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.FirstBurstLength -v $first_burst
./iscsiadm -m node -r $record -o update \
	-n node.session.iscsi.MaxBurstLength -v $max_burst
./iscsiadm -m node -r $record -o update \
	-n node.conn[0].iscsi.MaxRecvDataSegmentLength -v $max_recv_dlength
./iscsiadm -m node -r $record -l
}

function disktest_run() {
	bsizes="512 1024 2048 4096 8192 16384 32768 65536 131072 1000000"
	test x$bsize != x && bsizes=$bsize
	test x$bsize = xbonnie && return 0;
	for bs in $bsizes; do
		echo -n "disktest -T2 -K8 -B$bs -r -ID $device: "
		if ! disktest -T2 -K8 -B$bs -r -ID $device >/dev/null; then
			echo "FAILED"
			return 1;
		fi
		echo "PASSED"
		echo -n "disktest -T2 -K8 -B$bs -E16 -w -ID $device: "
		if ! disktest -T2 -K8 -B$bs -E16 -w -ID $device >/dev/null;then
			echo "FAILED"
			return 1;
		fi
		echo "PASSED"
	done
	return 0;
}

function fdisk_run() {
	echo "fdisk $device: "
	if ! echo "
p
d
n
p
1


w
q
" | fdisk $device 2>/dev/null >/dev/null; then
		echo "FAILED"
		return 1;
	fi
	echo "PASSED"
	return 0;
}

function mkfs_run() {
	echo -n "mke2fs $device: "
	if ! mke2fs $device"1" 2>/dev/null >/dev/null; then
		echo "FAILED"
		return 1;
	fi
	echo "PASSED"
	return 0;
}

function bonnie_run() {
	dir="/tmp/iscsi.bonnie.regression"
	bonnie_dir=`pwd`
	umount $dir 2>/dev/null >/dev/null
	rm -rf $dir; mkdir $dir
	echo -n "mount $dir: "
	if ! mount -t ext2 $device"1" $dir; then
		echo "FAILED"
		return 1;
	fi
	echo "PASSED"
	echo -n "bonnie++ -r0 -n10:0:0 -s16 -uroot -f -q: "
	pushd $dir >/dev/null
	if ! $bonnie_dir/bonnie++ -r0 -n10:0:0 -s16 -uroot -f -q 2>/dev/null >/dev/null; then
		popd >/dev/null
		umount $dir 2>/dev/null >/dev/null
		echo "FAILED"
		return 1;
	fi
	popd >/dev/null
	umount $dir 2>/dev/null >/dev/null
	echo "PASSED"
	return 0;
}

function fatal() {
	echo "regression.sh: $1"
	echo "Usage: regression.sh <rec#|-f> <device> [test#[:#]] [bsize]"
	exit 1
}

############################ main ###################################

test ! -e regression.dat && fatal "can not find regression.dat"
test ! -e disktest && fatal "can not find disktest"
test ! -e iscsiadm && fatal "can not find iscsiadm"
test ! -e bonnie++ && fatal "can not find bonnie++"
test x$1 = x && fatal "node record parameter error"
test x$2 = x && fatal "SCSI device parameter error"

device=$2
if test x$1 = "x-f" -o x$1 = "x--format"; then
	mkfs_run
	exit
fi

record=$1
test x$3 != x && begin=$3
test x$4 != x && bsize=$4

if test x$begin != x; then
	end=`echo $begin | awk -F: '{print $2}'`
	begin=`echo $begin | awk -F: '{print $1}'`
fi

printf "
BIG FAT WARNING!

Open-iSCSI Regression Test Suite is about to start. It is going
to use "$device" for its testing. iSCSI session could be re-opened
during the tests several times and as the result device name could
not match provided device name if some other SCSI activity happened
during the test.

Are you sure you want to continue? [y/n]: "
read line
if test x$line = xn -o x$line = xN -o x$line = xno -o x$line = xNO; then
	echo "aborting..."
	exit
fi

i=0
cat regression.dat | while read line; do
	if echo $line | grep "^#" >/dev/null; then continue; fi
	if echo $line | grep "^$" >/dev/null; then continue; fi
	if test x$begin != x; then
		if test x$begin != x$i -a x$end = x; then
			let i=i+1
			continue
		elif test x$begin != x -a x$end != x; then
			if test $i -lt $begin -o $i -gt $end; then
				let i=i+1
				continue
			fi
		fi
	fi
	imm_data_en=`echo $line | awk '/^[YesNo]+/ {print $1}'`
	if test x$imm_data_en = x; then continue; fi
	initial_r2t_en=`echo $line | awk '{print $2}'`
	hdrdgst_en=`echo $line | awk '{print $3}'`
	first_burst=`echo $line | awk '{print $4}'`
	max_burst=`echo $line | awk '{print $5}'`
	max_recv_dlength=`echo $line | awk '{print $6}'`
	max_r2t=`echo $line | awk '{print $7}'`
	update_cfg
	echo "================== TEST #$i BEGIN ===================="
	echo "imm_data_en = $imm_data_en"
	echo "initial_r2t_en = $initial_r2t_en"
	echo "hdrdgst_en = $hdrdgst_en"
	echo "first_burst = $first_burst"
	echo "max_burst = $max_burst"
	echo "max_recv_dlength = $max_recv_dlength"
	echo "max_r2t = $max_r2t"
	if ! disktest_run; then break; fi
	if ! fdisk_run; then break; fi
	if ! mkfs_run; then break; fi
	if ! bonnie_run; then break; fi
	let i=i+1
done
regress_signal
echo
echo "===================== THE END ========================"
