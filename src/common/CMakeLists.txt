# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# 收集当前目录下非 Exclude 指定目录下的文件(源文件和头文件)
CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/Debugging
  ${CMAKE_CURRENT_SOURCE_DIR}/Platform
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

# Manually set sources for Debugging directory as we don't want to include WheatyExceptionReport in common project
# It needs to be included both in authserver and worldserver for the static global variable to be properly initialized
# and to handle crash logs on windows
list(APPEND PRIVATE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Debugging/Errors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Debugging/Errors.h)

if(USE_COREPCH)
  set(PRIVATE_PCH_HEADER PrecompiledHeaders/commonPCH.h)
endif(USE_COREPCH)

# IDE 支持
GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DTRINITY_API_EXPORT_COMMON)

# 1. make common target
# 2. PRIVATE_SOURCES 默认情况下会包含头文件，但是 cmake 会帮我们过滤掉
add_library(common
  ${PRIVATE_SOURCES}
)

# 收集当前目录下(非 Exclude 指定目录)的所有文件夹
# Do NOT add any extra include directory here, as we don't want the common
# library to depend on anything else than TC deps, and itself.
# This way we ensure that if either a PR does that without modifying this file,
# a compile error will be generated, either this file will be modified so it
# is detected more easily.
# While it is OK to include files from other libs as long as they don't require
# linkage (enums, defines...) it is discouraged to do so unless necessary, as it will pullute
# include_directories leading to further unnoticed dependency aditions
# Linker Depencency requirements: none
CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

# 添加包含目录
target_include_directories(common
  PUBLIC
    # Provide the binary dir for all child targets
    ${CMAKE_BINARY_DIR}
    ${PUBLIC_INCLUDES}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

# 添加链接项；PUBLIC 下的 target 通过 add_subdirectory(deps) 暴露出来
target_link_libraries(common
  PRIVATE
    trinity-core-interface
  PUBLIC
    argon2
    boost
    fmt
    g3dlib
    Detour
    sfmt
    utf8cpp
    openssl
    valgrind
    threads
    jemalloc
    short_alloc
    magic_enum)

# 添加依赖项，确保 src/genrev/CMakeLists.txt 里面 cmake 的 revision_data.h 执行完后才会编译当前 target
add_dependencies(common revision_data.h)

# IDE支持
set_target_properties(common
    PROPERTIES
      FOLDER
        "server")

# 动态库编译支持
if(BUILD_SHARED_LIBS)
  if(UNIX)
    install(TARGETS common
      LIBRARY
        DESTINATION lib)
  elseif(WIN32)
    install(TARGETS common
      RUNTIME
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
  endif()
endif()

# precompile header 支持
# Generate precompiled header
if(USE_COREPCH)
  add_cxx_pch(common ${PRIVATE_PCH_HEADER})
endif()
