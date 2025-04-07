# src/aCoral-kernel/src/fs/CMakeLists.txt

# 设置FatFs源文件
set(FATFS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/fatfs_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fs.c
    ${SDK_ROOT}/third_party/fatfs/source/ff.c
    ${SDK_ROOT}/third_party/fatfs/port/diskio.c
    ${SDK_ROOT}/third_party/fatfs/port/sdcard.c    # 修正sdcard.c的路径
)

# 创建静态库
add_library(fatfs STATIC ${FATFS_SRCS})

# 添加头文件路径
target_include_directories(fatfs PUBLIC
    ${SDK_ROOT}/src/aCoral-kernel/include
    ${SDK_ROOT}/src/aCoral-kernel/include/fs
    ${SDK_ROOT}/third_party/fatfs/source
    ${SDK_ROOT}/third_party/fatfs/port
)

# 添加编译选项
target_compile_definitions(fatfs PRIVATE
    -DUSE_FATFS
    -DUSE_SD_CARD
)

# 添加依赖库
target_link_libraries(fatfs
    acoral  # aCoral内核库
)