# DO NOT MODIFY THIS FILE, IT WILL BE OVERRIDE!!!

# set this will supress some warnings
set(BUILDING_SDK "yes" CACHE INTERNAL "")

# basic config
if (NOT PROJ)
    get_filename_component(PROJ ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
    get_filename_component(PROJ ${PROJ} NAME)
    string(REPLACE " " "_" PROJ ${PROJ})
    message(STATUS "PROJ not set, use ${PROJ} as PROJ. Also, you can set it manually. e.g. -DPROJ=hello_world")
else()
    message("PROJ = ${PROJ}")
endif ()
cmake_minimum_required(VERSION 3.5)
include(./cmake/common.cmake)
include(./cmake/fix-mingw.cmake)
project(${PROJ} C CXX ASM)

# config self use headers
include(./cmake/macros.internal.cmake)
header_directories(${SDK_ROOT}/lib)
header_directories(src/${PROJ})
header_directories(third_party/fatfs/source)
header_directories(third_party/fatfs/port)
header_directories(src/aCoral-kernel/include)
header_directories(lib/drivers/include)
# build library first
add_subdirectory(lib)

# 添加FatFs源文件
add_source_files(third_party/fatfs/source/ff.c)

# 添加FatFs系统调用
add_source_files(third_party/fatfs/port/ffsystem.c)
add_source_files(third_party/fatfs/port/ffunicode.c)

# 添加硬盘IO接口实现
add_source_files(third_party/fatfs/port/diskio.c)
add_source_files(third_party/fatfs/port/sdcard.c)

# compile project
add_source_files(src/${PROJ}/*.c src/${PROJ}/*.s src/${PROJ}/*.S src/${PROJ}/*.cpp)
add_source_files(src/main.c)
include(./cmake/executable.cmake)

