#pragma once

// CMake uses config.cmake.hh to generate config.hh within the build folder.
#ifndef EDITOR_CONFIG_HH
#define EDITOR_CONFIG_HH

#define PACKAGE "Composer"
#define VERSION "2.0"

/* #undef STATIC_PLUGINS */

//#define SHARED_DATA_DIR ""

// FFMPEG libraries use changing include file names... Get them from CMake.
#define AVCODEC_INCLUDE <libavcodec/avcodec.h>
#define AVFORMAT_INCLUDE <libavformat/avformat.h>
#define AVRESAMPLE_INCLUDE <libavresample/avresample.h>
#define SWSCALE_INCLUDE <libswscale/swscale.h>
#define AVUTIL_INCLUDE <libavutil/avutil.h>
#define AVUTIL_OPT_INCLUDE </usr/include/libavutil/opt.h> //HACK to get AVOption class!
#define AVUTIL_MATH_INCLUDE </usr/include/libavutil/mathematics.h>

#endif
