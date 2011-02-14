#!/bin/bash
# Creates a build dir and does the required CMake mangling.

source pathconfig.sh

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cat > Toolchain.cmake << EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER ${CROSS_ID}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_ID}-g++)
set(CMAKE_FIND_ROOT_PATH ${CROSS_PREFIX}/${CROSS_ID})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(WINDRES ${CROSS_ID}-windres)
EOF

PATH="${CROSS_PREFIX}/bin:$PATH" cmake -DCMAKE_TOOLCHAIN_FILE=Toolchain.cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" "$SOURCE_DIR"

