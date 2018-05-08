find_path ( SPVTOOLS_INCLUDE_DIR_TMP
    NAMES
    libspirv.hpp
    optimizer.hpp

    PATHS
    /usr
    /usr/local
    ${CMAKE_SOURCE_DIR}/libs
    ${NATIVE_LIBRARY_DIR}
    ${COFFEE_ROOT_DIR}

    PATH_SUFFIXES
    include/spirv-tools
    )

find_library ( SPVTOOLS_LIBRARY_TMP
    NAMES SPIRV-Tools

    PATHS
    /usr
    /usr/local
    ${CMAKE_SOURCE_DIR}/libs
    ${NATIVE_LIBRARY_DIR}
    ${COFFEE_ROOT_DIR}

    PATH_SUFFIXES
    lib
    lib/${CMAKE_LIBRARY_ARCHITECTURE}
    lib/${CMAKE_LIBRARY_ARCHITECTURE}/Release
    "lib/${WINDOWS_ABI}"
    )
find_library ( SPVTOOLS_OPT_LIBRARY_TMP
    NAMES SPIRV-Tools-opt

    PATHS
    /usr
    /usr/local
    ${CMAKE_SOURCE_DIR}/libs
    ${NATIVE_LIBRARY_DIR}
    ${COFFEE_ROOT_DIR}

    PATH_SUFFIXES
    lib
    lib/${CMAKE_LIBRARY_ARCHITECTURE}
    lib/${CMAKE_LIBRARY_ARCHITECTURE}/Release
    "lib/${WINDOWS_ABI}"
    )

if(NOT TARGET SPVTools)
    add_library ( SPVTools STATIC IMPORTED )
endif()

set_target_properties( SPVTools PROPERTIES
    IMPORTED_LOCATION ${SPVTOOLS_LIBRARY_TMP}
    INTERFACE_LINK_LIBRARIES ${SPVTOOLS_OPT_LIBRARY_TMP}
    INTERFACE_INCLUDE_DIRECTORIES ${SPVTOOLS_INCLUDE_DIR_TMP}
    )

if(SPVTOOLS_LIBRARY_TMP AND SPVTOOLS_INCLUDE_DIR_TMP)
    set ( SPVTOOLS_FOUND TRUE )
endif()

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPIRVTools
    REQUIRED_VARS
    SPVTOOLS_INCLUDE_DIR_TMP
    SPVTOOLS_LIBRARY_TMP
    SPVTOOLS_OPT_LIBRARY_TMP
    SPVTOOLS_FOUND
    )