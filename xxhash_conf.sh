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
SRCFILES=$(ls src/*.{c,h} | grep -v xxhash)

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
		rm -f ${PWD}/src/xxhash.c ${PWD}/src/xxhash.h

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

		wget -P src $XXHASH_C 
		wget -P src $XXHASH_H
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
