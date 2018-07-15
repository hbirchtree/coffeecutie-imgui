project ( coffeeimgui )
cmake_minimum_required(VERSION 3.1)

include( toolchain/cmake/Modules/SubprojectFunctions.cmake )
init_subproject()

find_package ( StandardLibs REQUIRED )
if(COFFEE_BUILD_SDL2)
    find_package ( SDL2 REQUIRED )
endif()
import_coffee_lib ( Coffee REQUIRED )

set ( IMGUI_DIR
    ${CMAKE_SOURCE_DIR}/src/libs/imgui
    )

add_subdirectory(src/imgui)

add_subdirectory(examples/basic)

End_Subproject()
