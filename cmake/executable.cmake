if (NOT BUILDING_SDK)
    if(EXISTS ${SDK_ROOT}/libkendryte.a)
        add_library(kendryte STATIC IMPORTED)
        set_property(TARGET kendryte PROPERTY IMPORTED_LOCATION ${SDK_ROOT}/libkendryte.a)
        include_directories(${SDK_ROOT}/include/)
    else()
        header_directories(${SDK_ROOT}/lib)
        add_subdirectory(${SDK_ROOT}/lib)
    endif()
endif ()

removeDuplicateSubstring(${CMAKE_C_FLAGS} CMAKE_C_FLAGS)
removeDuplicateSubstring(${CMAKE_CXX_FLAGS} CMAKE_CXX_FLAGS)

message("SOURCE_FILES=${SOURCE_FILES}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 明确指定输出是ELF文件
set_target_properties(${PROJECT_NAME} PROPERTIES 
    LINKER_LANGUAGE C
    SUFFIX ".elf"
)

# 设置正确的链接选项
target_link_libraries(${PROJECT_NAME}
        -Wl,--start-group
        gcc m c
        -Wl,--whole-archive
        kendryte
        -Wl,--no-whole-archive
        -Wl,--end-group
        )
        
if (EXISTS ${SDK_ROOT}/src/${PROJ}/project.cmake)
    include(${SDK_ROOT}/src/${PROJ}/project.cmake)
endif ()

# 确保使用正确的后缀
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# 生成二进制文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --output-format=binary ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
        DEPENDS ${PROJECT_NAME}
        COMMENT "Generating .bin file ...")

# 生成反汇编文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJDUMP} -d -S ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf > ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.d
        DEPENDS ${PROJECT_NAME}
        COMMENT "Generating .d file ...")

# 显示信息
include(${CMAKE_CURRENT_LIST_DIR}/dump-config.cmake)
