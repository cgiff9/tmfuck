#!/bin/sh
#
# Swaps '#include <xxhash.h>' with '#include "xxhash.h" in
# all tmf source files (and vice versa).
#
# Will not operate outside the tmfuck source directory
#

# xxhash dev links
XXHASH_C_DEV="https://raw.githubusercontent.com/Cyan4973/xxHash/dev/xxhash.c"
XXHASH_H_DEV="https://raw.githubusercontent.com/Cyan4973/xxHash/dev/xxhash.h"

# xxhash latest release v0.8.2
XXHASH_C_0_8_2="https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.2/xxhash.c"
XXHASH_H_0_8_2="https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.2/xxhash.h"

# xxhash previous release v0.8.1
XXHASH_C_0_8_1="https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.1/xxhash.c"
XXHASH_H_0_8_1="https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.1/xxhash.h"

# default download is the latest release
XXHASH_C=$XXHASH_C_0_8_2;
XXHASH_H=$XXHASH_H_0_8_2;

PWD=$(pwd)
DIR=$(basename $PWD)

# All tmf source files listed here
SRCFILES="${PWD}/auto.c ${PWD}/block.c ${PWD}/file.c ${PWD}/machine.c ${PWD}/regex.c ${PWD}/stack.c ${PWD}/tape.c ${PWD}/tmfuck.c ${PWD}/var.h ${PWD}/auto.h ${PWD}/block.h ${PWD}/file.h ${PWD}/machine.h ${PWD}/regex.h ${PWD}/stack.h ${PWD}/tape.h ${PWD}/var.c"

if [ "$DIR" != "tmfuck" ] && [ "$DIR" != "tmf" ]; then
	echo 'What are you doing running this outside the tmfuck source directory? Get in there!'
	exit 1
fi

case $1 in
	
	"local")
		sed -e 's/#include <xxhash.h>/#include "xxhash.h"/g' -i $SRCFILES
		echo 'Set source files to local xxhash library (#include "xxhash.h")'
		grep -rnw "xxhash.h" $SRCFILES		
		;;
	
	"system")
		sed -e 's/#include "xxhash.h"/#include <xxhash.h>/g' -i $SRCFILES
		echo 'Set source files to system xxhash library (#include <xxhash.h>)'
		grep -rnw "xxhash.h" $SRCFILES
		;;

	"download")
		rm -f ${PWD}/xxhash.c ${PWD}/xxhash.h

		if [ "$2" == "dev" ]; then
			XXHASH_C=$XXHASH_C_DEV
			XXHASH_H=$XXHASH_H_DEV
			echo "Downloading development version of xxhash.c and xxhash.h"
		elif [ "$2" == "v0.8.1" ]; then
			XXHASH_C=$XXHASH_C_0_8_1
			XXHASH_H=$XXHASH_H_0_8_1
			echo "Downloading release v0.8.1 of xxhash.c and xxhash.h"
		else
			echo "Downloading release v0.8.2 of xxhash.c and xxhash.h"
		fi

		wget $XXHASH_C
		wget $XXHASH_H
		;;

	*)
		echo 'Script Usage:'
		echo '   xxhash_conf.sh local     (Set xxHash lib to local version in this folder)'
		echo '   xxhash_conf.sh system    (Set xxHash lib to installed system version)'
		echo '   xxhash_conf.sh download  (Download latest release of xxhash.c and xxhash.h)'
		exit 1
		;;

esac

exit 0
