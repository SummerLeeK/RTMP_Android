cmake_minimum_required(VERSION 3.18.1)

project(FFmpeg VERSION 1.0)

set(FFMPEG_SO_DICT ${PROJECT_SOURCE_DIR}/libs/arm64-v8a)
set(FFMPEG_DICT ${PROJECT_SOURCE_DIR}/3rdparty/FFmpeg/Android/aarch64)


set(YUVSOURCE ${PROJECT_SOURCE_DIR}/src/main/jni/libyuv/include)

set(YUV_IMPL ${PROJECT_SOURCE_DIR}/src/main/jni/libyuv/source)

include_directories(${FFMPEG_DICT}/include)


include_directories(${YUVSOURCE})

aux_source_directory(${YUV_IMPL} SOURCE3)

# 3. 添加ffmpeg相关的so库
add_library( avutil
        SHARED
        IMPORTED )
set_target_properties( avutil
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libavutil.so )

add_library(swresample
        SHARED
        IMPORTED )
set_target_properties(swresample
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libswresample.so )

add_library(avcodec
        SHARED
        IMPORTED )
set_target_properties( avcodec
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libavcodec.so )

add_library(avfilter
        SHARED
        IMPORTED)
set_target_properties( avfilter
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libavfilter.so )

add_library(swscale
        SHARED
        IMPORTED)
set_target_properties( swscale
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libswscale.so )

add_library(avformat
        SHARED
        IMPORTED)
set_target_properties( avformat
        PROPERTIES IMPORTED_LOCATION
        ${FFMPEG_SO_DICT}/libavformat.so)

#add_executable(FFmpeg ./src/main/jni/FFmpegVersion.c)
add_library(FFmpeg SHARED ${PROJECT_SOURCE_DIR}/src/main/jni/FFmpegVersion.c ${SOURCE3})


find_library(log-lib
             log)
target_link_libraries(FFmpeg
        avutil
        swresample
        avcodec
        avfilter
        swscale
        avformat
        ${log-lib})