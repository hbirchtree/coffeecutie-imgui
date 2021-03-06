set ( LIBDRM_SEARCH_PATHS
    /usr
    /usr/local
    )

find_path ( LIBDRM_INCLUDE_DIR
    drm.h
    PATH_SUFFIXES include/drm/ include/
    PATHS ${LIBDRM_SEARCH_PATHS}
    )

find_library ( LIBDRM_LIB_TMP
    NAMES drm
    PATH_SUFFIXES lib/
    PATHS ${LIBDRM_SEARCH_PATHS}
    )

if(LIBDRM_LIB_TMP AND LIBDRM_INCLUDE_DIR)
    set ( LIBDRM_LIBRARIES "${LIBDRM_LIB_TMP}" CACHE FILEPATH "" )
    set ( LIBDRM_FOUND TRUE )
endif()

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibDRM
    REQUIRED_VARS
    LIBDRM_LIBRARIES
    LIBDRM_INCLUDE_DIR
    )
