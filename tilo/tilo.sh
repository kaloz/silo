#!/bin/sh
# TILO - Trivial Image LOader
#
# by Jan Vondrak (C) 1998
#
# This is just a simple wrapper for maketilo...

usage () {
	echo "Usage: tilo [ -o output ] <kernel images> <root image>"
}

set -e

sun4u=
sun4c=
rootimg=
tilo_args=
output=
to_remove=

trap "rm -f $to_remove" 0 1 2 3 7 10 13 15

do_image () {
	[ $# = 1 -a -n "$1" ] || exit
	KERNEL=$1
	tag=
	sun4u_init=`nm $KERNEL | awk '/t sun4u_init$/{print$1}'`
	[ "$?" = 0 ] || exit 2
	if [ -n "$sun4u_init" ]; then
		if [ -n "$sun4u" ]; then
			echo "Multiple sun4u kernels"
			exit 1
		fi
		sun4u=$KERNEL
		tag=sun4u
	else
		if [ -n "$sun4c" ]; then
			echo "Multiple sun4cdm kernels"
			exit 1
		fi
		sun4c=$KERNEL
		tag=sun4cdm
	fi
	echo "Converting $KERNEL ($tag) to a.out:"
	elftoaout $KERNEL -o $KERNEL.out
	tail --bytes +33 $KERNEL.out > $KERNEL.raw
	rm $KERNEL.out
	echo Compressing $KERNEL.raw:
	gzip -c9v $KERNEL.raw > $KERNEL.gz
	to_remove="$to_remove $KERNEL.gz"
	PSIZE=`ls -l $KERNEL.gz | awk '{print$5}'`
	SIZE=`ls -l $KERNEL.raw | awk '{print$5}'`
	ROOTA=`nm $KERNEL | awk '/A _end$/{print$1}'`
	rm $KERNEL.raw
	echo "Sizes ($tag):"
	echo "  raw size     = $SIZE"
	echo "  packed size  = $PSIZE"
	echo "  root address = $ROOTA"
	if [ -n "$sun4u" ]; then
		tilo_args="$tilo_args sun4u=$KERNEL.gz size4u=$SIZE root4u=$ROOTA"
	else
		tilo_args="$tilo_args sun4c=$KERNEL.gz size4c=$SIZE root4c=$ROOTA"
	fi
}

do_root () {
	[ $# = 1 -a -n "$1" ] || exit
	if [ -n "$rootimg" ]; then
		echo "Multiple root images?"
		exit 1
	fi
	rootimg=$1
	if file $rootimg | grep -q "gzip compressed data"; then
		: # Nothing
	else
		echo Compressing $rootimg:
		gzip -c9v $rootimg > $rootimg.gz
		to_remove="$to_remove $rootimg.gz"
		rootimg=$rootimg.gz
	fi
	ROOT_SIZE=`ls -l $rootimg | awk '{print$5}'`
	echo Root image packed size = $ROOT_SIZE
	tilo_args="$tilo_args root=$rootimg"
}

while [ $# != 0 ]; do
	case "$1" in
		-h) usage; exit ;;
		-o) shift; output="$1" ;;
		-*) echo $"$0: Unrecognised option: $1" >&2
		    usage >&2; exit 1 ;;
		*)
			if [ ! -f "$1" ]; then
				echo "$1 does not exist"
				exit 1
			fi
			if file $1 | grep -q "MSB executable"; then
				do_image $1 || exit 1
			else
				# Suspect it is a root.bin
				do_root $1 || exit 1
			fi
			;;
	esac
	shift
done

test -z "$output" && output=tftpboot.img

if [ -z "$sun4u" -a -z "$sun4c" ]; then
	echo "No images provided"
	exit 1
fi

`echo $0 | sed 's/tilo$/maketilo/'` $tilo_args out=$output

rm -f $to_remove

TILO_SIZE=`ls -l $output | awk '{print$5}'`
echo TILO size = $TILO_SIZE
