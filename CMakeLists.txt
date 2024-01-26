# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# 安装路径: -DCMAKE_INSTALL_PREFIX=/home/ngpong/TrinityCore/env/
# 构建服务: -DSERVERS=ON
# 构建工具: -DTOOLS=ON
# 脚本编译类型: -DSCRIPTS=dynamic
# 安装时拷贝配置文件到目录: -DCOPY_CONF=ON
# 核心是否使用PCH: -DUSE_COREPCH=ON
# 脚本是否使用PCH: -DUSE_SCRIPTPCH=ON
# 构建类型: -DCMAKE_BUILD_TYPE=Debug
# 包含更多的调试信息: -DWITH_COREDEBUG=OFF
# 默认启动动态库: -DWITH_DYNAMIC_LINKING=ON
# 启用警告: -DWITH_WARNINGS=ON
# 生成单元测试: BUILD_TESTING
# 编译器选择: -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++

cmake_minimum_required(VERSION 3.11)

# 禁止在源码内构建项目
# add this options before PROJECT keyword
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# 导出 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set projectname (must be done AFTER setting configurationtypes)
project(TrinityCore)

# CMake policies (can not be handled elsewhere)
cmake_policy(SET CMP0005 NEW)
if(CMAKE_VERSION VERSION_LESS "3.16.0")
  cmake_policy(SET CMP0043 OLD) # Disable 'Ignore COMPILE_DEFINITIONS_<Config> properties'
else()
  cmake_policy(SET CMP0043 NEW) # Cotire isn't used so set to NEW
endif()
cmake_policy(SET CMP0054 NEW) # Only interpret if() arguments as variables or keywords when unquoted - prevents intepreting if(SOME_STRING_VARIABLE MATCHES "MSVC") as if(SOME_STRING_VARIABLE MATCHES "1")

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # find_package() uses <PackageName>_ROOT variables
endif()

# 设置 build 的 target 不使用 RPATH，除非我们 intsall 它
# Set RPATH-handing (CMake parameters)
set(CMAKE_SKIP_BUILD_RPATH 0)
set(CMAKE_BUILD_WITH_INSTALL_RPATH 0)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH 1)

# set macro-directory
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake/macros")

if(CMAKE_VERSION VERSION_LESS "3.16.0")
  list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/dep/cotire/CMake")
endif()

# build in Release-mode by default if not explicitly set
if(CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
  set(CMAKE_DEFAULT_BUILD_TYPE "RelWithDebInfo" CACHE INTERNAL "")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

include(CMakePrintHelpers)
include(CheckCXXSourceRuns)
include(CheckIncludeFiles)

# 映射一些与 scripts 相关的帮助函数
include(ConfigureScripts)

# 设置一些 option 变量
# set default buildoptions and print them
include(cmake/options.cmake)

# 是否启用 PCH
# turn off PCH totally if enabled (hidden setting, mainly for devs)
if(NOPCH)
  set(USE_COREPCH 0)
  set(USE_SCRIPTPCH 0)
endif()

# 1. 支持 c++20 编译
# 2. 声明一些 INTERFACE TARGET，这些 target 会被多个地方引用(包括deps内的依赖项)，而且会根据当前操作系统环境进行修改
#   * trinity-warning-interface
#   * trinity-compile-option-interface
#   * trinity-feature-interface
#   * trinity-default-interface ->
#       * trinity-compile-option-interface
#       * trinity-feature-interface
#   * trinity-no-warning-interface
#   * trinity-hidden-symbols-interface
#   * trinity-dependency-interface ->
#       * trinity-default-interface
#       * trinity-no-warning-interface
#       * trinity-hidden-symbols-interface
#   * trinity-core-interface ->
#       * trinity-default-interface
#       * trinity-warning-interface
include(ConfigureBaseTargets)

# 检查平台相关
# 配置一些基础变量
#   * CONF_DIR:${CMAKE_INSTALL_PREFIX}/etc
# 配置卸载功能，配置文件位于：${CMAKE_SOURCE_DIR}/src/cmake/platform/cmake_uninstall.in.cmake
# 配置 GOLD LINKER 功能，如果启用了USE_LD_GOLD的话
# 配置编译时相关的一些设置
#   * 检查编译器版本
#   * 为动态库(trinity-compile-option-interface)添加一些编译选项，如果启用了 BUILD_SHARED_LIBS
#   * 检查编译器的一些 bug，并启用一些 workaround，比如 TRINITY_NEED_CHARCONV_WORKAROUND
#   * 设置一些编译指令，如：警告、分析器相关设置(MSAN,TSAN)、动态库相关、添加一些预定义宏；设置的大部分是上一步暴露出来的 INTERFACE target
include(CheckPlatform)

# 暴露 GroupSources 宏，该宏用于一些 IDE 使用 cmake 生成工程时候的目录树文件组织
include(GroupSources)

# 暴露一些 Collect* 相关的帮助函数，这些函数主要用作于收集目录信息
include(AutoCollect)

# 暴露 ADD_CXX_PCH 函数，用于添加 PCH 的支持
find_package(PCHSupport)

find_package(MySQL)

# Git package 用于确保安装的 git 版本，似乎作用于一些 dirty-testing？
if(NOT WITHOUT_GIT)
  find_package(Git 1.7)
endif()

# 1. 初始化 BUILDDIR 变量
# 2. 用于 "${BUILDDIR}/revision_data.h" 文件的创建
# Find revision ID and hash of the sourcetree
include(cmake/genrev.cmake)

# 1. 展示配置信息
# 2. 根据配置信息设置了一些宏定义
# print out the results before continuing
include(cmake/showoptions.cmake)

# 1. 设置依赖项子目录
# 2. 依赖项内的每个模块要不就是暴露一些 IMPORTED target 或是现场直接编译一个 target
# add dependencies
add_subdirectory(dep)

# 设置源码工程子目录
# add core sources
add_subdirectory(src)

# 单元测试编译支持
include(CTest)
if(BUILD_TESTING)
  list(APPEND CMAKE_MODULE_PATH
    "${Catch2_SOURCE_DIR}/contrib")
  include(Catch)

  add_subdirectory(tests)

  # Catch cmakefile messes with our settings we explicitly leave up to the user
  # restore user preference
  if (NOT WITH_SOURCE_TREE STREQUAL "hierarchical-folders")
    set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
  endif()
endif()
