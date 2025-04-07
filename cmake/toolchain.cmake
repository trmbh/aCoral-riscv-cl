# 在任何其他设置之前，设置系统和编译器
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# 禁用所有Windows特定的链接器选项
set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_SHARED_LINKER_FLAGS "")
set(CMAKE_MODULE_LINKER_FLAGS "")
set(CMAKE_STATIC_LINKER_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS_INIT "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "")
set(CMAKE_STATIC_LINKER_FLAGS_INIT "")

if (WIN32)
    set(EXT ".exe")
else ()
    set(EXT "")
endif ()

message(STATUS "Check for RISCV toolchain ...")
if(NOT TOOLCHAIN)
    find_path(_TOOLCHAIN riscv64-unknown-elf-gcc${EXT})
    global_set(TOOLCHAIN "${_TOOLCHAIN}")
elseif(NOT "${TOOLCHAIN}" MATCHES "/$")
    global_set(TOOLCHAIN "${TOOLCHAIN}")
endif()

if (NOT TOOLCHAIN)
    message(FATAL_ERROR "TOOLCHAIN must be set, to absolute path of kendryte-toolchain dist/bin folder.")
endif ()

message(STATUS "Using ${TOOLCHAIN} RISCV toolchain")

# 设置工具链
global_set(CMAKE_C_COMPILER "${TOOLCHAIN}/riscv64-unknown-elf-gcc${EXT}")
global_set(CMAKE_CXX_COMPILER "${TOOLCHAIN}/riscv64-unknown-elf-g++${EXT}")
global_set(CMAKE_ASM_COMPILER "${TOOLCHAIN}/riscv64-unknown-elf-gcc${EXT}")
global_set(CMAKE_LINKER "${TOOLCHAIN}/riscv64-unknown-elf-ld${EXT}")
global_set(CMAKE_AR "${TOOLCHAIN}/riscv64-unknown-elf-ar${EXT}")
global_set(CMAKE_OBJCOPY "${TOOLCHAIN}/riscv64-unknown-elf-objcopy${EXT}")
global_set(CMAKE_SIZE "${TOOLCHAIN}/riscv64-unknown-elf-size${EXT}")
global_set(CMAKE_OBJDUMP "${TOOLCHAIN}/riscv64-unknown-elf-objdump${EXT}")
global_set(CMAKE_RANLIB "${TOOLCHAIN}/riscv64-unknown-elf-ranlib${EXT}")

# 设置编译器和链接器标志
set(CMAKE_C_FLAGS_INIT "")
set(CMAKE_CXX_FLAGS_INIT "")
set(CMAKE_ASM_FLAGS_INIT "")

# 设置查找规则
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 获取运行时库路径
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crt0.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRT0_OBJ)
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtbegin.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTBEGIN_OBJ)
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtend.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTEND_OBJ)
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crti.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTI_OBJ)
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-file-name=crtn.o OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE CRTN_OBJ)

# 设置链接命令
global_set(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> \"${CRTI_OBJ}\" \"${CRTBEGIN_OBJ}\" <OBJECTS> \"${CRTEND_OBJ}\" \"${CRTN_OBJ}\" -o <TARGET> <LINK_LIBRARIES>")

global_set(CMAKE_CXX_LINK_EXECUTABLE
        "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> \"${CRTI_OBJ}\" \"${CRTBEGIN_OBJ}\" <OBJECTS> \"${CRTEND_OBJ}\" \"${CRTN_OBJ}\" -o <TARGET> <LINK_LIBRARIES>")

# 验证工具链路径
get_filename_component(_BIN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
if (NOT "${TOOLCHAIN}" STREQUAL "${_BIN_DIR}")
    message(FATAL_ERROR "CMAKE_C_COMPILER is not in kendryte-toolchain dist/bin folder.")
endif ()
