cmake_minimum_required(VERSION 3.10)

set(_PROJECT_NAME tonlib-js)
set(_PROJECT_LANGUAGE CXX)

set(_PROJECT_MAJOR_VERSION 0)
set(_PROJECT_MINOR_VERSION 0)
set(_PROJECT_PATCH_VERSION 11)

set(MINIMUM_NODE_ABI 72)
set(NAN_VERSION 3.0.2)

set(SUBPROJECT_LIST
    "src/napi-gen"
    "src/tonlib-js")

# Cmake module path
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_ROOT_DIR}/cmake/modules")

set(_PROJECT_VERSION
        ${_PROJECT_MAJOR_VERSION}.${_PROJECT_MINOR_VERSION}.${_PROJECT_PATCH_VERSION})

project(${_PROJECT_NAME} LANGUAGES ${_PROJECT_LANGUAGE} VERSION ${_PROJECT_VERSION})

# Extlibs
set(WITH_GFLAGS OFF)

execute_process(COMMAND node -e "require('ton-cpp-src')"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE TON_SRC_DIR
        )
string(REPLACE "\n" "" TON_SRC_DIR ${TON_SRC_DIR})
string(REPLACE "\"" "" TON_SRC_DIR ${TON_SRC_DIR})
get_filename_component(TON_SRC_DIR "${TON_SRC_DIR}/ton" ABSOLUTE BASE_DIR ${PROJECT_ROOT_DIR})

add_subdirectory(${TON_SRC_DIR} ${PROJECT_BINARY_DIR}/node_modules/ton EXCLUDE_FROM_ALL)

# Targets
foreach (SUBPROJ ${SUBPROJECT_LIST})
    add_subdirectory(${SUBPROJ})
endforeach ()
