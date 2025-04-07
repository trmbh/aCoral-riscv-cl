# 禁用MinGW特定的链接器选项和行为
if(MINGW)
    # 禁用版本和子系统信息
    set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
    
    # 移除Windows特定的链接器标志
    string(REPLACE "--major-image-version 0 --minor-image-version 0" "" CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
    string(REPLACE "--major-image-version 0 --minor-image-version 0" "" CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
    
    # 设置输出格式为ELF
    set(CMAKE_EXECUTABLE_SUFFIX ".elf")
    
    # 禁用导入库
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)
endif() 