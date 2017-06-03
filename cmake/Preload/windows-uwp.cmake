# ASIO doesn't work
set ( COFFEE_BUILD_OPENSSL OFF CACHE BOOL "" )

set ( COFFEE_BUILD_ANGLE ON CACHE BOOL "" ) # This is for standalone builds, so that they don't fuck up
set ( COFFEE_BUILD_GLES ON CACHE BOOL "" )
set ( COFFEE_BUILD_OPENAL OFF CACHE BOOL "" )

set ( NATIVE_LIBRARY_DIR ${NATIVE_LIBRARY_DIR} CACHE PATH "" )

set ( SDL2_MAIN_C_FILE "${NATIVE_LIBRARY_DIR}/src/SDL_winrt_main_NonXAML.cpp" CACHE FILEPATH "" )
