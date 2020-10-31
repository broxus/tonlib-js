if (NOT NODE_MODULE_MINIMUM_ABI)
    set(NODE_MODULE_MINIMUM_ABI 46) # Don't build node modules for versions earlier than Node 4
endif ()
if (NOT NODE_MODULE_CACHE_DIR)
    set(NODE_MODULE_CACHE_DIR "${CMAKE_BINARY_DIR}")
endif ()


function(_node_module_download _TYPE _URL _FILE)
    file(REMOVE_RECURSE "${_FILE}")
    string(RANDOM LENGTH 32 _TMP)
    set(_TMP "${CMAKE_BINARY_DIR}/${_TMP}")
    message(STATUS "[Node.js] Downloading ${_TYPE}...")
    file(DOWNLOAD "${_URL}" "${_TMP}" STATUS _STATUS TLS_VERIFY ON)
    list(GET _STATUS 0 _STATUS_CODE)
    if (NOT _STATUS_CODE EQUAL 0)
        file(REMOVE "${_TMP}")
        list(GET _STATUS 1 _STATUS_MESSAGE)
        message(FATAL_ERROR "[Node.js] Failed to download ${_TYPE}: ${_STATUS_MESSAGE}")
    else ()
        get_filename_component(_DIR "${_FILE}" DIRECTORY)
        file(MAKE_DIRECTORY "${_DIR}")
        file(RENAME "${_TMP}" "${_FILE}")
    endif ()
endfunction()


function(_node_module_unpack_tar_gz _TYPE _URL _PATH _DEST)
    string(RANDOM LENGTH 32 _TMP)
    set(_TMP "${CMAKE_BINARY_DIR}/${_TMP}")
    _node_module_download("${_TYPE}" "${_URL}" "${_TMP}.tar.gz")
    file(REMOVE_RECURSE "${_DEST}" "${_TMP}")
    file(MAKE_DIRECTORY "${_TMP}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${_TMP}.tar.gz"
            WORKING_DIRECTORY "${_TMP}"
            RESULT_VARIABLE _STATUS_CODE
            OUTPUT_VARIABLE _STATUS_MESSAGE
            ERROR_VARIABLE _STATUS_MESSAGE)
    if (NOT _STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "[Node.js] Failed to unpack ${_TYPE}: ${_STATUS_MESSAGE}")
    endif ()
    get_filename_component(_DIR "${_DEST}" DIRECTORY)
    file(MAKE_DIRECTORY "${_DIR}")
    file(RENAME "${_TMP}/${_PATH}" "${_DEST}")
    file(REMOVE_RECURSE "${_TMP}" "${_TMP}.tar.gz")
endfunction()
