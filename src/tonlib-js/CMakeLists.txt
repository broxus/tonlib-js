set(SUBPROJ_NAME tonlib-js)
set(${SUBPROJ_NAME}_NAMESPACE tjs)

set(${SUBPROJ_NAME}_CXX_STANDARD 17)
set(${SUBPROJ_NAME}_CXX_EXTENSIONS OFF)
set(${SUBPROJ_NAME}_CXX_STANDARD_REQUIRED YES)

set(${SUBPROJ_NAME}_MAJOR_VERSION 0)
set(${SUBPROJ_NAME}_MINOR_VERSION 0)
set(${SUBPROJ_NAME}_PATCH_VERSION 0)

set(${SUBPROJ_NAME}_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/tonlibjs.hpp")

set(${SUBPROJ_NAME}_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tonlibjs.cpp")

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
        ${${SUBPROJ_NAME}_SOURCES})

# ############################################################### #
# Create target for build ####################################### #
# ############################################################### #

include(NodeHelpers)

# Library target
add_library(${SUBPROJ_NAME} INTERFACE)

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

cmake_parse_arguments("" "" "MINIMUM_NODE_ABI;NAN_VERSION;INSTALL_PATH;CACHE_DIR" "EXCLUDE_NODE_ABIS" ${ARGN})
if (NOT _MINIMUM_NODE_ABI)
    set(_MINIMUM_NODE_ABI "${NODE_MODULE_MINIMUM_ABI}")
endif ()
if (NOT _CACHE_DIR)
    set(_CACHE_DIR "${NODE_MODULE_CACHE_DIR}")
endif ()
if (NOT _INSTALL_PATH)
    set(_INSTALL_PATH "lib/{node_abi}/${SUBPROJ_NAME}.node")
endif ()
get_filename_component(_CACHE_DIR "${_CACHE_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if (_UNPARSED_ARGUMENTS)
    message(WARNING "[Node.js] Unused arguments: '${_UNPARSED_ARGUMENTS}'")
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
    if ((_ABI GREATER _MINIMUM_NODE_ABI OR _ABI EQUAL _MINIMUM_NODE_ABI) AND NOT _ABI IN_LIST _EXCLUDE_NODE_ABIS AND NOT DEFINED _NODE_ABI_${_ABI}_VERSION)
        list(APPEND _ABIS ${_ABI})
        list(GET _COLUMNS 0 _VERSION)
        string(STRIP "${_VERSION}" _NODE_ABI_${_ABI}_VERSION)
    endif ()
endforeach ()

# Install Nan
if (_NAN_VERSION AND NOT EXISTS "${_CACHE_DIR}/nan/${_NAN_VERSION}/nan.h")
    _node_module_unpack_tar_gz(
            "Nan ${_NAN_VERSION}"
            "https://registry.npmjs.org/nan/-/nan-${_NAN_VERSION}.tgz"
            "package"
            "${_CACHE_DIR}/nan/${_NAN_VERSION}"
    )
endif ()

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

    target_include_directories(${_TARGET} SYSTEM PRIVATE
            "${_CACHE_DIR}/node/${_NODE_VERSION}"
            )

    if (_NAN_VERSION)
        target_compile_options(${_TARGET} PRIVATE -std=c++11)
        target_include_directories(${_TARGET} SYSTEM PRIVATE
                "${_CACHE_DIR}/nan/${_NAN_VERSION}"
                )
    endif ()

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
    #    string(REPLACE "{node_abi}" "node-v${_ABI}" _OUTPUT_PATH "${_INSTALL_PATH}")
    #    get_filename_component(_OUTPUT_PATH "${_OUTPUT_PATH}" ABSOLUTE "${CMAKE_CURRENT_SOURCE_PATH}")
    #    add_custom_command(
    #            TARGET ${_TARGET}
    #            POST_BUILD
    #            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_TARGET}>" "${_OUTPUT_PATH}"
    #    )
endforeach ()

# Add a target that builds all Node ABIs.
add_custom_target("${SUBPROJ_NAME}.all")
add_dependencies("${SUBPROJ_NAME}.all" ${_TARGETS})

# Add a variable that allows users to iterate over all of the generated/dependendent targets.
set("${SUBPROJ_NAME}::abis" "${_ABIS}" PARENT_SCOPE)
set("${SUBPROJ_NAME}::targets" "${_TARGETS}" PARENT_SCOPE)
