cmake_minimum_required(VERSION 3.18.1)

project(RTMP VERSION 1.0)
set(RTMPSOURCE ${PROJECT_SOURCE_DIR}/src/main/cpp/rtmpdump/librtmp)

set(YUVSOURCE ${PROJECT_SOURCE_DIR}/src/main/cpp/libyuv/include)

set(RTMP_IMPL ${PROJECT_SOURCE_DIR}/src/main/cpp)
set(YUV_IMPL ${PROJECT_SOURCE_DIR}/src/main/cpp/libyuv/source)

add_definitions(-DNO_CRYPTO)

include_directories(${RTMPSOURCE})

include_directories(${YUVSOURCE})

aux_source_directory(${RTMPSOURCE} SOURCE)

aux_source_directory(${RTMP_IMPL} SOURCE2)
aux_source_directory(${YUV_IMPL} SOURCE3)


#add_executable(RTMP ${PROJECT_SOURCE_DIR}/src/main/cpp/RTMPHandle.c ${RTMP})

add_library(RTMP SHARED ${SOURCE} ${SOURCE2} ${SOURCE3})


find_library(log-lib
        log)
target_link_libraries(RTMP
        ${log-lib})
