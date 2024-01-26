# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

option(SERVERS          "Build worldserver and authserver"                            1)

set(SCRIPTS_AVAILABLE_OPTIONS none static dynamic minimal-static minimal-dynamic)

# 校验 SCRIPTS 参数设置
# Log a fatal error when the value of the SCRIPTS variable isn't a valid option.
if(SCRIPTS)
  list(FIND SCRIPTS_AVAILABLE_OPTIONS "${SCRIPTS}" SCRIPTS_INDEX)
  if(${SCRIPTS_INDEX} EQUAL -1)
    message(FATAL_ERROR "The value (${SCRIPTS}) of your SCRIPTS variable is invalid! "
                        "Allowed values are: ${SCRIPTS_AVAILABLE_OPTIONS} if you still "
                        "have problems search on forum for TCE00019.")
  endif()
endif()

set(SCRIPTS "static" CACHE STRING "Build core with scripts")
set_property(CACHE SCRIPTS PROPERTY STRINGS ${SCRIPTS_AVAILABLE_OPTIONS})

# 1. 遍历 src/server/scripts 目录下的文件夹，每个文件夹则作为一个 script_name
# 2. 创建 SCRIPTS_<script_name> 相关的 cache var，并限制预定义: default disabled static dynamic
# Build a list of all script modules when -DSCRIPT="custom" is selected
GetScriptModuleList(SCRIPT_MODULE_LIST)
foreach(SCRIPT_MODULE ${SCRIPT_MODULE_LIST})
  ScriptModuleNameToVariable(${SCRIPT_MODULE} SCRIPT_MODULE_VARIABLE)
  set(${SCRIPT_MODULE_VARIABLE} "default" CACHE STRING "Build type of the ${SCRIPT_MODULE} module.")
  set_property(CACHE ${SCRIPT_MODULE_VARIABLE} PROPERTY STRINGS default disabled static dynamic)
endforeach()

option(TOOLS            "Build map/vmap/mmap extraction/assembler tools"              1)
option(USE_SCRIPTPCH    "Use precompiled headers when compiling scripts"              1)
option(USE_COREPCH      "Use precompiled headers when compiling servers"              1)
option(WITH_DYNAMIC_LINKING "Enable dynamic library linking."                         0)

# 该段逻辑检测上面设置的 SCRIPTS_<script_name>，只要存在一个需要 dymanic 则设置返回 True，抑或是 SCRIPTS 为 dynamic 也返回 True
IsDynamicLinkingRequired(WITH_DYNAMIC_LINKING_FORCED)

# 依据最终结果设置 CMAKE 内置 BUILD_SHARED_LIBS 变量
if(WITH_DYNAMIC_LINKING AND WITH_DYNAMIC_LINKING_FORCED)
  set(WITH_DYNAMIC_LINKING_FORCED OFF)
endif()
if(WITH_DYNAMIC_LINKING OR WITH_DYNAMIC_LINKING_FORCED)
  set(BUILD_SHARED_LIBS ON)
else()
  set(BUILD_SHARED_LIBS OFF)
endif()

option(WITH_WARNINGS    "Show all warnings during compile"                            0)
option(WITH_COREDEBUG   "Include additional debug-code in core"                       0)
option(WITHOUT_METRICS  "Disable metrics reporting (i.e. InfluxDB and Grafana)"       0)
option(WITH_DETAILED_METRICS  "Enable detailed metrics reporting (i.e. time each session takes to update)" 0)
option(COPY_CONF        "Copy authserver and worldserver .conf.dist files to the project dir"      1)
set(WITH_SOURCE_TREE    "hierarchical" CACHE STRING "Build the source tree for IDE's.")
set_property(CACHE WITH_SOURCE_TREE PROPERTY STRINGS no flat hierarchical hierarchical-folders)
option(WITHOUT_GIT      "Disable the GIT testing routines"                            0)
option(BUILD_TESTING    "Build test suite" 0)

if(UNIX)
  option(USE_LD_GOLD    "Use GNU gold linker"                                        0)
endif()
