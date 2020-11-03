set(SUBPROJ_NAME tonlib-js)
set(${SUBPROJ_NAME}_NAMESPACE tjs)

set(${SUBPROJ_NAME}_CXX_STANDARD 17)
set(${SUBPROJ_NAME}_CXX_EXTENSIONS OFF)
set(${SUBPROJ_NAME}_CXX_STANDARD_REQUIRED YES)

set(${SUBPROJ_NAME}_MAJOR_VERSION 0)
set(${SUBPROJ_NAME}_MINOR_VERSION 0)
set(${SUBPROJ_NAME}_PATCH_VERSION 0)

set(${SUBPROJ_NAME}_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/client.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tl_napi.hpp")

set(${SUBPROJ_NAME}_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/client.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tl_napi.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tonlibjs.cpp")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gen)
set(${SUBPROJ_NAME}_NAPI
        "${CMAKE_CURRENT_SOURCE_DIR}/gen/tonlib_napi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/gen/tonlib_napi.cpp")
set_source_files_properties(${${SUBPROJ_NAME}_NAPI} PROPERTIES GENERATED TRUE)

add_custom_target(generate_napi
        COMMAND ${PROJECT_BINARY_DIR}/bin/napi-gen
        ${TON_SRC_DIR}/tl/generate/scheme/tonlib_api.tlo
        ${CMAKE_CURRENT_SOURCE_DIR}/gen
        ${PROJECT_ROOT_DIR})
add_dependencies(generate_napi napi-gen generate_common)

# ############################################################### #
# Options ####################################################### #
# ############################################################### #

include(OptionHelpers)
generate_basic_options_library(${SUBPROJ_NAME})

# ############################################################### #
# Library version ############################################### #
# ############################################################### #

set(${SUBPROJ_NAME}_VERSION
        ${${SUBPROJ_NAME}_MAJOR_VERSION}.${${SUBPROJ_NAME}_MINOR_VERSION}.${${SUBPROJ_NAME}_PATCH_VERSION})

# ############################################################### #
# Set all target sources ######################################## #
# ############################################################### #

set(
        ${SUBPROJ_NAME}_ALL_SRCS
        ${${SUBPROJ_NAME}_HEADERS}
        ${${SUBPROJ_NAME}_SOURCES}
        ${${SUBPROJ_NAME}_NAPI}
)

# ############################################################### #
# Create target for build ####################################### #
# ############################################################### #

include(NodeHelpers)

# Library target
add_library(${SUBPROJ_NAME} INTERFACE)
add_dependencies(${SUBPROJ_NAME} generate_napi)

target_sources(${SUBPROJ_NAME} INTERFACE ${${SUBPROJ_NAME}_ALL_SRCS})

target_include_directories(
        ${SUBPROJ_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${SUBPROJ_NAME} INTERFACE
        tonlib tdactor adnllite tl_lite_api tl-lite-utils ton_crypto ton_block lite-client-common smc-envelope ftabi)

# Size optimizations
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${SUBPROJ_NAME} INTERFACE -ffunction-sections -fdata-sections)
    target_link_libraries(${SUBPROJ_NAME} INTERFACE -Wl,--gc-sections)
endif ()

# Enable IPO
cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

include(CheckIPOSupported)
check_ipo_supported(RESULT result)
if (result)
    message("INTERPROCEDURAL_OPTIMIZATION enabled")
    set_target_properties(${SUBPROJ_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
else ()
    message("INTERPROCEDURAL_OPTIMIZATION disabled")
endif ()

# ############################################################### #
# NodeJS specific ############################################### #
# ############################################################### #

if (NOT MINIMUM_NODE_ABI)
    set(MINIMUM_NODE_ABI "${NODE_MODULE_MINIMUM_ABI}")
endif ()
if (NOT _CACHE_DIR)
    set(_CACHE_DIR "${NODE_MODULE_CACHE_DIR}")
endif ()

# Obtain a list of current Node versions and retrieves the latest version per ABI
if (NOT EXISTS "${_CACHE_DIR}/node/index.tab")
    _node_module_download(
            "Node.js version list"
            "https://nodejs.org/dist/index.tab"
            "${_CACHE_DIR}/node/index.tab"
    )
endif ()
file(STRINGS "${_CACHE_DIR}/node/index.tab" _INDEX_FILE)
list(REMOVE_AT _INDEX_FILE 0)
set(_ABIS)
foreach (_LINE IN LISTS _INDEX_FILE)
    string(REGEX MATCHALL "[^\t]*\t" _COLUMNS "${_LINE}")
    list(GET _COLUMNS 8 _ABI)
    string(STRIP "${_ABI}" _ABI)
    if ((_ABI GREATER MINIMUM_NODE_ABI OR _ABI EQUAL MINIMUM_NODE_ABI) AND NOT _ABI IN_LIST _EXCLUDE_NODE_ABIS AND NOT DEFINED _NODE_ABI_${_ABI}_VERSION)
        list(APPEND _ABIS ${_ABI})
        list(GET _COLUMNS 0 _VERSION)
        string(STRIP "${_VERSION}" _NODE_ABI_${_ABI}_VERSION)
    endif ()
endforeach ()

# Install Nan
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _NODE_ADDON_API_DIR
        )
string(REPLACE "\n" "" _NODE_ADDON_API_DIR ${_NODE_ADDON_API_DIR})
string(REPLACE "\"" "" _NODE_ADDON_API_DIR ${_NODE_ADDON_API_DIR})

execute_process(COMMAND node -e "require('nan')"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _NAN_API_DIR
        )
string(REPLACE "\n" "" _NAN_API_DIR ${_NAN_API_DIR})
string(REPLACE "\"" "" _NAN_API_DIR ${_NAN_API_DIR})

# Generate a target for every ABI
set(_TARGETS)
foreach (_ABI IN LISTS _ABIS)
    set(_NODE_VERSION ${_NODE_ABI_${_ABI}_VERSION})

    # Download the headers if we don't have them yet
    if (NOT EXISTS "${_CACHE_DIR}/node/${_NODE_VERSION}/node.h")
        _node_module_unpack_tar_gz(
                "headers for Node ${_NODE_VERSION}"
                "https://nodejs.org/download/release/${_NODE_VERSION}/node-${_NODE_VERSION}-headers.tar.gz"
                "node-${_NODE_VERSION}/include/node"
                "${_CACHE_DIR}/node/${_NODE_VERSION}"
        )
    endif ()

    # Generate the library
    set(_TARGET "${SUBPROJ_NAME}.abi-${_ABI}")
    add_library(${_TARGET} SHARED "${_CACHE_DIR}/empty.cpp")
    list(APPEND _TARGETS "${_TARGET}")

    # C identifiers can only contain certain characters (e.g. no dashes)
    string(REGEX REPLACE "[^a-z0-9]+" "_" NAME_IDENTIFIER "${SUBPROJ_NAME}")

    set_target_properties(${_TARGET} PROPERTIES
            OUTPUT_NAME "${_TARGET}"
            SOURCES ""
            PREFIX ""
            SUFFIX ".node"
            MACOSX_RPATH ON
            C_VISIBILITY_PRESET hidden
            CXX_VISIBILITY_PRESET hidden
            POSITION_INDEPENDENT_CODE TRUE

            CXX_STANDARD ${${SUBPROJ_NAME}_CXX_STANDARD}
            CXX_EXTENSIONS ${${SUBPROJ_NAME}_CXX_EXTENSIONS}
            CXX_STANDARD_REQUIRED ${${SUBPROJ_NAME}_CXX_STANDARD_REQUIRED}

            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
            )

    target_compile_definitions(${_TARGET} PRIVATE
            "MODULE_NAME=${NAME_IDENTIFIER}"
            "BUILDING_NODE_EXTENSION"
            "_LARGEFILE_SOURCE"
            "_FILE_OFFSET_BITS=64"
            )

    target_compile_options(${_TARGET} PRIVATE -std=c++17)
    target_include_directories(${_TARGET} SYSTEM PRIVATE
            "${_CACHE_DIR}/node/${_NODE_VERSION}"
            "${PROJECT_ROOT_DIR}/${_NAN_API_DIR}"
            "${_NODE_ADDON_API_DIR}"
            )

    target_link_libraries(${_TARGET} PRIVATE ${SUBPROJ_NAME})

    if (APPLE)
        # Ensures that linked symbols are loaded when the module is loaded instead of causing
        # unresolved symbol errors later during runtime.
        set_target_properties(${_TARGET} PROPERTIES
                LINK_FLAGS "-undefined dynamic_lookup -bind_at_load"
                )
        target_compile_definitions(${_TARGET} PRIVATE
                "_DARWIN_USE_64_BIT_INODE=1"
                )
    else ()
        # Ensures that linked symbols are loaded when the module is loaded instead of causing
        # unresolved symbol errors later during runtime.
        set_target_properties(${_TARGET} PROPERTIES
                LINK_FLAGS "-z now"
                )
    endif ()

    # Copy the file to the installation directory.
    if (INSTALL_PATH)
        get_filename_component(_OUTPUT_PATH "${INSTALL_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        add_custom_command(
                TARGET ${_TARGET}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_TARGET}>" "${_OUTPUT_PATH}/${_TARGET}.node"
        )
    endif ()
endforeach ()

# Add a target that builds all Node ABIs.
add_custom_target("${SUBPROJ_NAME}.all")
add_dependencies("${SUBPROJ_NAME}.all" ${_TARGETS})

# Add a variable that allows users to iterate over all of the generated/dependendent targets.
set("${SUBPROJ_NAME}::abis" "${_ABIS}" PARENT_SCOPE)
set("${SUBPROJ_NAME}::targets" "${_TARGETS}" PARENT_SCOPE)
