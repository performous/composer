#!/bin/bash
# This is sourced by the other scripts to determine some paths.
# Feel free to modify the variables at the top of this file.

SOURCE_DIR="`pwd`/../.."
BUILD_DIR="`pwd`/build"
INSTALL_DIR="$BUILD_DIR/stage"


# You shouldn't need to touch anything below this

CROSS_ID=i686-pc-mingw32

CROSS_CXX=`which ${CROSS_ID}"-g++"`

if [ $? -ne 0 ]; then
	echo "Couldn't find cross-compiler ${CROSS_ID}-g++!"
	echo "Please add it to your PATH."
	exit 1
fi

CROSS_PREFIX=`dirname "$CROSS_CXX"`/..
CROSS_PREFIX=`readlink -f "$CROSS_PREFIX"` 

