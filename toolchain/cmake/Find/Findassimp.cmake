set(ASSIMP_ROOT_DIR "${ASSIMP_ROOT_DIR}"
    CACHE PATH "Directory to search for Assimp")

FIND_PATH( ASSIMP_INCLUDE_DIR_TMP

    assimp/Importer.hpp

    PATHS
    ${CMAKE_BINARY_DIR}/libs/
    ${COFFEE_ROOT_DIR}/libs/
    ${COFFEE_EXT_LIBRARY_DIR}/assimp/
    ${NATIVE_LIBRARY_DIR}

    PATH_SUFFIXES include include

    NO_CMAKE_FIND_ROOT_PATH
    )

FIND_LIBRARY(
    ASSIMP_LIBRARY_TMP
    NAMES assimp
    PATHS ${CMAKE_BINARY_DIR}/libs/lib/ ${COFFEE_ROOT_DIR} ${NATIVE_LIBRARY_DIR}
    PATH_SUFFIXES
    64/link lib lib64
    "lib/${CMAKE_LIBRARY_ARCHITECTURE}" # CMake architecture path
    "lib/${ANDROID_ABI}"
    "${ANDROID_ABI}"

    NO_CMAKE_FIND_ROOT_PATH
    )

FIND_LIBRARY(
    IRRXML_LIBRARIES_TMP
    NAMES IrrXML
    PATHS ${CMAKE_BINARY_DIR}/libs/lib/ ${COFFEE_ROOT_DIR} ${NATIVE_LIBRARY_DIR}
    PATH_SUFFIXES
    64/link lib lib64
    "lib/${CMAKE_LIBRARY_ARCHITECTURE}" # CMake architecture path
    "lib/${ANDROID_ABI}"
    "${ANDROID_ABI}"

    NO_CMAKE_FIND_ROOT_PATH
    )

# Static zlib for platforms shipping without it
# Mainly used for Windows and MinGW*
FIND_LIBRARY(
    ZLIBSTATIC_LIBRARY_TMP
    NAMES zlibstatic
    PATHS ${CMAKE_BINARY_DIR}/libs/lib ${COFFEE_ROOT_DIR} ${NATIVE_LIBRARY_DIR}
    PATH_SUFFIXES
    "lib/${CMAKE_LIBRARY_ARCHITECTURE}"

    NO_CMAKE_FIND_ROOT_PATH
    )

set ( LINK_MODE STATIC )

if(NOT "${ASSIMP_LIBRARIES_TMP}" MATCHES ".a" OR
        NOT "${ASSIMP_LIBRARIES_TMP}" MATCHES ".lib")
    set ( LINK_MODE SHARED )
endif()

if(NOT TARGET Assimp)
    add_library ( Assimp ${LINK_MODE} IMPORTED )
endif()

IF (ASSIMP_INCLUDE_DIR_TMP AND ASSIMP_LIBRARY_TMP)
    SET(ASSIMP_FOUND TRUE)
    SET(ASSIMP_INCLUDE_DIR "${ASSIMP_INCLUDE_DIR_TMP}" CACHE PATH "")

    set_target_properties ( Assimp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INCLUDE_DIR}"
        IMPORTED_LOCATION "${ASSIMP_LIBRARY_TMP}"
        )

    get_filename_component ( ASSIMP_LIBRARIES_TMP "${ASSIMP_LIBRARY_TMP}"
        REALPATH )
    

    if(IRRXML_LIBRARIES_TMP)
        set ( ASSIMP_LIBRARIES_TMP "${ASSIMP_LIBRARIES_TMP};${IRRXML_LIBRARIES_TMP}" )

        SET(IRRXML_LIBRARIES "${IRRXML_LIBRARIES_TMP}" CACHE FILEPATH "")

        add_library ( IrrXML STATIC IMPORTED )
        set_target_properties ( IrrXML PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INCLUDE_DIR_TMP}"
            IMPORTED_LOCATION "${IRRXML_LIBRARIES}"
            )

        set_target_properties ( Assimp PROPERTIES
            INTERFACE_LINK_LIBRARIES IrrXML
            )
    endif()

    if(ZLIBSTATIC_LIBRARY_TMP)
        set ( ASSIMP_LIBRARIES_TMP
            "${ASSIMP_LIBRARIES_TMP};${ZLIBSTATIC_LIBRARY_TMP}" )
        add_library ( ZLibStatic STATIC IMPORTED )
        set_target_properties ( ZLibStatic PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INCLUDE_DIR}"
            IMPORTED_LOCATION "${ZLIBSTATIC_LIBRARY_TMP}"
           )
    endif()
    
    if(IRRXML_LIBRARIES_TMP AND ZLIBSTATIC_LIBRARY_TMP)
        set_target_properties ( Assimp PROPERTIES
            INTERFACE_LINK_LIBRARIES "IrrXML;ZLibStatic"
            )
    endif()

    SET(ASSIMP_LIBRARIES "${ASSIMP_LIBRARIES_TMP}" CACHE FILEPATH "")

ENDIF (ASSIMP_INCLUDE_DIR_TMP AND ASSIMP_LIBRARY_TMP)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(assimp
    REQUIRED_VARS
    ASSIMP_LIBRARIES
    ASSIMP_INCLUDE_DIR)
