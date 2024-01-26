# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

if(WIN32 AND MSVC)
  set(sources_windows
    ${CMAKE_SOURCE_DIR}/src/common/Debugging/WheatyExceptionReport.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Debugging/WheatyExceptionReport.h
    ${CMAKE_SOURCE_DIR}/src/common/WindowsSettings.manifest
  )
endif()

# 1. 创建 recvision_data.h (真实文件由 src/cmake/genrev.cmake 生成)
# 2. 为什么不在 genrev.cmake 文件里面直接创建呢？
add_subdirectory(genrev)

# 1. 创建 common 的构建 target
# 2. 添加 TRINITY_API_EXPORT_COMMON 宏定义
# 3. 如果支持则设置 precompile header
# 4. IDE 支持
# 5. 动态库支持
add_subdirectory(common)

# 1. 创建 worldserver 的构建 target
# 2. 创建 authserver 的构建 target
# 3. 创建 scripts 的构建 target
# 4. 创建 database 的构建 target
# 5. 创建 game 的构建 target
# 6. 创建 shared 的构建 target
if(SERVERS)
  add_subdirectory(server)
endif(SERVERS)

# 创建 TOOLS 的构建 target；在 src/tools 目录下的那些东西
if(TOOLS)
  add_subdirectory(tools)
endif(TOOLS)
