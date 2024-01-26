# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Enforce compileparameters for corebuilds under GCC
# This to stop a few silly crashes that could have been avoided IF people
# weren't doing some -O3 psychooptimizations etc.

if(CMAKE_COMPILER_IS_GNUCXX AND NOT MINGW)
  add_definitions(-fno-delete-null-pointer-checks)
endif()

if(WIN32)
  list(APPEND sources_windows
    ${CMAKE_SOURCE_DIR}/src/common/Platform/ServiceWin32.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Platform/ServiceWin32.h
  )
endif(WIN32)

# 1. 创建 database 的 target
#   * database ${BUILD_SHARED_LIBS} ->
#     * trinity-core-interface
#     * mysql
#     * common
# 2. 设置 TRINITY_API_EXPORT_DATABASE 宏定义
add_subdirectory(database)

# 1. 创建 shared 的 target
#   * database ${BUILD_SHARED_LIBS} ->
#     * trinity-core-interface
#     * database
# 2. 设置 TRINITY_API_EXPORT_SHARED 宏定义
add_subdirectory(shared)

# 1. 创建 game 的 target
#   * game-interface INTREACE ->
#     * shared
#     * Detour(路径导航的)
#   * game ${BUILD_SHARED_LIBS} ->
#     * trinity-core-interface
#     * game-interface
#     * efsw(文件修改跟踪)
# 2. 设置 TRINITY_API_EXPORT_GAME 宏定义
add_subdirectory(game)

# 1. 创建 authserver 的 target
#   * authserver EXEC ->
#     * trinity-core-interface
#     * shared
# 2. 依据 COPY_CONF 的设置决定是否在 install prase 拷贝配置文件到 install 目录
# 3. 设置 _TRINITY_REALM_CONFIG="${CONF_DIR}/authserver.conf" 宏定义
add_subdirectory(authserver)

# 1. 创建 scripts loader 的 target
#   * scripts STATIC ->
#     * trinity-core-interface
#     * game-interface
# 2. 创建 scripts 的 target
#   * scripts_<script_dir_name> SHARED ->
#     * trinity-core-interface
#     * game
# 3. 控制台打印 scripts 的编译信息
add_subdirectory(scripts)

# 1. 创建 worldserver 的 target
#   * worldserver EXEC ->
#     * trinity-core-interface
#     * scripts
#     * game
#     * gsoap
#     * readline
# 2. 依据 COPY_CONF 的设置决定是否在 install prase 拷贝配置文件到 install 目录
# 3. 设置 _TRINITY_CORE_CONFIG="${CONF_DIR}/authserver.conf" 宏定义
add_subdirectory(worldserver)
