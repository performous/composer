#pragma once

// CMake uses config.cmake.hh to generate config.hh within the build folder.
#ifndef EDITOR_CONFIG_HH
#define EDITOR_CONFIG_HH

#define PACKAGE "@CMAKE_PROJECT_NAME@"
#define VERSION "@PROJECT_VERSION@"

#cmakedefine STATIC_PLUGINS

//#define SHARED_DATA_DIR "@SHARE_INSTALL@"

// FFMPEG libraries use changing include file names... Get them from CMake.
#define AVCODEC_INCLUDE <@AVCodec_INCLUDE@>
#define AVFORMAT_INCLUDE <@AVFormat_INCLUDE@>
#define AVRESAMPLE_INCLUDE <@AVResample_INCLUDE@>
#define SWSCALE_INCLUDE <@SWScale_INCLUDE@>
#define AVUTIL_INCLUDE <@AVUtil_INCLUDE@>
#define AVUTIL_OPT_INCLUDE <libavutil/opt.h> //HACK to get AVOption class!
#define AVUTIL_MATH_INCLUDE <libavutil/mathematics.h>

#endif
